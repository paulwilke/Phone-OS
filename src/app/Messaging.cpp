#include "Messaging.h"

#include <cstdio>

#include "SystemEntry.h"
#include "Utf8.h"

namespace {

const char* outcomeText(CommandOutcome o) {
  switch (o) {
    case CommandOutcome::Executed: return "answered";
    case CommandOutcome::Busy: return "busy (a search is running)";
    default: return "refused";
  }
}

}  // namespace

Messaging::Messaging(Clock& clock, Modem& modem, Power& power, MessageStore& store, const Profile& profile,
                     SchoolTime& school, Notices& notices, Haptic& haptic, Logger& log)
    : clock_(clock),
      modem_(modem),
      power_(power),
      store_(store),
      profile_(profile),
      school_(school),
      notices_(notices),
      haptic_(haptic),
      log_(log),
      command_(*this, SmsCommand::Config()) {}

void Messaging::configure(const std::vector<std::string>& triggers) {
  SmsCommand::Config config;
  config.notice = profile_.commandNotice;
  config.triggers = triggers;
  command_.setConfig(config);
}

bool Messaging::radioUsable() { return radio_ != nullptr && profile_.radioAllowed && radio_->ready(); }

uint32_t Messaging::record(MsgSource source, MsgDir dir, const std::string& peer, const std::string& text, bool pending) {
  Message m;
  m.time = clock_.epochUtc();
  m.source = source;
  m.dir = dir;
  m.pending = pending;
  utf8::copy(m.peer, sizeof m.peer, peer.data(), peer.data() + peer.size());
  utf8::copy(m.text, sizeof m.text, text.data(), text.data() + text.size());
  return store_.add(m);
}

bool Messaging::poll() {
  bool modemActive = false;
  SmsMessage sms;
  while (modem_.pollSms(sms)) {
    modemActive = true;
    handleSms(sms);
  }
  handleRadio();
  handleSendFailures(modemActive);
  flushQueue();
  collectDeliveries();
  return modemActive;
}

void Messaging::handleSms(const SmsMessage& sms) {
  // A query is recognised first and never stored as a message.
  if (command_.handle(sms.phone, sms.body) == SmsCommand::Result::Handled) return;
  record(MsgSource::Sms, MsgDir::In, sms.phone, sms.body);
  // In school time only a parent's message makes itself felt.
  if (profile_.messageVibration && (!school_.active() || profile_.contacts.isAdmin(sms.phone.c_str())))
    haptic_.play(HapticPattern::Message);
}

// A profile without the radio gets no radio messages in the history either: otherwise a
// simulator that always attaches the radio would behave differently from the device, which starts
// it only when the profile allows.
void Messaging::handleRadio() {
  if (radio_ == nullptr || !profile_.radioAllowed) return;
  RadioMessage msg;
  while (radio_->poll(msg)) record(MsgSource::Radio, MsgDir::In, msg.peer, msg.text);
}

void Messaging::handleSendFailures(bool& activity) {
  SmsFailure failure;
  while (modem_.pollSendFailure(failure)) {
    activity = true;
    log_.log("SMS to " + nameOf(failure.phone) + " rejected: " + failure.reason);
    // The reply to a parent's query is invisible to the child, its failure too.
    if (hooks_ && hooks_->smsSendFailed(failure.phone)) continue;
    sysentry::record(store_, clock_.epochUtc(), sysentry::kSmsRejected, failure.phone);
    notices_.set(NoticeKind::SmsRejected);
    haptic_.play(HapticPattern::Failed);
  }
}

void Messaging::recordCommand(const CommandRecord& rec, CommandNotice notice) {
  // The log gets every query, whatever the profile says about visibility.
  log_.log("Remote query from " + (rec.phone.empty() ? std::string("?") : rec.phone) + " (" + nameOf(rec.phone) + "): " +
           outcomeText(rec.outcome));
  if (notice == CommandNotice::Silent) return;

  const bool refused = rec.outcome == CommandOutcome::Unauthorized;
  sysentry::record(store_, clock_.epochUtc(), refused ? sysentry::kQueryRefused : sysentry::kLocationQueried, rec.phone);
  if (notice == CommandNotice::Notify) notices_.set(refused ? NoticeKind::QueryRefused : NoticeKind::LocationQueried, rec.phone);
}

// Cellular first, then the radio. The radio has no phone numbers, only contact names: the
// recipient is the radio's own default contact, so none is passed.
bool Messaging::deliver(const std::string& peer, const std::string& text, MsgSource& path) {
  if (registered() && modem_.sendSms(peer, text)) {
    path = MsgSource::Sms;
    return true;
  }
  if (radioUsable() && radio_->send(std::string(), text)) {
    path = MsgSource::Radio;
    return true;
  }
  return false;
}

void Messaging::awaitReport(uint32_t seq) {
  // Beyond the limit the oldest is forgotten: without a report it simply stays unconfirmed.
  if (awaiting_.size() >= kAwaitingSize) awaiting_.pop_front();
  awaiting_.push_back(seq);
}

bool Messaging::sendText(const std::string& peer, const std::string& text) {
  // A text the modem would refuse for ever must not sit in the queue keeping the device awake.
  if (peer.empty() || text.empty() || text.size() > kMaxSmsBytes) return false;

  MsgSource path = MsgSource::Sms;
  if (deliver(peer, text, path)) {
    const uint32_t seq = record(path, MsgDir::Out, peer, text);
    if (path == MsgSource::Radio) {
      awaitReport(seq);
      notices_.set(NoticeKind::MessageSentRadio);
    }
    log_.log("Message sent to " + nameOf(peer) + (path == MsgSource::Radio ? " (radio): " : " (SMS): ") + text.substr(0, 60));
    return true;
  }

  // No path: wait until one comes back.
  if (outbox_.size() >= kOutboxSize) {
    notices_.set(NoticeKind::QueueFull);
    return false;
  }
  const uint32_t seq = record(MsgSource::Sms, MsgDir::Out, peer, text, true);
  outbox_.push_back({peer, text, seq});
  log_.log("Message waiting: no network and no radio, queued");
  notices_.set(NoticeKind::MessageWaiting);
  return true;
}

bool Messaging::sendRadioText(const std::string& peer, const std::string& text) {
  if (text.empty()) return false;
  if (!radioUsable() || !radio_->send(peer, text)) {
    notices_.set(NoticeKind::RadioUnreachable);
    return false;
  }
  const std::string to = peer.empty() ? radio_->defaultPeer() : peer;
  awaitReport(record(MsgSource::Radio, MsgDir::Out, to, text));
  log_.log("Message sent to " + to + " (radio): " + text.substr(0, 60));
  notices_.set(NoticeKind::MessageSentRadio);
  return true;
}

void Messaging::flushQueue() {
  if (outbox_.empty()) return;

  size_t sent = 0, viaRadio = 0;
  std::deque<Waiting> kept;
  for (Waiting& w : outbox_) {
    MsgSource path = MsgSource::Sms;
    if (deliver(w.peer, w.text, path)) {
      store_.markSent(w.seq, path);
      ++sent;
      if (path == MsgSource::Radio) {
        awaitReport(w.seq);
        ++viaRadio;
      }
    } else {
      kept.push_back(std::move(w));
    }
  }
  outbox_ = std::move(kept);
  // Say that what was waiting is out: the old "waits for network" must not stay on the screen.
  if (viaRadio) notices_.set(NoticeKind::MessageSentRadio);
  else if (sent) notices_.set(NoticeKind::MessageSent);
}

// The reports come in the order of sending, so the oldest awaited message gets the first one.
void Messaging::collectDeliveries() {
  if (radio_ == nullptr) return;
  RadioDelivery d;
  while (radio_->pollDelivery(d)) {
    if (awaiting_.empty()) continue;
    const uint32_t seq = awaiting_.front();
    awaiting_.pop_front();
    store_.markDelivered(seq, d.confirmed);
    log_.log(std::string("Message ") + (d.confirmed ? "delivered by radio: " : "not confirmed by radio: ") + d.peer);
    if (!d.confirmed) notices_.set(NoticeKind::RadioUnconfirmed);
  }
}

#include "Emergency.h"

#include "SystemEntry.h"

Emergency::Emergency(Clock& clock, Positioning& positioning, Messaging& messaging, SchoolTime& school,
                     MessageStore& store, const Profile& profile, i18n::Translator& translator, Haptic& haptic,
                     Logger& log)
    : clock_(clock), positioning_(positioning), messaging_(messaging), school_(school), store_(store), profile_(profile),
      translator_(translator), haptic_(haptic), log_(log), sos_(*this, SosPing::Config()), flow_(*this) {}

bool Emergency::begin(const std::string& target) {
  return flow_.begin(profile_.sosRecipient(target), school_.active());
}

// A template as the catalog has it, placeholders and all: every placeholder is given back as
// itself, so that SosPing fills it with the position later.
static std::string raw(i18n::Translator& translator, const std::string& language, const char* key) {
  const i18n::Arg args[] = {{"name", "{name}"},     {"head", "{head}"},         {"map", "{map}"},
                            {"coordinates", "{coordinates}"}, {"accuracy", "{accuracy}"}, {"seconds", "{seconds}"}};
  return translator.getIn(language, key, args, 6);
}

SosPing::Wording Emergency::wording() {
  const std::string& language = profile_.smsLanguage;
  SosPing::Wording w;
  w.emergency = raw(translator_, language, "sms.sos.emergency");
  w.emergencyAnonymous = raw(translator_, language, "sms.sos.emergency-anonymous");
  w.pickup = raw(translator_, language, "sms.sos.pickup");
  w.pickupAnonymous = raw(translator_, language, "sms.sos.pickup-anonymous");
  w.location = raw(translator_, language, "sms.sos.location");
  w.locationAnonymous = raw(translator_, language, "sms.sos.location-anonymous");
  w.position = raw(translator_, language, "sms.sos.position");
  w.positionNoMap = raw(translator_, language, "sms.sos.position-no-map");
  w.noGps = raw(translator_, language, "sms.sos.no-gps");
  return w;
}

bool Emergency::start(const std::string& peer, SosPing::Kind kind) {
  // The name and the wording can change with the profile while the device runs, so they are
  // read at every start and not once.
  sos_.setWho(profile_.childName);
  sos_.setMapTemplate(profile_.mapTemplate);
  sos_.setWording(wording());
  if (!sos_.start(peer.c_str(), clock_.uptimeSeconds(), kind)) return false;
  syncReceiver();
  const std::string name = profile_.contacts.displayName(peer.c_str());
  if (kind == SosPing::Kind::Query) log_.log("Remote query: searching, answer to " + name);
  else log_.log(std::string("SOS ping (") + (kind == SosPing::Kind::Pickup ? "pickup" : "emergency") + ") started to " + name);
  return true;
}

bool Emergency::startPing(const std::string& target, SosPing::Kind kind) { return start(target, kind); }

bool Emergency::startLocationReply(const std::string& phone) {
  // SosPing refuses a query while any search runs.
  return start(phone, SosPing::Kind::Query);
}

bool Emergency::sendText(const char* peer, const std::string& text) {
  sentTo_.clear();
  std::vector<std::string> to{peer};
  if (!sos_.isQuery()) {
    // The ping's peer is the first SOS recipient, unless the screen named another target: then that one alone.
    const std::vector<std::string> all = profile_.sosRecipients();
    if (!all.empty() && all.front() == peer) to = all;
  }
  for (const std::string& recipient : to) {
    if (messaging_.sendText(recipient, text)) sentTo_.push_back(recipient);
    else log_.log(std::string("SOS text not taken for ") + profile_.contacts.displayName(recipient.c_str()));
  }
  if (to.size() > 1)
    log_.log("SOS text to " + std::to_string(sentTo_.size()) + " of " + std::to_string(to.size()) + " recipients");
  return !sentTo_.empty();
}

bool Emergency::noteSendFailed(const std::string& phone) {
  const bool queryReply = sos_.stage() == SosPing::Stage::Done && sos_.isQuery() && sos_.peer() == phone;
  if (sos_.stage() == SosPing::Stage::Done && !sos_.isQuery()) {
    // One refused text of an emergency to several is not a failed emergency while another went out.
    bool ours = false;
    for (size_t i = 0; i < sentTo_.size() && !ours; ++i) {
      if (sentTo_[i] != phone) continue;
      sentTo_.erase(sentTo_.begin() + static_cast<long>(i));
      ours = true;
    }
    if (!ours) return false;
    if (!sentTo_.empty()) {
      // The child is told nothing: help is on its way through the others, and a "failed" now would frighten.
      log_.log(std::string("SOS text refused for ") + profile_.contacts.displayName(phone.c_str()) + "; the others went out");
      return true;
    }
    sos_.noteSendFailed(sos_.peer().c_str());   // every text was refused: the emergency failed
  } else {
    sos_.noteSendFailed(phone.c_str());
  }
  if (sos_.stage() == SosPing::Stage::Failed && lastStage_ == SosPing::Stage::Done) {
    // The stage changes here and not in poll(), so it is told here: Messaging has vibrated and
    // noticed this failure already (an emergency), or must stay silent (a query reply).
    lastStage_ = SosPing::Stage::Failed;
    log_.log(queryReply ? "Remote query answer FAILED" : "SOS ping FAILED: the network refused the text");
    if (queryReply) recordQueryFailure(phone);
  }
  return queryReply;
}

// A reply that never left is something the parents want to know, and a parent who stands in front
// of the device sees it only in the history. The profile's rule for how visible a remote query is
// applies to its failure as well: a query that is kept out of the history stays out, with its
// failure. The child gets no notice and no vibration, as for the reply itself.
void Emergency::recordQueryFailure(const std::string& peer) {
  if (profile_.commandNotice == CommandNotice::Silent) return;
  sysentry::record(store_, clock_.epochUtc(), sysentry::kLocationQueryFailed, peer);
}

// The receiver follows the ping: one reason for an emergency or pickup, another for a query
// reply, none once it is over. Called after every change, so there is no gap in between.
void Emergency::syncReceiver() {
  positioning_.want(Positioning::Reason::SosPing, sos_.active() && !sos_.isQuery());
  positioning_.want(Positioning::Reason::RemoteQuery, sos_.active() && sos_.isQuery());
}

void Emergency::poll() {
  flow_.tick();
  sos_.poll(clock_.uptimeSeconds());

  // The outcome is told when the stage changes, not continuously.
  const SosPing::Stage stage = sos_.stage();
  if (stage != lastStage_) {
    const bool done = stage == SosPing::Stage::Done;
    const bool failed = stage == SosPing::Stage::Failed;
    if (sos_.isQuery()) {
      if (done) log_.log("Remote query answer sent");
      else if (failed) {
        log_.log("Remote query answer FAILED");
        recordQueryFailure(sos_.peer());
      }
    } else if (done) {
      haptic_.play(HapticPattern::Sent);
      log_.log("SOS ping sent");
    } else if (failed) {
      haptic_.play(HapticPattern::Failed);
      log_.log("SOS ping FAILED");
    }
    lastStage_ = stage;
  }
  syncReceiver();
}

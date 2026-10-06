#pragma once

// Messages in and out. Owns the routing rule and everything that follows from it.
//
// **Incoming.** An SMS from a number that is not in the contact book (or a call target), from a
// sender that is no number at all, or from nobody, is dropped before anything else happens: not
// stored, not counted, no vibration, no notice, not offered to the remote query; one log line
// with the number and never the text. Then an SMS is offered to the remote query (SmsCommand): a query is never
// stored as a message, it is logged, written to the history as a system entry and perhaps shown
// as a notice, as the profile says, and it never vibrates. Anything else is stored unread and
// vibrates the message pattern — in school time only for a parent. A radio message is stored only
// when the profile allows the radio.
//
// **Outgoing: cellular, else radio, else the queue.** A message goes by cellular when the device
// is registered; otherwise by the radio when the profile allows it and it is ready (the radio
// knows contacts by name, not by number, so it takes its own default contact); otherwise it waits
// in a queue of kOutboxSize and a notice says so. The queue is tried at every poll, and a message
// waiting with no path at all does not count as work: it cannot leave, and staying awake for it
// helps nobody.
//
// **After sending.** A radio message is later confirmed or not by the radio; the reports come in
// the order of sending, so the oldest awaited message gets the first report (at most
// kAwaitingSize are awaited; the oldest is forgotten when a fifth is sent). An SMS gets no report
// and no state is claimed for it. A message the modem accepted and the network then refused is
// reported once by the modem: it is logged with the modem's reason, written to the history,
// shown as a notice and vibrated, and the SOS ping is told — except the reply to a parent's query,
// which stays out of the child's sight and is only logged.

#include <deque>
#include <string>
#include <vector>

#include "../hal/Clock.h"
#include "../hal/Haptic.h"
#include "../hal/Modem.h"
#include "../hal/Power.h"
#include "../hal/RadioLink.h"
#include "Cellular.h"
#include "DropLog.h"
#include "Logger.h"
#include "MessageStore.h"
#include "Notices.h"
#include "Profile.h"
#include "SchoolTime.h"
#include "SmsCommand.h"

class Messaging : private CommandHost {
public:
  static constexpr size_t kOutboxSize = 8;
  static constexpr size_t kAwaitingSize = 4;

  // What Messaging asks of the rest of the core.
  class Hooks {
  public:
    virtual ~Hooks() = default;
    // A parent's query asks for the position: start the search and the reply. False when one is
    // running already.
    virtual bool startLocationReply(const std::string& phone) = 0;
    // The network refused a message the modem had accepted. True when the message was the
    // silent reply to a parent's query: the child is then told nothing (no history entry, notice
    // or vibration).
    virtual bool smsSendFailed(const std::string& phone) = 0;
  };

  Messaging(Clock& clock, Modem& modem, Power& power, MessageStore& store, const Profile& profile, SchoolTime& school,
            Notices& notices, Haptic& haptic, Logger& log, DropLog& drops);

  // The radio is optional. The profile decides whether it may be used.
  void setRadio(RadioLink* radio) { radio_ = radio; }
  void setHooks(Hooks* hooks) { hooks_ = hooks; }
  // Reads what the remote query depends on: the words that trigger it (the trigger word of every
  // language, and the profile's extra words) and how visible it is on the device (the profile's
  // command notice). Call again when the languages or the profile change.
  void configure(const std::vector<std::string>& triggers);

  // Drains incoming SMS, radio messages, send failures and delivery reports, and tries the queue.
  // True when the modem did something (a message arrived or a send failed), which proves that it
  // is awake.
  bool poll();

  // Sends a message by the best path, or queues it. False when the recipient or text is empty or
  // the queue is full.
  bool sendText(const std::string& peer, const std::string& text);
  // Sends over the radio and nothing else, to a named radio contact ("" = the default one).
  bool sendRadioText(const std::string& peer, const std::string& text);

  // Removes a message from the history. A message still waiting in the queue is **not** cancelled:
  // the history is a record, the queue is a promise made to the user, and an emergency text that
  // has not left must not be lost to a tap on delete.
  bool remove(uint32_t seq) { return store_.remove(seq); }

  size_t pending() const { return outbox_.size(); }
  // Is there queued work the device should stay awake for? Only if the queue could leave: the
  // modem is powered (it will register and the queue goes) or the radio is usable. A message that
  // waits with no path at all cannot leave, and staying awake for it helps nobody.
  bool hasWork() { return !outbox_.empty() && (power_.isEnabled(PowerDomain::Cellular) || radioUsable()); }
  // Can the radio carry a message now: attached, allowed by the profile, and ready.
  bool radioUsable();
  bool registered() { return cellular::registered(power_, modem_); }

  MessageStore& store() { return store_; }

private:
  struct Waiting {
    std::string peer, text;
    uint32_t seq;
  };

  // CommandHost
  bool isAdmin(const std::string& phone) override { return profile_.contacts.isAdmin(phone.c_str()); }
  bool startLocationReply(const std::string& phone) override { return hooks_ && hooks_->startLocationReply(phone); }
  uint32_t uptimeSeconds() override { return clock_.uptimeSeconds(); }
  uint32_t epochUtc() override { return clock_.epochUtc(); }
  void recordCommand(const CommandRecord& record, CommandNotice notice) override;

  void handleSms(const SmsMessage& sms);
  void handleRadio();
  void handleSendFailures(bool& activity);
  void collectDeliveries();
  void flushQueue();
  bool deliver(const std::string& peer, const std::string& text, MsgSource& path);
  uint32_t record(MsgSource source, MsgDir dir, const std::string& peer, const std::string& text, bool pending = false);
  void awaitReport(uint32_t seq);
  std::string nameOf(const std::string& phone) const { return profile_.contacts.displayName(phone.c_str()); }

  Clock& clock_;
  Modem& modem_;
  Power& power_;
  MessageStore& store_;
  const Profile& profile_;
  SchoolTime& school_;
  Notices& notices_;
  Haptic& haptic_;
  Logger& log_;
  DropLog& drops_;
  RadioLink* radio_ = nullptr;
  Hooks* hooks_ = nullptr;
  SmsCommand command_;
  std::deque<Waiting> outbox_;
  std::deque<uint32_t> awaiting_;
};

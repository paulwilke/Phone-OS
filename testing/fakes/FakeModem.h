#pragma once

// A modem that behaves like the contract says and is driven by the test.
//
// It settles at once: begin() leaves it Ready when a network and a usable SIM are
// there, Registering without a network, Error with a locked or missing SIM.
//
// Scripting the network and the SIM:
//   setNetwork(operatorName, bars, voice = true)   empty name = no network
//   setSimStatus(status)
// Scripting what arrives:
//   injectSms(phone, body, timestamp)
//   injectIncomingCall(phone)              false when it cannot ring (not Ready, no voice)
// Scripting the other side of a call (events come in the order a real modem delivers them):
//   remoteAnswer()    a dialled call connects          -> Connected
//   remoteBusy()      the number is busy               -> Busy
//   remoteNoAnswer()  nobody answers                   -> NoAnswer
//   failDial(reason)  the network refuses the call     -> DialFailed
//   remoteHangup()    the other side ends: connected -> Ended, ringing -> Missed,
//                     dialling -> NoAnswer
// Scripting failures:
//   failNextSms(reason, count = 1)   the next messages are accepted and then reported
//                                    as failed, once each, with the modem's reason
//   setQueryAnswer(command, answer)  what query() will return; unknown commands give "OK"
//   setVolumeSupported(bool)
// Reading back: sent() (accepted messages), asleep(), callPhone().
// Powering down ends a call: connected -> Ended, ringing -> Missed, dialling -> Ended.

#include <deque>
#include <map>
#include <string>
#include <vector>

#include <hal/Clock.h>
#include <hal/Modem.h>

class FakeModem : public Modem {
public:
  struct SentSms {
    std::string phone;
    std::string body;
  };

  explicit FakeModem(Clock& clock) : clock_(clock) {}

  void begin() override {
    if (on_) return;
    on_ = true;
    settle();
  }
  void shutdown() override {
    if (state_ == ModemState::InCall) finish(CallEventType::Ended, true);
    else if (state_ == ModemState::RingingIn) finish(CallEventType::Missed, false);
    else if (state_ == ModemState::Dialing) finish(CallEventType::Ended, false);
    on_ = false;
    state_ = ModemState::Off;
  }
  void sleep() override { asleep_ = true; }
  void wake() override { asleep_ = false; }

  bool sendSms(const std::string& phone, const std::string& body) override {
    if (!canSend() || phone.empty() || body.empty() || body.size() > kMaxSmsBytes) return false;
    sent_.push_back({phone, body});
    if (failingSms_ > 0) {
      --failingSms_;
      failures_.push_back({phone, failReason_});
    }
    return true;
  }
  bool pollSms(SmsMessage& out) override {
    if (!on_ || inbox_.empty()) return false;
    out = inbox_.front();
    inbox_.pop_front();
    return true;
  }
  bool pollSendFailure(SmsFailure& out) override {
    if (failures_.empty()) return false;
    out = failures_.front();
    failures_.pop_front();
    return true;
  }

  bool dial(const std::string& phone) override {
    if (state_ != ModemState::Ready || !voice_ || phone.empty()) return false;
    callPhone_ = phone;
    state_ = ModemState::Dialing;
    return true;
  }
  bool answer() override {
    if (state_ != ModemState::RingingIn) return false;
    connect();
    return true;
  }
  bool hangup() override {
    if (state_ == ModemState::InCall) finish(CallEventType::Ended, true);
    else if (state_ == ModemState::RingingIn) finish(CallEventType::Missed, false);
    else if (state_ == ModemState::Dialing) finish(CallEventType::Ended, false);
    else return false;
    return true;
  }
  bool pollCallEvent(CallEvent& out) override {
    if (events_.empty()) return false;
    out = events_.front();
    events_.pop_front();
    return true;
  }

  // Levels outside 0 … 7 are clamped, as the real modem's own range is.
  bool setCallVolume(int level) override {
    if (!volumeSupported_) return false;
    volume_ = level < 0 ? 0 : (level > 7 ? 7 : level);
    return true;
  }
  int callVolume() const override { return volumeSupported_ ? volume_ : -1; }

  bool query(const std::string& command) override {
    // Only read forms: "AT+" something and a question mark at the end, no assignment.
    if (command.size() < 5 || command.compare(0, 3, "AT+") != 0 || command.back() != '?') return false;
    if (command.find('=') != std::string::npos || command.find(';') != std::string::npos) return false;
    const auto it = answers_.find(command);
    pendingAnswer_ = it == answers_.end() ? "OK" : it->second;
    hasAnswer_ = true;
    return true;
  }
  bool pollQueryResult(std::string& out) override {
    if (!hasAnswer_) return false;
    out = pendingAnswer_;
    hasAnswer_ = false;
    return true;
  }

  ModemState state() const override { return state_; }
  int signalBars() const override { return registered() ? bars_ : 0; }
  bool voiceAvailable() const override { return voice_; }
  std::string operatorName() const override { return registered() ? operator_ : ""; }
  SimStatus simStatus() const override { return sim_; }

  // Scripting.
  void setNetwork(const std::string& operatorName, int bars, bool voice = true) {
    operator_ = operatorName;
    bars_ = bars;
    voice_ = voice;
    settle();
  }
  void setSimStatus(SimStatus status) {
    sim_ = status;
    settle();
  }
  void injectSms(const std::string& phone, const std::string& body, uint32_t timestamp) {
    inbox_.push_back({phone, body, timestamp});
  }
  bool injectIncomingCall(const std::string& phone) {
    if (state_ != ModemState::Ready || !voice_) return false;
    callPhone_ = phone;
    state_ = ModemState::RingingIn;
    push(CallEventType::Incoming, 0, "");
    return true;
  }
  // The caller's number becomes known after the ring has begun (caller identification arrives
  // late, as on the real modem): the call is reported again with the number. Only while a call
  // rings.
  bool announceCaller(const std::string& phone) {
    if (state_ != ModemState::RingingIn) return false;
    callPhone_ = phone;
    push(CallEventType::Incoming, 0, "");
    return true;
  }
  void remoteAnswer() {
    if (state_ == ModemState::Dialing) connect();
  }
  void remoteBusy() { endDialled(CallEventType::Busy, ""); }
  void remoteNoAnswer() { endDialled(CallEventType::NoAnswer, ""); }
  void failDial(const std::string& reason) { endDialled(CallEventType::DialFailed, reason); }
  void remoteHangup() {
    if (state_ == ModemState::InCall) finish(CallEventType::Ended, true);
    else if (state_ == ModemState::RingingIn) finish(CallEventType::Missed, false);
    else if (state_ == ModemState::Dialing) finish(CallEventType::NoAnswer, false);
  }
  void failNextSms(const std::string& reason, int count = 1) {
    failReason_ = reason;
    failingSms_ = count;
  }
  void setQueryAnswer(const std::string& command, const std::string& answer) { answers_[command] = answer; }
  void setVolumeSupported(bool supported) { volumeSupported_ = supported; }

  const std::vector<SentSms>& sent() const { return sent_; }
  bool asleep() const { return asleep_; }
  const std::string& callPhone() const { return callPhone_; }

private:
  bool simUsable() const { return sim_ == SimStatus::Ready || sim_ == SimStatus::Unknown; }
  bool inCallState() const {
    return state_ == ModemState::Dialing || state_ == ModemState::RingingIn || state_ == ModemState::InCall;
  }
  bool registered() const { return on_ && simUsable() && !operator_.empty() && (state_ == ModemState::Ready || inCallState()); }
  bool canSend() const { return registered(); }

  // The state of an idle modem follows the SIM and the network.
  void settle() {
    if (!on_ || inCallState()) return;
    if (!simUsable()) state_ = ModemState::Error;
    else if (operator_.empty()) state_ = ModemState::Registering;
    else state_ = ModemState::Ready;
  }
  void push(CallEventType type, uint32_t duration, const std::string& reason) {
    events_.push_back({type, callPhone_, duration, reason});
  }
  void connect() {
    state_ = ModemState::InCall;
    connectedAtMs_ = clock_.uptimeMillis();
    push(CallEventType::Connected, 0, "");
  }
  // Ends the current call with `type`; `connected` says whether to report its length.
  void finish(CallEventType type, bool connected) {
    const uint32_t duration = connected ? (uint32_t)((clock_.uptimeMillis() - connectedAtMs_) / 1000) : 0;
    push(type, duration, "");
    callPhone_.clear();
    state_ = ModemState::Ready;
    settle();
  }
  void endDialled(CallEventType type, const std::string& reason) {
    if (state_ != ModemState::Dialing) return;
    push(type, 0, reason);
    callPhone_.clear();
    state_ = ModemState::Ready;
    settle();
  }

  Clock& clock_;
  bool on_ = false;
  ModemState state_ = ModemState::Off;
  std::string operator_ = "Provider";
  int bars_ = 4;
  bool voice_ = true;
  SimStatus sim_ = SimStatus::Ready;
  int volume_ = 4;
  bool volumeSupported_ = true;
  bool asleep_ = false;
  std::deque<SmsMessage> inbox_;
  std::deque<SmsFailure> failures_;
  std::deque<CallEvent> events_;
  std::vector<SentSms> sent_;
  std::string callPhone_;
  uint64_t connectedAtMs_ = 0;
  int failingSms_ = 0;
  std::string failReason_;
  std::map<std::string, std::string> answers_;
  std::string pendingAnswer_;
  bool hasAnswer_ = false;
};

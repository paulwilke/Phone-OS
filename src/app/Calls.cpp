#include "Calls.h"

#include "PhoneNumber.h"
#include "SystemEntry.h"

std::string Calls::nameOf(const std::string& phone) const {
  if (phone.empty()) return "unknown number";
  return profile_.contacts.displayName(phone.c_str());
}

bool Calls::callerAllowed() const { return !peer_.empty() && profile_.mayReceiveFrom(peer_); }

ModemState Calls::state() const {
  const ModemState s = modem_.state();
  return (s == ModemState::RingingIn && !callerAllowed()) ? ModemState::Ready : s;
}

bool Calls::modemBusy() const {
  const ModemState s = modem_.state();
  return s == ModemState::Dialing || s == ModemState::RingingIn || s == ModemState::InCall;
}

bool Calls::active() const {
  const ModemState s = state();
  return s == ModemState::Dialing || s == ModemState::RingingIn || s == ModemState::InCall;
}

void Calls::rejectUnknown(const std::string& number) {
  rejectedUnknown_ = true;
  peer_.clear();
  drops_.callRejected(number, profile_.contacts.countryCode());   // one line per caller per ten minutes
  modem_.hangup();
}

void Calls::clearCall() {
  peer_.clear();
  dialedOut_ = connected_ = hangupRequested_ = false;
}

bool Calls::refuseInSchoolTime() {
  int end = 0;
  if (!school_.active(&end)) return false;
  notices_.set(NoticeKind::SchoolTimeUntil, std::string(), end);
  return true;
}

bool Calls::call(const std::string& number) {
  if (refuseInSchoolTime()) return false;
  if (!profile_.mayCall(number)) {
    notices_.set(NoticeKind::CallNotAllowed);
    return false;
  }
  if (!power_.isEnabled(PowerDomain::Cellular)) {
    notices_.set(NoticeKind::CellularIsOff);
    return false;
  }
  if (!cellular::registered(power_, modem_)) {
    notices_.set(NoticeKind::NoNetwork);
    return false;
  }
  if (!modem_.dial(number)) {
    log_.log("Call to " + nameOf(number) + " refused by the modem");
    return false;
  }
  peer_ = number;
  dialedOut_ = true;
  connected_ = hangupRequested_ = false;
  return true;
}

bool Calls::hangup() {
  const bool ok = modem_.hangup();
  if (ok) {
    hangupRequested_ = true;  // the child ended it: nothing to report
    peer_.clear();
  }
  return ok;
}

bool Calls::changeVolume(int delta) {
  const int current = modem_.callVolume();
  if (modem_.state() != ModemState::InCall) return false;
  int wanted = current + delta;
  if (wanted < 0) wanted = 0;
  if (wanted > kMaxVolume) wanted = kMaxVolume;
  if (wanted == current) return false;
  return modem_.setCallVolume(wanted);
}

bool Calls::poll() {
  bool activity = false;
  CallEvent ev;
  while (modem_.pollCallEvent(ev)) {
    activity = true;
    handleEvent(ev);
  }
  ring();
  return activity;
}

void Calls::handleIncoming(const CallEvent& ev) {
  // A number-less call is decided in ring(), after the modem has had time to name the caller.
  if (ev.phone.empty()) {
    peer_.clear();
    return;
  }
  if (!profile_.mayReceiveFrom(ev.phone)) {
    if (!rejectedUnknown_) {
      rejectUnknown(ev.phone);
    }
    return;
  }
  peer_ = ev.phone;
  dialedOut_ = connected_ = hangupRequested_ = false;
  log_.log("Call from " + nameOf(ev.phone));
  // Decided with the number, so that the notice can name the caller.
  if (school_.active() && !rejectedInSchool_) {
    rejectedInSchool_ = true;
    notices_.set(NoticeKind::CallerInSchoolTime, ev.phone);
    log_.log("Call rejected in quiet hours: " + nameOf(ev.phone));
    if (profile_.quietCallVibration) haptic_.play(HapticPattern::Ring);
    modem_.hangup();
  }
}

void Calls::handleEvent(const CallEvent& ev) {
  // What the modem says about a call from nobody we know, one we rejected, is not for the
  // history, the notices or the log: the child never learns that it happened. A call we dialled is
  // about a number we allowed.
  if (ev.type != CallEventType::Incoming && !dialedOut_) {
    const std::string& who = ev.phone.empty() ? peer_ : ev.phone;
    if (!profile_.mayReceiveFrom(who)) return;
  }
  switch (ev.type) {
    case CallEventType::Incoming:
      handleIncoming(ev);
      break;
    case CallEventType::Connected:
      connected_ = true;
      log_.log("Call connected: " + nameOf(ev.phone));
      break;
    case CallEventType::Missed:
      sysentry::record(store_, clock_.epochUtc(), sysentry::kMissedCall, ev.phone);
      log_.log("Missed call from " + nameOf(ev.phone));
      peer_.clear();
      break;
    case CallEventType::Busy:
      reportFailed(ev, sysentry::kBusy);
      break;
    case CallEventType::NoAnswer:
      reportFailed(ev, sysentry::kNoAnswer);
      break;
    case CallEventType::DialFailed:
      reportFailed(ev, nullptr);
      break;
    case CallEventType::Ended:
      // A call that never connected is not a conversation that ended.
      if (dialedOut_ && !connected_ && !hangupRequested_) {
        reportFailed(ev, nullptr);
        break;
      }
      log_.log("Call ended: " + nameOf(ev.phone));
      clearCall();
      break;
  }
}

// `reason` is the one the event itself names; without one it is derived from the state of the
// network: first "no network", then "SMS only", then plainly "not connected".
void Calls::reportFailed(const CallEvent& ev, const char* reason) {
  std::string why;
  if (reason) why = reason;
  else if (!cellular::registered(power_, modem_)) why = sysentry::kNoNetwork;
  else if (!modem_.voiceAvailable()) why = sysentry::kSmsOnly;
  else why = sysentry::kNotConnected;

  const std::string& who = ev.phone.empty() ? peer_ : ev.phone;
  NoticeKind kind = NoticeKind::CallFailed;
  if (why == sysentry::kBusy) kind = NoticeKind::CallBusy;
  else if (why == sysentry::kNoAnswer) kind = NoticeKind::CallNoAnswer;
  else if (why == sysentry::kNoNetwork) kind = NoticeKind::CallNoNetwork;
  else if (why == sysentry::kSmsOnly) kind = NoticeKind::CallSmsOnly;
  notices_.set(kind, who);
  haptic_.play(HapticPattern::Failed);
  sysentry::record(store_, clock_.epochUtc(), sysentry::kCallFailed, who, why);
  // The modem's own words, for whoever looks later.
  log_.log("Call to " + nameOf(who) + " failed: " + why + (ev.reason.empty() ? "" : " | modem: " + ev.reason));
  clearCall();
}

// Ringing means vibrating, but only for a caller who is allowed. A call that shows no number is
// hung up after kQuietRejectSeconds, whatever the time of day.
void Calls::ring() {
  const bool nowRinging = modem_.state() == ModemState::RingingIn;
  const uint32_t now = clock_.uptimeSeconds();
  if (!nowRinging) {
    ringing_ = false;
    ringSeen_ = false;
    rejectedInSchool_ = false;
    rejectedUnknown_ = false;
    return;
  }
  if (!ringSeen_) {
    ringSeen_ = true;
    ringStarted_ = now;
  }

  // Nobody is known yet: nothing rings. The number never came (caller identification is off or
  // the number is withheld): hang up all the same, or the call would ring on unnoticed.
  if (!callerAllowed()) {
    if (now - ringStarted_ >= kQuietRejectSeconds && !rejectedUnknown_) rejectUnknown(std::string());
    return;
  }
  const bool school = school_.active();
  // School time has priority over everything else for the vibration setting.
  const bool vibrate = school ? profile_.quietCallVibration : profile_.callVibration;
  if (!vibrate) return;
  if (!ringing_ || now - lastRing_ >= kRingIntervalSeconds) {
    ringing_ = true;
    lastRing_ = now;
    haptic_.play(HapticPattern::Ring);
  }
}

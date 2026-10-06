#pragma once

// Calls: the rules for dialling and answering, what school time does to them, and what the child
// is told when a call does not happen.
//
// **Dialling** is allowed only outside school time, to an allowed number (the contact book or a
// call action), with cellular switched on and the device registered. Each refusal is a notice
// saying which.
//
// **Only the contact book may call the child** (Profile::mayReceiveFrom). A call from any other
// number, or with no number, is hung up at once: no ring, no vibration, no notice, no history
// entry, no missed call, and nothing on the screen — until it is over the call does not exist for
// the rest of the core (state() says Ready and peer() is empty). One log line says that it
// happened. The modem reports a call twice, first without a number and then with it; the decision
// is taken with the number, and a call that never shows one is rejected after kQuietRejectSeconds,
// at every time of day. Nothing rings before the caller is known to be allowed.
//
// **Incoming calls from contacts in school time are all rejected, parents included, without
// ringing** (decided 2026-10-02). What the child sees is a notice naming who tried, and the call
// is in the history as a missed call. Vibration for a call in school time is a setting of its own
// that defaults to off.
//
// **Outside school time** a ringing call vibrates the ring pattern every kRingIntervalSeconds
// until it stops: the device is mostly in a pocket and a sound in a lesson is the last thing
// anybody wants.
//
// **A call that did not happen** is told in one sentence, because the child cannot say why it
// failed and the parents find the reason in the history later: busy, no answer, no network, a
// network that allows SMS only, or simply not connected. The reason is derived when the modem
// only says that the dial failed or that the call ended without ever being connected: first "no
// network", then "SMS only", then "not connected". A call the child ended while it was still
// dialling is not a failure.

#include <string>

#include "../hal/Clock.h"
#include "../hal/Haptic.h"
#include "../hal/Modem.h"
#include "../hal/Power.h"
#include "Cellular.h"
#include "Logger.h"
#include "MessageStore.h"
#include "Notices.h"
#include "Profile.h"
#include "SchoolTime.h"

class Calls {
public:
  // A ringing call vibrates every two seconds: more often costs current, less often and the call
  // is overheard.
  static constexpr uint32_t kRingIntervalSeconds = 2;
  // A call without a number is rejected after this long.
  static constexpr uint32_t kQuietRejectSeconds = 3;
  static constexpr int kMaxVolume = 7;

  Calls(Clock& clock, Modem& modem, Power& power, MessageStore& store, const Profile& profile, SchoolTime& school,
        Notices& notices, Haptic& haptic, Logger& log)
      : clock_(clock),
        modem_(modem),
        power_(power),
        store_(store),
        profile_(profile),
        school_(school),
        notices_(notices),
        haptic_(haptic),
        log_(log) {}

  // Call events and the ring. True when the modem reported something (it is awake then).
  bool poll();

  // Says "school time until HH:MM" and returns true when school time is running: for everything
  // the child starts that school time forbids.
  bool refuseInSchoolTime();

  // Dials. False, with a notice, when the rules forbid it or the modem will not.
  bool call(const std::string& number);
  bool answer() { return modem_.answer(); }
  // Ends the call. A call the child ends while dialling is not reported as failed.
  bool hangup();

  // Volume 0 … 7, changed only during a call: the sound runs through the modem only then, and a
  // control without effect only confuses. False when nothing changed.
  bool changeVolume(int delta);
  bool setDefaultVolume(int level) { return modem_.setCallVolume(level); }
  int volume() const { return modem_.callVolume(); }
  bool volumeAdjustable() const { return modem_.callVolume() >= 0; }

  // What the rest of the device sees: a ringing call that is not (yet) known to come from an
  // allowed number is not there.
  ModemState state() const;
  // Dialling, ringing or connected, as the device shows it.
  bool active() const;
  // The modem is in a call or ringing, whoever it is: it must stay awake for that.
  bool modemBusy() const;
  // The other party of the current call, as a number; empty when there is none.
  const std::string& peer() const { return peer_; }

private:
  void handleEvent(const CallEvent& ev);
  void handleIncoming(const CallEvent& ev);
  void reportFailed(const CallEvent& ev, const char* reason);
  void ring();
  void rejectUnknown(const std::string& why);
  bool callerAllowed() const;
  void clearCall();
  std::string nameOf(const std::string& phone) const;

  Clock& clock_;
  Modem& modem_;
  Power& power_;
  MessageStore& store_;
  const Profile& profile_;
  SchoolTime& school_;
  Notices& notices_;
  Haptic& haptic_;
  Logger& log_;

  std::string peer_;
  // For the report of a call that did not happen: we dialled, it never connected, and the child
  // did not end it.
  bool dialedOut_ = false;
  bool connected_ = false;
  bool hangupRequested_ = false;
  // Ringing.
  bool ringing_ = false;
  uint32_t lastRing_ = 0;
  bool ringSeen_ = false;     // a call is ringing and ringStarted_ says since when
  uint32_t ringStarted_ = 0;
  bool rejectedInSchool_ = false;
  bool rejectedUnknown_ = false;  // the ringing call of a stranger or a hidden number was hung up
};

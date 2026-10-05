#pragma once

// Applies the power policy (PowerPolicy.h) to the hardware and keeps the one piece of state it
// needs: whether the modem has been put to sleep.
//
// The policy only decides; this component acts on the decision. The modem is put to sleep and
// woken, the radio chip gets or loses power as the need changes, and the main program is told
// whether the processor may sleep and for how long. What the policy needs to know about the rest
// of the device — a call, queued messages, a running search, the radio, Wi-Fi — comes in as a
// plain `Needs`, so that this component depends on none of the others.
//
// Also here, because they are the same kind of thing — what the device does with its own power:
// the front light, which draws current and is therefore switched off whenever the device locks,
// and the request to switch the device off.

#include <cstdint>

#include "../hal/Clock.h"
#include "../hal/Modem.h"
#include "../hal/Power.h"
#include "Logger.h"
#include "Notices.h"
#include "PowerPolicy.h"
#include "Profile.h"

class PowerCoordination {
public:
  // What is going on elsewhere that decides whether anything may sleep.
  struct Needs {
    bool callActive = false;
    bool pendingWork = false;   // queued messages that could leave, or a running search
    bool radioUsable = false;   // the radio software uses the chip
    bool wifiActive = false;
  };

  PowerCoordination(Clock& clock, Power& power, Modem& modem, const Profile& profile, Notices& notices, Logger& log)
      : clock_(clock), power_(power), modem_(modem), profile_(profile), notices_(notices), log_(log) {}

  // A key was pressed: quiet starts counting again.
  void noteInput() { lastInput_ = clock_.uptimeSeconds(); }
  // The modem did something (a message, a call, a switch of its power): it is not asleep.
  void noteModemAwake() { modemAsleep_ = false; }
  bool modemAsleep() const { return modemAsleep_; }

  // Takes the policy's decision and acts on it; returns it for the main program, which sleeps the
  // processor (that is hardware). Call from the main loop.
  PowerDecision apply(const Needs& needs, bool displayOn, uint32_t nextRenderInMs);

  // The reading light of the display. It draws current, so locking switches it off.
  bool toggleFrontlight();
  void setFrontlight(bool on) { power_.setEnabled(PowerDomain::Frontlight, on); }
  bool frontlightOn() { return power_.isEnabled(PowerDomain::Frontlight); }

  // Switching the device off is requested, not done here: the display must be cleared first
  // (e-ink keeps its last picture without power, and a switched-off device that still shows the
  // status page looks like a hanging one). If it does not work — a cable feeds the board — the
  // request is dropped and a notice says why.
  void requestPowerOff() { powerOffRequested_ = true; }
  bool powerOffRequested() const { return powerOffRequested_; }
  void notePowerOffFailed();

  int batteryPercent() { return power_.batteryPercent(); }
  ChargeState chargeState() { return power_.chargeState(); }
  int batteryMinutesRemaining() { return power_.batteryMinutesRemaining(); }

private:
  Clock& clock_;
  Power& power_;
  Modem& modem_;
  const Profile& profile_;
  Notices& notices_;
  Logger& log_;
  uint32_t lastInput_ = 0;
  bool modemAsleep_ = false;
  bool powerOffRequested_ = false;
};

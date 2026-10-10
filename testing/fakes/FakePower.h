#pragma once

// Power supply with a scripted battery and domains that can refuse to switch.
//
// Scripting:
//   setBattery(percent), setMinutesRemaining(minutes), setMillivolts(mv)   -1 = unknown
//   plugIn(state)       an external supply with the given state (Charging or Full)
//   unplug()            on battery
//   setChargerAnswers(false)   the charger does not answer: state Unknown
//   failDomain(domain, true)   that domain cannot be switched
//   allowPowerOff(bool)        powerOff() works or fails (a cable feeds the board)
// Reading back: powerOffCalls(), switchOnCount(domain) (how often a domain was
// switched from off to on) and switchedOnAtMs(domain) (the uptime of the last switch-on,
// when a clock was given), so a receiver can tell when it was restarted.
// All domains start off.

#include <hal/Clock.h>
#include <hal/Power.h>

class FakePower : public Power {
public:
  explicit FakePower(Clock* clock = nullptr) : clock_(clock) {}

  bool setEnabled(PowerDomain domain, bool on) override {
    const int i = (int)domain;
    if (failing_[i]) return false;
    if (on && !enabled_[i]) {
      ++switchedOn_[i];
      switchedOnAt_[i] = clock_ ? clock_->uptimeMillis() : 0;
    }
    enabled_[i] = on;
    return true;
  }
  bool isEnabled(PowerDomain domain) override { return enabled_[(int)domain]; }

  bool powerOff() override {
    ++powerOffCalls_;
    return powerOffAllowed_;
  }
  bool externalPower() override { return cableOnly_ || charge_ == ChargeState::Charging || charge_ == ChargeState::Full; }

  int batteryPercent() override { return percent_; }
  int batteryMillivolts() override { return millivolts_; }
  // Unknown while charging, as the contract says.
  int batteryMinutesRemaining() override { return externalPower() ? -1 : minutes_; }
  ChargeState chargeState() override { return chargerAnswers_ ? charge_ : ChargeState::Unknown; }

  // Scripting.
  void setBattery(int percent) { percent_ = percent; }
  void setMillivolts(int millivolts) { millivolts_ = millivolts; }
  void setMinutesRemaining(int minutes) { minutes_ = minutes; }
  void plugIn(ChargeState state) { charge_ = state == ChargeState::Full ? ChargeState::Full : ChargeState::Charging; }
  // A cable with a supply behind it while the charger neither charges nor reports the charge as done:
  // the charger's power-good bit is set and its charge state says "not charging".
  void plugInWithoutCharging() { charge_ = ChargeState::Discharging; cableOnly_ = true; }
  void unplug() { charge_ = ChargeState::Discharging; cableOnly_ = false; }
  void setChargerAnswers(bool answers) { chargerAnswers_ = answers; }
  void failDomain(PowerDomain domain, bool fails) { failing_[(int)domain] = fails; }
  void allowPowerOff(bool allowed) { powerOffAllowed_ = allowed; }
  int powerOffCalls() const { return powerOffCalls_; }
  int switchOnCount(PowerDomain domain) const { return switchedOn_[(int)domain]; }
  uint64_t switchedOnAtMs(PowerDomain domain) const { return switchedOnAt_[(int)domain]; }

private:
  bool cableOnly_ = false;
  bool enabled_[4] = {};
  bool failing_[4] = {};
  int switchedOn_[4] = {};
  uint64_t switchedOnAt_[4] = {};
  Clock* clock_;
  int percent_ = 80;
  int millivolts_ = 3900;
  int minutes_ = 600;
  ChargeState charge_ = ChargeState::Discharging;
  bool chargerAnswers_ = true;
  bool powerOffAllowed_ = true;
  int powerOffCalls_ = 0;
};

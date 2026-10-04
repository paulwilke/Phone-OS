#pragma once

// Power supply with a scripted battery and domains that can refuse to switch.
//
// Scripting:
//   setBattery(percent), setMinutesRemaining(minutes)   -1 = unknown
//   plugIn(state)       an external supply with the given state (Charging or Full)
//   unplug()            on battery
//   setChargerAnswers(false)   the charger does not answer: state Unknown
//   failDomain(domain, true)   that domain cannot be switched
//   allowPowerOff(bool)        powerOff() works or fails (a cable feeds the board)
// Reading back: powerOffCalls(), switchOnCount(domain) (how often a domain was
// switched from off to on, so a receiver can tell it was restarted).
// All domains start off.

#include <hal/Power.h>

class FakePower : public Power {
public:
  bool setEnabled(PowerDomain domain, bool on) override {
    const int i = (int)domain;
    if (failing_[i]) return false;
    if (on && !enabled_[i]) ++switchedOn_[i];
    enabled_[i] = on;
    return true;
  }
  bool isEnabled(PowerDomain domain) override { return enabled_[(int)domain]; }

  bool powerOff() override {
    ++powerOffCalls_;
    return powerOffAllowed_;
  }
  bool externalPower() override { return charge_ == ChargeState::Charging || charge_ == ChargeState::Full; }

  int batteryPercent() override { return percent_; }
  // Unknown while charging, as the contract says.
  int batteryMinutesRemaining() override { return externalPower() ? -1 : minutes_; }
  ChargeState chargeState() override { return chargerAnswers_ ? charge_ : ChargeState::Unknown; }

  // Scripting.
  void setBattery(int percent) { percent_ = percent; }
  void setMinutesRemaining(int minutes) { minutes_ = minutes; }
  void plugIn(ChargeState state) { charge_ = state == ChargeState::Full ? ChargeState::Full : ChargeState::Charging; }
  void unplug() { charge_ = ChargeState::Discharging; }
  void setChargerAnswers(bool answers) { chargerAnswers_ = answers; }
  void failDomain(PowerDomain domain, bool fails) { failing_[(int)domain] = fails; }
  void allowPowerOff(bool allowed) { powerOffAllowed_ = allowed; }
  int powerOffCalls() const { return powerOffCalls_; }
  int switchOnCount(PowerDomain domain) const { return switchedOn_[(int)domain]; }

private:
  bool enabled_[4] = {};
  bool failing_[4] = {};
  int switchedOn_[4] = {};
  int percent_ = 80;
  int minutes_ = 600;
  ChargeState charge_ = ChargeState::Discharging;
  bool chargerAnswers_ = true;
  bool powerOffAllowed_ = true;
  int powerOffCalls_ = 0;
};

#include "BoardPower.h"

#include "BatteryPlausibility.h"
#include "PinMap.h"

namespace board {

void BoardPower::begin(uint16_t batteryCapacityMah, GaugeCapacityWriter& writer, EventLog& log) {
  log_ = &log;
  if (charger_.detect()) {
    log.log(std::string("Battery: charger ") + chargerChipName(charger_.chip()) + " found");
    // The chip charges with 2048 mA unless it is told otherwise, which is more than 1 C for this cell, and
    // its watchdog sets it back to that every 40 seconds.
    const ChargeSetup setup = charger_.limitCharging(chargeCurrentLimitMa(batteryCapacityMah));
    if (!setup.ok) log.log("Battery: the charge current could not be limited, the charger keeps its own " + std::to_string(setup.chargeCurrentBeforeMa) + " mA");
    else if (setup.changed) log.log("Battery: charge current limited to " + std::to_string(setup.chargeCurrentMa) + " mA (it was " + std::to_string(setup.chargeCurrentBeforeMa) + " mA), the charger's watchdog is off");
    else log.log("Battery: charge current is " + std::to_string(setup.chargeCurrentMa) + " mA already, nothing to write");
  } else {
    log.log("Battery: no charger answers at either address, charge state and cable are unknown");
  }
  log.log(describeCapacityCheck(ensureCapacity(gauge_, writer, batteryCapacityMah)));
  // The boot state of the expander powers the receiver and the radio. Nobody has asked for them yet, and a
  // receiver that stays on drains the battery for nothing.
  setEnabled(PowerDomain::Positioning, false);
  setEnabled(PowerDomain::Radio, false);
}

bool BoardPower::setEnabled(PowerDomain domain, bool on) {
  switch (domain) {
    case PowerDomain::Cellular:
      if (on) modem_.begin();
      else modem_.shutdown();
      return true;
    case PowerDomain::Positioning:
    case PowerDomain::Radio: {
      const uint8_t rail = domain == PowerDomain::Positioning ? leg::kGnssPower : leg::kRadioPower;
      const bool was = expander_.level(rail);
      if (!expander_.set(rail, on)) return false;
      if (on && !was) ++switchOns_[(int)domain];
      return true;
    }
    case PowerDomain::Frontlight:
      gpio_.write(pin::kFrontlight, on);
      if (on && !frontlight_) ++switchOns_[(int)domain];
      frontlight_ = on;
      return true;
  }
  return false;
}

bool BoardPower::isEnabled(PowerDomain domain) {
  switch (domain) {
    case PowerDomain::Cellular: return modem_.state() != ModemState::Off;
    case PowerDomain::Positioning: return expander_.level(leg::kGnssPower);
    case PowerDomain::Radio: return expander_.level(leg::kRadioPower);
    case PowerDomain::Frontlight: return frontlight_;
  }
  return false;
}

BoardPower::Reading BoardPower::read() {
  if (millis_ && haveReading_ && (uint32_t)(millis_->millis32() - readAt_) < kReadingTtlMs) return last_;
  Reading r;
  if (!charger_.found()) charger_.detect();  // a charger that was not there at start may be asked again
  r.chargerOk = charger_.readStatus(r.charger);
  r.gaugeOk = gauge_.read(r.gauge);
  if (millis_) {
    last_ = r;
    haveReading_ = true;
    readAt_ = millis_->millis32();
  }
  return r;
}

BatteryInputs BoardPower::inputsOf(const Reading& r) const {
  BatteryInputs in;
  in.wasEstimated = estimating_;
  in.gaugePercent = r.gaugeOk ? r.gauge.percent : -1;
  in.gaugeMillivolts = r.gaugeOk ? r.gauge.millivolts : 0;
  in.chargerKnown = r.chargerOk;
  in.cable = r.chargerOk && r.charger.powerGood;
  in.chargeDone = r.chargerOk && r.charger.phase == ChargePhase::Done;
  in.notCharging = r.chargerOk && r.charger.phase == ChargePhase::NotCharging;
  return in;
}

bool BoardPower::externalPower() {
  const Reading r = read();
  return r.chargerOk && r.charger.powerGood;
}

ChargeState BoardPower::chargeState() {
  const Reading r = read();
  if (!r.chargerOk) return ChargeState::Unknown;
  if (!r.charger.powerGood) return ChargeState::Discharging;
  if (assessBattery(inputsOf(r)).full) return ChargeState::Full;
  if (r.charger.phase == ChargePhase::PreCharge || r.charger.phase == ChargePhase::FastCharge) return ChargeState::Charging;
  // A cable, but the charger neither charges nor calls the battery full: nothing here can be claimed.
  return ChargeState::Unknown;
}

int BoardPower::batteryPercent() {
  const Reading r = read();
  const BatteryAssessment a = assessBattery(inputsOf(r));
  if (a.estimated != estimating_) {
    estimating_ = a.estimated;   // also without a log: the rule needs to know what it answered last
    if (!log_) return a.percent;
    if (a.estimated)
      log_->log("Battery: the gauge says " + std::to_string(r.gauge.percent) + " % at " + std::to_string(r.gauge.millivolts) +
                " mV; showing about " + std::to_string(a.percent) + " % from the voltage");
    else
      log_->log("Battery: the gauge agrees with the voltage again");
  }
  return a.percent;
}

int BoardPower::batteryMillivolts() {
  const Reading r = read();
  if (r.gaugeOk && r.gauge.millivolts > 0) return r.gauge.millivolts;
  // Without a gauge the charger's own measurement stands in; the short status read does not carry it.
  uint8_t registers[kChargerRegisterCount];
  ChargerStatus full;
  if (charger_.readAll(registers, full) && full.batteryMv > 0) return full.batteryMv;
  return -1;
}

int BoardPower::batteryMinutesRemaining() {
  const Reading r = read();
  if (!r.gaugeOk) return -1;
  // The time is computed from the same capacity as the percentage: when one is not believed, neither is
  // the other. While a cable is attached the battery is not emptying.
  if (assessBattery(inputsOf(r)).contradicted) return -1;
  if (r.chargerOk && r.charger.powerGood) return -1;
  return r.gauge.minutesToEmpty;
}

bool BoardPower::powerOff() {
  if (!charger_.found() && !charger_.detect()) return false;
  ChargerStatus s;
  if (!charger_.readStatus(s) || s.powerGood) return false;
  // The consumers first, so that nothing is cut off in the middle of its work.
  setEnabled(PowerDomain::Frontlight, false);
  setEnabled(PowerDomain::Positioning, false);
  setEnabled(PowerDomain::Radio, false);
  expander_.set(leg::kHapticPower, false);
  expander_.set(leg::kAmplifier, false);
  expander_.set(leg::kModemPower, false);
  return charger_.disconnectBattery();
}

}  // namespace board

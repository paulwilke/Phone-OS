#include "Charger.h"

#include "PinMap.h"

namespace board {

namespace {
constexpr uint8_t kRegInputLimit = 0x00;
constexpr uint8_t kRegChargeCurrent = 0x04;
constexpr uint8_t kRegWatchdog = 0x07;
constexpr uint8_t kWatchdogBits = 0x30;
constexpr uint8_t kRegPrecharge = 0x05;
constexpr uint8_t kRegChargeVoltage = 0x06;
constexpr uint8_t kRegControl = 0x09;
constexpr uint8_t kRegStatus = 0x0B;
constexpr uint8_t kRegFault = 0x0C;
constexpr uint8_t kRegBattery = 0x0E;
constexpr uint8_t kRegSystem = 0x0F;
constexpr uint8_t kRegVbus = 0x11;
constexpr uint8_t kRegChargingCurrent = 0x12;
constexpr uint8_t kBatfetDisable = 0x20;

int watchdogSecondsOf(uint8_t reg) {
  switch ((reg & kWatchdogBits) >> 4) {
    case 1: return 40;
    case 2: return 80;
    case 3: return 160;
    default: return 0;
  }
}

void decodeStatusRegister(uint8_t status, ChargerStatus& s) {
  s.input = (InputKind)(status >> 5);
  s.phase = (ChargePhase)((status >> 3) & 0x03);
  s.powerGood = (status & 0x04) != 0;
}
}  // namespace

ChargerStatus decodeCharger(const uint8_t (&r)[kChargerRegisterCount]) {
  ChargerStatus s;
  decodeStatusRegister(r[kRegStatus], s);

  s.inputLimitEnabled = (r[kRegInputLimit] & 0x40) != 0;
  s.inputCurrentLimitMa = 100 + 50 * (r[kRegInputLimit] & 0x3F);
  s.chargeCurrentMa = 64 * (r[kRegChargeCurrent] & 0x7F);
  s.prechargeCurrentMa = 64 + 64 * (r[kRegPrecharge] >> 4);
  s.terminationCurrentMa = 64 + 64 * (r[kRegPrecharge] & 0x0F);
  s.chargeVoltageMv = 3840 + 16 * (r[kRegChargeVoltage] >> 2);
  s.watchdogSeconds = watchdogSecondsOf(r[kRegWatchdog]);
  s.adcStarted = (r[kRegAdcControl] & kAdcStartBits) != 0;
  s.batteryMv = 2304 + 20 * (r[kRegBattery] & 0x7F);
  s.systemMv = 2304 + 20 * (r[kRegSystem] & 0x7F);
  s.vbusGood = (r[kRegVbus] & 0x80) != 0;
  s.vbusMv = s.vbusGood ? 2600 + 100 * (r[kRegVbus] & 0x7F) : 0;
  s.chargingCurrentMa = 50 * (r[kRegChargingCurrent] & 0x7F);
  s.fault = r[kRegFault];
  return s;
}

const char* chargerChipName(ChargerChip chip) {
  switch (chip) {
    case ChargerChip::Sy6970: return "SY6970";
    case ChargerChip::Bq25896: return "BQ25896";
    case ChargerChip::None: break;
  }
  return "none";
}

const char* chargePhaseName(ChargePhase phase) {
  switch (phase) {
    case ChargePhase::NotCharging: return "not charging";
    case ChargePhase::PreCharge: return "pre-charge";
    case ChargePhase::FastCharge: return "fast charge";
    case ChargePhase::Done: return "charge done";
  }
  return "unknown";
}

const char* inputKindName(InputKind kind) {
  switch (kind) {
    case InputKind::None: return "no input";
    case InputKind::UsbHost: return "USB host";
    case InputKind::UsbCharging: return "USB charging port";
    case InputKind::UsbDedicated: return "USB dedicated charger";
    case InputKind::Adjustable: return "adjustable high-voltage adapter";
    case InputKind::UnknownAdapter: return "unknown adapter";
    case InputKind::NonStandard: return "non-standard adapter";
    case InputKind::OnTheGo: return "on-the-go";
  }
  return "unknown";
}

bool Charger::detect() {
  chip_ = ChargerChip::None;
  address_ = 0;
  if (bus_.probe(i2c::kChargerSy6970)) {
    chip_ = ChargerChip::Sy6970;
    address_ = i2c::kChargerSy6970;
  } else if (bus_.probe(i2c::kChargerBq25896)) {
    chip_ = ChargerChip::Bq25896;
    address_ = i2c::kChargerBq25896;
  }
  return found();
}

bool Charger::readStatus(ChargerStatus& out) {
  if (!found()) return false;
  uint8_t status = 0;
  if (!bus_.readRegister(address_, kRegStatus, status)) return false;
  // Only the status register was read; the measured values stay at zero rather than at the chip's offsets.
  out = ChargerStatus();
  decodeStatusRegister(status, out);
  return true;
}

bool Charger::readAll(uint8_t (&registers)[kChargerRegisterCount], ChargerStatus& out) {
  if (!found()) return false;
  const uint8_t start = 0x00;
  if (!bus_.writeRead(address_, &start, 1, registers, kChargerRegisterCount)) return false;
  out = decodeCharger(registers);
  return true;
}

bool Charger::disconnectBattery() {
  if (!found()) return false;
  uint8_t control = 0;
  if (!bus_.readRegister(address_, kRegControl, control)) return false;
  return bus_.writeRegister(address_, kRegControl, (uint8_t)(control | kBatfetDisable));
}

ChargeSetup Charger::limitCharging(int maxMa) {
  ChargeSetup setup;
  if (!found()) return setup;
  uint8_t watchdog = 0, current = 0;
  if (!bus_.readRegister(address_, kRegWatchdog, watchdog) || !bus_.readRegister(address_, kRegChargeCurrent, current)) return setup;
  setup.watchdogBeforeSeconds = watchdogSecondsOf(watchdog);
  setup.chargeCurrentBeforeMa = kChargeStepMa * (current & 0x7F);
  const uint8_t wantCurrent = (uint8_t)((current & 0x80) | (maxMa / kChargeStepMa));
  // The watchdog first: a chip that falls back to its defaults would undo the current a moment later.
  if (setup.watchdogBeforeSeconds != 0) {
    if (!bus_.writeRegister(address_, kRegWatchdog, (uint8_t)(watchdog & ~kWatchdogBits))) return setup;
    setup.changed = true;
  }
  if (wantCurrent != current) {
    if (!bus_.writeRegister(address_, kRegChargeCurrent, wantCurrent)) return setup;
    setup.changed = true;
  }
  if (!bus_.readRegister(address_, kRegWatchdog, watchdog) || !bus_.readRegister(address_, kRegChargeCurrent, current)) return setup;
  setup.watchdogSeconds = watchdogSecondsOf(watchdog);
  setup.chargeCurrentMa = kChargeStepMa * (current & 0x7F);
  setup.ok = setup.watchdogSeconds == 0 && current == wantCurrent;
  return setup;
}

}  // namespace board

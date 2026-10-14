#include "FuelGauge.h"

#include "PinMap.h"

namespace board {

namespace {
constexpr uint8_t kRegVoltage = 0x08;
constexpr uint8_t kRegCurrent = 0x0C;
constexpr uint8_t kRegRemaining = 0x10;
constexpr uint8_t kRegFullCharge = 0x12;
constexpr uint8_t kRegAverageCurrent = 0x14;
constexpr uint8_t kRegTimeToEmpty = 0x16;
constexpr uint8_t kRegInternalTemperature = 0x28;
constexpr uint8_t kRegCycleCount = 0x2A;
constexpr uint8_t kRegStateOfCharge = 0x2C;
constexpr uint8_t kRegDesignCapacity = 0x3C;

// Temperatures are in tenths of a kelvin.
constexpr int kZeroCelsiusTenthK = 2731;
}  // namespace

uint8_t FuelGauge::address() { return i2c::kFuelGauge; }

bool FuelGauge::read(GaugeReading& out) {
  uint16_t voltage, current, remaining, full, average, tte, temperature, cycles, soc, design;
  const uint8_t a = address();
  if (!bus_.readRegister16(a, kRegVoltage, voltage) || !bus_.readRegister16(a, kRegCurrent, current) ||
      !bus_.readRegister16(a, kRegRemaining, remaining) || !bus_.readRegister16(a, kRegFullCharge, full) ||
      !bus_.readRegister16(a, kRegAverageCurrent, average) || !bus_.readRegister16(a, kRegTimeToEmpty, tte) ||
      !bus_.readRegister16(a, kRegInternalTemperature, temperature) ||
      !bus_.readRegister16(a, kRegCycleCount, cycles) || !bus_.readRegister16(a, kRegStateOfCharge, soc) ||
      !bus_.readRegister16(a, kRegDesignCapacity, design))
    return false;
  GaugeReading r;
  r.millivolts = voltage;
  r.currentMa = (int16_t)current;
  r.averageCurrentMa = (int16_t)average;
  r.remainingMah = remaining;
  r.fullChargeMah = full;
  r.designMah = design;
  r.cycleCount = cycles;
  r.percent = soc > 100 ? 100 : (int)soc;
  r.minutesToEmpty = tte == 0xFFFF ? -1 : (int)tte;
  r.temperatureTenthC = (int)temperature - kZeroCelsiusTenthK;
  out = r;
  return true;
}

bool FuelGauge::readCapacities(uint16_t& designMah, uint16_t& fullChargeMah) {
  uint16_t design, full;
  if (!bus_.readRegister16(address(), kRegDesignCapacity, design) ||
      !bus_.readRegister16(address(), kRegFullCharge, full))
    return false;
  designMah = design;
  fullChargeMah = full;
  return true;
}

CapacityResult ensureCapacity(FuelGauge& gauge, GaugeCapacityWriter& writer, uint16_t configuredMah) {
  CapacityResult r;
  r.configured = configuredMah;
  if (!gauge.readCapacities(r.designBefore, r.fullChargeBefore)) return r;
  r.designAfter = r.designBefore;
  r.fullChargeAfter = r.fullChargeBefore;
  if (configuredMah < kMinBatteryMah || configuredMah > kMaxBatteryMah) {
    r.outcome = CapacityOutcome::InvalidSetting;
    return r;
  }
  // The design capacity is the battery's size and is written once; the full-charge capacity is what the gauge
  // learns from a discharge, and after the first write it is the gauge's alone (decision D-32: with the rule
  // "both equal the setting" the firmware wrote 1500 over a learned 1372 at every start).
  if (r.designBefore == configuredMah) {
    r.outcome = CapacityOutcome::AlreadyRight;
    return r;
  }
  if (!writer.setCapacities(configuredMah, configuredMah)) {
    r.outcome = CapacityOutcome::WriteRefused;
    return r;
  }
  if (!gauge.readCapacities(r.designAfter, r.fullChargeAfter) || r.designAfter != configuredMah ||
      r.fullChargeAfter != configuredMah) {
    r.outcome = CapacityOutcome::ReadBackDiffers;
    return r;
  }
  r.outcome = CapacityOutcome::Written;
  return r;
}

std::string describeCapacityCheck(const CapacityResult& r) {
  const std::string was = "design " + std::to_string(r.designBefore) + " mAh, full charge " +
                          std::to_string(r.fullChargeBefore) + " mAh";
  const std::string want = std::to_string(r.configured) + " mAh";
  switch (r.outcome) {
    case CapacityOutcome::GaugeAbsent: return "Battery: the fuel gauge does not answer, its capacity is not checked";
    case CapacityOutcome::InvalidSetting:
      return "Battery: the configured capacity " + want + " is not a plausible battery size, nothing written (gauge: " + was + ")";
    case CapacityOutcome::AlreadyRight:
      return "Battery: the fuel gauge capacity is " + want + ", nothing to write" +
             (r.fullChargeBefore != r.configured ? "; it has learned a full charge of " + std::to_string(r.fullChargeBefore) + " mAh" : "");
    case CapacityOutcome::Written:
      return "Battery: the fuel gauge held " + was + "; written " + want + " to both and read back";
    case CapacityOutcome::WriteRefused:
      return "Battery: the fuel gauge holds " + was + " but refused " + want + "; the percentage is not trusted";
    case CapacityOutcome::ReadBackDiffers:
      return "Battery: wrote " + want + " to the fuel gauge but it reports design " + std::to_string(r.designAfter) +
             " mAh, full charge " + std::to_string(r.fullChargeAfter) + " mAh";
  }
  return "";
}

}  // namespace board

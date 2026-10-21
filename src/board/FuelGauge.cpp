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

DataMemoryBlock dataMemoryBlock(uint16_t address, uint16_t value) {
  DataMemoryBlock b;
  b.bytes[0] = (uint8_t)(address & 0xFF);
  b.bytes[1] = (uint8_t)(address >> 8);
  b.bytes[2] = (uint8_t)(value >> 8);
  b.bytes[3] = (uint8_t)(value & 0xFF);
  const unsigned sum = b.bytes[0] + b.bytes[1] + b.bytes[2] + b.bytes[3];
  b.checksum = (uint8_t)(0xFF - (sum & 0xFF));
  b.length = sizeof(b.bytes) + 2;
  return b;
}

namespace {
// The Gauging Configuration's range in the manual's table (p.30): a value above it was not read from a chip.
constexpr uint16_t kGaugingConfigurationMax = 0x1FFF;

bool readEndOfDischarge(GaugeCapacityWriter& writer, EndOfDischarge& out) {
  DataMemoryWord words[4];
  words[0].address = kGaugingConfigurationAddress;
  words[1].address = kFixedEdv0Address;
  words[2].address = kFixedEdv1Address;
  words[3].address = kFixedEdv2Address;
  if (!writer.readDataMemory(words, 4)) return false;
  out.gaugingConfiguration = words[0].value;
  out.edv0Mv = words[1].value;
  out.edv1Mv = words[2].value;
  out.edv2Mv = words[3].value;
  return true;
}

bool sameEndOfDischarge(const EndOfDischarge& a, const EndOfDischarge& b) {
  return a.gaugingConfiguration == b.gaugingConfiguration && a.edv0Mv == b.edv0Mv && a.edv1Mv == b.edv1Mv &&
         a.edv2Mv == b.edv2Mv;
}

std::string thresholds(const EndOfDischarge& e) {
  return std::to_string(e.edv0Mv) + "/" + std::to_string(e.edv1Mv) + "/" + std::to_string(e.edv2Mv) + " mV, compensation " +
         (e.compensated() ? "on" : "off");
}
}  // namespace

EndOfDischargeResult ensureEndOfDischarge(FuelGauge& gauge, GaugeCapacityWriter& writer) {
  EndOfDischargeResult r;
  if (!gauge.present()) return r;
  if (!readEndOfDischarge(writer, r.before) || r.before.gaugingConfiguration > kGaugingConfigurationMax) {
    r.outcome = EndOfDischargeOutcome::ReadFailed;
    return r;
  }
  r.after = r.before;
  EndOfDischarge wanted = r.before;
  wanted.gaugingConfiguration = (uint16_t)(r.before.gaugingConfiguration & ~kEdvCompensationBit);
  wanted.edv0Mv = kEdv0Mv;
  wanted.edv1Mv = kEdv1Mv;
  wanted.edv2Mv = kEdv2Mv;
  if (sameEndOfDischarge(r.before, wanted)) {
    r.outcome = EndOfDischargeOutcome::AlreadyRight;
    return r;
  }
  DataMemoryWord changes[4];
  int n = 0;
  const auto change = [&](uint16_t address, uint16_t was, uint16_t want) {
    if (was == want) return;
    changes[n].address = address;
    changes[n].value = want;
    ++n;
  };
  change(kGaugingConfigurationAddress, r.before.gaugingConfiguration, wanted.gaugingConfiguration);
  change(kFixedEdv0Address, r.before.edv0Mv, wanted.edv0Mv);
  change(kFixedEdv1Address, r.before.edv1Mv, wanted.edv1Mv);
  change(kFixedEdv2Address, r.before.edv2Mv, wanted.edv2Mv);
  r.written = n;
  if (!writer.writeDataMemory(changes, (size_t)n)) {
    r.outcome = EndOfDischargeOutcome::WriteRefused;
    return r;
  }
  if (!readEndOfDischarge(writer, r.after) || !sameEndOfDischarge(r.after, wanted)) {
    r.outcome = EndOfDischargeOutcome::ReadBackDiffers;
    return r;
  }
  r.outcome = EndOfDischargeOutcome::Written;
  return r;
}

std::string describeEndOfDischargeCheck(const EndOfDischargeResult& r) {
  const std::string lead = "Battery: the fuel gauge's end-of-discharge thresholds ";
  switch (r.outcome) {
    case EndOfDischargeOutcome::GaugeAbsent: return lead + "are not checked, the fuel gauge does not answer";
    case EndOfDischargeOutcome::ReadFailed: return lead + "could not be read, nothing written";
    case EndOfDischargeOutcome::AlreadyRight: return lead + "are " + thresholds(r.before) + " already, nothing to write";
    case EndOfDischargeOutcome::Written:
      return lead + "set to " + thresholds(r.after) + " (they were " + thresholds(r.before) + ")";
    case EndOfDischargeOutcome::WriteRefused:
      return lead + "could not be set, the chip refused (they are " + thresholds(r.before) + ")";
    case EndOfDischargeOutcome::ReadBackDiffers:
      return lead + "could not be set, the chip reads back " + thresholds(r.after) + " (they were " + thresholds(r.before) + ")";
  }
  return "";
}

}  // namespace board

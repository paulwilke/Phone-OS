#pragma once

// The fuel gauge, a BQ27220 at 0x55. It counts charge instead of guessing from the voltage, but it ships
// with a battery size that is not ours: the device's gauge held the factory 3000 mAh while the battery is
// 1500 mAh, so it reported about half the real charge (docs/hardware-lessons.md). At start the capacity is
// therefore read and, only when it differs from the configured one, written.
//
// Reading is plain I2C through the seam. Writing the capacity is a procedure of its own on this chip
// (unseal, enter the configuration update, write the data memory with its checksum, leave, seal again);
// the device implements it through a library behind GaugeCapacityWriter, and the host tests stand a fake
// in its place that changes the registers the way the chip would.

#include <cstdint>
#include <string>

#include "I2cBus.h"

namespace board {

struct GaugeReading {
  int millivolts = 0;           // 0x08
  int currentMa = 0;            // 0x0C, negative while discharging
  int averageCurrentMa = 0;     // 0x14
  int remainingMah = 0;         // 0x10
  int fullChargeMah = 0;        // 0x12
  int designMah = 0;            // 0x3C
  int cycleCount = 0;           // 0x2A
  int percent = -1;             // 0x2C, clamped to 0..100
  int minutesToEmpty = -1;      // 0x16, -1 when the chip says "not discharging" (0xFFFF)
  int temperatureTenthC = 0;    // 0x28, internal temperature in tenths of a degree Celsius
};

// Writes the design and the full-charge capacity into the chip. Returns false when the chip refused.
class GaugeCapacityWriter {
public:
  virtual ~GaugeCapacityWriter() = default;
  virtual bool setCapacities(uint16_t designMah, uint16_t fullChargeMah) = 0;
};

class FuelGauge {
public:
  explicit FuelGauge(I2cBus& bus) : bus_(bus) {}

  bool present() { return bus_.probe(address()); }
  // Reads every value above; false when the chip does not answer (the reading is then not touched).
  bool read(GaugeReading& out);
  // The two capacities alone, which are all the start-up check needs.
  bool readCapacities(uint16_t& designMah, uint16_t& fullChargeMah);

private:
  static uint8_t address();
  I2cBus& bus_;
};

enum class CapacityOutcome : uint8_t {
  GaugeAbsent,      // no answer: nothing to check
  InvalidSetting,   // the configured capacity is outside what a battery of this kind can have: nothing written
  AlreadyRight,     // the design capacity equals the configured one: nothing written; the full charge is the gauge's own
  Written,          // they differed, were written and read back as configured
  WriteRefused,     // the chip refused the write
  ReadBackDiffers,  // the write was accepted but the chip still reports something else
};

struct CapacityResult {
  CapacityOutcome outcome = CapacityOutcome::GaugeAbsent;
  uint16_t designBefore = 0;
  uint16_t fullChargeBefore = 0;
  uint16_t designAfter = 0;
  uint16_t fullChargeAfter = 0;
  uint16_t configured = 0;
};

// The size the profile assumes when the configuration does not say (Profile::batteryCapacityMah; a test pins
// that the two agree). The firmware uses it before the configuration is read.
constexpr uint16_t kDefaultBatteryMah = 1500;
constexpr uint16_t kMinBatteryMah = 100;
constexpr uint16_t kMaxBatteryMah = 10000;

// Reads both capacities and, when either differs from `configuredMah`, writes the configured value to
// both and reads them back. Never writes when they agree (the chip's memory wears with every write) and
// never writes a value that is not a plausible battery size.
CapacityResult ensureCapacity(FuelGauge& gauge, GaugeCapacityWriter& writer, uint16_t configuredMah);

// The one log line that says what the check did.
std::string describeCapacityCheck(const CapacityResult& result);

}  // namespace board

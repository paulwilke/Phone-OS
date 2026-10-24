#pragma once

// The fuel gauge, a BQ27220 at 0x55. It counts charge instead of guessing from the voltage, but it ships
// with a battery size that is not ours: the device's gauge held the factory 3000 mAh while the battery is
// 1500 mAh, so it reported about half the real charge (docs/hardware/hardware-lessons.md). At start the capacity is
// therefore read and, only when it differs from the configured one, written.
//
// Reading is plain I2C through the seam. Writing the capacity is a procedure of its own on this chip
// (unseal, enter the configuration update, write the data memory with its checksum, leave, seal again);
// the device implements it through a library behind GaugeCapacityWriter, and the host tests stand a fake
// in its place that changes the registers the way the chip would.

#include <cstddef>
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

// One 16-bit item of the chip's data memory, by its absolute address (BQ27220 reference manual SLUUBD4A,
// table 3-2; docs/hardware/fuel-gauge-data-memory.md).
struct DataMemoryWord {
  uint16_t address = 0;
  uint16_t value = 0;
};

// Writes the design and the full-charge capacity into the chip, and reads and writes data-memory items in
// one configuration update each. Every call returns false when the chip refused or did not answer.
class GaugeCapacityWriter {
public:
  virtual ~GaugeCapacityWriter() = default;
  virtual bool setCapacities(uint16_t designMah, uint16_t fullChargeMah) = 0;
  // Fills `value` of each item from the chip.
  virtual bool readDataMemory(DataMemoryWord* words, size_t count) = 0;
  virtual bool writeDataMemory(const DataMemoryWord* words, size_t count) = 0;
};

// What is written from register 0x3E for one data-memory word, as the manual's example on p.65 and its
// compact form on p.52 do: the address low byte first, the value high byte first, then at 0x60 the
// complement of the byte sum of address and value and at 0x61 the length, the four bytes plus two.
struct DataMemoryBlock {
  uint8_t bytes[4] = {};  // to 0x3E, 0x3F, 0x40, 0x41
  uint8_t checksum = 0;   // to 0x60
  uint8_t length = 0;     // to 0x61
};
DataMemoryBlock dataMemoryBlock(uint16_t address, uint16_t value);

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

// The gauge's end of discharge (decision D-42). The gauge says 7 %, 3 % and 0 % at three voltages, EDV2, EDV1
// and EDV0, and learns its full charge at EDV2. The chip ships computing them from a profile of another cell
// (an 18650) and the load, and with this cell at 60 to 105 mA it reached EDV2 at about 3630 mV, five to eight
// hours before the switch-off, and learned a smaller full charge each time (1372, 1107, 988 mAh). With the
// compensation off the three are the fixed values below (manual p.12 and p.14).
//
// Placed on the discharge curve of 2026-10-21 at 61 mA (D-45) and moved by the two discharges of 2026-10-22 and
// 2026-10-24 at 63 mA (D-47): with EDV1 3420 and EDV2 3470 mV the gauge said 0 % at 3432 and 3438 mV, 1 h 26 min
// and 1 h 36 min before the switch-off at 3295 mV, with about 95 mAh (7 % of the 1350 mAh the cell delivered)
// still in the cell. So both sat about 40 mV too high for this cell. The energy log gives the voltage under the
// load of the device at each point before the switch-off (19:04:24 on 2026-10-22, 05:04:04 on 2026-10-24):
//   EDV0  3300 mV  the firmware's own switch-off (D-5): the gauge's 0 % is where the device goes off.
//   EDV1  3390 mV  3 % of 1350 mAh is 40 mAh, 38 minutes at 63 mA. The rows 36 minutes before the
//                  switch-off read 3396 mV (2026-10-22 18:28) and 3398 mV (2026-10-24 04:28); the lower, rounded
//                  down to 10 mV. The cell reaches 3390 mV 31 to 32 minutes before (33 mAh, 2.5 %), a little late
//                  rather than early, because the cell's knee below 3400 mV is steep (3396 to 3356 mV in 20 minutes).
//   EDV2  3440 mV  7 % of 1350 mAh is 95 mAh, 1.5 hours at 63 mA. The rows read 3432 mV (1 h 26 min before) and
//                  3438 mV (1 h 36 min before); the cell reaches 3440 mV about 1 h 40 min
//                  before in both (105 mAh, 7.8 %: 3440 mV is the nearest 10 mV to the 7 % point, 3432 to 3438 mV).
// A threshold reached too early is the failure measured (it cuts the count and teaches a smaller full charge),
// and with the modem awake (220 mA) the voltage is about 60 mV lower still. A threshold set lower than this
// would be reached late or never: the count would run past 0 % while the voltage is already on the knee.
constexpr uint16_t kEdv0Mv = 3300;
constexpr uint16_t kEdv1Mv = 3390;
constexpr uint16_t kEdv2Mv = 3440;

// Addresses in the chip's RAM copy of CEDV profile 1 (manual p.30), and the compensation bit of the
// Gauging Configuration (p.56-57: 0 uses the fixed values for all three).
constexpr uint16_t kGaugingConfigurationAddress = 0x929B;
constexpr uint16_t kFixedEdv0Address = 0x92B4;
constexpr uint16_t kFixedEdv1Address = 0x92B7;
constexpr uint16_t kFixedEdv2Address = 0x92BA;
constexpr uint16_t kEdvCompensationBit = 0x0008;

struct EndOfDischarge {
  uint16_t gaugingConfiguration = 0;
  uint16_t edv0Mv = 0;
  uint16_t edv1Mv = 0;
  uint16_t edv2Mv = 0;
  bool compensated() const { return (gaugingConfiguration & kEdvCompensationBit) != 0; }
};

enum class EndOfDischargeOutcome : uint8_t {
  GaugeAbsent,      // no answer: nothing to check
  ReadFailed,       // the chip did not give its values, or gave a configuration outside its range: nothing written
  AlreadyRight,     // the thresholds are ours and the compensation is off: nothing written
  Written,          // what differed was written and read back as wanted
  WriteRefused,     // the chip refused the write
  ReadBackDiffers,  // the write was accepted but the chip still holds something else
};

struct EndOfDischargeResult {
  EndOfDischargeOutcome outcome = EndOfDischargeOutcome::GaugeAbsent;
  EndOfDischarge before;
  EndOfDischarge after;
  int written = 0;  // how many of the four items were written
};

// Reads the Gauging Configuration and the three fixed thresholds and writes only the items that differ from
// kEdv0Mv..kEdv2Mv with the compensation off; every other bit of the configuration is kept as read. The write
// goes to the chip's RAM, as the capacity's does, so a start after the chip lost it writes it again.
EndOfDischargeResult ensureEndOfDischarge(FuelGauge& gauge, GaugeCapacityWriter& writer);

// The one log line that says what the check did.
std::string describeEndOfDischargeCheck(const EndOfDischargeResult& result);

}  // namespace board

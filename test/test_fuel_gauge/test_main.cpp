// The fuel gauge: reading, and the start-up check of its capacity against the configured battery size. The
// 2026-10-06 measurement is the data: the gauge held the factory 3000 mAh in a 1500 mAh battery.

#include <unity.h>

#include <board/FuelGauge.h>
#include <board/PinMap.h>
#include <fakes/FakeGaugeWriter.h>
#include <fakes/FakeI2cBus.h>

using namespace board;

void setUp() {}
void tearDown() {}

namespace {
struct Rig {
  FakeI2cBus bus;
  FuelGauge gauge{bus};
  FakeGaugeWriter writer{bus};
  Rig() {
    bus.addChip(i2c::kFuelGauge);
    bus.setRegister16(i2c::kFuelGauge, 0x3C, 3000);
    bus.setRegister16(i2c::kFuelGauge, 0x12, 3000);
  }
};
}  // namespace

void test_the_values_measured_on_the_device_read_back() {
  Rig r;
  const uint8_t a = i2c::kFuelGauge;
  r.bus.setRegister16(a, 0x08, 4118);
  r.bus.setRegister16(a, 0x10, 1655);
  r.bus.setRegister16(a, 0x2C, 56);
  r.bus.setRegister16(a, 0x2A, 0);
  r.bus.setRegister16(a, 0x16, 2858);
  r.bus.setRegister16(a, 0x14, (uint16_t)(int16_t)-35);
  r.bus.setRegister16(a, 0x28, 2981);
  GaugeReading g;
  TEST_ASSERT_TRUE(r.gauge.read(g));
  TEST_ASSERT_EQUAL(4118, g.millivolts);
  TEST_ASSERT_EQUAL(1655, g.remainingMah);
  TEST_ASSERT_EQUAL(3000, g.fullChargeMah);
  TEST_ASSERT_EQUAL(3000, g.designMah);
  TEST_ASSERT_EQUAL(56, g.percent);
  TEST_ASSERT_EQUAL(0, g.cycleCount);
  TEST_ASSERT_EQUAL(2858, g.minutesToEmpty);
  TEST_ASSERT_EQUAL(-35, g.averageCurrentMa);
  TEST_ASSERT_EQUAL(250, g.temperatureTenthC);
}

void test_the_limits_of_the_registers() {
  Rig r;
  r.bus.setRegister16(i2c::kFuelGauge, 0x2C, 140);
  r.bus.setRegister16(i2c::kFuelGauge, 0x16, 0xFFFF);
  r.bus.setRegister16(i2c::kFuelGauge, 0x0C, (uint16_t)(int16_t)-1200);
  GaugeReading g;
  TEST_ASSERT_TRUE(r.gauge.read(g));
  TEST_ASSERT_EQUAL(100, g.percent);
  TEST_ASSERT_EQUAL(-1, g.minutesToEmpty);
  TEST_ASSERT_EQUAL(-1200, g.currentMa);
}

void test_a_missing_gauge_is_not_read_and_leaves_the_reading_alone() {
  FakeI2cBus bus;
  FuelGauge gauge(bus);
  GaugeReading g;
  g.percent = 42;
  TEST_ASSERT_FALSE(gauge.present());
  TEST_ASSERT_FALSE(gauge.read(g));
  TEST_ASSERT_EQUAL(42, g.percent);
}

void test_the_factory_capacity_is_replaced_in_both_registers_and_read_back() {
  Rig r;
  const CapacityResult res = ensureCapacity(r.gauge, r.writer, 1500);
  TEST_ASSERT_TRUE(res.outcome == CapacityOutcome::Written);
  TEST_ASSERT_EQUAL(1, r.writer.calls());
  TEST_ASSERT_EQUAL(1500, r.writer.lastDesign());
  TEST_ASSERT_EQUAL(1500, r.writer.lastFullCharge());
  TEST_ASSERT_EQUAL(3000, res.designBefore);
  TEST_ASSERT_EQUAL(1500, res.designAfter);
  TEST_ASSERT_EQUAL(1500, res.fullChargeAfter);
  TEST_ASSERT_EQUAL(1500, r.bus.reg16(i2c::kFuelGauge, 0x3C));
}

// The chip's memory wears with every write, so a capacity that is already right is never written again.
void test_a_capacity_that_is_right_is_not_written() {
  Rig r;
  r.bus.setRegister16(i2c::kFuelGauge, 0x3C, 1500);
  r.bus.setRegister16(i2c::kFuelGauge, 0x12, 1500);
  const CapacityResult res = ensureCapacity(r.gauge, r.writer, 1500);
  TEST_ASSERT_TRUE(res.outcome == CapacityOutcome::AlreadyRight);
  TEST_ASSERT_EQUAL(0, r.writer.calls());
}

void test_the_second_start_after_a_write_writes_nothing() {
  Rig r;
  ensureCapacity(r.gauge, r.writer, 1500);
  const CapacityResult again = ensureCapacity(r.gauge, r.writer, 1500);
  TEST_ASSERT_TRUE(again.outcome == CapacityOutcome::AlreadyRight);
  TEST_ASSERT_EQUAL(1, r.writer.calls());
}

// After the first discharge the gauge reported a learned full charge of 1372 mAh, and the rule "both equal the
// setting" wrote 1500 over it at every start (2026-10-14, decision D-32). The design capacity decides; a learned
// full charge is the gauge's own and is named in the line.
void test_a_learned_full_charge_is_left_to_the_gauge_and_named() {
  Rig r;
  r.bus.setRegister16(i2c::kFuelGauge, 0x3C, 1500);  // design right, full charge learned
  r.bus.setRegister16(i2c::kFuelGauge, 0x12, 1372);
  const CapacityResult res = ensureCapacity(r.gauge, r.writer, 1500);
  TEST_ASSERT_TRUE(res.outcome == CapacityOutcome::AlreadyRight);
  TEST_ASSERT_EQUAL(0, r.writer.calls());
  TEST_ASSERT_EQUAL(1372, r.bus.reg16(i2c::kFuelGauge, 0x12));
  const std::string line = describeCapacityCheck(res);
  TEST_ASSERT_TRUE_MESSAGE(line.find("nothing to write; it has learned a full charge of 1372 mAh") != std::string::npos, line.c_str());
  Rig q;
  q.bus.setRegister16(i2c::kFuelGauge, 0x3C, 1200);  // a wrong design capacity is still written, with the full charge seeded
  q.bus.setRegister16(i2c::kFuelGauge, 0x12, 1500);
  TEST_ASSERT_TRUE(ensureCapacity(q.gauge, q.writer, 1500).outcome == CapacityOutcome::Written);
  TEST_ASSERT_EQUAL(1500, q.writer.lastFullCharge());
}

void test_a_refused_write_is_reported_and_leaves_the_chip_as_it_was() {
  Rig r;
  r.writer.refuse();
  const CapacityResult res = ensureCapacity(r.gauge, r.writer, 1500);
  TEST_ASSERT_TRUE(res.outcome == CapacityOutcome::WriteRefused);
  TEST_ASSERT_EQUAL(3000, r.bus.reg16(i2c::kFuelGauge, 0x3C));
}

void test_a_write_that_did_not_take_is_found_by_the_read_back() {
  Rig r;
  r.writer.acceptButKeep();
  const CapacityResult res = ensureCapacity(r.gauge, r.writer, 1500);
  TEST_ASSERT_TRUE(res.outcome == CapacityOutcome::ReadBackDiffers);
  TEST_ASSERT_EQUAL(3000, res.designAfter);
}

void test_a_missing_gauge_writes_nothing() {
  FakeI2cBus bus;
  FuelGauge gauge(bus);
  FakeGaugeWriter writer(bus);
  TEST_ASSERT_TRUE(ensureCapacity(gauge, writer, 1500).outcome == CapacityOutcome::GaugeAbsent);
  TEST_ASSERT_EQUAL(0, writer.calls());
}

void test_an_implausible_setting_is_never_written() {
  const uint16_t settings[] = {0, 99, 10001, 65535};
  for (uint16_t s : settings) {
    Rig r;
    TEST_ASSERT_TRUE(ensureCapacity(r.gauge, r.writer, s).outcome == CapacityOutcome::InvalidSetting);
    TEST_ASSERT_EQUAL(0, r.writer.calls());
  }
}

void test_the_log_line_says_what_happened() {
  Rig r;
  const std::string written = describeCapacityCheck(ensureCapacity(r.gauge, r.writer, 1500));
  TEST_ASSERT_TRUE(written.find("3000") != std::string::npos);
  TEST_ASSERT_TRUE(written.find("written 1500 mAh to both") != std::string::npos);
  TEST_ASSERT_TRUE(describeCapacityCheck(ensureCapacity(r.gauge, r.writer, 1500)).find("nothing to write") != std::string::npos);
  Rig q;
  q.writer.refuse();
  TEST_ASSERT_TRUE(describeCapacityCheck(ensureCapacity(q.gauge, q.writer, 1500)).find("refused") != std::string::npos);
  FakeI2cBus none;
  FuelGauge g(none);
  FakeGaugeWriter w(none);
  TEST_ASSERT_TRUE(describeCapacityCheck(ensureCapacity(g, w, 1500)).find("does not answer") != std::string::npos);
}

// ---- the end-of-discharge thresholds (D-42) -------------------------------------------------------------------
// The chip's defaults, as the fake holds them: compensation on (0x102A), 3031/3385/3501 mV. With them the gauge
// said 7 % at about 3630 mV and learned 1372, 1107 and 988 mAh from four discharges.

void test_the_thresholds_are_set_with_the_compensation_off_and_the_other_bits_kept() {
  Rig r;
  const EndOfDischargeResult res = ensureEndOfDischarge(r.gauge, r.writer);
  TEST_ASSERT_TRUE(res.outcome == EndOfDischargeOutcome::Written);
  TEST_ASSERT_EQUAL_HEX16(0x1022, r.writer.dataMemory(0x929B));  // 0x102A without EDV_CMP (0x0008)
  TEST_ASSERT_EQUAL(3300, r.writer.dataMemory(0x92B4));
  TEST_ASSERT_EQUAL(3390, r.writer.dataMemory(0x92B7));
  TEST_ASSERT_EQUAL(3440, r.writer.dataMemory(0x92BA));
  TEST_ASSERT_EQUAL(4, res.written);
  TEST_ASSERT_EQUAL(3031, res.before.edv0Mv);
  TEST_ASSERT_TRUE(res.before.compensated());
  TEST_ASSERT_FALSE(res.after.compensated());
  TEST_ASSERT_EQUAL(2, r.writer.dataMemoryReads());  // the read and the read-back
}

// The thresholds lie at the firmware's switch-off and below the voltage at which the gauge fell to 7 %.
void test_the_thresholds_lie_between_the_switch_off_and_the_fall_in_order() {
  TEST_ASSERT_EQUAL(3300, kEdv0Mv);
  TEST_ASSERT_TRUE(kEdv0Mv < kEdv1Mv && kEdv1Mv < kEdv2Mv);
  TEST_ASSERT_TRUE(kEdv2Mv < 3621);
}

// Only the four items, and of them only those that differ: a chip that already has the compensation off and
// one threshold right gets the other two and nothing else.
void test_only_the_items_that_differ_are_written() {
  Rig r;
  r.writer.setDataMemory(0x929B, 0x1022);
  r.writer.setDataMemory(0x92B4, 3300);
  const EndOfDischargeResult res = ensureEndOfDischarge(r.gauge, r.writer);
  TEST_ASSERT_TRUE(res.outcome == EndOfDischargeOutcome::Written);
  TEST_ASSERT_EQUAL(2, (int)r.writer.dataMemoryWrites().size());
  TEST_ASSERT_EQUAL_HEX16(0x92B7, r.writer.dataMemoryWrites()[0]);
  TEST_ASSERT_EQUAL_HEX16(0x92BA, r.writer.dataMemoryWrites()[1]);
  TEST_ASSERT_EQUAL(0, r.writer.calls());  // the capacities are not touched
}

void test_thresholds_that_are_right_are_not_written_and_the_second_start_writes_nothing() {
  Rig r;
  ensureEndOfDischarge(r.gauge, r.writer);
  const size_t first = r.writer.dataMemoryWrites().size();
  const EndOfDischargeResult again = ensureEndOfDischarge(r.gauge, r.writer);
  TEST_ASSERT_TRUE(again.outcome == EndOfDischargeOutcome::AlreadyRight);
  TEST_ASSERT_EQUAL(first, r.writer.dataMemoryWrites().size());
}

// A chip that lost its RAM after a write (a reset) is found at the next start and written again.
void test_thresholds_lost_by_the_chip_are_written_again_at_the_next_start() {
  Rig r;
  ensureEndOfDischarge(r.gauge, r.writer);
  r.writer.setDataMemory(0x929B, 0x102A);
  r.writer.setDataMemory(0x92B4, 3031);
  r.writer.setDataMemory(0x92B7, 3385);
  r.writer.setDataMemory(0x92BA, 3501);
  TEST_ASSERT_TRUE(ensureEndOfDischarge(r.gauge, r.writer).outcome == EndOfDischargeOutcome::Written);
  TEST_ASSERT_EQUAL(3440, r.writer.dataMemory(0x92BA));
}

// D-47: the numbers of D-45 (3300/3420/3470 mV, compensation off) were on the device from 2026-10-21 and made the
// gauge say 0 % an hour and a half before the switch-off. A chip that holds them is not "already right": the next
// start writes the two upper ones (and nothing else) and says so in the one line.
void test_the_numbers_of_d45_on_the_chip_are_replaced_by_3390_and_3440_with_one_line() {
  TEST_ASSERT_EQUAL(3300, kEdv0Mv);
  TEST_ASSERT_EQUAL(3390, kEdv1Mv);
  TEST_ASSERT_EQUAL(3440, kEdv2Mv);
  Rig r;
  r.writer.setDataMemory(0x929B, 0x1022);
  r.writer.setDataMemory(0x92B4, 3300);
  r.writer.setDataMemory(0x92B7, 3420);
  r.writer.setDataMemory(0x92BA, 3470);
  const EndOfDischargeResult res = ensureEndOfDischarge(r.gauge, r.writer);
  TEST_ASSERT_TRUE(res.outcome == EndOfDischargeOutcome::Written);
  TEST_ASSERT_EQUAL(2, (int)r.writer.dataMemoryWrites().size());
  TEST_ASSERT_EQUAL_HEX16(0x92B7, r.writer.dataMemoryWrites()[0]);
  TEST_ASSERT_EQUAL_HEX16(0x92BA, r.writer.dataMemoryWrites()[1]);
  TEST_ASSERT_EQUAL_HEX16(0x1022, r.writer.dataMemory(0x929B));
  TEST_ASSERT_EQUAL_STRING(
      "Battery: the fuel gauge's end-of-discharge thresholds set to 3300/3390/3440 mV, compensation off "
      "(they were 3300/3420/3470 mV, compensation off)",
      describeEndOfDischargeCheck(res).c_str());
  TEST_ASSERT_TRUE(ensureEndOfDischarge(r.gauge, r.writer).outcome == EndOfDischargeOutcome::AlreadyRight);
}

void test_a_compensated_chip_with_our_numbers_gets_only_the_compensation_bit() {
  Rig r;
  r.writer.setDataMemory(0x92B4, 3300);
  r.writer.setDataMemory(0x92B7, 3390);
  r.writer.setDataMemory(0x92BA, 3440);
  r.writer.setDataMemory(0x929B, 0x0008);
  TEST_ASSERT_TRUE(ensureEndOfDischarge(r.gauge, r.writer).outcome == EndOfDischargeOutcome::Written);
  TEST_ASSERT_EQUAL(1, (int)r.writer.dataMemoryWrites().size());
  TEST_ASSERT_EQUAL_HEX16(0x0000, r.writer.dataMemory(0x929B));
}

void test_an_unreadable_or_implausible_configuration_writes_nothing() {
  Rig r;
  r.writer.failDataMemoryRead();
  TEST_ASSERT_TRUE(ensureEndOfDischarge(r.gauge, r.writer).outcome == EndOfDischargeOutcome::ReadFailed);
  Rig q;
  q.writer.setDataMemory(0x929B, 0xFFFF);  // outside the manual's 0x0 to 0x1FFF: not what a chip holds
  TEST_ASSERT_TRUE(ensureEndOfDischarge(q.gauge, q.writer).outcome == EndOfDischargeOutcome::ReadFailed);
  TEST_ASSERT_EQUAL(0, (int)r.writer.dataMemoryWrites().size() + (int)q.writer.dataMemoryWrites().size());
}

void test_a_refused_or_lost_threshold_write_is_reported() {
  Rig r;
  r.writer.keepDataMemory();
  const EndOfDischargeResult kept = ensureEndOfDischarge(r.gauge, r.writer);
  TEST_ASSERT_TRUE(kept.outcome == EndOfDischargeOutcome::ReadBackDiffers);
  TEST_ASSERT_EQUAL(3501, kept.after.edv2Mv);
  Rig q;
  q.writer.refuseDataMemory();
  const EndOfDischargeResult refused = ensureEndOfDischarge(q.gauge, q.writer);
  TEST_ASSERT_TRUE(refused.outcome == EndOfDischargeOutcome::WriteRefused);
  TEST_ASSERT_EQUAL(3501, q.writer.dataMemory(0x92BA));
  TEST_ASSERT_EQUAL_STRING(
      "Battery: the fuel gauge's end-of-discharge thresholds could not be set, the chip refused (they are 3031/3385/3501 mV, compensation on)",
      describeEndOfDischargeCheck(refused).c_str());
}

void test_a_missing_gauge_checks_no_thresholds() {
  FakeI2cBus bus;
  FuelGauge gauge(bus);
  FakeGaugeWriter writer(bus);
  TEST_ASSERT_TRUE(ensureEndOfDischarge(gauge, writer).outcome == EndOfDischargeOutcome::GaugeAbsent);
  TEST_ASSERT_EQUAL(0, writer.dataMemoryReads());
}

void test_the_threshold_log_lines_say_what_happened() {
  Rig r;
  const std::string written = describeEndOfDischargeCheck(ensureEndOfDischarge(r.gauge, r.writer));
  TEST_ASSERT_EQUAL_STRING(
      "Battery: the fuel gauge's end-of-discharge thresholds set to 3300/3390/3440 mV, compensation off "
      "(they were 3031/3385/3501 mV, compensation on)",
      written.c_str());
  TEST_ASSERT_EQUAL_STRING(
      "Battery: the fuel gauge's end-of-discharge thresholds are 3300/3390/3440 mV, compensation off already, nothing to write",
      describeEndOfDischargeCheck(ensureEndOfDischarge(r.gauge, r.writer)).c_str());
  Rig q;
  q.writer.keepDataMemory();
  const std::string kept = describeEndOfDischargeCheck(ensureEndOfDischarge(q.gauge, q.writer));
  TEST_ASSERT_TRUE_MESSAGE(kept.find("could not be set") != std::string::npos, kept.c_str());
  Rig f;
  f.writer.failDataMemoryRead();
  TEST_ASSERT_TRUE(describeEndOfDischargeCheck(ensureEndOfDischarge(f.gauge, f.writer)).find("could not be read") != std::string::npos);
}

// The bytes of one block against the facts taken from the manual (docs/hardware/fuel-gauge-data-memory.md): the address
// low byte first to 0x3E, the value high byte first from 0x40 (p.65, 1200 mAh as 0x04, 0xB0), the checksum
// 0xFF minus the low byte of the sum of address and value, the length four plus two (p.52: address 0x9221 with
// one data byte 0x00 gives 0x4C and 5).
void test_a_data_memory_block_has_the_manuals_bytes_checksum_and_length() {
  const DataMemoryBlock edv2 = dataMemoryBlock(0x92BA, 3501);  // the default Fixed EDV 2, 0x0DAD
  TEST_ASSERT_EQUAL_HEX8(0xBA, edv2.bytes[0]);
  TEST_ASSERT_EQUAL_HEX8(0x92, edv2.bytes[1]);
  TEST_ASSERT_EQUAL_HEX8(0x0D, edv2.bytes[2]);
  TEST_ASSERT_EQUAL_HEX8(0xAD, edv2.bytes[3]);
  TEST_ASSERT_EQUAL_HEX8(0xF9, edv2.checksum);
  TEST_ASSERT_EQUAL(6, edv2.length);
  const DataMemoryBlock design = dataMemoryBlock(0x929F, 1200);
  TEST_ASSERT_EQUAL_HEX8(0x04, design.bytes[2]);
  TEST_ASSERT_EQUAL_HEX8(0xB0, design.bytes[3]);
  // SensorLib's formula for the capacity it writes: 0xFF - ((0x9F + 0x92 + 0x04 + 0xB0) & 0xFF).
  TEST_ASSERT_EQUAL_HEX8(0x1A, design.checksum);
  // The p.52 example's sum (0x21 + 0x92 + 0x00) as the low address byte and a zero value: the same checksum.
  TEST_ASSERT_EQUAL_HEX8(0x4C, dataMemoryBlock(0x00B3, 0x0000).checksum);
  const DataMemoryBlock config = dataMemoryBlock(0x929B, 0x1022);
  TEST_ASSERT_EQUAL_HEX8(0x9B, config.bytes[0]);
  TEST_ASSERT_EQUAL_HEX8(0x10, config.bytes[2]);
  TEST_ASSERT_EQUAL_HEX8(0x22, config.bytes[3]);
  TEST_ASSERT_EQUAL_HEX8((uint8_t)(0xFF - ((0x9B + 0x92 + 0x10 + 0x22) & 0xFF)), config.checksum);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_the_values_measured_on_the_device_read_back);
  RUN_TEST(test_the_limits_of_the_registers);
  RUN_TEST(test_a_missing_gauge_is_not_read_and_leaves_the_reading_alone);
  RUN_TEST(test_the_factory_capacity_is_replaced_in_both_registers_and_read_back);
  RUN_TEST(test_a_capacity_that_is_right_is_not_written);
  RUN_TEST(test_the_second_start_after_a_write_writes_nothing);
  RUN_TEST(test_a_learned_full_charge_is_left_to_the_gauge_and_named);
  RUN_TEST(test_a_refused_write_is_reported_and_leaves_the_chip_as_it_was);
  RUN_TEST(test_a_write_that_did_not_take_is_found_by_the_read_back);
  RUN_TEST(test_a_missing_gauge_writes_nothing);
  RUN_TEST(test_an_implausible_setting_is_never_written);
  RUN_TEST(test_the_log_line_says_what_happened);
  RUN_TEST(test_the_thresholds_are_set_with_the_compensation_off_and_the_other_bits_kept);
  RUN_TEST(test_the_thresholds_lie_between_the_switch_off_and_the_fall_in_order);
  RUN_TEST(test_only_the_items_that_differ_are_written);
  RUN_TEST(test_thresholds_that_are_right_are_not_written_and_the_second_start_writes_nothing);
  RUN_TEST(test_thresholds_lost_by_the_chip_are_written_again_at_the_next_start);
  RUN_TEST(test_the_numbers_of_d45_on_the_chip_are_replaced_by_3390_and_3440_with_one_line);
  RUN_TEST(test_a_compensated_chip_with_our_numbers_gets_only_the_compensation_bit);
  RUN_TEST(test_an_unreadable_or_implausible_configuration_writes_nothing);
  RUN_TEST(test_a_refused_or_lost_threshold_write_is_reported);
  RUN_TEST(test_a_missing_gauge_checks_no_thresholds);
  RUN_TEST(test_the_threshold_log_lines_say_what_happened);
  RUN_TEST(test_a_data_memory_block_has_the_manuals_bytes_checksum_and_length);
  return UNITY_END();
}

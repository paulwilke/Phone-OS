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

void test_either_capacity_differing_is_enough_and_both_are_written() {
  Rig r;
  r.bus.setRegister16(i2c::kFuelGauge, 0x3C, 1500);  // design right, full charge wrong
  r.bus.setRegister16(i2c::kFuelGauge, 0x12, 1200);
  TEST_ASSERT_TRUE(ensureCapacity(r.gauge, r.writer, 1500).outcome == CapacityOutcome::Written);
  TEST_ASSERT_EQUAL(1500, r.writer.lastDesign());
  TEST_ASSERT_EQUAL(1500, r.writer.lastFullCharge());
  Rig q;
  q.bus.setRegister16(i2c::kFuelGauge, 0x3C, 1200);  // the other way round
  q.bus.setRegister16(i2c::kFuelGauge, 0x12, 1500);
  TEST_ASSERT_TRUE(ensureCapacity(q.gauge, q.writer, 1500).outcome == CapacityOutcome::Written);
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

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_the_values_measured_on_the_device_read_back);
  RUN_TEST(test_the_limits_of_the_registers);
  RUN_TEST(test_a_missing_gauge_is_not_read_and_leaves_the_reading_alone);
  RUN_TEST(test_the_factory_capacity_is_replaced_in_both_registers_and_read_back);
  RUN_TEST(test_a_capacity_that_is_right_is_not_written);
  RUN_TEST(test_the_second_start_after_a_write_writes_nothing);
  RUN_TEST(test_either_capacity_differing_is_enough_and_both_are_written);
  RUN_TEST(test_a_refused_write_is_reported_and_leaves_the_chip_as_it_was);
  RUN_TEST(test_a_write_that_did_not_take_is_found_by_the_read_back);
  RUN_TEST(test_a_missing_gauge_writes_nothing);
  RUN_TEST(test_an_implausible_setting_is_never_written);
  RUN_TEST(test_the_log_line_says_what_happened);
  return UNITY_END();
}

// The diagnosis of both power chips: read-only, and complete with the values measured on the device.

#include <unity.h>

#include <BoardPowerRig.h>
#include <board/BatteryDiagnostic.h>

using namespace board;

void setUp() {}
void tearDown() {}

namespace {
bool has(const std::vector<std::string>& lines, const std::string& part) {
  for (const auto& l : lines)
    if (l.find(part) != std::string::npos) return true;
  return false;
}
}  // namespace

void test_the_measurement_shows_every_value_and_the_verdict() {
  BoardPowerRig r(false);
  r.scriptMeasured();
  const auto lines = formatBatteryDiagnostic(readBatteryDiagnostic(r.gauge, r.charger), 1500);
  TEST_ASSERT_TRUE(has(lines, "gauge: voltage 4118 mV"));
  TEST_ASSERT_TRUE(has(lines, "remaining 1655 mAh, full charge 3000 mAh, design 3000 mAh (configured 1500 mAh: the gauge holds another battery size)"));
  TEST_ASSERT_TRUE(has(lines, "state of charge 56 %, cycle count 0, time to empty 2858 min"));
  TEST_ASSERT_TRUE(has(lines, "charger: SY6970 at 0x6A"));
  TEST_ASSERT_TRUE(has(lines, "00=48"));
  TEST_ASSERT_TRUE(has(lines, "04=20"));
  TEST_ASSERT_TRUE(has(lines, "05=13"));
  TEST_ASSERT_TRUE(has(lines, "06=5E"));
  TEST_ASSERT_TRUE(has(lines, "0B=BE"));
  TEST_ASSERT_TRUE(has(lines, "0E=5D"));
  TEST_ASSERT_TRUE(has(lines, "input current limit 500 mA (limit register in force), input: unknown adapter"));
  TEST_ASSERT_TRUE(has(lines, "charge done, power good yes"));
  TEST_ASSERT_TRUE(has(lines, "charge voltage 4208 mV, charge current 2048 mA"));
  TEST_ASSERT_TRUE(has(lines, "battery 4164 mV"));
  TEST_ASSERT_TRUE(has(lines, "verdict: the gauge's 56 % is contradicted"));
  TEST_ASSERT_TRUE(has(lines, "shows full"));
}

void test_a_right_capacity_is_not_called_out() {
  BoardPowerRig r;
  const auto lines = formatBatteryDiagnostic(readBatteryDiagnostic(r.gauge, r.charger), 1500);
  TEST_ASSERT_FALSE(has(lines, "configured"));
  TEST_ASSERT_TRUE(has(lines, "verdict: the gauge's percentage agrees with the voltage"));
}

void test_the_diagnosis_writes_nothing_to_either_chip() {
  BoardPowerRig r;
  r.scriptMeasured();
  const size_t before = r.bus.transactions().size();
  readBatteryDiagnostic(r.gauge, r.charger);
  const auto& all = r.bus.transactions();
  for (size_t i = before; i < all.size(); ++i) {
    if (all[i].isWrite) TEST_ASSERT_TRUE_MESSAGE(all[i].data.size() <= 1, "a write that carries data");
  }
  TEST_ASSERT_EQUAL(0, r.writer.calls());
}

void test_the_older_charger_is_named_with_its_address() {
  BoardPowerRig r(false);
  r.bus.removeChip(i2c::kChargerSy6970);
  r.bus.addChip(i2c::kChargerBq25896);
  const auto lines = formatBatteryDiagnostic(readBatteryDiagnostic(r.gauge, r.charger), 1500);
  TEST_ASSERT_TRUE(has(lines, "charger: BQ25896 at 0x6B"));
}

void test_missing_chips_are_said_in_one_line_each() {
  FakeI2cBus bus;
  FuelGauge gauge(bus);
  Charger charger(bus);
  const auto lines = formatBatteryDiagnostic(readBatteryDiagnostic(gauge, charger), 1500);
  TEST_ASSERT_EQUAL(2, lines.size());
  TEST_ASSERT_TRUE(has(lines, "gauge: BQ27220 does not answer"));
  TEST_ASSERT_TRUE(has(lines, "charger: no answer at 0x6A (SY6970) or 0x6B (BQ25896)"));
}

void test_the_temperature_below_zero_keeps_its_sign() {
  BoardPowerRig r(false);
  r.bus.setRegister16(i2c::kFuelGauge, 0x28, 2731 - 5);  // -0.5 C
  const auto lines = formatBatteryDiagnostic(readBatteryDiagnostic(r.gauge, r.charger), 1500);
  TEST_ASSERT_TRUE(has(lines, "temperature -0.5 C"));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_the_measurement_shows_every_value_and_the_verdict);
  RUN_TEST(test_a_right_capacity_is_not_called_out);
  RUN_TEST(test_the_diagnosis_writes_nothing_to_either_chip);
  RUN_TEST(test_the_older_charger_is_named_with_its_address);
  RUN_TEST(test_missing_chips_are_said_in_one_line_each);
  RUN_TEST(test_the_temperature_below_zero_keeps_its_sign);
  return UNITY_END();
}

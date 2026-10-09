// The energy log: one reading of the fuel gauge every ten minutes in /energy.log, so that a battery that ran
// down fast can be explained afterwards. The numbers are those read from the device on 2026-10-09.

#include <unity.h>

#include <BoardStorageRigs.h>
#include <board/EnergyLog.h>
#include <board/PinMap.h>
#include <fakes/FakeI2cBus.h>

using namespace board;

void setUp() {}
void tearDown() {}

namespace {
struct Rig {
  SdStack stack;
  FakeI2cBus bus;
  FuelGauge gauge{bus};
  FakeClock clock;
  EnergyLog log{stack.card, gauge, clock};
  explicit Rig(bool gaugePresent = true) {
    if (gaugePresent) {
      bus.addChip(i2c::kFuelGauge);
      bus.setRegister16(i2c::kFuelGauge, 0x2C, 63);
      bus.setRegister16(i2c::kFuelGauge, 0x08, 3754);
      bus.setRegister16(i2c::kFuelGauge, 0x0C, (uint16_t)(int16_t)-42);
      bus.setRegister16(i2c::kFuelGauge, 0x10, 939);
    }
  }
  std::string file() { return stack.volume.file(kEnergyLogPath); }
};
}  // namespace

void test_a_line_has_the_time_the_uptime_the_gauge_and_the_state() {
  Rig r;
  r.clock.advanceSeconds(600);
  r.clock.setEpochUtc(1791489827u);  // set after the uptime: the epoch advances with it
  TEST_ASSERT_TRUE(r.log.record(600000, EnergyState::ModemSleep));
  TEST_ASSERT_EQUAL_STRING("1791489827 600 63 3754 -42 939 modem-sleep\n", r.file().c_str());
}

void test_an_unknown_time_is_a_dash_never_1970() {
  Rig r;
  r.clock.advanceSeconds(30);
  r.log.record(30000, EnergyState::Awake);
  TEST_ASSERT_EQUAL_STRING("- 30 63 3754 -42 939 awake\n", r.file().c_str());
}

void test_the_three_states_have_their_words() {
  GaugeReading g;
  TEST_ASSERT_TRUE(EnergyLog::format(1, 2, &g, EnergyState::Cable).find(" cable") != std::string::npos);
  TEST_ASSERT_TRUE(EnergyLog::format(1, 2, &g, EnergyState::ModemSleep).find(" modem-sleep") != std::string::npos);
  TEST_ASSERT_TRUE(EnergyLog::format(1, 2, &g, EnergyState::Awake).find(" awake") != std::string::npos);
}

void test_a_gauge_that_does_not_answer_still_leaves_a_line_with_dashes() {
  Rig r(false);
  r.log.record(0, EnergyState::Awake);
  TEST_ASSERT_EQUAL_STRING("- 0 - - - - awake\n", r.file().c_str());
}

void test_a_line_is_due_at_the_start_and_then_every_ten_minutes() {
  Rig r;
  TEST_ASSERT_TRUE(r.log.due(5000));
  r.log.record(5000, EnergyState::Awake);
  TEST_ASSERT_FALSE(r.log.due(5000 + kEnergyIntervalMs - 1));
  TEST_ASSERT_TRUE(r.log.due(5000 + kEnergyIntervalMs));
}

void test_the_interval_survives_the_wrap_of_the_millisecond_counter() {
  Rig r;
  const uint32_t before = 0xFFFFFFFFu - 1000;
  r.log.record(before, EnergyState::Awake);
  TEST_ASSERT_FALSE(r.log.due(before + 1000));
  TEST_ASSERT_TRUE(r.log.due(before + kEnergyIntervalMs));
}

void test_lines_are_appended_not_replaced() {
  Rig r;
  r.log.record(0, EnergyState::Awake);
  r.log.record(kEnergyIntervalMs, EnergyState::Cable);
  const std::string f = r.file();
  TEST_ASSERT_EQUAL(2, (int)std::count(f.begin(), f.end(), '\n'));
}

void test_a_missing_card_writes_nothing_and_does_not_queue() {
  Rig r;
  r.stack.volume.removeCard();
  TEST_ASSERT_FALSE(r.log.record(0, EnergyState::Awake));
  r.stack.volume.insertCard();
  TEST_ASSERT_FALSE(r.stack.volume.has(kEnergyLogPath));
}

void test_a_failed_write_is_reported_and_the_next_line_waits_for_its_time() {
  Rig r;
  r.stack.volume.failAllWrites(true);
  TEST_ASSERT_FALSE(r.log.record(0, EnergyState::Awake));
  TEST_ASSERT_FALSE(r.log.due(1000));
}

void test_below_32_kb_free_nothing_is_written() {
  Rig r;
  r.stack.volume.setCapacity(kLogReserveBytes - 1);
  TEST_ASSERT_FALSE(r.log.record(0, EnergyState::Awake));
  TEST_ASSERT_FALSE(r.stack.volume.has(kEnergyLogPath));
}

void test_a_full_file_becomes_the_old_one_and_a_new_one_begins() {
  Rig r;
  const std::string big(kEnergyLogRotateBytes, 'x');
  r.stack.volume.writeFile(kEnergyLogPath, reinterpret_cast<const uint8_t*>(big.data()), big.size());
  r.stack.volume.writeFile(kEnergyLogOldPath, reinterpret_cast<const uint8_t*>("older"), 5);
  TEST_ASSERT_TRUE(r.log.record(0, EnergyState::Awake));
  TEST_ASSERT_EQUAL((int)kEnergyLogRotateBytes, (int)r.stack.volume.file(kEnergyLogOldPath).size());
  TEST_ASSERT_EQUAL_STRING("- 0 63 3754 -42 939 awake\n", r.file().c_str());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_a_line_has_the_time_the_uptime_the_gauge_and_the_state);
  RUN_TEST(test_an_unknown_time_is_a_dash_never_1970);
  RUN_TEST(test_the_three_states_have_their_words);
  RUN_TEST(test_a_gauge_that_does_not_answer_still_leaves_a_line_with_dashes);
  RUN_TEST(test_a_line_is_due_at_the_start_and_then_every_ten_minutes);
  RUN_TEST(test_the_interval_survives_the_wrap_of_the_millisecond_counter);
  RUN_TEST(test_lines_are_appended_not_replaced);
  RUN_TEST(test_a_missing_card_writes_nothing_and_does_not_queue);
  RUN_TEST(test_a_failed_write_is_reported_and_the_next_line_waits_for_its_time);
  RUN_TEST(test_below_32_kb_free_nothing_is_written);
  RUN_TEST(test_a_full_file_becomes_the_old_one_and_a_new_one_begins);
  return UNITY_END();
}

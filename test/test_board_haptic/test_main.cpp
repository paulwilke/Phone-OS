// The vibration driver and the motor's supply: the register sequence, the supply that is on only while the
// motor is used, and a motor that does not answer.

#include <unity.h>

#include <BoardInputRigs.h>

using namespace board;

void setUp() {}
void tearDown() {}

namespace {
constexpr uint8_t kAddr = i2c::kHaptic;
}

void test_begin_puts_the_chip_into_erm_open_loop_with_the_first_library() {
  FakeI2cBus bus;
  bus.addChip(kAddr);
  bus.setRegister(kAddr, 0x1A, 0xFF);  // N_ERM_LRA set: an LRA motor
  bus.setRegister(kAddr, 0x1D, 0x00);
  Drv2605 drv(bus);
  TEST_ASSERT_TRUE(drv.begin());
  TEST_ASSERT_EQUAL_HEX8(0x7F, bus.reg(kAddr, 0x1A));  // bit 7 cleared, the rest kept
  TEST_ASSERT_EQUAL_HEX8(0x20, bus.reg(kAddr, 0x1D));  // open loop set
  TEST_ASSERT_EQUAL(1, bus.reg(kAddr, 0x03));          // library 1
  TEST_ASSERT_EQUAL(0, bus.reg(kAddr, 0x01));          // internal trigger
  TEST_ASSERT_EQUAL_HEX8(0x64, bus.reg(kAddr, 0x13));
}

void test_the_other_bits_of_the_two_registers_survive() {
  FakeI2cBus bus;
  bus.addChip(kAddr);
  bus.setRegister(kAddr, 0x1A, 0xB5);
  bus.setRegister(kAddr, 0x1D, 0x81);
  Drv2605 drv(bus);
  drv.begin();
  TEST_ASSERT_EQUAL_HEX8(0x35, bus.reg(kAddr, 0x1A));
  TEST_ASSERT_EQUAL_HEX8(0xA1, bus.reg(kAddr, 0x1D));
}

void test_a_chip_that_does_not_answer_fails_the_start() {
  FakeI2cBus bus;
  Drv2605 drv(bus);
  TEST_ASSERT_FALSE(drv.begin());
}

void test_firing_writes_the_effect_ends_the_sequence_and_goes() {
  FakeI2cBus bus;
  bus.addChip(kAddr);
  Drv2605 drv(bus);
  TEST_ASSERT_TRUE(drv.fire(15));
  TEST_ASSERT_EQUAL(15, bus.reg(kAddr, 0x04));
  TEST_ASSERT_EQUAL(0, bus.reg(kAddr, 0x05));
  TEST_ASSERT_EQUAL(1, bus.reg(kAddr, 0x0C));
}

// The supply comes on with the first pulse and goes off two seconds after the last.
void test_the_motor_has_supply_only_while_it_is_used() {
  BoardHapticRig r;
  TEST_ASSERT_FALSE(r.expander.level(leg::kHapticPower));
  r.impl.play(HapticPattern::Click);
  r.impl.service();
  TEST_ASSERT_TRUE(r.expander.level(leg::kHapticPower));
  TEST_ASSERT_EQUAL(1, r.fired().size());
  r.advance(1999);
  r.impl.service();
  TEST_ASSERT_TRUE(r.expander.level(leg::kHapticPower));
  r.advance(1);
  r.impl.service();
  TEST_ASSERT_FALSE(r.expander.level(leg::kHapticPower));
}

void test_the_supply_stays_on_between_the_pulses_of_a_pattern() {
  BoardHapticRig r;
  r.impl.play(HapticPattern::Sent);
  for (int i = 0; i < 40; ++i) {
    r.impl.service();
    r.advance(10);
    TEST_ASSERT_TRUE(r.expander.level(leg::kHapticPower));
  }
  TEST_ASSERT_EQUAL(3, r.fired().size());
}

void test_the_driver_is_started_again_after_the_supply_was_cut() {
  BoardHapticRig r;
  r.impl.play(HapticPattern::Click);
  r.impl.service();
  r.advance(2000);
  r.impl.service();
  r.bus.setRegister(i2c::kHaptic, 0x03, 0);  // the chip lost its settings with its supply
  r.impl.play(HapticPattern::Click);
  r.impl.service();
  TEST_ASSERT_EQUAL(1, r.bus.reg(i2c::kHaptic, 0x03));
  TEST_ASSERT_EQUAL(2, r.fired().size());
}

// A motor that does not answer loses its pulses; the pattern runs out and nothing stays switched on.
void test_a_motor_that_does_not_answer_loses_the_pulses_and_the_supply_goes_off_again() {
  BoardHapticRig r;
  r.bus.removeChip(i2c::kHaptic);
  r.impl.play(HapticPattern::Message);
  for (int i = 0; i < 50; ++i) {
    r.impl.service();
    r.advance(10);
  }
  TEST_ASSERT_EQUAL(0, r.fired().size());
  TEST_ASSERT_FALSE(r.impl.busy());
  TEST_ASSERT_FALSE(r.expander.level(leg::kHapticPower));
}

void test_a_refused_supply_write_loses_the_pulse_too() {
  BoardHapticRig r;
  r.bus.refuseRegisterWrite(i2c::kExpander, xl9555::kOutput0);
  r.impl.play(HapticPattern::Click);
  r.impl.service();
  TEST_ASSERT_EQUAL(0, r.fired().size());
  TEST_ASSERT_FALSE(r.impl.busy());
}

// A loop that came back late does not squeeze the pulses together: the pause between two pulses counts
// from the moment the first was fired, so the rhythm the child learns stays the rhythm.
void test_a_late_loop_keeps_the_pause_between_the_pulses() {
  BoardHapticRig r;
  r.impl.play(HapticPattern::Sent);
  r.advance(1000);
  r.impl.service();
  TEST_ASSERT_EQUAL(1, r.fired().size());
  r.advance(149);
  r.impl.service();
  TEST_ASSERT_EQUAL(1, r.fired().size());
  r.advance(1);
  r.impl.service();
  TEST_ASSERT_EQUAL(2, r.fired().size());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_begin_puts_the_chip_into_erm_open_loop_with_the_first_library);
  RUN_TEST(test_the_other_bits_of_the_two_registers_survive);
  RUN_TEST(test_a_chip_that_does_not_answer_fails_the_start);
  RUN_TEST(test_firing_writes_the_effect_ends_the_sequence_and_goes);
  RUN_TEST(test_the_motor_has_supply_only_while_it_is_used);
  RUN_TEST(test_the_supply_stays_on_between_the_pulses_of_a_pattern);
  RUN_TEST(test_the_driver_is_started_again_after_the_supply_was_cut);
  RUN_TEST(test_a_motor_that_does_not_answer_loses_the_pulses_and_the_supply_goes_off_again);
  RUN_TEST(test_a_refused_supply_write_loses_the_pulse_too);
  RUN_TEST(test_a_late_loop_keeps_the_pause_between_the_pulses);
  return UNITY_END();
}

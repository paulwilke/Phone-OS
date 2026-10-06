// The start-up order of the board, driven against a scripted expander and pins: what is written in which
// order, how long the resets are held, and what happens when the expander is silent.

#include <unity.h>

#include <vector>

#include <board/PinMap.h>
#include <board/PowerUp.h>
#include <fakes/FakeDelay.h>
#include <fakes/FakeGpio.h>
#include <fakes/FakeI2cBus.h>

using namespace board;

void setUp() {}
void tearDown() {}

namespace {
struct Rig {
  FakeDelay delay;
  FakeI2cBus bus{&delay};
  FakeGpio gpio{&delay};
  Xl9555 expander{bus};
  Rig() { bus.addChip(i2c::kExpander); }
};
}  // namespace

void test_the_expander_gets_outputs_first_then_the_boot_state() {
  Rig r;
  TEST_ASSERT_TRUE(r.expander.begin());
  const auto writes = r.bus.writesTo(i2c::kExpander);
  TEST_ASSERT_EQUAL(5, writes.size());  // the probe, then four registers
  TEST_ASSERT_EQUAL(0, writes[0].size());
  const std::vector<uint8_t> expected[] = {{xl9555::kConfig0, 0x00}, {xl9555::kConfig1, 0x00},
                                           {xl9555::kOutput0, 0x9E}, {xl9555::kOutput1, 0x02}};
  for (int i = 0; i < 4; ++i) TEST_ASSERT_TRUE(expected[i] == writes[i + 1]);
}

void test_the_modem_is_off_after_start_and_nothing_else_it_should_not_be() {
  Rig r;
  TEST_ASSERT_TRUE(r.expander.begin());
  TEST_ASSERT_FALSE(r.expander.level(leg::kModemPower));
  TEST_ASSERT_FALSE(r.expander.level(leg::kHapticPower));
  TEST_ASSERT_FALSE(r.expander.level(leg::kAmplifier));
}

void test_setting_a_leg_rewrites_its_port_and_keeps_the_others() {
  Rig r;
  r.expander.begin();
  TEST_ASSERT_TRUE(r.expander.set(leg::kModemPower, true));
  TEST_ASSERT_EQUAL_HEX8(0x9F, r.bus.reg(i2c::kExpander, xl9555::kOutput0));
  TEST_ASSERT_TRUE(r.expander.set(leg::kAudioSelect, true));  // leg 10 is bit 2 of port 1
  TEST_ASSERT_EQUAL_HEX8(0x06, r.bus.reg(i2c::kExpander, xl9555::kOutput1));
  TEST_ASSERT_TRUE(r.expander.set(leg::kModemPower, false));
  TEST_ASSERT_EQUAL_HEX8(0x9E, r.bus.reg(i2c::kExpander, xl9555::kOutput0));
}

void test_a_refused_write_leaves_the_remembered_level() {
  Rig r;
  r.expander.begin();
  r.bus.refuseRegisterWrite(i2c::kExpander, xl9555::kOutput0);
  TEST_ASSERT_FALSE(r.expander.set(leg::kGnssPower, false));
  TEST_ASSERT_TRUE(r.expander.level(leg::kGnssPower));
}

void test_a_leg_that_does_not_exist_is_refused() {
  Rig r;
  r.expander.begin();
  TEST_ASSERT_FALSE(r.expander.set(16, true));
  TEST_ASSERT_FALSE(r.expander.level(16));
}

void test_a_silent_expander_is_reported_and_nothing_behind_it_is_touched() {
  FakeDelay delay;
  FakeI2cBus bus(&delay);
  FakeGpio gpio(&delay);
  Xl9555 expander(bus);
  TEST_ASSERT_FALSE(powerUp(expander, gpio, delay));
  TEST_ASSERT_FALSE(expander.ready());
  TEST_ASSERT_FALSE(expander.set(leg::kModemPower, true));
  TEST_ASSERT_EQUAL(0, delay.elapsedMs());
  // The processor's own pins are still set, so the front light is dark.
  TEST_ASSERT_TRUE(gpio.mode(pin::kFrontlight) == Gpio::Mode::Output);
  TEST_ASSERT_FALSE(gpio.level(pin::kFrontlight));
}

void test_the_resets_are_held_low_for_20_ms_and_settle_for_50() {
  Rig r;
  TEST_ASSERT_TRUE(powerUp(r.expander, r.gpio, r.delay));
  // After the four register writes of begin() come four writes: touch low, touch released, keyboard low,
  // keyboard released. Touch is on port 0 (leg 7, bit 0x80), the keyboard on port 1 (leg 9, bit 0x02).
  std::vector<FakeI2cBus::Transaction> w;
  for (const auto& t : r.bus.transactions())
    if (t.address == i2c::kExpander && t.isWrite && t.data.size() == 2) w.push_back(t);
  TEST_ASSERT_EQUAL(8, w.size());
  TEST_ASSERT_EQUAL_HEX8(0x1E, w[4].data[1]);  // 0x9E with the touch reset low
  TEST_ASSERT_EQUAL(0, w[4].atMs);
  TEST_ASSERT_EQUAL_HEX8(0x9E, w[5].data[1]);
  TEST_ASSERT_EQUAL(20, w[5].atMs);
  TEST_ASSERT_EQUAL_HEX8(0x00, w[6].data[1]);  // 0x02 with the keyboard reset low
  TEST_ASSERT_EQUAL(70, w[6].atMs);
  TEST_ASSERT_EQUAL_HEX8(0x02, w[7].data[1]);
  TEST_ASSERT_EQUAL(90, w[7].atMs);
  TEST_ASSERT_EQUAL(140, r.delay.elapsedMs());
}

void test_both_resets_end_released_and_the_pins_are_set() {
  Rig r;
  TEST_ASSERT_TRUE(powerUp(r.expander, r.gpio, r.delay));
  TEST_ASSERT_TRUE(r.expander.level(leg::kTouchReset));
  TEST_ASSERT_TRUE(r.expander.level(leg::kKeyboardReset));
  TEST_ASSERT_FALSE(r.gpio.level(pin::kFrontlight));
  TEST_ASSERT_TRUE(r.gpio.mode(pin::kBootButton) == Gpio::Mode::Input);
  TEST_ASSERT_TRUE(r.gpio.mode(pin::kSpiMiso) == Gpio::Mode::InputPullUp);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_the_expander_gets_outputs_first_then_the_boot_state);
  RUN_TEST(test_the_modem_is_off_after_start_and_nothing_else_it_should_not_be);
  RUN_TEST(test_setting_a_leg_rewrites_its_port_and_keeps_the_others);
  RUN_TEST(test_a_refused_write_leaves_the_remembered_level);
  RUN_TEST(test_a_leg_that_does_not_exist_is_refused);
  RUN_TEST(test_a_silent_expander_is_reported_and_nothing_behind_it_is_touched);
  RUN_TEST(test_the_resets_are_held_low_for_20_ms_and_settle_for_50);
  RUN_TEST(test_both_resets_end_released_and_the_pins_are_set);
  return UNITY_END();
}

// The keyboard controller: the start-up sequence, the FIFO, and a chip that is not there.

#include <unity.h>

#include <board/PinMap.h>
#include <board/Tca8418.h>
#include <fakes/FakeDelay.h>
#include <fakes/FakeI2cBus.h>

using namespace board;

void setUp() {}
void tearDown() {}

namespace {
constexpr uint8_t kAddr = i2c::kKeyboard;

struct Rig {
  FakeDelay delay;
  FakeI2cBus bus{&delay};
  Tca8418 chip{bus, delay};
  Rig() {
    bus.addChip(kAddr);
    bus.addFifo(kAddr, 0x04, 0x03);
  }
};
}  // namespace

void test_start_sets_the_matrix_and_turns_the_scanner_on_last() {
  Rig r;
  TEST_ASSERT_TRUE(r.chip.begin());
  TEST_ASSERT_TRUE(r.chip.ready());
  TEST_ASSERT_EQUAL_HEX8(0xFF, r.bus.reg(kAddr, 0x1D));  // rows 0..7
  TEST_ASSERT_EQUAL_HEX8(0xFF, r.bus.reg(kAddr, 0x1E));  // columns 0..7
  TEST_ASSERT_EQUAL_HEX8(0x03, r.bus.reg(kAddr, 0x1F));  // columns 8 and 9
  TEST_ASSERT_EQUAL_HEX8(0x11, r.bus.reg(kAddr, 0x01));
  // The scanner is switched off before anything else and on after everything else.
  const auto writes = r.bus.writesTo(kAddr);
  std::vector<std::vector<uint8_t>> registerWrites;
  for (const auto& w : writes)
    if (w.size() == 2) registerWrites.push_back(w);
  TEST_ASSERT_TRUE((std::vector<uint8_t>{0x01, 0x00}) == registerWrites.front());
  bool scannerOnIsLastConfig = false;
  for (size_t i = 0; i < registerWrites.size(); ++i)
    if (registerWrites[i][0] == 0x01 && registerWrites[i][1] == 0x11) scannerOnIsLastConfig = true;
  TEST_ASSERT_TRUE(scannerOnIsLastConfig);
}

// Register 0x29 disables the debounce per row when a bit is set. 0x03 would have switched it off for
// twenty of the 35 keys.
void test_the_debounce_stays_on_for_every_row() {
  Rig r;
  r.bus.setRegister(kAddr, 0x29, 0xFF);  // whatever an earlier session left
  r.chip.begin();
  TEST_ASSERT_EQUAL_HEX8(0x00, r.bus.reg(kAddr, 0x29));
}

// The chip survives a reset of the processor and may hold events of the last session.
void test_events_left_over_from_an_earlier_session_are_thrown_away() {
  Rig r;
  r.bus.pushFifo(kAddr, 0x04, {0x8A, 0x0A, 0x95});
  TEST_ASSERT_TRUE(r.chip.begin());
  KeyEvent e;
  TEST_ASSERT_FALSE(r.chip.next(e));
}

void test_a_press_and_a_release_come_out_in_order_with_their_codes() {
  Rig r;
  r.chip.begin();
  r.bus.pushFifo(kAddr, 0x04, {0x8A, 0x0A});  // press code 10, release code 10
  KeyEvent e;
  TEST_ASSERT_TRUE(r.chip.next(e));
  TEST_ASSERT_EQUAL(10, e.code);
  TEST_ASSERT_TRUE(e.pressed);
  TEST_ASSERT_TRUE(r.chip.next(e));
  TEST_ASSERT_EQUAL(10, e.code);
  TEST_ASSERT_FALSE(e.pressed);
  TEST_ASSERT_FALSE(r.chip.next(e));
}

void test_each_event_clears_the_interrupt_flags() {
  Rig r;
  r.chip.begin();
  const int before = r.bus.writeCount(kAddr, 0x02);
  r.bus.pushFifo(kAddr, 0x04, {0x95});
  KeyEvent e;
  r.chip.next(e);
  TEST_ASSERT_EQUAL(before + 1, r.bus.writeCount(kAddr, 0x02));
  TEST_ASSERT_EQUAL_HEX8(0x1F, r.bus.reg(kAddr, 0x02));
}

void test_nothing_is_read_from_a_chip_that_was_not_started() {
  Rig r;
  r.bus.pushFifo(kAddr, 0x04, {0x95});
  KeyEvent e;
  TEST_ASSERT_FALSE(r.chip.next(e));
}

void test_a_missing_keyboard_is_reported_and_not_started() {
  FakeDelay delay;
  FakeI2cBus bus(&delay);
  Tca8418 chip(bus, delay);
  TEST_ASSERT_FALSE(chip.begin());
  TEST_ASSERT_FALSE(chip.ready());
  KeyEvent e;
  TEST_ASSERT_FALSE(chip.next(e));
}

void test_a_refused_register_write_fails_the_start() {
  Rig r;
  r.bus.refuseRegisterWrite(kAddr, 0x1F);
  TEST_ASSERT_FALSE(r.chip.begin());
  TEST_ASSERT_FALSE(r.chip.ready());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_start_sets_the_matrix_and_turns_the_scanner_on_last);
  RUN_TEST(test_the_debounce_stays_on_for_every_row);
  RUN_TEST(test_events_left_over_from_an_earlier_session_are_thrown_away);
  RUN_TEST(test_a_press_and_a_release_come_out_in_order_with_their_codes);
  RUN_TEST(test_each_event_clears_the_interrupt_flags);
  RUN_TEST(test_nothing_is_read_from_a_chip_that_was_not_started);
  RUN_TEST(test_a_missing_keyboard_is_reported_and_not_started);
  RUN_TEST(test_a_refused_register_write_fails_the_start);
  return UNITY_END();
}

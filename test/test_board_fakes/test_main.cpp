// The scripted bus, pins and port the board tests stand on: a fake that answers wrongly would make every
// driver test wrong, so what it promises is pinned here.

#include <unity.h>

#include <fakes/FakeDelay.h>
#include <fakes/FakeGpio.h>
#include <fakes/FakeI2cBus.h>
#include <fakes/FakeUartPort.h>

void setUp() {}
void tearDown() {}

void test_a_chip_nobody_added_does_not_acknowledge() {
  FakeI2cBus bus;
  TEST_ASSERT_FALSE(bus.probe(0x20));
  bus.addChip(0x20);
  TEST_ASSERT_TRUE(bus.probe(0x20));
  bus.removeChip(0x20);
  TEST_ASSERT_FALSE(bus.probe(0x20));
}

void test_a_write_lands_in_the_registers_and_reads_back() {
  FakeI2cBus bus;
  bus.addChip(0x20);
  const uint8_t frame[] = {0x02, 0x9E, 0x02};  // auto-increment: registers 2 and 3
  TEST_ASSERT_TRUE(bus.write(0x20, frame, 3));
  uint8_t v = 0;
  TEST_ASSERT_TRUE(bus.readRegister(0x20, 0x02, v));
  TEST_ASSERT_EQUAL_HEX8(0x9E, v);
  TEST_ASSERT_TRUE(bus.readRegister(0x20, 0x03, v));
  TEST_ASSERT_EQUAL_HEX8(0x02, v);
  TEST_ASSERT_EQUAL_HEX8(0x9E, bus.reg(0x20, 0x02));
}

void test_a_16_bit_register_is_little_endian() {
  FakeI2cBus bus;
  bus.addChip(0x55);
  bus.setRegister16(0x55, 0x08, 4118);
  uint16_t v = 0;
  TEST_ASSERT_TRUE(bus.readRegister16(0x55, 0x08, v));
  TEST_ASSERT_EQUAL(4118, v);
  TEST_ASSERT_EQUAL_HEX8(0x16, bus.reg(0x55, 0x08));
  TEST_ASSERT_EQUAL_HEX8(0x10, bus.reg(0x55, 0x09));
}

void test_a_refused_write_fails_and_changes_nothing() {
  FakeI2cBus bus;
  bus.addChip(0x6A);
  bus.setRegister(0x6A, 0x09, 0x40);
  bus.refuseRegisterWrite(0x6A, 0x09);
  TEST_ASSERT_FALSE(bus.writeRegister(0x6A, 0x09, 0x60));
  TEST_ASSERT_EQUAL_HEX8(0x40, bus.reg(0x6A, 0x09));
  TEST_ASSERT_EQUAL(0, bus.writeCount(0x6A, 0x09));
  bus.refuseWrites(0x6A, true);
  TEST_ASSERT_FALSE(bus.writeRegister(0x6A, 0x05, 1));
  bus.refuseWrites(0x6A, false);
  TEST_ASSERT_TRUE(bus.writeRegister(0x6A, 0x05, 1));
}

void test_write_counts_and_the_transaction_log_are_kept() {
  FakeDelay delay;
  FakeI2cBus bus(&delay);
  bus.addChip(0x5A);
  bus.writeRegister(0x5A, 0x01, 7);
  delay.delayMs(30);
  bus.writeRegister(0x5A, 0x01, 8);
  TEST_ASSERT_EQUAL(2, bus.writeCount(0x5A, 0x01));
  TEST_ASSERT_EQUAL(2, bus.writesTo(0x5A).size());
  TEST_ASSERT_EQUAL(0, bus.transactions()[0].atMs);
  TEST_ASSERT_EQUAL(30, bus.transactions()[1].atMs);
}

void test_gpio_remembers_mode_levels_and_history() {
  FakeDelay delay;
  FakeGpio gpio(&delay);
  TEST_ASSERT_FALSE(gpio.hasMode(41));
  gpio.setMode(41, Gpio::Mode::Output);
  gpio.write(41, true);
  delay.delayMs(5);
  gpio.write(41, false);
  TEST_ASSERT_TRUE(gpio.hasMode(41));
  TEST_ASSERT_TRUE(gpio.mode(41) == Gpio::Mode::Output);
  TEST_ASSERT_FALSE(gpio.level(41));
  TEST_ASSERT_EQUAL(2, gpio.events().size());
  TEST_ASSERT_EQUAL(5, gpio.events()[1].atMs);
  gpio.setInput(0, true);
  TEST_ASSERT_TRUE(gpio.read(0));
}

void test_the_uart_delivers_what_was_fed_and_keeps_what_was_sent() {
  FakeUartPort port;
  TEST_ASSERT_EQUAL(-1, port.read());
  port.feed("AB");
  TEST_ASSERT_EQUAL('A', port.read());
  TEST_ASSERT_EQUAL('B', port.read());
  TEST_ASSERT_EQUAL(-1, port.read());
  const uint8_t out[] = {'h', 'i'};
  TEST_ASSERT_EQUAL(2, port.write(out, 2));
  port.limitWritesTo(1);
  TEST_ASSERT_EQUAL(1, port.write(out, 2));
  TEST_ASSERT_EQUAL_STRING("hih", port.sent().c_str());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_a_chip_nobody_added_does_not_acknowledge);
  RUN_TEST(test_a_write_lands_in_the_registers_and_reads_back);
  RUN_TEST(test_a_16_bit_register_is_little_endian);
  RUN_TEST(test_a_refused_write_fails_and_changes_nothing);
  RUN_TEST(test_write_counts_and_the_transaction_log_are_kept);
  RUN_TEST(test_gpio_remembers_mode_levels_and_history);
  RUN_TEST(test_the_uart_delivers_what_was_fed_and_keeps_what_was_sent);
  return UNITY_END();
}

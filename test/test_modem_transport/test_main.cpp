// The modem's transport on scripted pins and a scripted UART: the bytes, the supply, the power key, DTR and the
// sound path.

#include <unity.h>

#include <board/ModemTransport.h>
#include <board/PinMap.h>
#include <fakes/FakeDelay.h>
#include <fakes/FakeEventLog.h>
#include <fakes/FakeGpio.h>
#include <fakes/FakeI2cBus.h>
#include <fakes/FakeMillis.h>
#include <fakes/FakeUartPort.h>

using namespace board;

void setUp() {}
void tearDown() {}

namespace {
struct Rig {
  FakeDelay delay;
  FakeI2cBus bus{&delay};
  FakeGpio gpio{&delay};
  FakeUartPort uart;
  FakeMillis millis;
  FakeEventLog log;
  Xl9555 expander{bus};
  ModemTransport transport{uart, expander, gpio, delay, millis, log};
  Rig() {
    bus.addChip(i2c::kExpander);
    expander.begin();
    transport.begin();
  }
  // The levels of one leg over time as the expander was written, from the log: (time, level).
  std::vector<std::pair<uint32_t, bool>> legHistory(uint8_t leg) {
    std::vector<std::pair<uint32_t, bool>> out;
    const uint8_t reg = leg < 8 ? xl9555::kOutput0 : xl9555::kOutput1;
    const uint8_t bit = (uint8_t)(1u << (leg % 8));
    for (const auto& t : bus.transactions())
      if (t.address == i2c::kExpander && t.isWrite && t.data.size() == 2 && t.data[0] == reg)
        out.push_back({t.atMs, (t.data[1] & bit) != 0});
    return out;
  }
};
}  // namespace

void test_bytes_go_to_the_port_and_come_from_it() {
  Rig r;
  const char cmd[] = "AT\r";
  r.transport.write(cmd, 3);
  TEST_ASSERT_EQUAL_STRING("AT\r", r.uart.sent().c_str());
  TEST_ASSERT_EQUAL(-1, r.transport.read());
  r.uart.feed("OK");
  TEST_ASSERT_EQUAL('O', r.transport.read());
  TEST_ASSERT_EQUAL('K', r.transport.read());
  TEST_ASSERT_EQUAL(-1, r.transport.read());
}

void test_a_port_that_takes_part_of_a_write_is_given_the_rest() {
  Rig r;
  r.uart.limitWritesTo(4);
  const char cmd[] = "AT+CPIN?\r";
  r.transport.write(cmd, 9);
  TEST_ASSERT_EQUAL_STRING("AT+CPIN?\r", r.uart.sent().c_str());
}

void test_a_port_that_takes_nothing_is_reported_and_does_not_hang() {
  Rig r;
  r.uart.limitWritesTo(0);
  const char cmd[] = "AT\r";
  r.transport.write(cmd, 3);
  TEST_ASSERT_TRUE(r.log.contains("took 0 of 3 bytes"));
}

void test_the_time_is_the_counter_of_the_processor() {
  Rig r;
  r.millis.setTo(12345);
  TEST_ASSERT_EQUAL_UINT32(12345, r.transport.now());
}

void test_the_supply_is_leg_zero_and_never_presses_the_key() {
  Rig r;
  r.transport.power(true);
  TEST_ASSERT_TRUE(r.expander.level(leg::kModemPower));
  TEST_ASSERT_FALSE(r.expander.level(leg::kModemPowerKey));
  r.transport.power(false);
  TEST_ASSERT_FALSE(r.expander.level(leg::kModemPower));
  // A press of the power key switches a running modem off: no write to its leg happened.
  TEST_ASSERT_EQUAL(0, r.bus.reg(i2c::kExpander, xl9555::kOutput1) & 0x01);
  for (const auto& h : r.legHistory(leg::kModemPowerKey)) TEST_ASSERT_FALSE(h.second);
}

// High for 100 ms, low for 1200 ms, high again: the pulse the modem was started with.
void test_the_power_key_pulse_has_the_shape_it_was_brought_up_with() {
  Rig r;
  const uint32_t t0 = r.delay.elapsedMs();
  r.transport.pwrkeyPulse();
  const auto h = r.legHistory(leg::kModemPowerKey);
  // The last three writes are the pulse.
  TEST_ASSERT_TRUE(h.size() >= 3);
  const auto a = h[h.size() - 3], b = h[h.size() - 2], c = h[h.size() - 1];
  TEST_ASSERT_TRUE(a.second);
  TEST_ASSERT_FALSE(b.second);
  TEST_ASSERT_TRUE(c.second);
  TEST_ASSERT_EQUAL(100, b.first - a.first);
  TEST_ASSERT_EQUAL(1200, c.first - b.first);
  TEST_ASSERT_EQUAL(t0 + 1300, r.delay.elapsedMs());
}

void test_dtr_is_a_pin_of_the_processor_and_starts_low() {
  Rig r;
  TEST_ASSERT_TRUE(r.gpio.mode(pin::kModemDtr) == Gpio::Mode::Output);
  TEST_ASSERT_FALSE(r.gpio.level(pin::kModemDtr));
  r.transport.setDtr(true);
  TEST_ASSERT_TRUE(r.gpio.level(pin::kModemDtr));
  r.transport.setDtr(false);
  TEST_ASSERT_FALSE(r.gpio.level(pin::kModemDtr));
}

// Without the switch on the expander the amplifier would play to nobody or pop: the order is part of the contract.
void test_the_sound_goes_to_the_modem_before_the_amplifier_comes_on_and_the_other_way_round() {
  Rig r;
  const size_t start = r.bus.transactions().size();
  r.transport.setAudioToModem(true);
  TEST_ASSERT_TRUE(r.expander.level(leg::kAudioSelect));
  TEST_ASSERT_TRUE(r.expander.level(leg::kAmplifier));
  std::vector<uint8_t> onOrder;
  for (size_t i = start; i < r.bus.transactions().size(); ++i)
    if (r.bus.transactions()[i].isWrite && r.bus.transactions()[i].data.size() == 2) onOrder.push_back(r.bus.transactions()[i].data[0]);
  TEST_ASSERT_EQUAL(2, onOrder.size());
  TEST_ASSERT_EQUAL_HEX8(xl9555::kOutput1, onOrder[0]);  // the switch to the modem first
  TEST_ASSERT_EQUAL_HEX8(xl9555::kOutput0, onOrder[1]);  // then the amplifier
  std::vector<uint8_t> order;
  const size_t from = r.bus.transactions().size();
  r.transport.setAudioToModem(false);
  TEST_ASSERT_FALSE(r.expander.level(leg::kAmplifier));
  TEST_ASSERT_FALSE(r.expander.level(leg::kAudioSelect));
  const auto& all = r.bus.transactions();
  for (size_t i = from; i < all.size(); ++i)
    if (all[i].isWrite && all[i].data.size() == 2) order.push_back(all[i].data[0]);
  TEST_ASSERT_EQUAL(2, order.size());
  TEST_ASSERT_EQUAL_HEX8(xl9555::kOutput0, order[0]);  // the amplifier off first
  TEST_ASSERT_EQUAL_HEX8(xl9555::kOutput1, order[1]);  // then the switch
}

void test_the_amplifier_is_off_after_a_call_and_the_switch_back_to_the_codec() {
  Rig r;
  r.transport.setAudioToModem(true);
  r.transport.setAudioToModem(false);
  TEST_ASSERT_FALSE(r.expander.level(leg::kAmplifier));
  TEST_ASSERT_FALSE(r.expander.level(leg::kAudioSelect));
}

void test_an_expander_that_refuses_is_said_in_the_log() {
  Rig r;
  r.bus.refuseRegisterWrite(i2c::kExpander, xl9555::kOutput0);
  r.transport.power(true);
  TEST_ASSERT_TRUE(r.log.contains("refused to switch the supply on"));
  TEST_ASSERT_FALSE(r.expander.level(leg::kModemPower));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_bytes_go_to_the_port_and_come_from_it);
  RUN_TEST(test_a_port_that_takes_part_of_a_write_is_given_the_rest);
  RUN_TEST(test_a_port_that_takes_nothing_is_reported_and_does_not_hang);
  RUN_TEST(test_the_time_is_the_counter_of_the_processor);
  RUN_TEST(test_the_supply_is_leg_zero_and_never_presses_the_key);
  RUN_TEST(test_the_power_key_pulse_has_the_shape_it_was_brought_up_with);
  RUN_TEST(test_dtr_is_a_pin_of_the_processor_and_starts_low);
  RUN_TEST(test_the_sound_goes_to_the_modem_before_the_amplifier_comes_on_and_the_other_way_round);
  RUN_TEST(test_the_amplifier_is_off_after_a_call_and_the_switch_back_to_the_codec);
  RUN_TEST(test_an_expander_that_refuses_is_said_in_the_log);
  return UNITY_END();
}

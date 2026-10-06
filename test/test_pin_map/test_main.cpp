// Host tests for the pin map: the numbers that were checked against the manufacturer's code and confirmed on
// the device stay as they are, and no two things share a pin.

#include <unity.h>

#include <set>

#include <board/PinMap.h>

using namespace board;

void setUp() {}
void tearDown() {}

void test_no_two_functions_share_a_gpio() {
  const uint8_t pins[] = {
      pin::kI2cSda,       pin::kI2cScl,         pin::kSpiSck,        pin::kSpiMosi,       pin::kSpiMiso,
      pin::kDisplayCs,    pin::kSdCs,           pin::kRadioCs,       pin::kDisplayDc,     pin::kDisplayReset,
      pin::kDisplayBusy,  pin::kFrontlight,     pin::kKeyboardInterrupt, pin::kKeyboardBacklight,
      pin::kTouchInterrupt, pin::kBootButton,   pin::kModemRxd,      pin::kModemTxd,      pin::kModemRing,
      pin::kModemDtr,     pin::kGnssUartRx,     pin::kGnssUartTx,    pin::kGnssPps,       pin::kMotionInterrupt,
      pin::kRadioReset,   pin::kRadioDio1,      pin::kRadioBusy};
  std::set<uint8_t> seen;
  for (uint8_t p : pins) TEST_ASSERT_TRUE_MESSAGE(seen.insert(p).second, "a GPIO is used twice");
  TEST_ASSERT_EQUAL(sizeof(pins), seen.size());
}

void test_the_pins_that_were_confirmed_on_the_device() {
  TEST_ASSERT_EQUAL(13, pin::kI2cSda);
  TEST_ASSERT_EQUAL(14, pin::kI2cScl);
  TEST_ASSERT_EQUAL(36, pin::kSpiSck);
  TEST_ASSERT_EQUAL(33, pin::kSpiMosi);
  TEST_ASSERT_EQUAL(47, pin::kSpiMiso);
  TEST_ASSERT_EQUAL(34, pin::kDisplayCs);
  TEST_ASSERT_EQUAL(35, pin::kDisplayDc);
  TEST_ASSERT_EQUAL(9, pin::kDisplayReset);
  TEST_ASSERT_EQUAL(37, pin::kDisplayBusy);
  TEST_ASSERT_EQUAL(41, pin::kFrontlight);
  TEST_ASSERT_EQUAL(48, pin::kSdCs);
  TEST_ASSERT_EQUAL(15, pin::kKeyboardInterrupt);
  TEST_ASSERT_EQUAL(12, pin::kTouchInterrupt);
}

// The lines to the modem were once passed on with the schematic's names, which are the modem's view: the
// processor receives on the modem's TXD and sends on its RXD. The modem answered AT only after the fix.
void test_the_modem_uart_is_crossed() {
  TEST_ASSERT_EQUAL(10, pin::kModemRxd);
  TEST_ASSERT_EQUAL(11, pin::kModemTxd);
  TEST_ASSERT_EQUAL(pin::kModemTxd, pin::kModemUartRx);
  TEST_ASSERT_EQUAL(pin::kModemRxd, pin::kModemUartTx);
  TEST_ASSERT_NOT_EQUAL(pin::kModemUartRx, pin::kModemUartTx);
}

void test_the_receiver_uart_is_named_from_the_processors_side() {
  TEST_ASSERT_EQUAL(2, pin::kGnssUartRx);
  TEST_ASSERT_EQUAL(16, pin::kGnssUartTx);
}

// The manufacturer writes P10 for the first leg of the second port; it is leg 8. The audio switch, P12, is
// leg 10 — an early specification said 12.
void test_expander_legs_are_numbered_in_sequence() {
  TEST_ASSERT_EQUAL(0, leg::kModemPower);
  TEST_ASSERT_EQUAL(7, leg::kTouchReset);
  TEST_ASSERT_EQUAL(8, leg::kModemPowerKey);
  TEST_ASSERT_EQUAL(9, leg::kKeyboardReset);
  TEST_ASSERT_EQUAL(10, leg::kAudioSelect);
  TEST_ASSERT_EQUAL(6, leg::kAmplifier);
}

void test_no_two_legs_share_a_number_and_all_exist() {
  const uint8_t legs[] = {leg::kModemPower, leg::kRadioPower, leg::kGnssPower, leg::kMotionRail,
                          leg::kRadioAntenna, leg::kHapticPower, leg::kAmplifier, leg::kTouchReset,
                          leg::kModemPowerKey, leg::kKeyboardReset, leg::kAudioSelect};
  std::set<uint8_t> seen;
  for (uint8_t l : legs) {
    TEST_ASSERT_TRUE(l < leg::kCount);
    TEST_ASSERT_TRUE(seen.insert(l).second);
  }
}

void test_the_eight_i2c_addresses_and_both_chargers() {
  TEST_ASSERT_EQUAL_HEX8(0x18, i2c::kAudioCodec);
  TEST_ASSERT_EQUAL_HEX8(0x1A, i2c::kTouch);
  TEST_ASSERT_EQUAL_HEX8(0x20, i2c::kExpander);
  TEST_ASSERT_EQUAL_HEX8(0x28, i2c::kMotion);
  TEST_ASSERT_EQUAL_HEX8(0x34, i2c::kKeyboard);
  TEST_ASSERT_EQUAL_HEX8(0x55, i2c::kFuelGauge);
  TEST_ASSERT_EQUAL_HEX8(0x5A, i2c::kHaptic);
  TEST_ASSERT_EQUAL_HEX8(0x6A, i2c::kChargerSy6970);
  TEST_ASSERT_EQUAL_HEX8(0x6B, i2c::kChargerBq25896);
}

// The boot state read back on the device, bit by bit, so a wrong constant names the leg it breaks.
void test_the_boot_state_means_what_the_notes_say() {
  const uint16_t state = (uint16_t)(xl9555::kBootPort0 | (xl9555::kBootPort1 << 8));
  auto high = [&](uint8_t l) { return (state & (1u << l)) != 0; };
  TEST_ASSERT_EQUAL_HEX8(0x9E, xl9555::kBootPort0);
  TEST_ASSERT_EQUAL_HEX8(0x02, xl9555::kBootPort1);
  TEST_ASSERT_FALSE(high(leg::kModemPower));
  TEST_ASSERT_TRUE(high(leg::kRadioPower));
  TEST_ASSERT_TRUE(high(leg::kGnssPower));
  TEST_ASSERT_TRUE(high(leg::kMotionRail));
  TEST_ASSERT_TRUE(high(leg::kRadioAntenna));
  TEST_ASSERT_FALSE(high(leg::kHapticPower));
  TEST_ASSERT_FALSE(high(leg::kAmplifier));
  TEST_ASSERT_TRUE_MESSAGE(high(leg::kTouchReset), "touch must start released (active low)");
  TEST_ASSERT_FALSE(high(leg::kModemPowerKey));
  TEST_ASSERT_TRUE_MESSAGE(high(leg::kKeyboardReset), "keyboard must start released (active low)");
  TEST_ASSERT_FALSE(high(leg::kAudioSelect));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_no_two_functions_share_a_gpio);
  RUN_TEST(test_the_pins_that_were_confirmed_on_the_device);
  RUN_TEST(test_the_modem_uart_is_crossed);
  RUN_TEST(test_the_receiver_uart_is_named_from_the_processors_side);
  RUN_TEST(test_expander_legs_are_numbered_in_sequence);
  RUN_TEST(test_no_two_legs_share_a_number_and_all_exist);
  RUN_TEST(test_the_eight_i2c_addresses_and_both_chargers);
  RUN_TEST(test_the_boot_state_means_what_the_notes_say);
  return UNITY_END();
}

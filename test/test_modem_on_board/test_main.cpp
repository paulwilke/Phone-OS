// The modem driver on the board's transport: the real driver, the real ModemTransport, the real expander class,
// and a scripted modem at the far end of the UART. What the driver's own tests prove with a fake transport is
// proven here to reach the right pins in the right order: the supply, the power key only when the modem is
// silent, the sound path during a call, DTR for sleep.

#include <unity.h>

#include <ModemScript.h>
#include <board/ModemTransport.h>
#include <board/PinMap.h>
#include <fakes/FakeDelay.h>
#include <fakes/FakeEventLog.h>
#include <fakes/FakeGpio.h>
#include <fakes/FakeI2cBus.h>

using namespace board;
using namespace modemtest;

void setUp() {}
void tearDown() {}

namespace {

// The scripted modem as a serial port and as the processor's counter.
class ModemOnUart : public UartPort, public MillisSource {
public:
  explicit ModemOnUart(FakeAtTransport& t) : t_(t) {}
  size_t write(const uint8_t* data, size_t len) override {
    t_.write(reinterpret_cast<const char*>(data), len);
    return len;
  }
  int read() override { return t_.read(); }
  uint32_t millis32() override { return t_.now(); }

private:
  FakeAtTransport& t_;
};

struct BoardRig {
  FakeAtTransport t;
  ModemOnUart wire{t};
  FakeDelay delay;
  FakeI2cBus bus{&delay};
  FakeGpio gpio{&delay};
  FakeEventLog log;
  Xl9555 expander{bus};
  ModemTransport transport{wire, expander, gpio, delay, wire, log};
  A7682EDriver::Config config;
  A7682EDriver driver;
  size_t pulsesForwarded = 0;

  static A7682EDriver::Config defaultConfig() {
    A7682EDriver::Config c;
    c.apn = kApn;
    c.simPin = kPin;
    return c;
  }
  BoardRig() : config(defaultConfig()), driver(transport, config) {
    bus.addChip(i2c::kExpander);
    expander.begin();
    transport.begin();
  }

  // The scripted modem has no power key of its own; the pulse the transport made on the expander's leg is
  // what wakes it. A pulse is a falling edge on the key's leg.
  void forwardPulses() {
    size_t edges = 0;
    bool last = false;
    for (const auto& tr : bus.transactions()) {
      if (tr.address != i2c::kExpander || !tr.isWrite || tr.data.size() != 2 || tr.data[0] != xl9555::kOutput1) continue;
      const bool high = (tr.data[1] & 0x01) != 0;
      if (last && !high) ++edges;
      last = high;
    }
    while (pulsesForwarded < edges) {
      t.pwrkeyPulse();
      ++pulsesForwarded;
    }
  }
  size_t pulses() {
    forwardPulses();
    return pulsesForwarded;
  }
  void run(uint32_t ms) {
    for (uint32_t spent = 0; spent < ms; spent += 50) {
      t.advance(50);
      driver.poll();
      forwardPulses();
    }
  }
  bool runTo(ModemState state, uint32_t maxMs = 400000) {
    for (uint32_t spent = 0; spent < maxMs && driver.state() != state; spent += 50) run(50);
    return driver.state() == state;
  }
  bool start() {
    driver.begin();
    return runTo(ModemState::Ready);
  }
};

}  // namespace

void test_a_running_modem_is_started_without_the_power_key() {
  BoardRig r;
  Script().apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  TEST_ASSERT_TRUE(r.expander.level(leg::kModemPower));
  TEST_ASSERT_EQUAL(0, r.pulses());
  TEST_ASSERT_FALSE(r.expander.level(leg::kModemPowerKey));
}

// A modem that is silent is woken with one pulse of 1.3 s, and then the driver goes on.
void test_a_silent_modem_gets_one_pulse_and_then_answers() {
  BoardRig r;
  Script().apply(r.t);
  r.t.setCold(true);
  TEST_ASSERT_TRUE(r.start());
  TEST_ASSERT_EQUAL(1, r.pulses());
  TEST_ASSERT_TRUE(r.delay.elapsedMs() >= kPowerKeyHighMs + kPowerKeyLowMs);
}

void test_the_whole_start_up_sends_no_unexpected_command() {
  BoardRig r;
  Script().apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  TEST_ASSERT_EQUAL(0, r.t.unexpected().size());
  TEST_ASSERT_TRUE(r.driver.simStatus() == SimStatus::Ready);
}

void test_the_sound_goes_to_the_modem_for_a_call_and_back_when_it_ends() {
  BoardRig r;
  Script().apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  TEST_ASSERT_FALSE(r.expander.level(leg::kAmplifier));
  TEST_ASSERT_FALSE(r.expander.level(leg::kAudioSelect));
  TEST_ASSERT_TRUE(r.driver.dial("+491701111111"));
  r.run(500);
  TEST_ASSERT_TRUE(r.driver.state() == ModemState::Dialing);
  TEST_ASSERT_TRUE(r.expander.level(leg::kAudioSelect));
  TEST_ASSERT_TRUE(r.expander.level(leg::kAmplifier));
  TEST_ASSERT_TRUE(r.driver.hangup());
  r.run(3000);
  TEST_ASSERT_FALSE(r.expander.level(leg::kAmplifier));
  TEST_ASSERT_FALSE(r.expander.level(leg::kAudioSelect));
}

void test_dtr_follows_the_sleep_and_the_wake_of_the_driver() {
  BoardRig r;
  Script().apply(r.t);
  TEST_ASSERT_TRUE(r.start());
  TEST_ASSERT_FALSE(r.gpio.level(pin::kModemDtr));
  r.t.reply("AT+CSCLK=1", {});
  r.driver.sleep();
  r.run(2000);
  TEST_ASSERT_TRUE(r.gpio.level(pin::kModemDtr));
  r.driver.wake();
  r.run(1000);
  TEST_ASSERT_FALSE(r.gpio.level(pin::kModemDtr));
}

void test_shutting_the_modem_down_cuts_the_supply_last() {
  BoardRig r;
  Script().apply(r.t);
  r.t.reply("AT+CPOF", {});
  TEST_ASSERT_TRUE(r.start());
  r.driver.shutdown();
  r.run(10000);
  TEST_ASSERT_TRUE(r.driver.state() == ModemState::Off);
  TEST_ASSERT_FALSE(r.expander.level(leg::kModemPower));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_a_running_modem_is_started_without_the_power_key);
  RUN_TEST(test_a_silent_modem_gets_one_pulse_and_then_answers);
  RUN_TEST(test_the_whole_start_up_sends_no_unexpected_command);
  RUN_TEST(test_the_sound_goes_to_the_modem_for_a_call_and_back_when_it_ends);
  RUN_TEST(test_dtr_follows_the_sleep_and_the_wake_of_the_driver);
  RUN_TEST(test_shutting_the_modem_down_cuts_the_supply_last);
  return UNITY_END();
}

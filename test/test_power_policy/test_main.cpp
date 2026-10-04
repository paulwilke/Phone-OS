// Host tests for the sleep rules. Only the decision is tested, not the power draw:
// whether the numbers add up shows only a 24-hour measurement on the device.
// The rules are numbered as in the header of PowerPolicy.h.

#include <unity.h>

#include <app/PowerPolicy.h>

void setUp() {}
void tearDown() {}

namespace {

PowerInputs idleDevice() {
  PowerInputs in;
  in.nowSeconds = 10000;
  in.lastInputSeconds = 10000 - PowerPolicy::kIdleSeconds;  // just quiet enough
  in.callActive = false;
  in.pendingWork = false;
  in.modemSleepAllowed = true;
  in.displayOn = false;
  in.nextRenderInMs = 1000;
  // The normal case: on battery and registered. Both must hold or nothing sleeps
  // (rules 6 and 7).
  in.externalPower = false;
  in.modemRegistered = true;
  return in;
}

}  // namespace

void test_a_quiet_device_lets_everything_sleep() {
  const PowerDecision d = PowerPolicy::decide(idleDevice());
  TEST_ASSERT_TRUE(d.modemShouldSleep);
  TEST_ASSERT_TRUE(d.cpuMaySleep);
  TEST_ASSERT_EQUAL_UINT32(1000, d.cpuSleepMs);
}

// Rule 3, and its boundary.
void test_the_modem_sleeps_after_exactly_the_idle_time() {
  PowerInputs in = idleDevice();
  in.lastInputSeconds = in.nowSeconds - (PowerPolicy::kIdleSeconds - 1);
  TEST_ASSERT_FALSE(PowerPolicy::decide(in).modemShouldSleep);
  in.lastInputSeconds = in.nowSeconds - PowerPolicy::kIdleSeconds;
  TEST_ASSERT_TRUE(PowerPolicy::decide(in).modemShouldSleep);
}

void test_a_recent_key_press_keeps_everything_awake() {
  PowerInputs in = idleDevice();
  in.lastInputSeconds = in.nowSeconds - 5;
  const PowerDecision d = PowerPolicy::decide(in);
  TEST_ASSERT_FALSE(d.modemShouldSleep);
  TEST_ASSERT_FALSE(d.cpuMaySleep);
}

// Rule 1.
void test_a_call_keeps_everything_awake_even_when_quiet() {
  PowerInputs in = idleDevice();
  in.lastInputSeconds = 0;  // no key for ages
  in.callActive = true;
  const PowerDecision d = PowerPolicy::decide(in);
  TEST_ASSERT_FALSE(d.modemShouldSleep);
  TEST_ASSERT_FALSE(d.cpuMaySleep);
}

void test_pending_work_keeps_the_modem_awake() {
  PowerInputs in = idleDevice();
  in.pendingWork = true;
  const PowerDecision d = PowerPolicy::decide(in);
  TEST_ASSERT_FALSE(d.modemShouldSleep);
  TEST_ASSERT_FALSE(d.cpuMaySleep);
}

// Rule 2. The processor must stay awake as well: it would miss messages of the awake modem.
void test_the_setting_can_forbid_modem_sleep() {
  PowerInputs in = idleDevice();
  in.modemSleepAllowed = false;
  const PowerDecision d = PowerPolicy::decide(in);
  TEST_ASSERT_FALSE(d.modemShouldSleep);
  TEST_ASSERT_FALSE(d.cpuMaySleep);
}

void test_the_display_on_blocks_cpu_sleep_but_not_modem_sleep() {
  PowerInputs in = idleDevice();
  in.displayOn = true;
  const PowerDecision d = PowerPolicy::decide(in);
  TEST_ASSERT_TRUE(d.modemShouldSleep);
  TEST_ASSERT_FALSE(d.cpuMaySleep);
}

void test_cpu_sleep_never_overshoots_the_next_render() {
  PowerInputs in = idleDevice();
  in.nextRenderInMs = 200;
  TEST_ASSERT_EQUAL_UINT32(200, PowerPolicy::decide(in).cpuSleepMs);

  in.nextRenderInMs = PowerPolicy::kMaxCpuSleepMs - 1;
  TEST_ASSERT_EQUAL_UINT32(PowerPolicy::kMaxCpuSleepMs - 1, PowerPolicy::decide(in).cpuSleepMs);
  in.nextRenderInMs = PowerPolicy::kMaxCpuSleepMs;
  TEST_ASSERT_EQUAL_UINT32(PowerPolicy::kMaxCpuSleepMs, PowerPolicy::decide(in).cpuSleepMs);

  in.nextRenderInMs = 60000;  // far away: wake up regularly all the same
  TEST_ASSERT_EQUAL_UINT32(PowerPolicy::kMaxCpuSleepMs, PowerPolicy::decide(in).cpuSleepMs);

  in.nextRenderInMs = 0;  // due already: no sleep of zero length
  TEST_ASSERT_EQUAL_UINT32(PowerPolicy::kMaxCpuSleepMs, PowerPolicy::decide(in).cpuSleepMs);
}

void test_a_clock_jump_back_does_not_confuse_it() {
  PowerInputs in = idleDevice();
  in.lastInputSeconds = in.nowSeconds + 500;  // an input "in the future"
  const PowerDecision d = PowerPolicy::decide(in);
  TEST_ASSERT_FALSE(d.modemShouldSleep);  // when in doubt, stay awake
  TEST_ASSERT_FALSE(d.cpuMaySleep);
}

// A modem without power cannot lose a message, so the processor may sleep even though
// the modem does not "sleep". Otherwise switching it off would gain nothing, because
// the processor would then stay awake all the time.
void test_a_switched_off_modem_lets_the_cpu_sleep() {
  PowerInputs in = idleDevice();
  in.cellularEnabled = false;
  in.modemSleepAllowed = false;  // even then
  const PowerDecision d = PowerPolicy::decide(in);
  TEST_ASSERT_FALSE(d.modemShouldSleep);  // nothing to put to sleep
  TEST_ASSERT_TRUE(d.cpuMaySleep);
}

// Rule 5. Cause: the boot state of the port expander powers the radio chip, and with no
// radio software running nobody read it: it received around the clock on every device
// whose configuration has no radio.
void test_the_radio_has_no_power_unless_it_is_in_use() {
  PowerInputs in = idleDevice();
  in.radioRunning = false;
  TEST_ASSERT_FALSE(PowerPolicy::decide(in).radioShouldBePowered);
  in.radioRunning = true;
  TEST_ASSERT_TRUE(PowerPolicy::decide(in).radioShouldBePowered);
}

// ... whether or not the device is quiet. decide() returns early during a call; had
// the radio rule come after that return, the radio would have lost power in the middle
// of a call. This test is why rule 5 is answered first: keep it.
void test_the_radio_survives_a_call_and_pending_work() {
  PowerInputs in = idleDevice();
  in.radioRunning = true;

  in.callActive = true;
  TEST_ASSERT_TRUE(PowerPolicy::decide(in).radioShouldBePowered);

  in.callActive = false;
  in.pendingWork = true;
  TEST_ASSERT_TRUE(PowerPolicy::decide(in).radioShouldBePowered);

  // Just used, so not quiet: the radio stays on then too.
  in.pendingWork = false;
  in.lastInputSeconds = in.nowSeconds;
  TEST_ASSERT_TRUE(PowerPolicy::decide(in).radioShouldBePowered);
}

// Rule 6. Saving brings nothing where power is plentiful, and light sleep takes the USB
// console with it: the ESP32-S3 switches its USB bridge off, and the log of a session on
// the cable broke off for that reason.
void test_nothing_sleeps_on_external_power() {
  PowerInputs in = idleDevice();
  in.externalPower = true;
  const PowerDecision d = PowerPolicy::decide(in);
  TEST_ASSERT_FALSE(d.modemShouldSleep);
  TEST_ASSERT_FALSE(d.cpuMaySleep);
  // The radio does not depend on the mains but on whether somebody needs it.
  in.radioRunning = true;
  TEST_ASSERT_TRUE(PowerPolicy::decide(in).radioShouldBePowered);
}

// Rule 7. Asleep, the modem no longer answers AT commands and registration never
// happens. On the device it went to sleep 60 s after start, during the operator search.
void test_a_modem_without_a_network_stays_awake() {
  PowerInputs in = idleDevice();
  in.modemRegistered = false;
  const PowerDecision d = PowerPolicy::decide(in);
  TEST_ASSERT_FALSE(d.modemShouldSleep);
  // And the processor stays awake too, or it would sleep through the modem's answer.
  TEST_ASSERT_FALSE(d.cpuMaySleep);
}

// Rule 8. Without a test in the previous code base. In light sleep the WLAN connection
// drops and the web server stops answering.
void test_wlan_keeps_the_cpu_awake_but_lets_the_modem_sleep() {
  PowerInputs in = idleDevice();
  in.wifiActive = true;
  const PowerDecision d = PowerPolicy::decide(in);
  TEST_ASSERT_FALSE(d.cpuMaySleep);
  TEST_ASSERT_EQUAL_UINT32(0, d.cpuSleepMs);
  TEST_ASSERT_TRUE(d.modemShouldSleep);

  in.wifiActive = false;
  TEST_ASSERT_TRUE(PowerPolicy::decide(in).cpuMaySleep);
}

void test_wlan_keeps_the_cpu_awake_with_the_modem_switched_off_too() {
  PowerInputs in = idleDevice();
  in.cellularEnabled = false;
  in.wifiActive = true;
  TEST_ASSERT_FALSE(PowerPolicy::decide(in).cpuMaySleep);
}

// The sleep time is zero when the processor may not sleep.
void test_no_sleep_time_without_permission() {
  PowerInputs in = idleDevice();
  in.displayOn = true;
  in.nextRenderInMs = 500;
  TEST_ASSERT_EQUAL_UINT32(0, PowerPolicy::decide(in).cpuSleepMs);
}

// The radio answer is a function of one input only. Every combination of the others is
// run, so that no rule, whatever its place in the code, can take the radio's power
// away or give it to it.
void test_the_radio_follows_only_its_own_input_in_every_combination() {
  for (unsigned bits = 0; bits < 512; ++bits) {
    PowerInputs in = idleDevice();
    in.callActive = bits & 1;
    in.pendingWork = bits & 2;
    in.modemSleepAllowed = bits & 4;
    in.cellularEnabled = bits & 8;
    in.displayOn = bits & 16;
    in.externalPower = bits & 32;
    in.modemRegistered = bits & 64;
    in.wifiActive = bits & 128;
    in.lastInputSeconds = (bits & 256) ? in.nowSeconds : 0;
    in.radioRunning = true;
    TEST_ASSERT_TRUE(PowerPolicy::decide(in).radioShouldBePowered);
    in.radioRunning = false;
    TEST_ASSERT_FALSE(PowerPolicy::decide(in).radioShouldBePowered);
  }
}

// What may sleep implies what the rules require, over every combination.
void test_sleep_implies_all_its_conditions_in_every_combination() {
  for (unsigned bits = 0; bits < 1024; ++bits) {
    PowerInputs in = idleDevice();
    in.callActive = bits & 1;
    in.pendingWork = bits & 2;
    in.modemSleepAllowed = bits & 4;
    in.cellularEnabled = bits & 8;
    in.displayOn = bits & 16;
    in.externalPower = bits & 32;
    in.modemRegistered = bits & 64;
    in.wifiActive = bits & 128;
    const bool quiet = bits & 256;
    in.lastInputSeconds = quiet ? 0 : in.nowSeconds;
    in.nextRenderInMs = (bits & 512) ? 0 : 700;
    const PowerDecision d = PowerPolicy::decide(in);

    if (d.modemShouldSleep) {
      TEST_ASSERT_TRUE(quiet && in.cellularEnabled && in.modemSleepAllowed && in.modemRegistered);
      TEST_ASSERT_FALSE(in.callActive || in.pendingWork || in.externalPower);
    }
    if (d.cpuMaySleep) {
      TEST_ASSERT_TRUE(quiet && !in.displayOn && !in.wifiActive);
      TEST_ASSERT_TRUE(d.modemShouldSleep || !in.cellularEnabled);
      TEST_ASSERT_FALSE(in.callActive || in.pendingWork || in.externalPower);
      TEST_ASSERT_TRUE(d.cpuSleepMs > 0 && d.cpuSleepMs <= PowerPolicy::kMaxCpuSleepMs);
    } else {
      TEST_ASSERT_EQUAL_UINT32(0, d.cpuSleepMs);
    }
    // And the other way round: when every condition holds, it does sleep.
    if (quiet && !in.displayOn && !in.wifiActive && !in.callActive && !in.pendingWork && !in.externalPower &&
        (in.cellularEnabled ? (in.modemSleepAllowed && in.modemRegistered) : true)) {
      TEST_ASSERT_TRUE(d.cpuMaySleep);
    }
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_a_quiet_device_lets_everything_sleep);
  RUN_TEST(test_the_modem_sleeps_after_exactly_the_idle_time);
  RUN_TEST(test_a_recent_key_press_keeps_everything_awake);
  RUN_TEST(test_a_call_keeps_everything_awake_even_when_quiet);
  RUN_TEST(test_pending_work_keeps_the_modem_awake);
  RUN_TEST(test_the_setting_can_forbid_modem_sleep);
  RUN_TEST(test_the_display_on_blocks_cpu_sleep_but_not_modem_sleep);
  RUN_TEST(test_cpu_sleep_never_overshoots_the_next_render);
  RUN_TEST(test_a_clock_jump_back_does_not_confuse_it);
  RUN_TEST(test_a_switched_off_modem_lets_the_cpu_sleep);
  RUN_TEST(test_the_radio_has_no_power_unless_it_is_in_use);
  RUN_TEST(test_the_radio_survives_a_call_and_pending_work);
  RUN_TEST(test_nothing_sleeps_on_external_power);
  RUN_TEST(test_a_modem_without_a_network_stays_awake);
  RUN_TEST(test_wlan_keeps_the_cpu_awake_but_lets_the_modem_sleep);
  RUN_TEST(test_wlan_keeps_the_cpu_awake_with_the_modem_switched_off_too);
  RUN_TEST(test_no_sleep_time_without_permission);
  RUN_TEST(test_the_radio_follows_only_its_own_input_in_every_combination);
  RUN_TEST(test_sleep_implies_all_its_conditions_in_every_combination);
  return UNITY_END();
}

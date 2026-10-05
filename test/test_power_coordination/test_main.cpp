// Host tests for the power coordination: the policy applied to the modem, the radio chip and the
// front light.

#include <unity.h>

#include <CoreFakes.h>
#include <app/PowerCoordination.h>

void setUp() {}
void tearDown() {}

namespace {

struct Rig : CoreFakes {
  PowerCoordination power_;
  PowerCoordination::Needs needs;

  explicit Rig(Profile p = ProfileBuilder().build())
      : CoreFakes(std::move(p), 100), power_(clock, power, modem, profile, notices, logger) {}

  // The processor sleep decision with the display off and nothing due for a long time.
  PowerDecision idle() { return power_.apply(needs, false, 10000); }
  size_t count(const std::string& part) const {
    size_t n = 0;
    for (const std::string& line : log.lines()) n += line.find(part) != std::string::npos ? 1 : 0;
    return n;
  }
};

}  // namespace

void test_the_modem_sleeps_after_a_quiet_minute() {
  Rig r;
  r.power_.noteInput();
  r.advance(PowerPolicy::kIdleSeconds - 1);
  r.idle();
  TEST_ASSERT_FALSE(r.modem.asleep());
  r.advance(1);
  r.idle();
  TEST_ASSERT_TRUE(r.modem.asleep());
  TEST_ASSERT_TRUE(r.power_.modemAsleep());
  r.idle();                                                         // and only once
  r.idle();
  TEST_ASSERT_EQUAL(1, r.count("Modem sleeps"));
}

void test_a_key_press_wakes_the_modem() {
  Rig r;
  r.idle();                                                         // 100 s without input: asleep
  TEST_ASSERT_TRUE(r.modem.asleep());
  r.power_.noteInput();
  r.idle();
  TEST_ASSERT_FALSE(r.modem.asleep());
  TEST_ASSERT_FALSE(r.power_.modemAsleep());
  r.idle();                                                         // awake already: nothing again
  TEST_ASSERT_EQUAL(1, r.count("Modem woken"));
}

void test_no_modem_sleep_keeps_everything_awake() {
  Rig r(ProfileBuilder().modemSleep(false).build());
  const PowerDecision d = r.idle();
  TEST_ASSERT_FALSE(r.modem.asleep());
  TEST_ASSERT_FALSE(d.modemShouldSleep);
  TEST_ASSERT_FALSE(d.cpuMaySleep);
}

void test_external_power_keeps_everything_awake() {
  Rig r;
  r.power.plugIn(ChargeState::Full);                                // full: no longer charging, still a cable
  const PowerDecision d = r.idle();
  TEST_ASSERT_FALSE(r.modem.asleep());
  TEST_ASSERT_FALSE(d.cpuMaySleep);
  r.power.unplug();                                                 // counter-check: on battery it sleeps
  TEST_ASSERT_TRUE(r.idle().cpuMaySleep);
}

void test_running_wifi_keeps_the_processor_awake() {
  Rig r;
  TEST_ASSERT_TRUE(r.idle().cpuMaySleep);
  r.needs.wifiActive = true;
  TEST_ASSERT_FALSE(r.idle().cpuMaySleep);
}

void test_a_switched_off_modem_is_left_alone() {
  Rig r;
  r.idle();
  TEST_ASSERT_TRUE(r.modem.asleep());
  r.power.setEnabled(PowerDomain::Cellular, false);
  r.power_.noteInput();                                             // would wake it if it were on
  r.idle();
  TEST_ASSERT_TRUE(r.modem.asleep());                               // not woken
  TEST_ASSERT_EQUAL(0, r.count("Modem woken"));
  // And it is not put to sleep either.
  Rig s;
  s.power.setEnabled(PowerDomain::Cellular, false);
  s.idle();
  TEST_ASSERT_FALSE(s.modem.asleep());
  TEST_ASSERT_EQUAL(0, s.count("Modem sleeps"));
}

void test_the_radio_chip_follows_the_need() {
  Rig r;
  r.idle();
  TEST_ASSERT_FALSE(r.power.isEnabled(PowerDomain::Radio));
  r.needs.radioUsable = true;
  r.idle();
  TEST_ASSERT_TRUE(r.power.isEnabled(PowerDomain::Radio));
  TEST_ASSERT_EQUAL(1, r.count("Radio: power on"));
  r.idle();                                                         // logged at the change only
  TEST_ASSERT_EQUAL(1, r.count("Radio: power on"));
  r.needs.radioUsable = false;
  r.idle();
  TEST_ASSERT_FALSE(r.power.isEnabled(PowerDomain::Radio));
  TEST_ASSERT_EQUAL(1, r.count("Radio: power off"));
}

void test_a_radio_chip_that_will_not_switch_is_tried_again() {
  Rig r;
  r.power.failDomain(PowerDomain::Radio, true);
  r.needs.radioUsable = true;
  r.idle();
  TEST_ASSERT_EQUAL(0, r.count("Radio: power on"));                 // not claimed
  r.power.failDomain(PowerDomain::Radio, false);
  r.idle();
  TEST_ASSERT_TRUE(r.power.isEnabled(PowerDomain::Radio));
}

void test_pending_work() {
  Rig r;
  r.idle();
  TEST_ASSERT_TRUE(r.modem.asleep());
  r.needs.pendingWork = true;                                       // a queued message wakes the modem ...
  const PowerDecision d = r.idle();
  TEST_ASSERT_FALSE(r.modem.asleep());
  TEST_ASSERT_FALSE(d.cpuMaySleep);
  r.needs.pendingWork = false;                                      // ... and when it is gone, it may sleep again
  r.idle();
  TEST_ASSERT_TRUE(r.modem.asleep());
}

void test_a_call_keeps_the_modem_awake() {
  Rig r;
  r.needs.callActive = true;
  r.idle();
  TEST_ASSERT_FALSE(r.modem.asleep());
}

void test_a_modem_without_a_network_does_not_sleep() {
  Rig r;
  r.networkLost();
  r.idle();
  TEST_ASSERT_FALSE(r.modem.asleep());                              // asleep it would never register
  r.networkBack();
  r.idle();
  TEST_ASSERT_TRUE(r.modem.asleep());
}

void test_the_processor_may_sleep_when_everything_is_idle() {
  Rig r;
  const PowerDecision d = r.power_.apply(r.needs, false, 500);
  TEST_ASSERT_TRUE(d.cpuMaySleep);
  TEST_ASSERT_EQUAL_UINT32(500, d.cpuSleepMs);                      // until the next redraw
  TEST_ASSERT_EQUAL_UINT32(PowerPolicy::kMaxCpuSleepMs, r.power_.apply(r.needs, false, 10000).cpuSleepMs);
  TEST_ASSERT_FALSE(r.power_.apply(r.needs, true, 500).cpuMaySleep);  // a lit display stays awake
}

void test_the_front_light_toggles() {
  Rig r;
  TEST_ASSERT_FALSE(r.power_.frontlightOn());
  TEST_ASSERT_TRUE(r.power_.toggleFrontlight());
  TEST_ASSERT_TRUE(r.power_.frontlightOn());
  TEST_ASSERT_TRUE(r.notices.current().kind == NoticeKind::LightOn);
  TEST_ASSERT_TRUE(r.power_.toggleFrontlight());
  TEST_ASSERT_FALSE(r.power_.frontlightOn());
  TEST_ASSERT_TRUE(r.notices.current().kind == NoticeKind::LightOff);
  r.power_.setFrontlight(true);                                     // locking switches it off without a notice
  r.notices.clear();
  r.power_.setFrontlight(false);
  TEST_ASSERT_FALSE(r.power_.frontlightOn());
  TEST_ASSERT_TRUE(r.notices.current().none());
}

void test_a_light_that_will_not_switch_says_nothing() {
  Rig r;
  r.power.failDomain(PowerDomain::Frontlight, true);
  TEST_ASSERT_FALSE(r.power_.toggleFrontlight());
  TEST_ASSERT_TRUE(r.notices.current().none());
}

void test_a_refused_power_off_is_noticed() {
  Rig r;
  r.power_.requestPowerOff();
  TEST_ASSERT_TRUE(r.power_.powerOffRequested());
  r.power_.notePowerOffFailed();
  TEST_ASSERT_FALSE(r.power_.powerOffRequested());
  TEST_ASSERT_TRUE(r.notices.current().kind == NoticeKind::PowerOffRefused);
  TEST_ASSERT_EQUAL(1, r.count("Shutdown refused"));
}

void test_the_battery_is_read_through() {
  Rig r;
  r.power.setBattery(37);
  r.power.setMinutesRemaining(123);
  TEST_ASSERT_EQUAL(37, r.power_.batteryPercent());
  TEST_ASSERT_EQUAL(123, r.power_.batteryMinutesRemaining());
  r.power.plugIn(ChargeState::Charging);
  TEST_ASSERT_TRUE(ChargeState::Charging == r.power_.chargeState());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_the_modem_sleeps_after_a_quiet_minute);
  RUN_TEST(test_a_key_press_wakes_the_modem);
  RUN_TEST(test_no_modem_sleep_keeps_everything_awake);
  RUN_TEST(test_external_power_keeps_everything_awake);
  RUN_TEST(test_running_wifi_keeps_the_processor_awake);
  RUN_TEST(test_a_switched_off_modem_is_left_alone);
  RUN_TEST(test_the_radio_chip_follows_the_need);
  RUN_TEST(test_a_radio_chip_that_will_not_switch_is_tried_again);
  RUN_TEST(test_pending_work);
  RUN_TEST(test_a_call_keeps_the_modem_awake);
  RUN_TEST(test_a_modem_without_a_network_does_not_sleep);
  RUN_TEST(test_the_processor_may_sleep_when_everything_is_idle);
  RUN_TEST(test_the_front_light_toggles);
  RUN_TEST(test_a_light_that_will_not_switch_says_nothing);
  RUN_TEST(test_a_refused_power_off_is_noticed);
  RUN_TEST(test_the_battery_is_read_through);
  return UNITY_END();
}

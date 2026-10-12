// The low-voltage guard: a minute below the limit on the battery, nothing else.

#include <unity.h>

#include <board/LowVoltageGuard.h>

using namespace board;

void setUp() {}
void tearDown() {}

void test_a_minute_below_the_limit_on_the_battery_switches_off() {
  LowVoltageGuard g;
  TEST_ASSERT_FALSE(g.update(0, 3290, true));
  TEST_ASSERT_FALSE(g.update(30000, 3280, true));
  TEST_ASSERT_FALSE(g.update(59999, 3270, true));
  TEST_ASSERT_TRUE(g.update(60000, 3270, true));
}

void test_a_dip_while_the_modem_sends_does_not() {
  LowVoltageGuard g;
  TEST_ASSERT_FALSE(g.update(0, 3250, true));      // a dip ...
  TEST_ASSERT_FALSE(g.update(5000, 3600, true));   // ... and the voltage is back: the count starts again
  TEST_ASSERT_FALSE(g.update(70000, 3250, true));  // a new dip, a long time later
  TEST_ASSERT_FALSE(g.update(100000, 3250, true));
  TEST_ASSERT_TRUE(g.update(130000, 3250, true));
}

void test_the_limit_itself_is_not_below() {
  LowVoltageGuard g;
  TEST_ASSERT_FALSE(g.update(0, kLowVoltageMv, true));
  TEST_ASSERT_FALSE(g.update(120000, kLowVoltageMv, true));
  TEST_ASSERT_FALSE(g.update(121000, kLowVoltageMv - 1, true));
}

void test_a_cable_or_an_unknown_reading_starts_the_count_again() {
  LowVoltageGuard g;
  g.update(0, 3200, true);
  TEST_ASSERT_FALSE(g.update(50000, 3200, false));  // a cable: the voltage is not the cell's
  TEST_ASSERT_FALSE(g.update(60000, 3200, true));   // counted from here
  TEST_ASSERT_FALSE(g.update(70000, 0, true));      // unknown
  TEST_ASSERT_FALSE(g.update(80000, 3200, true));
  TEST_ASSERT_FALSE(g.update(139999, 3200, true));
  TEST_ASSERT_TRUE(g.update(140000, 3200, true));
}

void test_the_millisecond_counter_wrapping_is_no_problem() {
  LowVoltageGuard g;
  const uint32_t start = 0xFFFFFFFFu - 10000u;
  TEST_ASSERT_FALSE(g.update(start, 3200, true));
  TEST_ASSERT_FALSE(g.update(start + 59999u, 3200, true));
  TEST_ASSERT_TRUE(g.update(start + 60000u, 3200, true));
}

void test_the_limit_is_the_empty_mark_of_the_bar() {
  TEST_ASSERT_EQUAL(3300, kLowVoltageMv);
  TEST_ASSERT_EQUAL(3300, kBatteryEmptyMv);
}

void test_the_picture_says_why_it_switches_off() {
  TEST_ASSERT_EQUAL_STRING("power.battery-empty", switchOffWordKey(true));
  TEST_ASSERT_EQUAL_STRING("power.switched-off", switchOffWordKey(false));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_a_minute_below_the_limit_on_the_battery_switches_off);
  RUN_TEST(test_a_dip_while_the_modem_sends_does_not);
  RUN_TEST(test_the_limit_itself_is_not_below);
  RUN_TEST(test_a_cable_or_an_unknown_reading_starts_the_count_again);
  RUN_TEST(test_the_millisecond_counter_wrapping_is_no_problem);
  RUN_TEST(test_the_limit_is_the_empty_mark_of_the_bar);
  RUN_TEST(test_the_picture_says_why_it_switches_off);
  return UNITY_END();
}

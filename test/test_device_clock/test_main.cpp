// The device's clock on a scripted millisecond counter: the 64-bit uptime out of a counter that wraps after
// 49 days, and the epoch that is unknown until somebody knows it. The Clock contract runs against it in
// test_hardware_contracts.

#include <unity.h>

#include <board/DeviceClock.h>
#include <fakes/FakeMillis.h>

using namespace board;

void setUp() {}
void tearDown() {}

void test_the_uptime_follows_the_counter() {
  FakeMillis ms;
  DeviceClock clock(ms);
  ms.advance(1500);
  TEST_ASSERT_EQUAL_UINT64(1500, clock.uptimeMillis());
  TEST_ASSERT_EQUAL_UINT32(1, clock.uptimeSeconds());
}

// The processor's counter wraps after 49.7 days; the uptime must not.
void test_the_uptime_goes_on_across_the_wrap_of_the_counter() {
  FakeMillis ms;
  ms.setTo(0xFFFFFF00u);
  DeviceClock clock(ms);
  ms.advance(0x100);  // the counter is 0 now
  TEST_ASSERT_EQUAL_UINT64(0x100, clock.uptimeMillis());
  ms.advance(1000);
  TEST_ASSERT_EQUAL_UINT64(0x100 + 1000, clock.uptimeMillis());
}

void test_two_wraps_make_98_days() {
  FakeMillis ms;
  DeviceClock clock(ms);
  uint64_t expected = 0;
  for (int i = 0; i < 8; ++i) {  // reading often, as the main loop does
    ms.advance(0x40000000u);
    expected += 0x40000000u;
    TEST_ASSERT_EQUAL_UINT64(expected, clock.uptimeMillis());
  }
  TEST_ASSERT_TRUE(clock.uptimeMillis() > (uint64_t)2 * 0xFFFFFFFFull);
}

// A counter that reads lower than before holds the uptime; it never goes back.
void test_a_counter_that_goes_back_holds_the_uptime() {
  FakeMillis ms;
  DeviceClock clock(ms);
  ms.advance(5000);
  TEST_ASSERT_EQUAL_UINT64(5000, clock.uptimeMillis());
  ms.setTo(4000);
  TEST_ASSERT_EQUAL_UINT64(5000, clock.uptimeMillis());
  ms.setTo(6000);  // and counts on from the highest reading
  TEST_ASSERT_EQUAL_UINT64(6000, clock.uptimeMillis());
}

void test_the_epoch_is_unknown_until_it_is_set_and_advances_with_the_uptime() {
  FakeMillis ms;
  DeviceClock clock(ms);
  TEST_ASSERT_EQUAL_UINT32(0, clock.epochUtc());
  ms.advance(7000);
  TEST_ASSERT_TRUE(clock.setEpochUtc(1790000000u));
  TEST_ASSERT_EQUAL_UINT32(1790000000u, clock.epochUtc());
  ms.advance(10999);
  TEST_ASSERT_EQUAL_UINT32(1790000010u, clock.epochUtc());
}

void test_the_epoch_survives_the_wrap_of_the_counter() {
  FakeMillis ms;
  ms.setTo(0xFFFFF000u);
  DeviceClock clock(ms);
  clock.setEpochUtc(1790000000u);
  ms.advance(0x2000);
  TEST_ASSERT_EQUAL_UINT32(1790000000u + 8, clock.epochUtc());
}

void test_a_date_before_2020_is_refused_and_nothing_changes() {
  FakeMillis ms;
  DeviceClock clock(ms);
  clock.setEpochUtc(1790000000u);
  TEST_ASSERT_FALSE(clock.setEpochUtc(946684800u));
  TEST_ASSERT_FALSE(clock.setEpochUtc(1));
  TEST_ASSERT_EQUAL_UINT32(1790000000u, clock.epochUtc());
}

void test_zero_makes_the_time_unknown_again() {
  FakeMillis ms;
  DeviceClock clock(ms);
  clock.setEpochUtc(1790000000u);
  TEST_ASSERT_TRUE(clock.setEpochUtc(0));
  ms.advance(5000);
  TEST_ASSERT_EQUAL_UINT32(0, clock.epochUtc());
}

void test_setting_the_epoch_does_not_move_the_uptime() {
  FakeMillis ms;
  DeviceClock clock(ms);
  ms.advance(4000);
  const uint64_t before = clock.uptimeMillis();
  clock.setEpochUtc(1790000000u);
  TEST_ASSERT_EQUAL_UINT64(before, clock.uptimeMillis());
}

void test_the_zone_offset_has_its_limits_and_is_independent_of_the_epoch() {
  FakeMillis ms;
  DeviceClock clock(ms);
  TEST_ASSERT_EQUAL(0, clock.tzOffsetMinutes());
  TEST_ASSERT_TRUE(clock.setTzOffsetMinutes(-720));
  TEST_ASSERT_TRUE(clock.setTzOffsetMinutes(840));
  TEST_ASSERT_FALSE(clock.setTzOffsetMinutes(841));
  TEST_ASSERT_FALSE(clock.setTzOffsetMinutes(-721));
  TEST_ASSERT_EQUAL(840, clock.tzOffsetMinutes());
  clock.setEpochUtc(1790000000u);
  clock.setEpochUtc(0);
  TEST_ASSERT_EQUAL(840, clock.tzOffsetMinutes());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_the_uptime_follows_the_counter);
  RUN_TEST(test_the_uptime_goes_on_across_the_wrap_of_the_counter);
  RUN_TEST(test_two_wraps_make_98_days);
  RUN_TEST(test_a_counter_that_goes_back_holds_the_uptime);
  RUN_TEST(test_the_epoch_is_unknown_until_it_is_set_and_advances_with_the_uptime);
  RUN_TEST(test_the_epoch_survives_the_wrap_of_the_counter);
  RUN_TEST(test_a_date_before_2020_is_refused_and_nothing_changes);
  RUN_TEST(test_zero_makes_the_time_unknown_again);
  RUN_TEST(test_setting_the_epoch_does_not_move_the_uptime);
  RUN_TEST(test_the_zone_offset_has_its_limits_and_is_independent_of_the_epoch);
  return UNITY_END();
}

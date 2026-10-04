#pragma once

// The contract of Clock. Rig: `Clock& clock()`, `void advance(uint32_t ms)` (uptime
// moves by at least `ms`; a real clock may sleep).

#include <hal/Clock.h>

#include "Contract.h"

template <class Rig>
struct ClockContract {
  // Uptime never goes back, however often it is read.
  static void uptime_never_goes_back() {
    Rig rig;
    uint64_t last = rig.clock().uptimeMillis();
    for (int i = 0; i < 50; ++i) {
      if (i % 7 == 0) rig.advance(13);
      const uint64_t now = rig.clock().uptimeMillis();
      TEST_ASSERT_TRUE(now >= last);
      last = now;
    }
  }

  static void uptime_advances_by_at_least_the_time_advanced() {
    Rig rig;
    const uint64_t before = rig.clock().uptimeMillis();
    rig.advance(1500);
    TEST_ASSERT_TRUE(rig.clock().uptimeMillis() >= before + 1500);
  }

  static void uptime_seconds_is_uptime_millis_divided_by_1000() {
    Rig rig;
    rig.advance(2500);
    const uint32_t seconds = rig.clock().uptimeSeconds();
    const uint64_t ms = rig.clock().uptimeMillis();
    TEST_ASSERT_TRUE((uint64_t)seconds * 1000 <= ms);
    TEST_ASSERT_TRUE(seconds >= 2);
  }

  // The epoch is either not known (0) or a plausible date: never 1970.
  static void the_epoch_is_unknown_or_plausible() {
    Rig rig;
    const uint32_t epoch = rig.clock().epochUtc();
    TEST_ASSERT_TRUE(epoch == 0 || epoch >= Clock::kEarliestEpoch);
  }

  static void a_date_before_2020_is_refused_and_changes_nothing() {
    Rig rig;
    TEST_ASSERT_TRUE(rig.clock().setEpochUtc(1790000000u));
    TEST_ASSERT_FALSE(rig.clock().setEpochUtc(946684800u));  // 2000-01-01
    TEST_ASSERT_FALSE(rig.clock().setEpochUtc(1));
    TEST_ASSERT_FALSE(rig.clock().setEpochUtc(Clock::kEarliestEpoch - 1));
    const uint32_t epoch = rig.clock().epochUtc();
    TEST_ASSERT_TRUE(epoch >= 1790000000u && epoch < 1790000000u + 5);
    TEST_ASSERT_TRUE(rig.clock().setEpochUtc(Clock::kEarliestEpoch));
  }

  static void a_known_epoch_advances_with_the_uptime() {
    Rig rig;
    TEST_ASSERT_TRUE(rig.clock().setEpochUtc(1790000000u));
    TEST_ASSERT_TRUE(rig.clock().epochUtc() >= 1790000000u);
    rig.advance(10000);
    const uint32_t epoch = rig.clock().epochUtc();
    TEST_ASSERT_TRUE(epoch >= 1790000010u);
    TEST_ASSERT_TRUE(epoch < 1790000010u + 5);
  }

  static void setting_zero_makes_the_time_unknown_again_and_it_stays_unknown() {
    Rig rig;
    rig.clock().setEpochUtc(1790000000u);
    TEST_ASSERT_TRUE(rig.clock().setEpochUtc(0));
    TEST_ASSERT_EQUAL_UINT32(0, rig.clock().epochUtc());
    rig.advance(5000);
    TEST_ASSERT_EQUAL_UINT32(0, rig.clock().epochUtc());  // nothing is guessed
  }

  static void setting_the_epoch_again_replaces_the_old_one() {
    Rig rig;
    rig.clock().setEpochUtc(1790000000u);
    rig.advance(3000);
    rig.clock().setEpochUtc(1800000000u);
    const uint32_t epoch = rig.clock().epochUtc();
    TEST_ASSERT_TRUE(epoch >= 1800000000u && epoch < 1800000000u + 5);
  }

  // Setting the wall-clock time must not move the uptime: durations measured across
  // the moment the network time arrives would otherwise jump.
  static void setting_the_epoch_does_not_move_the_uptime() {
    Rig rig;
    rig.advance(4000);
    const uint64_t before = rig.clock().uptimeMillis();
    rig.clock().setEpochUtc(1790000000u);
    const uint64_t after = rig.clock().uptimeMillis();
    TEST_ASSERT_TRUE(after >= before);
    TEST_ASSERT_TRUE(after - before < 1000);
  }

  static void the_offset_is_always_valid_and_the_limits_are_accepted() {
    Rig rig;
    const int initial = rig.clock().tzOffsetMinutes();
    TEST_ASSERT_TRUE(initial >= Clock::kMinOffsetMinutes && initial <= Clock::kMaxOffsetMinutes);
    TEST_ASSERT_TRUE(rig.clock().setTzOffsetMinutes(Clock::kMinOffsetMinutes));
    TEST_ASSERT_EQUAL(Clock::kMinOffsetMinutes, rig.clock().tzOffsetMinutes());
    TEST_ASSERT_TRUE(rig.clock().setTzOffsetMinutes(Clock::kMaxOffsetMinutes));
    TEST_ASSERT_EQUAL(Clock::kMaxOffsetMinutes, rig.clock().tzOffsetMinutes());
    TEST_ASSERT_TRUE(rig.clock().setTzOffsetMinutes(60));
    TEST_ASSERT_EQUAL(60, rig.clock().tzOffsetMinutes());
  }

  static void an_impossible_offset_is_refused_and_changes_nothing() {
    Rig rig;
    rig.clock().setTzOffsetMinutes(120);
    TEST_ASSERT_FALSE(rig.clock().setTzOffsetMinutes(Clock::kMaxOffsetMinutes + 1));
    TEST_ASSERT_FALSE(rig.clock().setTzOffsetMinutes(Clock::kMinOffsetMinutes - 1));
    TEST_ASSERT_EQUAL(120, rig.clock().tzOffsetMinutes());
  }

  static void the_offset_is_independent_of_the_epoch() {
    Rig rig;
    rig.clock().setTzOffsetMinutes(-300);
    rig.clock().setEpochUtc(1790000000u);
    TEST_ASSERT_EQUAL(-300, rig.clock().tzOffsetMinutes());
    rig.clock().setEpochUtc(0);
    TEST_ASSERT_EQUAL(-300, rig.clock().tzOffsetMinutes());
  }
};

#define RUN_CLOCK_CONTRACT(Rig)                                                     \
  RUN_TEST(ClockContract<Rig>::uptime_never_goes_back);                             \
  RUN_TEST(ClockContract<Rig>::uptime_advances_by_at_least_the_time_advanced);      \
  RUN_TEST(ClockContract<Rig>::uptime_seconds_is_uptime_millis_divided_by_1000);    \
  RUN_TEST(ClockContract<Rig>::the_epoch_is_unknown_or_plausible);                  \
  RUN_TEST(ClockContract<Rig>::a_date_before_2020_is_refused_and_changes_nothing);  \
  RUN_TEST(ClockContract<Rig>::a_known_epoch_advances_with_the_uptime);             \
  RUN_TEST(ClockContract<Rig>::setting_zero_makes_the_time_unknown_again_and_it_stays_unknown); \
  RUN_TEST(ClockContract<Rig>::setting_the_epoch_again_replaces_the_old_one);       \
  RUN_TEST(ClockContract<Rig>::setting_the_epoch_does_not_move_the_uptime);         \
  RUN_TEST(ClockContract<Rig>::the_offset_is_always_valid_and_the_limits_are_accepted); \
  RUN_TEST(ClockContract<Rig>::an_impossible_offset_is_refused_and_changes_nothing); \
  RUN_TEST(ClockContract<Rig>::the_offset_is_independent_of_the_epoch)

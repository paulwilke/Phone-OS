// Host tests for school time on the clock, and its one exception: an emergency lifts the lock
// until the end of the school time that is running.

#include <unity.h>

#include <CoreFakes.h>
#include <app/SchoolTime.h>

void setUp() {}
void tearDown() {}

namespace {

// The window of most tests: Tuesday 14:00-15:00; the clock says 14:32 local.
Profile tuesday() { return ProfileBuilder().schoolTime("tue", "14:00", "15:00").build(); }

}  // namespace

void test_school_time_follows_the_profile_and_the_clock() {
  CoreFakes f(tuesday());
  int end = -1;
  TEST_ASSERT_TRUE(f.school.active(&end));
  TEST_ASSERT_EQUAL(15 * 60, end);
  f.advance(28 * 60 - 1);                                // 14:59:59
  TEST_ASSERT_TRUE(f.school.active());
  f.advance(1);                                          // 15:00:00: the end minute itself is free
  TEST_ASSERT_FALSE(f.school.active());
}

// The window is on the card but switched off: it is not school time for anybody, and there is
// nothing for an emergency to lift.
void test_a_switched_off_window_is_no_school_time_and_nothing_to_lift() {
  CoreFakes f(ProfileBuilder().schoolTime("tue", "14:00", "15:00", false).build());
  TEST_ASSERT_FALSE(f.school.active());
  TEST_ASSERT_FALSE(f.school.lift());
  TEST_ASSERT_EQUAL(0, f.school.liftedUntil());
}

void test_no_school_time_without_a_known_time() {
  CoreFakes f(tuesday());
  f.clock.setEpochUtc(0);
  TEST_ASSERT_FALSE(f.school.active());
  TEST_ASSERT_FALSE(f.school.lift());                    // nothing to lift
  TEST_ASSERT_EQUAL(0, f.school.liftedUntil());
}

void test_lifting_ends_exactly_at_the_end_of_the_window() {
  CoreFakes f(tuesday());
  TEST_ASSERT_TRUE(f.school.lift());
  TEST_ASSERT_FALSE(f.school.active());
  TEST_ASSERT_EQUAL(coretest::kTuesdayAfternoon + 28 * 60, f.school.liftedUntil());   // 14:32:00 + 28 min = 15:00:00
  f.advance(28 * 60 - 1);
  TEST_ASSERT_FALSE(f.school.active());                  // still lifted one second before the end
  f.advance(1);
  TEST_ASSERT_FALSE(f.school.active());                  // and the window is over anyway
}

void test_the_end_is_counted_from_the_second_not_the_minute() {
  CoreFakes f(tuesday());
  f.advance(45);                                         // 14:32:45
  TEST_ASSERT_TRUE(f.school.lift());
  TEST_ASSERT_EQUAL(coretest::kTuesdayAfternoon + 45 + (28 * 60 - 45), f.school.liftedUntil());   // exactly 15:00:00
}

void test_a_lift_does_not_reach_the_next_window() {
  CoreFakes f(ProfileBuilder().schoolTime("tue", "14:00", "15:00").schoolTime("tue", "15:30", "16:00").build());
  f.school.lift();
  f.advance(30 * 60);                                    // 15:02, between the windows
  TEST_ASSERT_FALSE(f.school.active());
  f.advance(30 * 60);                                    // 15:32: the second window begins
  TEST_ASSERT_TRUE(f.school.active());                   // locked again
}

void test_a_lift_does_not_reach_a_window_that_begins_where_it_ends() {
  CoreFakes f(ProfileBuilder().schoolTime("tue", "14:00", "15:00").schoolTime("tue", "15:00", "16:00").build());
  f.school.lift();
  f.advance(28 * 60 - 1);
  TEST_ASSERT_FALSE(f.school.active());                  // 14:59:59 is lifted
  f.advance(1);
  TEST_ASSERT_TRUE(f.school.active());                   // 15:00:00 belongs to the next window
}

void test_a_window_across_midnight_is_lifted_until_its_end() {
  CoreFakes f(ProfileBuilder().schoolTime("tue", "14:00", "01:00").build());
  f.advance(60 * 60);                                    // 15:32
  TEST_ASSERT_TRUE(f.school.lift());
  const uint32_t now = f.clock.epochUtc();
  TEST_ASSERT_EQUAL(now + (9 * 60 + 28) * 60, f.school.liftedUntil());   // 15:32 -> 01:00
}

void test_lifting_when_no_school_time_runs_changes_nothing() {
  CoreFakes f(tuesday());
  f.advance(3600);                                       // 15:32
  TEST_ASSERT_FALSE(f.school.lift());
  TEST_ASSERT_EQUAL(0, f.school.liftedUntil());
  TEST_ASSERT_FALSE(f.log.contains("Quiet hours"));
}

void test_lifting_is_logged_and_never_dropped_from_the_log() {
  CoreFakes f(ProfileBuilder().schoolTime("tue", "14:00", "15:00").logLevel(loglevel::Level::Errors).build());
  f.school.lift();
  TEST_ASSERT_TRUE(f.log.contains("Quiet hours lifted by emergency until 15:00"));
}

void test_the_profile_may_change_while_the_device_runs() {
  CoreFakes f(tuesday());
  TEST_ASSERT_TRUE(f.school.active());
  f.profile.quietHours = QuietHours();                   // the parents removed the windows
  TEST_ASSERT_FALSE(f.school.active());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_school_time_follows_the_profile_and_the_clock);
  RUN_TEST(test_a_switched_off_window_is_no_school_time_and_nothing_to_lift);
  RUN_TEST(test_no_school_time_without_a_known_time);
  RUN_TEST(test_lifting_ends_exactly_at_the_end_of_the_window);
  RUN_TEST(test_the_end_is_counted_from_the_second_not_the_minute);
  RUN_TEST(test_a_lift_does_not_reach_the_next_window);
  RUN_TEST(test_a_lift_does_not_reach_a_window_that_begins_where_it_ends);
  RUN_TEST(test_a_window_across_midnight_is_lifted_until_its_end);
  RUN_TEST(test_lifting_when_no_school_time_runs_changes_nothing);
  RUN_TEST(test_lifting_is_logged_and_never_dropped_from_the_log);
  RUN_TEST(test_the_profile_may_change_while_the_device_runs);
  return UNITY_END();
}

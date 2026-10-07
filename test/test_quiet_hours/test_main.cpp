// Host tests for the quiet hours (school time).
//
// Day numbers in the helper count from Monday 2026-10-05, a date checked against
// an independent calendar, so 0 = Monday … 6 = Sunday.

#include <unity.h>

#include <app/QuietHours.h>
#include <app/TimeZone.h>

using Result = QuietHours::AddResult;

void setUp() {}
void tearDown() {}

namespace {

// Unix time of 2026-10-05 00:00 UTC, a Monday.
constexpr uint32_t kMonday = 1791158400u;

uint32_t at(int day, int hour, int minute) {
  return kMonday + (uint32_t)day * 86400u + (uint32_t)hour * 3600u + (uint32_t)minute * 60u;
}

QuietHours school() {
  QuietHours q;
  q.add("mon-fri", "08:00", "13:30");
  return q;
}

}  // namespace

// ---------------------------------------------------------------------------
// Is a quiet period running?
// ---------------------------------------------------------------------------

// Checks the assumption of the other tests themselves.
void test_the_anchor_date_is_a_monday() {
  QuietHours q;
  q.add("mon", "00:00", "23:59");
  TEST_ASSERT_TRUE(q.active(at(0, 12, 0), 0));
  TEST_ASSERT_FALSE(q.active(at(1, 12, 0), 0));
}

void test_active_inside_the_window_only() {
  QuietHours q = school();
  int end = 0;
  TEST_ASSERT_FALSE(q.active(at(0, 7, 59), 0));
  TEST_ASSERT_TRUE(q.active(at(0, 8, 0), 0, &end));
  TEST_ASSERT_EQUAL(13 * 60 + 30, end);
  TEST_ASSERT_TRUE(q.active(at(0, 13, 29), 0));
  TEST_ASSERT_FALSE(q.active(at(0, 13, 30), 0));  // the end minute is free again
}

void test_the_end_minute_is_left_alone_when_nothing_is_active() {
  QuietHours q = school();
  int end = 4711;
  TEST_ASSERT_FALSE(q.active(at(0, 20, 0), 0, &end));
  TEST_ASSERT_EQUAL(4711, end);
}

void test_the_weekend_is_free() {
  QuietHours q = school();
  TEST_ASSERT_TRUE(q.active(at(4, 10, 0), 0));   // Friday
  TEST_ASSERT_FALSE(q.active(at(5, 10, 0), 0));  // Saturday
  TEST_ASSERT_FALSE(q.active(at(6, 10, 0), 0));  // Sunday
}

// The device computes in local time: 08:00 in Berlin in October is 06:00 UTC.
void test_the_time_zone_offset_is_applied() {
  QuietHours q = school();
  TEST_ASSERT_FALSE(q.active(at(0, 5, 59), 120));
  TEST_ASSERT_TRUE(q.active(at(0, 6, 0), 120));
  // 23:30 UTC on Sunday is already Monday 08:30 in +09:00.
  TEST_ASSERT_TRUE(q.active(at(6, 23, 30), 9 * 60));
}

// The offset has to come from the zone rules: on Monday 2026-10-26, the day after
// the clocks went back, 08:00 in Berlin is 07:00 UTC, not 06:00.
void test_school_time_follows_the_change_to_winter_time() {
  QuietHours q = school();
  const tz::Zone* berlin = tz::find("Europe/Berlin");
  const uint32_t monday = at(21, 0, 0);  // 2026-10-26
  TEST_ASSERT_FALSE(q.active(monday + 6 * 3600 + 59 * 60, tz::offsetMinutes(*berlin, monday + 6 * 3600 + 59 * 60)));
  TEST_ASSERT_TRUE(q.active(monday + 7 * 3600, tz::offsetMinutes(*berlin, monday + 7 * 3600)));
  const uint32_t before = at(14, 0, 0);  // 2026-10-19, still summer time
  TEST_ASSERT_TRUE(q.active(before + 6 * 3600, tz::offsetMinutes(*berlin, before + 6 * 3600)));
}

void test_window_across_midnight() {
  QuietHours q;
  q.add("sun-thu", "22:00", "06:00");  // night rest before school days
  int end = 0;
  TEST_ASSERT_TRUE(q.active(at(6, 23, 0), 0, &end));  // Sunday evening
  TEST_ASSERT_EQUAL(6 * 60, end);
  TEST_ASSERT_TRUE(q.active(at(0, 5, 0), 0, &end));  // Monday morning belongs to Sunday
  TEST_ASSERT_EQUAL(6 * 60, end);
  TEST_ASSERT_FALSE(q.active(at(0, 6, 0), 0));
  TEST_ASSERT_FALSE(q.active(at(4, 23, 0), 0));  // Friday evening is free
  TEST_ASSERT_FALSE(q.active(at(5, 5, 0), 0));   // Saturday morning is free
}

// The morning part belongs to the day before even when the morning's own day is not
// in the list.
void test_the_morning_of_a_night_window_belongs_to_the_day_it_began() {
  QuietHours q;
  q.add("fri", "22:00", "06:00");
  TEST_ASSERT_TRUE(q.active(at(5, 5, 0), 0));    // Saturday morning
  TEST_ASSERT_FALSE(q.active(at(4, 5, 0), 0));   // Friday morning: the window has not begun
  TEST_ASSERT_TRUE(q.active(at(4, 22, 0), 0));
  TEST_ASSERT_FALSE(q.active(at(5, 22, 0), 0));  // Saturday evening
}

// A window that ends at 00:00 runs to the end of the day and not into the next.
void test_an_end_of_midnight_means_the_end_of_that_day() {
  QuietHours q;
  q.add("mon", "20:00", "00:00");
  int end = -1;
  TEST_ASSERT_TRUE(q.active(at(0, 23, 59), 0, &end));
  TEST_ASSERT_EQUAL(0, end);
  TEST_ASSERT_FALSE(q.active(at(1, 0, 0), 0));
  TEST_ASSERT_FALSE(q.active(at(0, 19, 59), 0));
}

// Without a time there is no lock (see the header).
void test_unknown_time_means_no_lock() {
  QuietHours q = school();
  TEST_ASSERT_FALSE(q.active(0, 0));
  TEST_ASSERT_FALSE(q.active(0, 120));
}

// Unix time 0 is Thursday 00:00 UTC; an unknown clock must not be read as that.
void test_unknown_time_is_not_read_as_the_start_of_1970() {
  QuietHours q;
  q.add("thu", "00:00", "01:00");
  TEST_ASSERT_FALSE(q.active(0, 0));
  TEST_ASSERT_TRUE(q.active(60, 0));
}

void test_nothing_configured_never_locks() {
  QuietHours q;
  TEST_ASSERT_FALSE(q.active(at(0, 10, 0), 0));
}

void test_several_windows() {
  QuietHours q;
  q.add("mon-fri", "08:00", "13:30");
  q.add("mon,wed", "14:30", "16:00");  // afternoon lessons
  TEST_ASSERT_FALSE(q.active(at(0, 14, 0), 0));
  TEST_ASSERT_TRUE(q.active(at(0, 15, 0), 0));
  TEST_ASSERT_FALSE(q.active(at(1, 15, 0), 0));  // Tuesday: none
  TEST_ASSERT_TRUE(q.active(at(2, 15, 0), 0));
}

// With overlapping windows the lock lasts until the last one ends. The end that is
// reported decides until when an emergency may lift the lock.
void test_overlapping_windows_report_the_latest_end() {
  QuietHours q;
  q.add("mon", "08:00", "10:00");
  q.add("mon", "09:00", "12:00");
  int end = 0;
  TEST_ASSERT_TRUE(q.active(at(0, 9, 30), 0, &end));
  TEST_ASSERT_EQUAL(12 * 60, end);
  TEST_ASSERT_TRUE(q.active(at(0, 8, 30), 0, &end));
  TEST_ASSERT_EQUAL(10 * 60, end);
}

void test_the_latest_end_also_wins_across_midnight() {
  QuietHours q;
  q.add("mon", "20:00", "23:00");
  q.add("mon", "21:00", "02:00");  // ends the next morning
  int end = 0;
  TEST_ASSERT_TRUE(q.active(at(0, 21, 30), 0, &end));
  TEST_ASSERT_EQUAL(2 * 60, end);
}

// Windows that only touch are two periods: at the end minute of the first the
// second takes over, with its own end.
void test_touching_windows_are_reported_one_at_a_time() {
  QuietHours q;
  q.add("mon", "08:00", "10:00");
  q.add("mon", "10:00", "12:00");
  int end = 0;
  TEST_ASSERT_TRUE(q.active(at(0, 9, 59), 0, &end));
  TEST_ASSERT_EQUAL(10 * 60, end);
  TEST_ASSERT_TRUE(q.active(at(0, 10, 0), 0, &end));
  TEST_ASSERT_EQUAL(12 * 60, end);
}

// ---------------------------------------------------------------------------
// Windows that are switched off
// ---------------------------------------------------------------------------

void test_a_disabled_window_never_makes_school_time() {
  QuietHours q;
  q.add("mon-fri", "08:00", "12:30", false);
  TEST_ASSERT_EQUAL(1, (int)q.count());                 // kept, so that it can be switched on again
  TEST_ASSERT_FALSE(q.window(0).enabled);
  int end = 77;
  TEST_ASSERT_FALSE(q.active(at(0, 9, 0), 0, &end));
  TEST_ASSERT_EQUAL(77, end);                           // and the end is left alone
}

void test_an_enabled_window_is_the_default_of_add_and_makes_school_time() {
  QuietHours q;
  q.add("mon-fri", "08:00", "12:30");
  TEST_ASSERT_TRUE(q.window(0).enabled);
  TEST_ASSERT_TRUE(q.active(at(0, 9, 0), 0));
}

// A disabled window next to an enabled one must not stretch the lock or its reported end.
void test_a_disabled_window_does_not_change_what_the_enabled_ones_report() {
  QuietHours q;
  q.add("mon", "08:00", "10:00");
  q.add("mon", "09:00", "12:00", false);
  int end = 0;
  TEST_ASSERT_TRUE(q.active(at(0, 9, 30), 0, &end));
  TEST_ASSERT_EQUAL(10 * 60, end);
  TEST_ASSERT_FALSE(q.active(at(0, 11, 0), 0));         // only the disabled one covers 11:00
}

// A disabled window across midnight is skipped in both of its halves.
void test_a_disabled_window_across_midnight_is_skipped_evening_and_morning() {
  QuietHours q;
  q.add("mon", "22:00", "06:00", false);
  TEST_ASSERT_FALSE(q.active(at(0, 23, 0), 0));
  TEST_ASSERT_FALSE(q.active(at(1, 5, 0), 0));
}

// Switching a window on later must not break the limit, so a disabled one counts.
void test_a_disabled_window_still_counts_towards_the_two_a_day() {
  QuietHours q;
  TEST_ASSERT_TRUE(q.add("mon", "08:00", "09:00", false) == QuietHours::AddResult::Added);
  TEST_ASSERT_TRUE(q.add("mon", "10:00", "11:00") == QuietHours::AddResult::Added);
  TEST_ASSERT_TRUE(q.add("mon", "12:00", "13:00") == QuietHours::AddResult::DayFull);
}

// A local time just after 1970-01-01 UTC with a negative offset is still the
// evening of the day before; a computation that rounds towards zero gets the
// day wrong.
void test_local_time_before_the_epoch_day_is_the_previous_day() {
  QuietHours q;
  q.add("wed", "18:00", "20:00");  // 1969-12-31 was a Wednesday
  TEST_ASSERT_TRUE(q.active(1, -300, nullptr));  // 19:00 local
}

// ---------------------------------------------------------------------------
// Reading days and times
// ---------------------------------------------------------------------------

void test_parse_days() {
  uint8_t m = 0;
  TEST_ASSERT_TRUE(QuietHours::parseDays("mon-fri", m));
  TEST_ASSERT_EQUAL_HEX8(0x1F, m);
  TEST_ASSERT_TRUE(QuietHours::parseDays("Mon,Wed,Sun", m));
  TEST_ASSERT_EQUAL_HEX8(0x45, m);
  TEST_ASSERT_TRUE(QuietHours::parseDays("daily", m));
  TEST_ASSERT_EQUAL_HEX8(0x7F, m);
  TEST_ASSERT_TRUE(QuietHours::parseDays("DAILY", m));
  TEST_ASSERT_EQUAL_HEX8(0x7F, m);
  TEST_ASSERT_TRUE(QuietHours::parseDays("sat", m));
  TEST_ASSERT_EQUAL_HEX8(0x20, m);
  TEST_ASSERT_TRUE(QuietHours::parseDays("tue-tue", m));
  TEST_ASSERT_EQUAL_HEX8(0x02, m);
}

// "fri-mon" counts forwards over the weekend.
void test_a_range_may_cross_the_weekend() {
  uint8_t m = 0;
  TEST_ASSERT_TRUE(QuietHours::parseDays("fri-mon", m));
  TEST_ASSERT_EQUAL_HEX8(0x71, m);
  TEST_ASSERT_TRUE(QuietHours::parseDays("tue-mon", m));  // the whole week
  TEST_ASSERT_EQUAL_HEX8(0x7F, m);
}

void test_blanks_around_entries_are_ignored() {
  uint8_t m = 0;
  TEST_ASSERT_TRUE(QuietHours::parseDays(" mon , wed ", m));
  TEST_ASSERT_EQUAL_HEX8(0x05, m);
  TEST_ASSERT_TRUE(QuietHours::parseDays("mon - fri", m));
  TEST_ASSERT_EQUAL_HEX8(0x1F, m);
}

void test_parse_days_rejects_what_is_not_a_day_list() {
  uint8_t m = 0x55;
  TEST_ASSERT_FALSE(QuietHours::parseDays("", m));
  TEST_ASSERT_FALSE(QuietHours::parseDays(nullptr, m));
  TEST_ASSERT_FALSE(QuietHours::parseDays("   ", m));
  TEST_ASSERT_FALSE(QuietHours::parseDays("monday", m));
  TEST_ASSERT_FALSE(QuietHours::parseDays("mo-fr", m));  // only the three-letter names
  TEST_ASSERT_FALSE(QuietHours::parseDays("mon-xyz", m));
  TEST_ASSERT_FALSE(QuietHours::parseDays("mon-fri-sun", m));
  TEST_ASSERT_FALSE(QuietHours::parseDays("mon,,tue", m));
  TEST_ASSERT_FALSE(QuietHours::parseDays("mon,", m));
  TEST_ASSERT_FALSE(QuietHours::parseDays("daily,mon", m));
  TEST_ASSERT_EQUAL_HEX8(0x55, m);  // untouched on failure
}

void test_parse_time() {
  int t = -1;
  TEST_ASSERT_TRUE(QuietHours::parseTime("08:05", t));
  TEST_ASSERT_EQUAL(485, t);
  TEST_ASSERT_TRUE(QuietHours::parseTime("8:05", t));
  TEST_ASSERT_EQUAL(485, t);
  TEST_ASSERT_TRUE(QuietHours::parseTime("00:00", t));
  TEST_ASSERT_EQUAL(0, t);
  TEST_ASSERT_TRUE(QuietHours::parseTime("23:59", t));
  TEST_ASSERT_EQUAL(23 * 60 + 59, t);
}

void test_parse_time_rejects_nonsense() {
  int t = 7;
  TEST_ASSERT_FALSE(QuietHours::parseTime("24:00", t));
  TEST_ASSERT_FALSE(QuietHours::parseTime("08:60", t));
  TEST_ASSERT_FALSE(QuietHours::parseTime("8", t));
  TEST_ASSERT_FALSE(QuietHours::parseTime("08:00x", t));
  TEST_ASSERT_FALSE(QuietHours::parseTime("08:00 ", t));
  TEST_ASSERT_FALSE(QuietHours::parseTime(" 08:00", t));
  TEST_ASSERT_FALSE(QuietHours::parseTime("8:5", t));
  TEST_ASSERT_FALSE(QuietHours::parseTime("+8:00", t));
  TEST_ASSERT_FALSE(QuietHours::parseTime("-1:00", t));
  TEST_ASSERT_FALSE(QuietHours::parseTime("008:00", t));
  TEST_ASSERT_FALSE(QuietHours::parseTime("08:000", t));
  TEST_ASSERT_FALSE(QuietHours::parseTime(":30", t));
  TEST_ASSERT_FALSE(QuietHours::parseTime("", t));
  TEST_ASSERT_FALSE(QuietHours::parseTime(nullptr, t));
  TEST_ASSERT_EQUAL(7, t);
}

// ---------------------------------------------------------------------------
// Adding windows
// ---------------------------------------------------------------------------

void test_bad_entries_are_refused_with_a_reason_and_not_added() {
  QuietHours q;
  TEST_ASSERT_TRUE(q.add("mon-fri", "08:00", "08:00") == Result::EmptyWindow);
  TEST_ASSERT_TRUE(q.add("xx", "08:00", "09:00") == Result::BadDays);
  TEST_ASSERT_TRUE(q.add("mon", "8", "9") == Result::BadTime);
  TEST_ASSERT_TRUE(q.add("mon", "08:00", "25:00") == Result::BadTime);
  TEST_ASSERT_EQUAL(0, q.count());
}

void test_a_window_is_stored_as_given() {
  QuietHours q;
  TEST_ASSERT_TRUE(q.add("mon,wed", "14:30", "16:00") == Result::Added);
  TEST_ASSERT_EQUAL(1, q.count());
  TEST_ASSERT_EQUAL_HEX8(0x05, q.window(0).days);
  TEST_ASSERT_EQUAL(14 * 60 + 30, q.window(0).from);
  TEST_ASSERT_EQUAL(16 * 60, q.window(0).to);
}

// At most two windows begin on any one day.
void test_a_day_takes_two_windows_and_no_more() {
  QuietHours q;
  TEST_ASSERT_TRUE(q.add("mon-fri", "08:00", "10:00") == Result::Added);
  TEST_ASSERT_TRUE(q.add("mon-fri", "11:00", "13:00") == Result::Added);
  TEST_ASSERT_TRUE(q.add("wed", "14:00", "16:00") == Result::DayFull);
  // A refused window changes nothing, not even for the days that still had room.
  TEST_ASSERT_TRUE(q.add("sat,wed", "14:00", "16:00") == Result::DayFull);
  TEST_ASSERT_EQUAL(2, q.count());
  TEST_ASSERT_FALSE(q.active(at(5, 15, 0), 0));
  TEST_ASSERT_TRUE(q.add("sat", "14:00", "16:00") == Result::Added);
}

// A window that crosses midnight counts for the day it begins on.
void test_a_window_across_midnight_counts_for_its_first_day() {
  QuietHours q;
  TEST_ASSERT_TRUE(q.add("mon", "22:00", "06:00") == Result::Added);
  TEST_ASSERT_TRUE(q.add("tue", "08:00", "09:00") == Result::Added);
  TEST_ASSERT_TRUE(q.add("tue", "10:00", "11:00") == Result::Added);
  TEST_ASSERT_TRUE(q.add("tue", "12:00", "13:00") == Result::DayFull);
  TEST_ASSERT_TRUE(q.add("mon", "08:00", "09:00") == Result::Added);
  TEST_ASSERT_TRUE(q.add("mon", "10:00", "11:00") == Result::DayFull);
}

void test_a_full_week_fits() {
  QuietHours q;
  TEST_ASSERT_TRUE(q.add("daily", "08:00", "12:00") == Result::Added);
  TEST_ASSERT_TRUE(q.add("daily", "14:00", "16:00") == Result::Added);
  TEST_ASSERT_EQUAL(QuietHours::kMaxWindows / 7, q.count());
  TEST_ASSERT_TRUE(q.add("daily", "18:00", "19:00") == Result::DayFull);
  for (int d = 0; d < 7; ++d) {
    TEST_ASSERT_TRUE(q.active(at(d, 9, 0), 0));
    TEST_ASSERT_TRUE(q.active(at(d, 15, 0), 0));
    TEST_ASSERT_FALSE(q.active(at(d, 13, 0), 0));
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_the_anchor_date_is_a_monday);
  RUN_TEST(test_active_inside_the_window_only);
  RUN_TEST(test_the_end_minute_is_left_alone_when_nothing_is_active);
  RUN_TEST(test_the_weekend_is_free);
  RUN_TEST(test_the_time_zone_offset_is_applied);
  RUN_TEST(test_school_time_follows_the_change_to_winter_time);
  RUN_TEST(test_window_across_midnight);
  RUN_TEST(test_the_morning_of_a_night_window_belongs_to_the_day_it_began);
  RUN_TEST(test_an_end_of_midnight_means_the_end_of_that_day);
  RUN_TEST(test_unknown_time_means_no_lock);
  RUN_TEST(test_unknown_time_is_not_read_as_the_start_of_1970);
  RUN_TEST(test_nothing_configured_never_locks);
  RUN_TEST(test_several_windows);
  RUN_TEST(test_overlapping_windows_report_the_latest_end);
  RUN_TEST(test_the_latest_end_also_wins_across_midnight);
  RUN_TEST(test_touching_windows_are_reported_one_at_a_time);
  RUN_TEST(test_local_time_before_the_epoch_day_is_the_previous_day);
  RUN_TEST(test_a_disabled_window_never_makes_school_time);
  RUN_TEST(test_an_enabled_window_is_the_default_of_add_and_makes_school_time);
  RUN_TEST(test_a_disabled_window_does_not_change_what_the_enabled_ones_report);
  RUN_TEST(test_a_disabled_window_across_midnight_is_skipped_evening_and_morning);
  RUN_TEST(test_a_disabled_window_still_counts_towards_the_two_a_day);
  RUN_TEST(test_parse_days);
  RUN_TEST(test_a_range_may_cross_the_weekend);
  RUN_TEST(test_blanks_around_entries_are_ignored);
  RUN_TEST(test_parse_days_rejects_what_is_not_a_day_list);
  RUN_TEST(test_parse_time);
  RUN_TEST(test_parse_time_rejects_nonsense);
  RUN_TEST(test_bad_entries_are_refused_with_a_reason_and_not_added);
  RUN_TEST(test_a_window_is_stored_as_given);
  RUN_TEST(test_a_day_takes_two_windows_and_no_more);
  RUN_TEST(test_a_window_across_midnight_counts_for_its_first_day);
  RUN_TEST(test_a_full_week_fits);
  return UNITY_END();
}

// Host tests for the calendar arithmetic. The expected values come from an
// independent source (Python's datetime), not from the code under test.

#include <unity.h>

#include <string>

#include <app/Calendar.h>

using namespace calendar;

void setUp() {}
void tearDown() {}

void test_days_from_civil_known_dates() {
  TEST_ASSERT_EQUAL_INT64(0, daysFromCivil(1970, 1, 1));
  TEST_ASSERT_EQUAL_INT64(-1, daysFromCivil(1969, 12, 31));
  TEST_ASSERT_EQUAL_INT64(11017, daysFromCivil(2000, 3, 1));
  TEST_ASSERT_EQUAL_INT64(1791158400 / 86400, daysFromCivil(2026, 10, 5));
}

// The leap day exists in 2028 and 2000, not in 2027 and 2100.
void test_leap_years() {
  TEST_ASSERT_EQUAL_INT64(2, daysFromCivil(2028, 3, 1) - daysFromCivil(2028, 2, 28));
  TEST_ASSERT_EQUAL_INT64(1, daysFromCivil(2027, 3, 1) - daysFromCivil(2027, 2, 28));
  TEST_ASSERT_EQUAL_INT64(2, daysFromCivil(2000, 3, 1) - daysFromCivil(2000, 2, 28));
  TEST_ASSERT_EQUAL_INT64(1, daysFromCivil(2100, 3, 1) - daysFromCivil(2100, 2, 28));
  TEST_ASSERT_EQUAL_INT64(366, daysFromCivil(2029, 1, 1) - daysFromCivil(2028, 1, 1));
}

void test_year_of_changes_exactly_at_midnight_of_new_year() {
  TEST_ASSERT_EQUAL_INT(1969, yearOf(-1));
  TEST_ASSERT_EQUAL_INT(1970, yearOf(0));
  TEST_ASSERT_EQUAL_INT(2026, yearOf(1798761599));  // 2026-12-31 23:59:59 UTC
  TEST_ASSERT_EQUAL_INT(2027, yearOf(1798761600));
  TEST_ASSERT_EQUAL_INT(2028, yearOf(1835438400));  // 2028-02-29 12:00 UTC, the leap day
}

void test_weekday_counts_from_monday() {
  TEST_ASSERT_EQUAL_INT(3, weekday(0));                          // 1970-01-01 was a Thursday
  TEST_ASSERT_EQUAL_INT(0, weekday(daysFromCivil(2026, 10, 5)));  // Monday
  TEST_ASSERT_EQUAL_INT(6, weekday(daysFromCivil(2026, 10, 4)));  // Sunday
  TEST_ASSERT_EQUAL_INT(1, weekday(daysFromCivil(2028, 2, 29)));  // Tuesday, the leap day
  TEST_ASSERT_EQUAL_INT(2, weekday(-1));                          // 1969-12-31, a Wednesday
}

void test_to_local_applies_the_offset_and_rolls_over_the_day() {
  const int64_t monday = 1791158400;  // 2026-10-05 00:00 UTC
  LocalTime t = toLocal(monday, 0);
  TEST_ASSERT_EQUAL_INT(0, t.weekday);
  TEST_ASSERT_EQUAL_INT(0, t.minuteOfDay);

  t = toLocal(monday + 23 * 3600 + 30 * 60, 60);  // 23:30 UTC is 00:30 the next day in +01:00
  TEST_ASSERT_EQUAL_INT(1, t.weekday);
  TEST_ASSERT_EQUAL_INT(30, t.minuteOfDay);

  t = toLocal(monday + 3600, -120);  // 01:00 UTC is 23:00 the day before in -02:00
  TEST_ASSERT_EQUAL_INT(6, t.weekday);
  TEST_ASSERT_EQUAL_INT(23 * 60, t.minuteOfDay);
  TEST_ASSERT_EQUAL_INT64(daysFromCivil(2026, 10, 4), t.days);
}

// Rounding towards zero would give a negative minute of the day here.
void test_to_local_before_the_epoch_in_local_time_rounds_down() {
  const LocalTime t = toLocal(1, -300);  // 1970-01-01 00:00:01 UTC is 19:00 on 1969-12-31
  TEST_ASSERT_EQUAL_INT64(-1, t.days);
  TEST_ASSERT_EQUAL_INT(19 * 60, t.minuteOfDay);
  TEST_ASSERT_EQUAL_INT(2, t.weekday);
}

// Values from Python's datetime.date.fromordinal, not from the code under test.
void test_civil_from_days_known_dates_and_the_round_trip() {
  const struct { int64_t days; int y, m, d; } table[] = {
      {0, 1970, 1, 1}, {-1, 1969, 12, 31}, {11017, 2000, 3, 1}, {11016, 2000, 2, 29},
      {20731, 2026, 10, 5}, {20878, 2027, 3, 1}, {21609, 2029, 3, 1}, {47541, 2100, 3, 1},
      {47540, 2100, 2, 28}, {47539, 2100, 2, 27}, {-25508, 1900, 3, 1}, {59, 1970, 3, 1},
      {60, 1970, 3, 2}, {365, 1971, 1, 1}, {10957, 2000, 1, 1}};
  for (const auto& row : table) {
    const Civil c = civilFromDays(row.days);
    TEST_ASSERT_EQUAL_INT_MESSAGE(row.y, c.year, std::to_string(row.days).c_str());
    TEST_ASSERT_EQUAL_INT_MESSAGE(row.m, c.month, std::to_string(row.days).c_str());
    TEST_ASSERT_EQUAL_INT_MESSAGE(row.d, c.day, std::to_string(row.days).c_str());
  }
  // The two functions are inverses over a few thousand days, leap days included.
  for (int64_t days = -800; days < 14000; days += 7) {
    const Civil c = civilFromDays(days);
    TEST_ASSERT_EQUAL_INT64(days, daysFromCivil(c.year, c.month, c.day));
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_days_from_civil_known_dates);
  RUN_TEST(test_leap_years);
  RUN_TEST(test_year_of_changes_exactly_at_midnight_of_new_year);
  RUN_TEST(test_weekday_counts_from_monday);
  RUN_TEST(test_to_local_applies_the_offset_and_rolls_over_the_day);
  RUN_TEST(test_to_local_before_the_epoch_in_local_time_rounds_down);
  RUN_TEST(test_civil_from_days_known_dates_and_the_round_trip);
  return UNITY_END();
}

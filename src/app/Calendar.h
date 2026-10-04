#pragma once

// Calendar arithmetic on Unix time, shared by the time zone rules and the quiet
// hours. It works on days counted from 1970-01-01 in the civil (Gregorian)
// calendar, so leap years follow from the arithmetic and no table of month
// lengths is needed.

#include <cstdint>

namespace calendar {

// Days since 1970-01-01 for a date (month 1-12, day 1-31).
int64_t daysFromCivil(int year, unsigned month, unsigned day);

// The year that contains the given Unix time (UTC), also before 1970.
int yearOf(int64_t epochSeconds);

// Day of the week of a day count: 0 = Monday … 6 = Sunday. Monday first because
// the quiet hours are written Monday to Sunday.
int weekday(int64_t days);

// A Unix time seen on a wall clock.
struct LocalTime {
  int64_t days;      // days since 1970-01-01 on the local calendar
  int minuteOfDay;   // 0 … 1439
  int weekday;       // 0 = Monday … 6 = Sunday
};

// Converts UTC plus an offset in minutes to local calendar fields. The division
// rounds down, so a time shortly after 1970-01-01 with a negative offset lands on
// the previous evening instead of on a negative minute.
LocalTime toLocal(int64_t epochUtc, int offsetMinutes);

}  // namespace calendar

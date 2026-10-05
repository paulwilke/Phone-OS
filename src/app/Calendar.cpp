#include "Calendar.h"

namespace calendar {
namespace {

int64_t floorDiv(int64_t a, int64_t b) {
  int64_t q = a / b;
  if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
  return q;
}

}  // namespace

// Howard Hinnant's days_from_civil: the year starts in March so the leap day is
// the last day of the (shifted) year.
int64_t daysFromCivil(int year, unsigned month, unsigned day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yearOfEra = (unsigned)(year - era * 400);
  const unsigned dayOfYear = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
  return (int64_t)era * 146097 + (int64_t)dayOfEra - 719468;
}

Civil civilFromDays(int64_t days) {
  const int64_t z = days + 719468;
  const int64_t era = floorDiv(z, 146097);
  const unsigned dayOfEra = (unsigned)(z - era * 146097);
  const unsigned yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
  const unsigned dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
  const unsigned shiftedMonth = (5 * dayOfYear + 2) / 153;  // 0 = March
  const unsigned day = dayOfYear - (153 * shiftedMonth + 2) / 5 + 1;
  const unsigned month = shiftedMonth < 10 ? shiftedMonth + 3 : shiftedMonth - 9;
  return Civil{(int)(yearOfEra + era * 400) + (month <= 2 ? 1 : 0), (int)month, (int)day};
}

int yearOf(int64_t epochSeconds) {
  const int64_t z = floorDiv(epochSeconds, 86400) + 719468;
  const int64_t era = floorDiv(z, 146097);
  const unsigned dayOfEra = (unsigned)(z - era * 146097);
  const unsigned yearOfEra =
      (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
  const unsigned dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
  const unsigned shiftedMonth = (5 * dayOfYear + 2) / 153;  // 0 = March
  const bool januaryOrFebruary = shiftedMonth >= 10;
  return (int)(yearOfEra + era * 400) + (januaryOrFebruary ? 1 : 0);
}

int weekday(int64_t days) {
  // 1970-01-01 was a Thursday.
  const int64_t w = (days + 3) % 7;
  return (int)(w < 0 ? w + 7 : w);
}

LocalTime toLocal(int64_t epochUtc, int offsetMinutes) {
  const int64_t local = epochUtc + (int64_t)offsetMinutes * 60;
  const int64_t days = floorDiv(local, 86400);
  const int minute = (int)((local - days * 86400) / 60);
  return LocalTime{days, minute, weekday(days)};
}

}  // namespace calendar

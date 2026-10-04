#include "TimeZone.h"

#include <cstring>

#include "Calendar.h"

namespace tz {
namespace {

const Zone kZones[] = {
    {"Europe/Berlin", 60, Rule::EU},        {"Europe/Vienna", 60, Rule::EU},
    {"Europe/Zurich", 60, Rule::EU},        {"Europe/Paris", 60, Rule::EU},
    {"Europe/Madrid", 60, Rule::EU},        {"Europe/Rome", 60, Rule::EU},
    {"Europe/Amsterdam", 60, Rule::EU},     {"Europe/Brussels", 60, Rule::EU},
    {"Europe/Prague", 60, Rule::EU},        {"Europe/Warsaw", 60, Rule::EU},
    {"Europe/Copenhagen", 60, Rule::EU},    {"Europe/Stockholm", 60, Rule::EU},
    {"Europe/London", 0, Rule::EU},         {"Europe/Dublin", 0, Rule::EU},
    {"Europe/Lisbon", 0, Rule::EU},         {"Atlantic/Canary", 0, Rule::EU},
    {"Europe/Athens", 120, Rule::EU},       {"Europe/Helsinki", 120, Rule::EU},
    {"Europe/Kyiv", 120, Rule::EU},         {"Europe/Istanbul", 180, Rule::None},
    {"Europe/Moscow", 180, Rule::None},     {"UTC", 0, Rule::None},
    {"America/New_York", -300, Rule::US},   {"America/Chicago", -360, Rule::US},
    {"America/Denver", -420, Rule::US},     {"America/Los_Angeles", -480, Rule::US},
};

// The nth Sunday of a month (n = 1 … 4), or the last one (n = 0), as a day count.
int64_t sunday(int year, unsigned month, int n) {
  if (n > 0) {
    const int64_t first = calendar::daysFromCivil(year, month, 1);
    // Sunday is 6 in the Monday-first numbering.
    return first + (6 - calendar::weekday(first)) + (int64_t)(n - 1) * 7;
  }
  // The day before the first of the next month is the last day of this one; this
  // way no table of month lengths (and no leap-year rule for February) is needed.
  const int64_t last = month == 12 ? calendar::daysFromCivil(year + 1, 1, 1) - 1
                                   : calendar::daysFromCivil(year, month + 1, 1) - 1;
  return last - (calendar::weekday(last) + 1) % 7;
}

}  // namespace

size_t zoneCount() { return sizeof(kZones) / sizeof(kZones[0]); }

const Zone& zoneAt(size_t i) { return kZones[i]; }

const Zone* find(const char* name) {
  if (!name || !*name) return nullptr;
  for (const Zone& z : kZones) {
    if (strcmp(z.name, name) == 0) return &z;
  }
  return nullptr;
}

int offsetMinutes(const Zone& zone, uint32_t epochUtc) {
  if (zone.rule == Rule::None) return zone.baseMinutes;
  const int year = calendar::yearOf((int64_t)epochUtc);
  int64_t start = 0, end = 0;  // summer time, as Unix times
  if (zone.rule == Rule::EU) {
    start = sunday(year, 3, 0) * 86400 + 3600;
    end = sunday(year, 10, 0) * 86400 + 3600;
  } else {
    // 02:00 on the wall, expressed in UTC: before the change the wall shows
    // winter time, after the end of summer time it still shows summer time.
    start = sunday(year, 3, 2) * 86400 + 2 * 3600 - (int64_t)zone.baseMinutes * 60;
    end = sunday(year, 11, 1) * 86400 + 2 * 3600 - (int64_t)(zone.baseMinutes + 60) * 60;
  }
  const bool summer = (int64_t)epochUtc >= start && (int64_t)epochUtc < end;
  return zone.baseMinutes + (summer ? 60 : 0);
}

}  // namespace tz

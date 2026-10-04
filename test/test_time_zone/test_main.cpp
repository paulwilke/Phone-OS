// Host tests for time zones and the changes between summer and winter time.
//
// The transition instants below were taken from the IANA time zone database
// (Python zoneinfo), not computed with the code under test, so they are an
// independent check. Each is the first second of the new offset.

#include <unity.h>

#include <cstdio>
#include <cstring>

#include <app/TimeZone.h>
#include <app/Calendar.h>

void setUp() {}
void tearDown() {}

namespace {

struct Transition {
  const char* zone;
  uint32_t instant;  // Unix time of the first second with the new offset
  int before;        // offset in minutes up to the second before
  int after;
};

const Transition kTransitions[] = {
    {"Europe/Berlin", 1774746000u, 60, 120},  // 2026-03-29 01:00 UTC
    {"Europe/Berlin", 1792890000u, 120, 60},  // 2026-10-25 01:00 UTC
    {"Europe/London", 1774746000u, 0, 60},  // 2026-03-29 01:00 UTC
    {"Europe/London", 1792890000u, 60, 0},  // 2026-10-25 01:00 UTC
    {"Europe/Athens", 1774746000u, 120, 180},  // 2026-03-29 01:00 UTC
    {"Europe/Athens", 1792890000u, 180, 120},  // 2026-10-25 01:00 UTC
    {"America/New_York", 1772953200u, -300, -240},  // 2026-03-08 07:00 UTC
    {"America/New_York", 1793512800u, -240, -300},  // 2026-11-01 06:00 UTC
    {"America/Chicago", 1772956800u, -360, -300},  // 2026-03-08 08:00 UTC
    {"America/Chicago", 1793516400u, -300, -360},  // 2026-11-01 07:00 UTC
    {"America/Denver", 1772960400u, -420, -360},  // 2026-03-08 09:00 UTC
    {"America/Denver", 1793520000u, -360, -420},  // 2026-11-01 08:00 UTC
    {"America/Los_Angeles", 1772964000u, -480, -420},  // 2026-03-08 10:00 UTC
    {"America/Los_Angeles", 1793523600u, -420, -480},  // 2026-11-01 09:00 UTC
    {"Europe/Berlin", 1806195600u, 60, 120},  // 2027-03-28 01:00 UTC
    {"Europe/Berlin", 1824944400u, 120, 60},  // 2027-10-31 01:00 UTC
    {"Europe/London", 1806195600u, 0, 60},  // 2027-03-28 01:00 UTC
    {"Europe/London", 1824944400u, 60, 0},  // 2027-10-31 01:00 UTC
    {"Europe/Athens", 1806195600u, 120, 180},  // 2027-03-28 01:00 UTC
    {"Europe/Athens", 1824944400u, 180, 120},  // 2027-10-31 01:00 UTC
    {"America/New_York", 1805007600u, -300, -240},  // 2027-03-14 07:00 UTC
    {"America/New_York", 1825567200u, -240, -300},  // 2027-11-07 06:00 UTC
    {"America/Chicago", 1805011200u, -360, -300},  // 2027-03-14 08:00 UTC
    {"America/Chicago", 1825570800u, -300, -360},  // 2027-11-07 07:00 UTC
    {"America/Denver", 1805014800u, -420, -360},  // 2027-03-14 09:00 UTC
    {"America/Denver", 1825574400u, -360, -420},  // 2027-11-07 08:00 UTC
    {"America/Los_Angeles", 1805018400u, -480, -420},  // 2027-03-14 10:00 UTC
    {"America/Los_Angeles", 1825578000u, -420, -480},  // 2027-11-07 09:00 UTC
    {"Europe/Berlin", 1837645200u, 60, 120},  // 2028-03-26 01:00 UTC
    {"Europe/Berlin", 1856394000u, 120, 60},  // 2028-10-29 01:00 UTC
    {"Europe/London", 1837645200u, 0, 60},  // 2028-03-26 01:00 UTC
    {"Europe/London", 1856394000u, 60, 0},  // 2028-10-29 01:00 UTC
    {"Europe/Athens", 1837645200u, 120, 180},  // 2028-03-26 01:00 UTC
    {"Europe/Athens", 1856394000u, 180, 120},  // 2028-10-29 01:00 UTC
    {"America/New_York", 1836457200u, -300, -240},  // 2028-03-12 07:00 UTC
    {"America/New_York", 1857016800u, -240, -300},  // 2028-11-05 06:00 UTC
    {"America/Chicago", 1836460800u, -360, -300},  // 2028-03-12 08:00 UTC
    {"America/Chicago", 1857020400u, -300, -360},  // 2028-11-05 07:00 UTC
    {"America/Denver", 1836464400u, -420, -360},  // 2028-03-12 09:00 UTC
    {"America/Denver", 1857024000u, -360, -420},  // 2028-11-05 08:00 UTC
    {"America/Los_Angeles", 1836468000u, -480, -420},  // 2028-03-12 10:00 UTC
    {"America/Los_Angeles", 1857027600u, -420, -480},  // 2028-11-05 09:00 UTC
};

int offset(const char* zone, uint32_t epoch) {
  const tz::Zone* z = tz::find(zone);
  TEST_ASSERT_NOT_NULL_MESSAGE(z, zone);
  return tz::offsetMinutes(*z, epoch);
}

uint32_t utc(int y, unsigned m, unsigned d, int hh, int mm) {
  return (uint32_t)(calendar::daysFromCivil(y, m, d) * 86400 + hh * 3600 + mm * 60);
}

}  // namespace

// Every transition of 2026, 2027 and 2028 in seven zones: Europe with three
// different winter offsets and the four US zones. The second before the instant
// still has the old offset, the instant itself the new one. 2028 is a leap year.
void test_transition_instants_of_2026_to_2028() {
  for (const Transition& t : kTransitions) {
    char label[80];
    snprintf(label, sizeof(label), "%s at %u", t.zone, (unsigned)t.instant);
    TEST_ASSERT_EQUAL_INT_MESSAGE(t.before, offset(t.zone, t.instant - 1), label);
    TEST_ASSERT_EQUAL_INT_MESSAGE(t.after, offset(t.zone, t.instant), label);
  }
}

void test_there_are_six_transitions_for_each_of_the_seven_zones() {
  TEST_ASSERT_EQUAL(7 * 6, sizeof(kTransitions) / sizeof(kTransitions[0]));
}

// The offset changes exactly twice a year in a zone with a rule and never in one
// without, checked hour by hour so that no day is skipped.
void test_each_zone_changes_twice_a_year_or_never() {
  for (size_t i = 0; i < tz::zoneCount(); ++i) {
    const tz::Zone& z = tz::zoneAt(i);
    for (int year = 2026; year <= 2028; ++year) {
      int changes = 0;
      const uint32_t begin = utc(year, 1, 1, 0, 0), end = utc(year + 1, 1, 1, 0, 0);
      int previous = tz::offsetMinutes(z, begin);
      for (uint32_t t = begin + 3600; t < end; t += 3600) {
        const int now = tz::offsetMinutes(z, t);
        if (now != previous) ++changes;
        previous = now;
      }
      char label[64];
      snprintf(label, sizeof(label), "%s %d", z.name, year);
      TEST_ASSERT_EQUAL_INT_MESSAGE(z.rule == tz::Rule::None ? 0 : 2, changes, label);
    }
  }
}

// Winter time is the base offset, summer time one hour more, in the middle of the
// seasons of each year, for every zone in the list.
void test_every_zone_has_its_base_offset_in_winter_and_one_hour_more_in_summer() {
  for (size_t i = 0; i < tz::zoneCount(); ++i) {
    const tz::Zone& z = tz::zoneAt(i);
    for (int year = 2026; year <= 2028; ++year) {
      TEST_ASSERT_EQUAL_INT_MESSAGE(z.baseMinutes, tz::offsetMinutes(z, utc(year, 1, 15, 12, 0)), z.name);
      TEST_ASSERT_EQUAL_INT_MESSAGE(z.baseMinutes, tz::offsetMinutes(z, utc(year, 12, 15, 12, 0)), z.name);
      const int summer = z.baseMinutes + (z.rule == tz::Rule::None ? 0 : 60);
      TEST_ASSERT_EQUAL_INT_MESSAGE(summer, tz::offsetMinutes(z, utc(year, 7, 15, 12, 0)), z.name);
    }
  }
}

// Zones in the same rule change at the same instant (EU) or at the same local
// wall time (US), however different their offsets.
void test_zones_of_one_rule_share_the_instant() {
  TEST_ASSERT_EQUAL(60, offset("Europe/London", utc(2026, 3, 29, 1, 0)));
  TEST_ASSERT_EQUAL(120, offset("Europe/Berlin", utc(2026, 3, 29, 1, 0)));
  TEST_ASSERT_EQUAL(180, offset("Europe/Athens", utc(2026, 3, 29, 1, 0)));
  TEST_ASSERT_EQUAL(0, offset("Europe/London", utc(2026, 3, 29, 0, 59)));
}

void test_zones_without_a_rule_never_change() {
  TEST_ASSERT_EQUAL(180, offset("Europe/Moscow", utc(2026, 7, 1, 0, 0)));
  TEST_ASSERT_EQUAL(180, offset("Europe/Istanbul", utc(2026, 1, 1, 0, 0)));
  TEST_ASSERT_EQUAL(0, offset("UTC", utc(2026, 7, 1, 0, 0)));
}

void test_unknown_zone_is_not_found() {
  TEST_ASSERT_NULL(tz::find("Mars/Olympus"));
  TEST_ASSERT_NULL(tz::find(""));
  TEST_ASSERT_NULL(tz::find(nullptr));
  TEST_ASSERT_NULL(tz::find("europe/berlin"));  // names are exact
  TEST_ASSERT_NULL(tz::find("Europe/Berlin "));
}

void test_zone_names_are_unique_and_offsets_plausible() {
  for (size_t i = 0; i < tz::zoneCount(); ++i) {
    const tz::Zone& a = tz::zoneAt(i);
    TEST_ASSERT_TRUE(a.baseMinutes >= -12 * 60 && a.baseMinutes <= 14 * 60);
    TEST_ASSERT_EQUAL_PTR(&a, tz::find(a.name));
    for (size_t j = i + 1; j < tz::zoneCount(); ++j) {
      TEST_ASSERT_TRUE(strcmp(a.name, tz::zoneAt(j).name) != 0);
    }
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_transition_instants_of_2026_to_2028);
  RUN_TEST(test_there_are_six_transitions_for_each_of_the_seven_zones);
  RUN_TEST(test_each_zone_changes_twice_a_year_or_never);
  RUN_TEST(test_every_zone_has_its_base_offset_in_winter_and_one_hour_more_in_summer);
  RUN_TEST(test_zones_of_one_rule_share_the_instant);
  RUN_TEST(test_zones_without_a_rule_never_change);
  RUN_TEST(test_unknown_zone_is_not_found);
  RUN_TEST(test_zone_names_are_unique_and_offsets_plausible);
  return UNITY_END();
}

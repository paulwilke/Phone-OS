// Which time to believe: a network that can be hours off, a receiver that is exact when it has a fix, and a
// clock that must not jump on the word of a source that lies.

#include <unity.h>

#include <board/DeviceClock.h>
#include <board/TimeSync.h>
#include <fakes/FakeEventLog.h>
#include <fakes/FakeMillis.h>

using namespace board;

void setUp() {}
void tearDown() {}

namespace {
constexpr uint32_t kNoon = 1790000000u;

struct Rig {
  FakeMillis ms;
  DeviceClock clock{ms};
  FakeEventLog log;
  TimeSync sync{clock, log};
  void pass(uint32_t seconds) { ms.advance(seconds * 1000); }
};
}  // namespace

void test_the_first_time_from_the_network_is_taken_with_its_zone() {
  Rig r;
  r.sync.offerNetwork(kNoon, 120);
  TEST_ASSERT_EQUAL_UINT32(kNoon, r.clock.epochUtc());
  TEST_ASSERT_EQUAL(120, r.clock.tzOffsetMinutes());
  TEST_ASSERT_TRUE(r.sync.source() == TimeSync::Source::Network);
  TEST_ASSERT_EQUAL(1, r.log.lines().size());
}

void test_the_first_time_from_the_satellites_is_taken() {
  Rig r;
  r.sync.offerSatellite(kNoon);
  TEST_ASSERT_EQUAL_UINT32(kNoon, r.clock.epochUtc());
  TEST_ASSERT_TRUE(r.sync.source() == TimeSync::Source::Satellite);
}

// --- times that are no times ---------------------------------------------------------------------

void test_a_time_before_2020_or_from_2070_on_is_ignored_and_said_once() {
  Rig r;
  r.sync.offerNetwork(946684800u, 0);   // 2000: a modem that never synchronised
  r.sync.offerNetwork(1104537600u, 0);  // 2005
  r.sync.offerNetwork(3155760000u, 0);  // 2070
  r.sync.offerSatellite(315964800u);    // 1980: a receiver that has not learned the date
  r.sync.offerSatellite(3500000000u);
  TEST_ASSERT_EQUAL_UINT32(0, r.clock.epochUtc());
  TEST_ASSERT_TRUE(r.sync.source() == TimeSync::Source::None);
  TEST_ASSERT_EQUAL(2, r.log.lines().size());  // one for each source, not one for each offer
}

void test_the_last_second_before_2070_is_a_time() {
  Rig r;
  r.sync.offerNetwork(3155759999u, 0);
  TEST_ASSERT_EQUAL_UINT32(3155759999u, r.clock.epochUtc());
}

void test_the_same_reading_offered_again_changes_nothing_and_logs_nothing() {
  Rig r;
  r.sync.offerNetwork(kNoon, 60);
  r.pass(3600);
  r.sync.offerNetwork(kNoon, 60);  // the modem's reading from an hour ago
  TEST_ASSERT_EQUAL_UINT32(kNoon + 3600, r.clock.epochUtc());
  TEST_ASSERT_EQUAL(1, r.log.lines().size());
}

// --- the network ----------------------------------------------------------------------------------

void test_a_network_time_close_to_the_clock_corrects_a_drift() {
  Rig r;
  r.sync.offerNetwork(kNoon, 60);
  r.pass(3600);
  r.sync.offerNetwork(kNoon + 3600 + 12, 60);  // the clock is 12 s slow
  TEST_ASSERT_EQUAL_UINT32(kNoon + 3612, r.clock.epochUtc());
}

void test_a_small_step_back_is_taken_but_a_big_one_is_a_jump_back_and_held() {
  Rig r;
  r.sync.offerNetwork(kNoon, 60);
  r.pass(100);
  r.sync.offerNetwork(kNoon + 100 - 20, 60);  // 20 s behind: the clock was fast
  TEST_ASSERT_EQUAL_UINT32(kNoon + 80, r.clock.epochUtc());
  r.pass(100);
  r.sync.offerNetwork(kNoon - 5 * 3600, 60);  // five hours behind: the operator's clock is stale
  TEST_ASSERT_EQUAL_UINT32(kNoon + 180, r.clock.epochUtc());  // the clock did not go back
  TEST_ASSERT_TRUE(r.log.contains("held back"));
}

void test_a_big_step_forward_is_held_back_too() {
  Rig r;
  r.sync.offerNetwork(kNoon, 60);
  r.pass(60);
  r.sync.offerNetwork(kNoon + 60 + 7200, 60);
  TEST_ASSERT_EQUAL_UINT32(kNoon + 60, r.clock.epochUtc());
}

// A source that lies once does not move the clock; one that is right twice does.
void test_a_suspect_time_is_taken_when_the_next_reading_agrees_with_it() {
  Rig r;
  r.sync.offerNetwork(kNoon, 60);
  r.pass(60);
  r.sync.offerNetwork(kNoon + 60 + 7200, 60);  // suspect
  r.pass(3600);
  r.sync.offerNetwork(kNoon + 60 + 7200 + 3600 + 4, 60);  // the next hour: consistent with the suspect
  TEST_ASSERT_EQUAL_UINT32(kNoon + 60 + 7200 + 3600 + 4, r.clock.epochUtc());
  TEST_ASSERT_TRUE(r.log.contains("confirmed by a second reading"));
}

void test_two_suspects_that_disagree_with_each_other_move_nothing() {
  Rig r;
  r.sync.offerNetwork(kNoon, 60);
  r.pass(60);
  r.sync.offerNetwork(kNoon + 60 + 7200, 60);
  r.pass(3600);
  r.sync.offerNetwork(kNoon + 60 + 3600 + 5 * 3600, 60);  // another story
  TEST_ASSERT_EQUAL_UINT32(kNoon + 3660, r.clock.epochUtc());
  r.pass(3600);
  r.sync.offerNetwork(kNoon + 3660 + 3600 + 1, 60);  // and a reading that fits the clock is taken as the clock's own
  TEST_ASSERT_EQUAL_UINT32(kNoon + 7260, r.clock.epochUtc());  // one second apart is not worth a step
}

void test_a_suspect_is_forgotten_when_the_clock_and_the_network_agree_again() {
  Rig r;
  r.sync.offerNetwork(kNoon, 60);
  r.pass(60);
  r.sync.offerNetwork(kNoon + 60 + 7200, 60);  // suspect
  r.pass(60);
  r.sync.offerNetwork(kNoon + 120, 60);  // the clock was right
  r.pass(3600);
  r.sync.offerNetwork(kNoon + 120 + 3600 + 7200, 60);  // the old suspect's story would "agree" with this: it must not
  TEST_ASSERT_EQUAL_UINT32(kNoon + 3720, r.clock.epochUtc());
}

void test_the_zone_follows_only_a_time_that_is_taken() {
  Rig r;
  r.sync.offerNetwork(kNoon, 60);
  r.pass(60);
  r.sync.offerNetwork(kNoon + 60 + 7200, 300);  // suspect: its zone is held with it
  TEST_ASSERT_EQUAL(60, r.clock.tzOffsetMinutes());
  r.pass(3600);
  r.sync.offerNetwork(kNoon + 60 + 7200 + 3600 + 1, 300);
  TEST_ASSERT_EQUAL(300, r.clock.tzOffsetMinutes());
}

void test_an_impossible_zone_offset_is_not_taken_and_does_not_stop_the_time() {
  Rig r;
  r.sync.offerNetwork(kNoon, 2000);
  TEST_ASSERT_EQUAL_UINT32(kNoon, r.clock.epochUtc());
  TEST_ASSERT_EQUAL(0, r.clock.tzOffsetMinutes());
}

// --- the satellites -------------------------------------------------------------------------------

void test_a_satellite_time_corrects_a_network_time_that_was_wrong() {
  Rig r;
  r.sync.offerNetwork(kNoon, 60);
  r.pass(10);
  r.sync.offerSatellite(kNoon + 10 + 3600);  // the operator was an hour behind
  TEST_ASSERT_EQUAL_UINT32(kNoon + 3610, r.clock.epochUtc());
  TEST_ASSERT_TRUE(r.sync.source() == TimeSync::Source::Satellite);
}

void test_a_satellite_time_within_two_seconds_leaves_the_clock_alone() {
  Rig r;
  r.sync.offerSatellite(kNoon);
  const size_t lines = r.log.lines().size();
  r.pass(10);
  r.sync.offerSatellite(kNoon + 10 + 2);
  r.pass(1);
  r.sync.offerSatellite(kNoon + 11 - 2);
  TEST_ASSERT_EQUAL_UINT32(kNoon + 11, r.clock.epochUtc());
  TEST_ASSERT_EQUAL(lines, r.log.lines().size());
}

void test_the_network_does_not_overrule_the_satellites_for_a_day() {
  Rig r;
  r.sync.offerSatellite(kNoon);
  r.pass(3600);
  r.sync.offerNetwork(kNoon + 3600 - 7200, 60);  // the operator is two hours behind
  TEST_ASSERT_EQUAL_UINT32(kNoon + 3600, r.clock.epochUtc());
  TEST_ASSERT_TRUE(r.sync.source() == TimeSync::Source::Satellite);
  r.pass(24 * 3600);
  r.sync.offerNetwork(kNoon + 3600 + 24 * 3600 + 7, 60);  // a day later the network may speak again
  TEST_ASSERT_EQUAL_UINT32(kNoon + 3600 + 24 * 3600 + 7, r.clock.epochUtc());
}

void test_the_satellites_staying_in_agreement_keep_their_precedence() {
  Rig r;
  r.sync.offerSatellite(kNoon);
  for (int hour = 1; hour <= 30; ++hour) {
    r.pass(3600);
    r.sync.offerSatellite(kNoon + hour * 3600);
  }
  r.sync.offerNetwork(kNoon + 30 * 3600 - 9000, 60);
  TEST_ASSERT_EQUAL_UINT32(kNoon + 30 * 3600, r.clock.epochUtc());
}

void test_a_satellite_time_that_agrees_makes_the_clock_rest_on_it() {
  Rig r;
  r.sync.offerNetwork(kNoon, 60);
  r.pass(5);
  r.sync.offerSatellite(kNoon + 5 + 1);
  TEST_ASSERT_TRUE(r.sync.source() == TimeSync::Source::Satellite);
}

void test_every_change_of_the_clock_is_one_line_in_the_log() {
  Rig r;
  r.sync.offerNetwork(kNoon, 60);
  r.pass(10);
  r.sync.offerSatellite(kNoon + 10 + 3600);
  TEST_ASSERT_EQUAL(2, r.log.lines().size());
  TEST_ASSERT_TRUE(r.log.contains("set from the network, the time was unknown"));
  TEST_ASSERT_TRUE(r.log.contains("set from the satellites, 3600 s from the time before"));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_the_first_time_from_the_network_is_taken_with_its_zone);
  RUN_TEST(test_the_first_time_from_the_satellites_is_taken);
  RUN_TEST(test_a_time_before_2020_or_from_2070_on_is_ignored_and_said_once);
  RUN_TEST(test_the_last_second_before_2070_is_a_time);
  RUN_TEST(test_the_same_reading_offered_again_changes_nothing_and_logs_nothing);
  RUN_TEST(test_a_network_time_close_to_the_clock_corrects_a_drift);
  RUN_TEST(test_a_small_step_back_is_taken_but_a_big_one_is_a_jump_back_and_held);
  RUN_TEST(test_a_big_step_forward_is_held_back_too);
  RUN_TEST(test_a_suspect_time_is_taken_when_the_next_reading_agrees_with_it);
  RUN_TEST(test_two_suspects_that_disagree_with_each_other_move_nothing);
  RUN_TEST(test_a_suspect_is_forgotten_when_the_clock_and_the_network_agree_again);
  RUN_TEST(test_the_zone_follows_only_a_time_that_is_taken);
  RUN_TEST(test_an_impossible_zone_offset_is_not_taken_and_does_not_stop_the_time);
  RUN_TEST(test_a_satellite_time_corrects_a_network_time_that_was_wrong);
  RUN_TEST(test_a_satellite_time_within_two_seconds_leaves_the_clock_alone);
  RUN_TEST(test_the_network_does_not_overrule_the_satellites_for_a_day);
  RUN_TEST(test_the_satellites_staying_in_agreement_keep_their_precedence);
  RUN_TEST(test_a_satellite_time_that_agrees_makes_the_clock_rest_on_it);
  RUN_TEST(test_every_change_of_the_clock_is_one_line_in_the_log);
  return UNITY_END();
}

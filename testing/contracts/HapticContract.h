#pragma once

// The contract of Haptic. Rig: `Haptic& haptic()`, `void advance(uint32_t ms)` (time
// passes by at least that much), `std::vector<uint8_t> fired()` (the effect numbers
// fired so far) and `std::vector<uint64_t> firedAt()` (the uptime of each in ms).

#include <vector>

#include <hal/Haptic.h>

#include "Contract.h"

template <class Rig>
struct HapticContract {
  // Runs the service loop for `ms`, in steps of 10 ms like a main loop.
  static void run(Rig& rig, uint32_t ms) {
    for (uint32_t t = 0; t <= ms; t += 10) {
      rig.haptic().service();
      rig.advance(10);
    }
  }

  static void nothing_is_fired_without_a_pattern() {
    Rig rig;
    TEST_ASSERT_FALSE(rig.haptic().busy());
    run(rig, 500);
    TEST_ASSERT_EQUAL(0, rig.fired().size());
  }

  static void play_returns_at_once_and_marks_the_haptic_busy() {
    Rig rig;
    rig.haptic().play(HapticPattern::Message);
    TEST_ASSERT_TRUE(rig.haptic().busy());
    TEST_ASSERT_EQUAL(0, rig.fired().size());  // nothing happens until service() runs
  }

  static void a_click_is_one_pulse_and_then_it_is_not_busy() {
    Rig rig;
    rig.haptic().play(HapticPattern::Click);
    run(rig, 100);
    TEST_ASSERT_EQUAL(1, rig.fired().size());
    TEST_ASSERT_FALSE(rig.haptic().busy());
  }

  static void it_stays_busy_until_the_last_pulse_has_been_fired() {
    Rig rig;
    rig.haptic().play(HapticPattern::Sent);
    rig.haptic().service();
    TEST_ASSERT_EQUAL(1, rig.fired().size());
    TEST_ASSERT_TRUE(rig.haptic().busy());
    run(rig, 400);
    TEST_ASSERT_EQUAL(3, rig.fired().size());
    TEST_ASSERT_FALSE(rig.haptic().busy());
  }

  // The service loop does not fire the pulses of the future early.
  static void a_pulse_is_not_fired_before_its_time() {
    Rig rig;
    rig.haptic().play(HapticPattern::Message);
    rig.haptic().service();
    rig.advance(100);
    rig.haptic().service();
    TEST_ASSERT_EQUAL(1, rig.fired().size());
  }

  static void the_patterns_have_the_pulse_counts_the_child_learns() {
    const struct { HapticPattern pattern; size_t pulses; } cases[] = {
        {HapticPattern::Click, 1}, {HapticPattern::Ring, 1}, {HapticPattern::Message, 2},
        {HapticPattern::Sent, 3}, {HapticPattern::Failed, 2}};
    for (const auto& c : cases) {
      Rig rig;
      rig.haptic().play(c.pattern);
      run(rig, 1000);
      TEST_ASSERT_EQUAL(c.pulses, rig.fired().size());
    }
  }

  static void pulses_of_one_pattern_are_at_least_150_ms_apart() {
    Rig rig;
    rig.haptic().play(HapticPattern::Sent);
    run(rig, 1000);
    const auto at = rig.firedAt();
    TEST_ASSERT_EQUAL(3, at.size());
    TEST_ASSERT_TRUE(at[1] - at[0] >= 150);
    TEST_ASSERT_TRUE(at[2] - at[1] >= 150);
  }

  // The newest pattern is what the child needs to know: the rest of the old one is dropped.
  static void a_pattern_started_while_another_plays_replaces_it() {
    Rig rig;
    rig.haptic().play(HapticPattern::Sent);  // three short
    rig.haptic().service();
    TEST_ASSERT_EQUAL(1, rig.fired().size());
    rig.haptic().play(HapticPattern::Ring);  // one long
    run(rig, 1000);
    const auto effects = rig.fired();
    TEST_ASSERT_EQUAL(2, effects.size());  // the first short pulse, then the long one
    TEST_ASSERT_EQUAL(1, effects[0]);
    TEST_ASSERT_EQUAL(15, effects[1]);
    TEST_ASSERT_FALSE(rig.haptic().busy());
  }

  static void the_same_pattern_can_be_played_again() {
    Rig rig;
    rig.haptic().play(HapticPattern::Message);
    run(rig, 500);
    rig.haptic().play(HapticPattern::Message);
    run(rig, 500);
    TEST_ASSERT_EQUAL(4, rig.fired().size());
  }

  static void failed_ends_in_a_long_pulse_unlike_message() {
    Rig rig;
    rig.haptic().play(HapticPattern::Failed);
    run(rig, 1000);
    const auto effects = rig.fired();
    TEST_ASSERT_EQUAL(2, effects.size());
    TEST_ASSERT_TRUE(effects[0] != effects[1]);
  }
};

#define RUN_HAPTIC_CONTRACT(Rig)                                                          \
  RUN_TEST(HapticContract<Rig>::nothing_is_fired_without_a_pattern);                       \
  RUN_TEST(HapticContract<Rig>::play_returns_at_once_and_marks_the_haptic_busy);           \
  RUN_TEST(HapticContract<Rig>::a_click_is_one_pulse_and_then_it_is_not_busy);             \
  RUN_TEST(HapticContract<Rig>::it_stays_busy_until_the_last_pulse_has_been_fired);        \
  RUN_TEST(HapticContract<Rig>::a_pulse_is_not_fired_before_its_time);                     \
  RUN_TEST(HapticContract<Rig>::the_patterns_have_the_pulse_counts_the_child_learns);      \
  RUN_TEST(HapticContract<Rig>::pulses_of_one_pattern_are_at_least_150_ms_apart);          \
  RUN_TEST(HapticContract<Rig>::a_pattern_started_while_another_plays_replaces_it);        \
  RUN_TEST(HapticContract<Rig>::the_same_pattern_can_be_played_again);                     \
  RUN_TEST(HapticContract<Rig>::failed_ends_in_a_long_pulse_unlike_message)

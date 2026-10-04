// Host tests for the vibration patterns and the non-blocking player.

#include <unity.h>

#include <vector>

#include <hal/HapticPattern.h>

namespace {

// Plays a pattern and returns when which effect was fired.
struct Hit {
  uint32_t at;
  uint8_t effect;
};

std::vector<Hit> run(HapticPattern p, uint32_t until = 5000) {
  HapticPlayer player;
  std::vector<Hit> hits;
  player.start(p, 1000);
  for (uint32_t t = 1000; t < 1000 + until; t += 10) {
    uint8_t e;
    while (player.poll(t, e)) hits.push_back({t - 1000, e});
  }
  return hits;
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_click_is_one_short_pulse() {
  const auto h = run(HapticPattern::Click);
  TEST_ASSERT_EQUAL(1, h.size());
  TEST_ASSERT_EQUAL(1, h[0].effect);
}

void test_message_is_two_short_pulses() {
  const auto h = run(HapticPattern::Message);
  TEST_ASSERT_EQUAL(2, h.size());
  TEST_ASSERT_EQUAL_UINT32(0, h[0].at);
  TEST_ASSERT_EQUAL_UINT32(150, h[1].at);
}

void test_sent_is_three_short_pulses() {
  const auto h = run(HapticPattern::Sent);
  TEST_ASSERT_EQUAL(3, h.size());
  TEST_ASSERT_EQUAL_UINT32(300, h[2].at);
}

void test_ring_is_one_long_pulse() {
  const auto h = run(HapticPattern::Ring);
  TEST_ASSERT_EQUAL(1, h.size());
  TEST_ASSERT_EQUAL(15, h[0].effect);
}

// Short, then long: the pattern must differ from "message" (short, short) and from
// "ring" (long only), or the whole exercise is pointless.
void test_failed_is_short_then_long() {
  const auto h = run(HapticPattern::Failed);
  TEST_ASSERT_EQUAL(2, h.size());
  TEST_ASSERT_EQUAL(1, h[0].effect);
  TEST_ASSERT_EQUAL(15, h[1].effect);
  TEST_ASSERT_EQUAL_UINT32(250, h[1].at);
}

void test_all_patterns_are_distinct() {
  const HapticPattern all[] = {HapticPattern::Click, HapticPattern::Ring, HapticPattern::Message,
                               HapticPattern::Sent, HapticPattern::Failed};
  for (size_t a = 0; a < 5; ++a) {
    for (size_t b = a + 1; b < 5; ++b) {
      const auto x = run(all[a]), y = run(all[b]);
      bool same = x.size() == y.size();
      for (size_t i = 0; same && i < x.size(); ++i) same = x[i].at == y[i].at && x[i].effect == y[i].effect;
      TEST_ASSERT_FALSE(same);
    }
  }
}

// The pauses are long enough that two pulses do not blur into one, and the longest
// pattern is over in well under two seconds.
void test_pauses_are_not_shorter_than_150_ms_and_patterns_are_short() {
  const HapticPattern all[] = {HapticPattern::Click, HapticPattern::Ring, HapticPattern::Message,
                               HapticPattern::Sent, HapticPattern::Failed};
  for (HapticPattern p : all) {
    const HapticSequence seq = hapticSequence(p);
    TEST_ASSERT_TRUE(seq.count >= 1);
    uint32_t total = 0;
    for (uint8_t i = 0; i < seq.count; ++i) {
      const bool last = i + 1 == seq.count;
      if (last) {
        TEST_ASSERT_EQUAL(0, seq.steps[i].afterMs);  // nothing to wait for after the last pulse
      } else {
        TEST_ASSERT_TRUE(seq.steps[i].afterMs >= 150);
      }
      total += seq.steps[i].afterMs;
    }
    TEST_ASSERT_TRUE(total < 2000);
  }
}

void test_busy_until_the_last_pulse_is_out() {
  HapticPlayer p;
  uint8_t e;
  TEST_ASSERT_FALSE(p.busy());
  p.start(HapticPattern::Message, 0);
  TEST_ASSERT_TRUE(p.busy());
  TEST_ASSERT_TRUE(p.poll(0, e));
  TEST_ASSERT_TRUE(p.busy());
  TEST_ASSERT_FALSE(p.poll(100, e));  // too early: the pause is running
  TEST_ASSERT_FALSE(p.poll(149, e));
  TEST_ASSERT_TRUE(p.poll(150, e));
  TEST_ASSERT_FALSE(p.busy());
  TEST_ASSERT_FALSE(p.poll(10000, e));  // nothing more
}

void test_a_player_that_was_never_started_fires_nothing() {
  HapticPlayer p;
  uint8_t e = 99;
  TEST_ASSERT_FALSE(p.poll(0, e));
  TEST_ASSERT_FALSE(p.poll(123456, e));
  TEST_ASSERT_EQUAL(99, e);
}

// A new pattern replaces a running one, and the pulses still to come from the old one are not fired.
void test_a_new_pattern_replaces_the_running_one() {
  HapticPlayer p;
  uint8_t e;
  p.start(HapticPattern::Sent, 0);
  TEST_ASSERT_TRUE(p.poll(0, e));
  p.start(HapticPattern::Ring, 50);
  TEST_ASSERT_TRUE(p.poll(50, e));
  TEST_ASSERT_EQUAL(15, e);
  TEST_ASSERT_FALSE(p.poll(10000, e));
  TEST_ASSERT_FALSE(p.busy());
}

// A pattern that is started again starts from the beginning.
void test_a_pattern_can_be_played_again() {
  HapticPlayer p;
  uint8_t e;
  p.start(HapticPattern::Message, 0);
  p.poll(0, e);
  p.poll(150, e);
  p.start(HapticPattern::Message, 1000);
  TEST_ASSERT_TRUE(p.poll(1000, e));
  TEST_ASSERT_FALSE(p.poll(1100, e));
  TEST_ASSERT_TRUE(p.poll(1150, e));
}

// A late poll fires one step, and the next one is due a pause after *that* moment:
// two pulses are never fired back to back to catch up.
void test_a_late_poll_does_not_fire_pulses_back_to_back() {
  HapticPlayer p;
  uint8_t e;
  p.start(HapticPattern::Message, 0);
  TEST_ASSERT_TRUE(p.poll(1000, e));   // one second late
  TEST_ASSERT_FALSE(p.poll(1000, e));  // the second pulse waits its pause
  TEST_ASSERT_FALSE(p.poll(1149, e));
  TEST_ASSERT_TRUE(p.poll(1150, e));
}

// The counter wraps after 49 days; the pattern must not hang then.
void test_survives_millis_wraparound() {
  HapticPlayer p;
  uint8_t e;
  p.start(HapticPattern::Message, 0xFFFFFFF0u);
  TEST_ASSERT_TRUE(p.poll(0xFFFFFFF0u, e));
  TEST_ASSERT_FALSE(p.poll(0xFFFFFFF5u, e));
  TEST_ASSERT_TRUE(p.poll(0xFFFFFFF0u + 150, e));  // runs over zero
}

void test_survives_millis_wraparound_between_start_and_first_pulse() {
  HapticPlayer p;
  uint8_t e;
  p.start(HapticPattern::Sent, 0xFFFFFFFFu);
  TEST_ASSERT_TRUE(p.poll(0xFFFFFFFFu, e));
  TEST_ASSERT_FALSE(p.poll(10, e));
  TEST_ASSERT_TRUE(p.poll(149, e));  // 150 ms after 0xFFFFFFFF
  TEST_ASSERT_TRUE(p.poll(149 + 150, e));
  TEST_ASSERT_FALSE(p.busy());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_click_is_one_short_pulse);
  RUN_TEST(test_message_is_two_short_pulses);
  RUN_TEST(test_sent_is_three_short_pulses);
  RUN_TEST(test_ring_is_one_long_pulse);
  RUN_TEST(test_failed_is_short_then_long);
  RUN_TEST(test_all_patterns_are_distinct);
  RUN_TEST(test_pauses_are_not_shorter_than_150_ms_and_patterns_are_short);
  RUN_TEST(test_busy_until_the_last_pulse_is_out);
  RUN_TEST(test_a_player_that_was_never_started_fires_nothing);
  RUN_TEST(test_a_new_pattern_replaces_the_running_one);
  RUN_TEST(test_a_pattern_can_be_played_again);
  RUN_TEST(test_a_late_poll_does_not_fire_pulses_back_to_back);
  RUN_TEST(test_survives_millis_wraparound);
  RUN_TEST(test_survives_millis_wraparound_between_start_and_first_pulse);
  return UNITY_END();
}

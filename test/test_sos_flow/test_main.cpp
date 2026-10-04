// Host tests for the SOS escalation flow: what happens between the press and the
// ping. Every transition, the countdown, the five taps, and that nothing is sent on
// cancel. The host records every call in order.

#include <unity.h>

#include <string>
#include <vector>

#include <app/SosFlow.h>

using Kind = SosPing::Kind;
using Phase = SosFlow::Phase;

void setUp() {}
void tearDown() {}

namespace {

struct FakeHost : SosFlow::Host {
  uint64_t now = 1000000;  // milliseconds
  bool active = false;
  bool pingStarts = true;
  std::vector<std::string> calls;  // "search+", "search-", "lift", "ping:<kind>:<target>"
  int searches = 0;                // currently held

  uint64_t uptimeMillis() override { return now; }
  bool pingActive() override { return active; }
  void startSearch() override {
    ++searches;
    calls.push_back("search+");
  }
  void stopSearch() override {
    --searches;
    calls.push_back("search-");
  }
  bool startPing(const std::string& target, Kind kind) override {
    calls.push_back(std::string("ping:") + (kind == Kind::Emergency ? "emergency" : kind == Kind::Pickup ? "pickup" : "query") +
                    ":" + target);
    return pingStarts;
  }
  void liftQuietLock() override { calls.push_back("lift"); }

  int count(const std::string& what) const {
    int n = 0;
    for (const auto& c : calls) n += c == what;
    return n;
  }
  bool pinged() const {
    for (const auto& c : calls) {
      if (c.rfind("ping:", 0) == 0) return true;
    }
    return false;
  }
};

const char* kTarget = "+491701111111";

}  // namespace

// ---------------------------------------------------------------------------
// Outside school time
// ---------------------------------------------------------------------------

// The search starts with the press, not with the choice.
void test_the_press_starts_the_search_and_opens_the_choice() {
  FakeHost h;
  SosFlow f(h);
  TEST_ASSERT_TRUE(f.begin(kTarget, false));
  TEST_ASSERT_TRUE(f.phase() == Phase::Choosing);
  TEST_ASSERT_EQUAL(1, h.searches);
  TEST_ASSERT_EQUAL(1, (int)h.calls.size());
  TEST_ASSERT_EQUAL_STRING("search+", h.calls[0].c_str());
  TEST_ASSERT_EQUAL_STRING(kTarget, f.target().c_str());
  TEST_ASSERT_EQUAL(0, f.tapsLeft());
}

void test_choosing_emergency_starts_the_ping_and_hands_over_the_search() {
  FakeHost h;
  SosFlow f(h);
  f.begin(kTarget, false);
  h.now += 5 * 1000;
  TEST_ASSERT_TRUE(f.choose(Kind::Emergency));
  TEST_ASSERT_TRUE(f.phase() == Phase::Idle);
  // The ping starts first, then the flow lets go of its claim: the receiver is never unpowered between.
  TEST_ASSERT_EQUAL(3, (int)h.calls.size());
  TEST_ASSERT_EQUAL_STRING("ping:emergency:+491701111111", h.calls[1].c_str());
  TEST_ASSERT_EQUAL_STRING("search-", h.calls[2].c_str());
  TEST_ASSERT_EQUAL(0, h.searches);
}

void test_choosing_pickup_starts_a_pickup_ping() {
  FakeHost h;
  SosFlow f(h);
  f.begin(kTarget, false);
  TEST_ASSERT_TRUE(f.choose(Kind::Pickup));
  TEST_ASSERT_EQUAL_STRING("ping:pickup:+491701111111", h.calls[1].c_str());
}

// The location reply is a parent's query and is never on offer to the child.
void test_a_query_cannot_be_chosen() {
  FakeHost h;
  SosFlow f(h);
  f.begin(kTarget, false);
  TEST_ASSERT_FALSE(f.choose(Kind::Query));
  TEST_ASSERT_FALSE(h.pinged());
  TEST_ASSERT_TRUE(f.phase() == Phase::Choosing);
}

// With no choice the emergency goes out by itself after 40 seconds, not before.
void test_the_countdown_sends_the_emergency_at_forty_seconds() {
  FakeHost h;
  SosFlow f(h);
  f.begin(kTarget, false);
  TEST_ASSERT_EQUAL(40u, f.autoLeft());
  h.now += 1 * 1000;
  TEST_ASSERT_EQUAL(39u, f.autoLeft());
  h.now += 38 * 1000;  // 39 s gone
  f.tick();
  TEST_ASSERT_TRUE(f.phase() == Phase::Choosing);
  TEST_ASSERT_FALSE(h.pinged());
  TEST_ASSERT_EQUAL(1u, f.autoLeft());
  h.now += 1 * 1000;  // 40 s
  TEST_ASSERT_EQUAL(0u, f.autoLeft());
  f.tick();
  TEST_ASSERT_TRUE(f.phase() == Phase::Idle);
  TEST_ASSERT_EQUAL(1, h.count("ping:emergency:+491701111111"));
  TEST_ASSERT_EQUAL(0, h.searches);
}

// The call to tick() may come late; the emergency still goes out, and only once.
void test_a_late_tick_still_sends_once() {
  FakeHost h;
  SosFlow f(h);
  f.begin(kTarget, false);
  h.now += 500 * 1000;
  f.tick();
  f.tick();
  h.now += 500 * 1000;
  f.tick();
  TEST_ASSERT_EQUAL(1, h.count("ping:emergency:+491701111111"));
}

void test_a_choice_made_in_time_cancels_the_countdown() {
  FakeHost h;
  SosFlow f(h);
  f.begin(kTarget, false);
  h.now += 10 * 1000;
  f.choose(Kind::Pickup);
  h.now += 100 * 1000;
  f.tick();
  TEST_ASSERT_EQUAL(1, h.count("ping:pickup:+491701111111"));
  TEST_ASSERT_EQUAL(0, h.count("ping:emergency:+491701111111"));
}

// Cancelling stops the search and sends nothing, now or later.
void test_leaving_the_choice_sends_nothing() {
  FakeHost h;
  SosFlow f(h);
  f.begin(kTarget, false);
  h.now += 10 * 1000;
  f.leave();
  TEST_ASSERT_TRUE(f.phase() == Phase::Idle);
  TEST_ASSERT_EQUAL(0, h.searches);
  h.now += 1000 * 1000;
  f.tick();
  TEST_ASSERT_FALSE(h.pinged());
  TEST_ASSERT_EQUAL(0, f.autoLeft());
  TEST_ASSERT_FALSE(f.choose(Kind::Emergency));
  TEST_ASSERT_FALSE(h.pinged());
  TEST_ASSERT_EQUAL(0, h.count("lift"));
}

void test_a_new_press_after_cancelling_starts_a_fresh_countdown() {
  FakeHost h;
  SosFlow f(h);
  f.begin(kTarget, false);
  h.now += 30 * 1000;
  f.leave();
  h.now += 100 * 1000;
  f.begin(kTarget, false);
  TEST_ASSERT_EQUAL(40u, f.autoLeft());
  TEST_ASSERT_EQUAL(2, h.count("search+"));
  TEST_ASSERT_EQUAL(1, h.searches);
}

// ---------------------------------------------------------------------------
// In school time
// ---------------------------------------------------------------------------

void test_school_time_opens_the_warning_with_five_taps_to_go() {
  FakeHost h;
  SosFlow f(h);
  TEST_ASSERT_TRUE(f.begin(kTarget, true));
  TEST_ASSERT_TRUE(f.phase() == Phase::QuietConfirm);
  TEST_ASSERT_EQUAL(5, f.tapsLeft());
  TEST_ASSERT_EQUAL(0u, f.autoLeft());
  TEST_ASSERT_EQUAL(1, h.searches);  // the search starts here too
}

void test_four_taps_do_nothing() {
  FakeHost h;
  SosFlow f(h);
  f.begin(kTarget, true);
  for (int i = 1; i <= 4; ++i) {
    TEST_ASSERT_FALSE(f.tap());
    TEST_ASSERT_EQUAL(5 - i, f.tapsLeft());
  }
  TEST_ASSERT_TRUE(f.phase() == Phase::QuietConfirm);
  TEST_ASSERT_FALSE(h.pinged());
  TEST_ASSERT_EQUAL(0, h.count("lift"));
  TEST_ASSERT_EQUAL(1, h.searches);
}

// The fifth tap sends the emergency and lifts the lock, the lock first.
void test_the_fifth_tap_sends_the_emergency_and_lifts_the_lock() {
  FakeHost h;
  SosFlow f(h);
  f.begin(kTarget, true);
  for (int i = 0; i < 4; ++i) f.tap();
  TEST_ASSERT_TRUE(f.tap());
  TEST_ASSERT_TRUE(f.phase() == Phase::Idle);
  TEST_ASSERT_EQUAL(4, (int)h.calls.size());
  TEST_ASSERT_EQUAL_STRING("search+", h.calls[0].c_str());
  TEST_ASSERT_EQUAL_STRING("lift", h.calls[1].c_str());
  TEST_ASSERT_EQUAL_STRING("ping:emergency:+491701111111", h.calls[2].c_str());
  TEST_ASSERT_EQUAL_STRING("search-", h.calls[3].c_str());
  TEST_ASSERT_EQUAL(0, h.searches);
  TEST_ASSERT_EQUAL(0, f.tapsLeft());
}

// More taps after the last one are no new emergency.
void test_taps_after_the_last_one_are_ignored() {
  FakeHost h;
  SosFlow f(h);
  f.begin(kTarget, true);
  for (int i = 0; i < 5; ++i) f.tap();
  TEST_ASSERT_FALSE(f.tap());
  TEST_ASSERT_FALSE(f.tap());
  TEST_ASSERT_EQUAL(1, h.count("lift"));
  TEST_ASSERT_EQUAL(1, h.count("ping:emergency:+491701111111"));
}

// In school time the press must be deliberate: no countdown, and no pickup.
void test_school_time_has_no_countdown_and_no_choice() {
  FakeHost h;
  SosFlow f(h);
  f.begin(kTarget, true);
  h.now += 10000 * 1000;
  f.tick();
  TEST_ASSERT_TRUE(f.phase() == Phase::QuietConfirm);
  TEST_ASSERT_FALSE(h.pinged());
  TEST_ASSERT_FALSE(f.choose(Kind::Emergency));
  TEST_ASSERT_FALSE(f.choose(Kind::Pickup));
  TEST_ASSERT_FALSE(h.pinged());
}

void test_leaving_the_warning_sends_nothing_and_forgets_the_taps() {
  FakeHost h;
  SosFlow f(h);
  f.begin(kTarget, true);
  for (int i = 0; i < 4; ++i) f.tap();
  f.leave();
  TEST_ASSERT_TRUE(f.phase() == Phase::Idle);
  TEST_ASSERT_FALSE(h.pinged());
  TEST_ASSERT_EQUAL(0, h.count("lift"));
  TEST_ASSERT_EQUAL(0, h.searches);
  TEST_ASSERT_FALSE(f.tap());  // a tap with no warning open
  f.begin(kTarget, true);
  TEST_ASSERT_EQUAL(5, f.tapsLeft());  // the four earlier taps do not count
}

// ---------------------------------------------------------------------------
// The tap window: five taps, each within ten seconds of the one before
// ---------------------------------------------------------------------------

// A deliberate emergency is tapped quickly; a pocket taps now and then.
void test_a_pause_of_ten_seconds_starts_the_count_again() {
  FakeHost h;
  SosFlow f(h);
  f.begin(kTarget, true);
  for (int i = 0; i < 4; ++i) f.tap();
  TEST_ASSERT_EQUAL(1, f.tapsLeft());
  h.now += 10000;  // exactly the window
  TEST_ASSERT_FALSE(f.tap());  // would have been the fifth; it is the first of a new series
  TEST_ASSERT_EQUAL(4, f.tapsLeft());
  TEST_ASSERT_FALSE(h.pinged());
  TEST_ASSERT_EQUAL(0, h.count("lift"));
}

void test_taps_nine_point_nine_seconds_apart_count() {
  FakeHost h;
  SosFlow f(h);
  f.begin(kTarget, true);
  for (int i = 0; i < 4; ++i) {
    TEST_ASSERT_FALSE(f.tap());
    h.now += 9900;
  }
  TEST_ASSERT_TRUE(f.tap());  // 39.6 s after the first tap, still one series
  TEST_ASSERT_EQUAL(1, h.count("lift"));
  TEST_ASSERT_EQUAL(1, h.count("ping:emergency:+491701111111"));
}

void test_the_window_edge_is_between_9999_and_10000_milliseconds() {
  FakeHost h;
  SosFlow f(h);
  f.begin(kTarget, true);
  f.tap();
  h.now += 9999;
  f.tap();
  TEST_ASSERT_EQUAL(3, f.tapsLeft());  // counted
  h.now += 10000;
  f.tap();
  TEST_ASSERT_EQUAL(4, f.tapsLeft());  // a new series
}

// The pause is measured from the last tap, not from the first.
void test_the_window_runs_from_the_last_tap() {
  FakeHost h;
  SosFlow f(h);
  f.begin(kTarget, true);
  for (int i = 0; i < 4; ++i) {
    f.tap();
    h.now += 8000;  // 32 s in all, but never a pause of ten seconds
  }
  TEST_ASSERT_TRUE(f.tap());
}

// The lock is lifted by the fifth tap of a series, and by nothing before.
void test_after_a_pause_five_new_taps_are_needed_and_lift_the_lock_once() {
  FakeHost h;
  SosFlow f(h);
  f.begin(kTarget, true);
  for (int i = 0; i < 4; ++i) f.tap();
  h.now += 10000;
  for (int i = 0; i < 4; ++i) {
    TEST_ASSERT_FALSE(f.tap());
    TEST_ASSERT_EQUAL(0, h.count("lift"));
  }
  TEST_ASSERT_TRUE(f.tap());  // the fifth of the new series
  TEST_ASSERT_EQUAL(1, h.count("lift"));
  TEST_ASSERT_EQUAL(1, h.count("ping:emergency:+491701111111"));
}

// The screen shows five again once the window has passed, without waiting for a tap.
void test_tick_resets_the_displayed_count_after_the_window() {
  FakeHost h;
  SosFlow f(h);
  f.begin(kTarget, true);
  f.tap();
  f.tap();
  f.tap();
  h.now += 9999;
  f.tick();
  TEST_ASSERT_EQUAL(2, f.tapsLeft());
  h.now += 1;
  f.tick();
  TEST_ASSERT_EQUAL(5, f.tapsLeft());
  TEST_ASSERT_TRUE(f.phase() == Phase::QuietConfirm);  // the warning stays open
  TEST_ASSERT_EQUAL(1, h.searches);
}

// Time between the press and the first tap does not matter: the child reads the warning first.
void test_the_first_tap_may_come_late() {
  FakeHost h;
  SosFlow f(h);
  f.begin(kTarget, true);
  h.now += 600000;
  f.tap();
  TEST_ASSERT_EQUAL(4, f.tapsLeft());
}

void test_a_clock_that_goes_back_keeps_the_taps() {
  FakeHost h;
  SosFlow f(h);
  f.begin(kTarget, true);
  f.tap();
  f.tap();
  h.now -= 500000;
  f.tick();
  TEST_ASSERT_EQUAL(3, f.tapsLeft());
  f.tap();
  TEST_ASSERT_EQUAL(2, f.tapsLeft());
}

// The countdown is shown in whole seconds, rounded up, from the millisecond clock.
void test_the_countdown_is_rounded_up_to_whole_seconds() {
  FakeHost h;
  SosFlow f(h);
  f.begin(kTarget, false);
  h.now += 500;
  TEST_ASSERT_EQUAL(40u, f.autoLeft());
  h.now += 500;
  TEST_ASSERT_EQUAL(39u, f.autoLeft());
  h.now += 38400;  // 39.4 s gone
  TEST_ASSERT_EQUAL(1u, f.autoLeft());
  f.tick();
  TEST_ASSERT_FALSE(h.pinged());
  h.now += 600;
  TEST_ASSERT_EQUAL(0u, f.autoLeft());
}

// ---------------------------------------------------------------------------
// Presses that change nothing, and a ping that cannot start
// ---------------------------------------------------------------------------

void test_a_press_while_a_ping_runs_changes_nothing() {
  FakeHost h;
  h.active = true;
  SosFlow f(h);
  TEST_ASSERT_TRUE(f.begin(kTarget, false));
  TEST_ASSERT_TRUE(f.phase() == Phase::Idle);
  TEST_ASSERT_TRUE(h.calls.empty());
}

// An open choice keeps its countdown, and an open warning the taps made so far.
void test_a_second_press_does_not_restart_the_flow() {
  FakeHost h;
  SosFlow f(h);
  f.begin(kTarget, false);
  h.now += 30 * 1000;
  TEST_ASSERT_TRUE(f.begin(kTarget, false));
  TEST_ASSERT_EQUAL(10u, f.autoLeft());
  TEST_ASSERT_EQUAL(1, h.count("search+"));
  f.leave();

  FakeHost g;
  SosFlow q(g);
  q.begin(kTarget, true);
  q.tap();
  q.tap();
  TEST_ASSERT_TRUE(q.begin(kTarget, true));
  TEST_ASSERT_EQUAL(3, q.tapsLeft());
  TEST_ASSERT_EQUAL(1, g.count("search+"));
}

void test_without_a_recipient_nothing_starts() {
  FakeHost h;
  SosFlow f(h);
  TEST_ASSERT_FALSE(f.begin("", false));
  TEST_ASSERT_FALSE(f.begin("", true));
  TEST_ASSERT_TRUE(h.calls.empty());
  TEST_ASSERT_TRUE(f.phase() == Phase::Idle);
}

// If the ping cannot start, the flow still closes and lets go of the receiver; the
// caller learns of it from the return value.
void test_a_ping_that_cannot_start_closes_the_flow_and_releases_the_search() {
  FakeHost h;
  h.pingStarts = false;
  SosFlow f(h);
  f.begin(kTarget, false);
  TEST_ASSERT_FALSE(f.choose(Kind::Emergency));
  TEST_ASSERT_TRUE(f.phase() == Phase::Idle);
  TEST_ASSERT_EQUAL(0, h.searches);

  FakeHost g;
  g.pingStarts = false;
  SosFlow q(g);
  q.begin(kTarget, true);
  for (int i = 0; i < 4; ++i) q.tap();
  TEST_ASSERT_FALSE(q.tap());
  TEST_ASSERT_EQUAL(1, g.count("lift"));  // the lock is lifted: the child asked, and the host decides the rest
  TEST_ASSERT_EQUAL(0, g.searches);
}

// A clock that goes back keeps the countdown at its full length rather than underflowing.
void test_a_clock_that_goes_back_does_not_fire_the_countdown() {
  FakeHost h;
  SosFlow f(h);
  f.begin(kTarget, false);
  h.now -= 500 * 1000;
  TEST_ASSERT_EQUAL(40u, f.autoLeft());
  f.tick();
  TEST_ASSERT_FALSE(h.pinged());
}

// Whatever the order of presses, the search is held exactly while the flow is open,
// and a ping starts only from a choice, a countdown or a fifth tap.
void test_random_sequences_keep_the_search_balanced() {
  uint32_t state = 7;
  auto next = [&state](uint32_t n) {
    state = state * 1664525u + 1013904223u;
    return (state >> 8) % n;
  };
  for (int round = 0; round < 200; ++round) {
    FakeHost h;
    SosFlow f(h);
    for (int step = 0; step < 60; ++step) {
      const bool wasOpen = f.phase() != Phase::Idle;
      const size_t pings = h.calls.size();
      switch (next(7)) {
        case 0: f.begin(kTarget, next(2) == 0); break;
        case 1: f.tick(); break;
        case 2: f.choose(next(2) ? Kind::Emergency : Kind::Pickup); break;
        case 3: f.tap(); break;
        case 4: f.leave(); break;
        case 5: h.now += (next(30)) * 1000; break;
        default: h.now += (next(60)) * 1000; break;
      }
      TEST_ASSERT_EQUAL((f.phase() != Phase::Idle) ? 1 : 0, h.searches);
      for (size_t i = pings; i < h.calls.size(); ++i) {
        if (h.calls[i].rfind("ping:", 0) == 0) TEST_ASSERT_TRUE(wasOpen);  // never a ping from a closed flow
      }
    }
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_the_press_starts_the_search_and_opens_the_choice);
  RUN_TEST(test_choosing_emergency_starts_the_ping_and_hands_over_the_search);
  RUN_TEST(test_choosing_pickup_starts_a_pickup_ping);
  RUN_TEST(test_a_query_cannot_be_chosen);
  RUN_TEST(test_the_countdown_sends_the_emergency_at_forty_seconds);
  RUN_TEST(test_a_late_tick_still_sends_once);
  RUN_TEST(test_a_choice_made_in_time_cancels_the_countdown);
  RUN_TEST(test_leaving_the_choice_sends_nothing);
  RUN_TEST(test_a_new_press_after_cancelling_starts_a_fresh_countdown);
  RUN_TEST(test_school_time_opens_the_warning_with_five_taps_to_go);
  RUN_TEST(test_four_taps_do_nothing);
  RUN_TEST(test_the_fifth_tap_sends_the_emergency_and_lifts_the_lock);
  RUN_TEST(test_taps_after_the_last_one_are_ignored);
  RUN_TEST(test_school_time_has_no_countdown_and_no_choice);
  RUN_TEST(test_leaving_the_warning_sends_nothing_and_forgets_the_taps);
  RUN_TEST(test_a_pause_of_ten_seconds_starts_the_count_again);
  RUN_TEST(test_taps_nine_point_nine_seconds_apart_count);
  RUN_TEST(test_the_window_edge_is_between_9999_and_10000_milliseconds);
  RUN_TEST(test_the_window_runs_from_the_last_tap);
  RUN_TEST(test_after_a_pause_five_new_taps_are_needed_and_lift_the_lock_once);
  RUN_TEST(test_tick_resets_the_displayed_count_after_the_window);
  RUN_TEST(test_the_first_tap_may_come_late);
  RUN_TEST(test_a_clock_that_goes_back_keeps_the_taps);
  RUN_TEST(test_the_countdown_is_rounded_up_to_whole_seconds);
  RUN_TEST(test_a_press_while_a_ping_runs_changes_nothing);
  RUN_TEST(test_a_second_press_does_not_restart_the_flow);
  RUN_TEST(test_without_a_recipient_nothing_starts);
  RUN_TEST(test_a_ping_that_cannot_start_closes_the_flow_and_releases_the_search);
  RUN_TEST(test_a_clock_that_goes_back_does_not_fire_the_countdown);
  RUN_TEST(test_random_sequences_keep_the_search_balanced);
  return UNITY_END();
}

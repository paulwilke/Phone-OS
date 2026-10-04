// Host tests for the refresh policy, the frame tracker and the refresh controller.

#include <unity.h>

#include <RefreshRig.h>
#include <ui/FrameTracker.h>
#include <ui/RefreshController.h>
#include <ui/RefreshPolicy.h>

using namespace ui;
using Action = RefreshPolicy::Action;

void setUp() {}
void tearDown() {}

namespace {
constexpr uint32_t kHour = 3600UL * 1000UL;
}

// ---- RefreshPolicy -------------------------------------------------------------------------

void test_an_unchanged_picture_is_never_refreshed_within_the_static_time() {
  RefreshPolicy policy;
  for (uint32_t t = 0; t < 4 * kHour; t += 1000) TEST_ASSERT_TRUE(policy.decide(false, t) == Action::None);
}

void test_a_changed_picture_is_a_partial_refresh_and_every_tenth_is_full() {
  RefreshPolicy policy;
  int partial = 0, full = 0;
  for (int i = 1; i <= 30; ++i) {
    const Action a = policy.decide(true, 1000 + i);
    if (a == Action::Full) {
      ++full;
      TEST_ASSERT_EQUAL(0, i % 10);   // the 10th, 20th and 30th change
    } else {
      TEST_ASSERT_TRUE(a == Action::Partial);
      ++partial;
    }
  }
  TEST_ASSERT_EQUAL(27, partial);
  TEST_ASSERT_EQUAL(3, full);
}

void test_a_static_picture_is_refreshed_after_the_static_time_and_then_left_alone_again() {
  RefreshPolicy policy;
  TEST_ASSERT_TRUE(policy.decide(false, 4 * kHour - 1) == Action::None);
  TEST_ASSERT_TRUE(policy.decide(false, 4 * kHour) == Action::Full);
  TEST_ASSERT_TRUE(policy.decide(false, 4 * kHour + 1) == Action::None);
  TEST_ASSERT_TRUE(policy.decide(false, 8 * kHour - 1) == Action::None);
  TEST_ASSERT_TRUE(policy.decide(false, 8 * kHour) == Action::Full);
}

void test_a_full_refresh_by_changes_moves_the_static_deadline() {
  RefreshPolicy policy;
  for (int i = 0; i < 10; ++i) policy.decide(true, 3 * kHour);   // the tenth is full, at 3 h
  TEST_ASSERT_TRUE(policy.decide(false, 4 * kHour) == Action::None);          // not 4 h after the start any more
  TEST_ASSERT_TRUE(policy.decide(false, 7 * kHour - 1) == Action::None);
  TEST_ASSERT_TRUE(policy.decide(false, 7 * kHour) == Action::Full);
}

void test_a_partial_refresh_does_not_move_the_static_deadline() {
  RefreshPolicy policy;
  TEST_ASSERT_TRUE(policy.decide(true, 3 * kHour) == Action::Partial);
  TEST_ASSERT_TRUE(policy.decide(false, 4 * kHour) == Action::Full);          // the panel was last fully cleared at 0
}

void test_the_policy_counts_milliseconds_across_the_wrap_of_the_counter() {
  RefreshConfig config;
  config.maxStaticMs = 1000;
  RefreshPolicy policy(config, 0xFFFFFC00u);
  TEST_ASSERT_TRUE(policy.decide(false, 0xFFFFFF00u) == Action::None);   // 768 ms
  TEST_ASSERT_TRUE(policy.decide(false, 0x00000000u) == Action::Full);   // 1024 ms, the counter has wrapped
  TEST_ASSERT_TRUE(policy.decide(false, 0x00000100u) == Action::None);   // 256 ms since
}

void test_every_change_is_full_when_the_limit_is_one_or_zero() {
  for (uint16_t every : {uint16_t(0), uint16_t(1)}) {
    RefreshConfig config;
    config.fullEvery = every;
    RefreshPolicy policy(config);
    for (int i = 0; i < 5; ++i) TEST_ASSERT_TRUE(policy.decide(true, i) == Action::Full);
  }
}

void test_a_static_time_of_zero_turns_the_static_refresh_off() {
  RefreshConfig config;
  config.maxStaticMs = 0;
  RefreshPolicy policy(config);
  TEST_ASSERT_TRUE(policy.decide(false, 0xFFFFFFFFu) == Action::None);
}

void test_a_full_refresh_noted_from_outside_restarts_the_count_and_the_deadline() {
  RefreshPolicy policy;
  for (int i = 0; i < 9; ++i) policy.decide(true, i);
  TEST_ASSERT_EQUAL(9, policy.partialsSinceFull());
  policy.noteFull(2 * kHour);
  TEST_ASSERT_EQUAL(0, policy.partialsSinceFull());
  for (int i = 0; i < 9; ++i) TEST_ASSERT_TRUE(policy.decide(true, 2 * kHour + i) == Action::Partial);
  TEST_ASSERT_TRUE(policy.decide(true, 2 * kHour + 10) == Action::Full);
  TEST_ASSERT_TRUE(policy.decide(false, 6 * kHour - 1) == Action::None);
}

// ---- FrameTracker --------------------------------------------------------------------------

void test_the_first_frame_is_a_change_and_an_equal_one_after_it_is_not() {
  FrameTracker tracker;
  TEST_ASSERT_TRUE(tracker.update(0));          // even a checksum of 0: nothing was known before
  TEST_ASSERT_FALSE(tracker.update(0));
  TEST_ASSERT_TRUE(tracker.update(1));
  TEST_ASSERT_FALSE(tracker.update(1));
  TEST_ASSERT_TRUE(tracker.update(0));          // back to an earlier picture is a change too
}

void test_the_tracker_counts_frames_and_changes() {
  FrameTracker tracker;
  tracker.update(5);
  tracker.update(5);
  tracker.update(6);
  TEST_ASSERT_EQUAL(3, tracker.frames());
  TEST_ASSERT_EQUAL(2, tracker.changes());
}

// ---- RefreshController ---------------------------------------------------------------------

namespace {
struct CountingPanel : Panel {
  int partial = 0, full = 0;
  void refreshPartial() override { ++partial; }
  void refreshFull() override { ++full; }
};
}  // namespace

void test_the_first_frame_is_a_full_refresh_because_the_panel_content_is_unknown() {
  CountingPanel panel;
  RefreshController controller(panel);
  TEST_ASSERT_TRUE(controller.present(42, 0) == Action::Full);
  TEST_ASSERT_EQUAL(1, panel.full);
  TEST_ASSERT_EQUAL(0, panel.partial);
}

void test_the_controller_tells_the_panel_exactly_what_the_policy_decided() {
  CountingPanel panel;
  RefreshController controller(panel);
  controller.present(1, 0);                                  // boot: full
  TEST_ASSERT_TRUE(controller.present(1, 1000) == Action::None);
  TEST_ASSERT_TRUE(controller.present(2, 2000) == Action::Partial);
  TEST_ASSERT_TRUE(controller.present(2, 3000) == Action::None);
  TEST_ASSERT_EQUAL(1, panel.partial);
  TEST_ASSERT_EQUAL(1, panel.full);
  const RefreshController::Stats& s = controller.stats();
  TEST_ASSERT_EQUAL(4, s.frames);
  TEST_ASSERT_EQUAL(2, s.skipped);
  TEST_ASSERT_EQUAL(1, s.partial);
  TEST_ASSERT_EQUAL(1, s.full);
  TEST_ASSERT_EQUAL(4, controller.tracker().frames());
}

void test_the_ninth_partial_after_the_boot_refresh_is_followed_by_a_full_one() {
  CountingPanel panel;
  RefreshController controller(panel);
  controller.present(0, 0);                                   // boot, full: counts from here
  for (uint32_t i = 1; i <= 9; ++i) TEST_ASSERT_TRUE(controller.present(i, i) == Action::Partial);
  TEST_ASSERT_TRUE(controller.present(10, 10) == Action::Full);
}

void test_invalidate_makes_the_next_frame_a_full_refresh_even_if_it_is_unchanged() {
  CountingPanel panel;
  RefreshController controller(panel);
  controller.present(7, 0);
  TEST_ASSERT_TRUE(controller.present(7, 1) == Action::None);
  controller.invalidate();                                   // the panel was powered off
  TEST_ASSERT_TRUE(controller.present(7, 2) == Action::Full);
  TEST_ASSERT_TRUE(controller.present(7, 3) == Action::None);
  TEST_ASSERT_TRUE(controller.present(8, 4) == Action::Partial);
}

void test_invalidate_restarts_the_count_of_partial_refreshes() {
  CountingPanel panel;
  RefreshController controller(panel);
  controller.present(0, 0);
  for (uint32_t i = 1; i <= 5; ++i) controller.present(i, i);       // five partials
  controller.invalidate();
  controller.present(5, 10);                                        // full
  for (uint32_t i = 6; i <= 14; ++i) TEST_ASSERT_TRUE(controller.present(i, i + 10) == Action::Partial);   // nine more
  TEST_ASSERT_TRUE(controller.present(15, 30) == Action::Full);
}

void test_the_rig_reports_a_budget_overrun_with_the_history() {
  RefreshRig rig;
  rig.frame(0, [](Display& d) { d.fillRect({0, 0, 10, 10}, true); });
  rig.frame(1000, [](Display& d) { d.fillRect({0, 0, 11, 10}, true); });
  rig.expect("two frames", 1, 1);
  TEST_ASSERT_TRUE(rig.history().find("frame 1 at 1000 ms: partial") != std::string::npos);
  rig.mark();
  rig.frame(2000, [](Display& d) { d.fillRect({0, 0, 11, 10}, true); });
  rig.expect("nothing since the mark", 0, 0);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_an_unchanged_picture_is_never_refreshed_within_the_static_time);
  RUN_TEST(test_a_changed_picture_is_a_partial_refresh_and_every_tenth_is_full);
  RUN_TEST(test_a_static_picture_is_refreshed_after_the_static_time_and_then_left_alone_again);
  RUN_TEST(test_a_full_refresh_by_changes_moves_the_static_deadline);
  RUN_TEST(test_a_partial_refresh_does_not_move_the_static_deadline);
  RUN_TEST(test_the_policy_counts_milliseconds_across_the_wrap_of_the_counter);
  RUN_TEST(test_every_change_is_full_when_the_limit_is_one_or_zero);
  RUN_TEST(test_a_static_time_of_zero_turns_the_static_refresh_off);
  RUN_TEST(test_a_full_refresh_noted_from_outside_restarts_the_count_and_the_deadline);
  RUN_TEST(test_the_first_frame_is_a_change_and_an_equal_one_after_it_is_not);
  RUN_TEST(test_the_tracker_counts_frames_and_changes);
  RUN_TEST(test_the_first_frame_is_a_full_refresh_because_the_panel_content_is_unknown);
  RUN_TEST(test_the_controller_tells_the_panel_exactly_what_the_policy_decided);
  RUN_TEST(test_the_ninth_partial_after_the_boot_refresh_is_followed_by_a_full_one);
  RUN_TEST(test_invalidate_makes_the_next_frame_a_full_refresh_even_if_it_is_unchanged);
  RUN_TEST(test_invalidate_restarts_the_count_of_partial_refreshes);
  RUN_TEST(test_the_rig_reports_a_budget_overrun_with_the_history);
  return UNITY_END();
}

// The loop statistics: the longest pass, the sleep share, the half-hour window.

#include <unity.h>

#include <board/LoopStats.h>

using namespace board;

void setUp() {}
void tearDown() {}

void test_nothing_is_due_before_ten_minutes_and_before_the_first_pass() {
  LoopStats s;
  TEST_ASSERT_FALSE(s.due(1000000));                      // no pass yet
  s.pass(1000, 5, 0);
  TEST_ASSERT_FALSE(s.due(1000 + kLoopStatsIntervalMs - 1));
  TEST_ASSERT_TRUE(s.due(1000 + kLoopStatsIntervalMs));
}

void test_the_line_has_the_longest_pass_the_count_and_the_sleep_share() {
  LoopStats s;
  uint32_t now = 0;
  for (int i = 0; i < 4; ++i) {                            // four passes: work 10, 11320, 3, 4 ms
    const uint32_t work[] = {10, 11320, 3, 4};
    s.pass(now, work[i], 0);
  }
  s.pass(now + 100, 5, 60000 * 9);                         // and sleep for nine minutes of the thirty
  const std::string line = s.takeLine(kLoopStatsIntervalMs);
  TEST_ASSERT_EQUAL_STRING("Loop: longest pass 11320 ms, 5 passes in 1800 s, asleep 30 %", line.c_str());
}

void test_a_new_window_starts_after_the_line() {
  LoopStats s;
  s.pass(0, 900, 100);
  s.takeLine(kLoopStatsIntervalMs);
  TEST_ASSERT_FALSE(s.due(kLoopStatsIntervalMs + 1));
  s.pass(kLoopStatsIntervalMs + 5, 7, 0);
  const std::string line = s.takeLine(2 * kLoopStatsIntervalMs);
  TEST_ASSERT_EQUAL_STRING("Loop: longest pass 7 ms, 1 passes in 1800 s, asleep 0 %", line.c_str());
}

void test_the_sleep_share_never_passes_100_and_an_empty_window_is_safe() {
  LoopStats s;
  s.pass(0, 1, kLoopStatsIntervalMs * 2);                  // more sleep than the window holds (a coarse clock)
  TEST_ASSERT_TRUE(s.takeLine(kLoopStatsIntervalMs).find("asleep 100 %") != std::string::npos);
  LoopStats empty;
  empty.pass(500, 1, 0);
  TEST_ASSERT_TRUE(empty.takeLine(500).find("asleep 0 %") != std::string::npos);   // a window of no length
}

void test_the_millisecond_counter_wrapping_is_no_problem() {
  LoopStats s;
  const uint32_t start = 0xFFFFFFFFu - 1000u;
  s.pass(start, 3, 0);
  TEST_ASSERT_FALSE(s.due(start + kLoopStatsIntervalMs - 1));
  TEST_ASSERT_TRUE(s.due(start + kLoopStatsIntervalMs));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_nothing_is_due_before_ten_minutes_and_before_the_first_pass);
  RUN_TEST(test_the_line_has_the_longest_pass_the_count_and_the_sleep_share);
  RUN_TEST(test_a_new_window_starts_after_the_line);
  RUN_TEST(test_the_sleep_share_never_passes_100_and_an_empty_window_is_safe);
  RUN_TEST(test_the_millisecond_counter_wrapping_is_no_problem);
  return UNITY_END();
}

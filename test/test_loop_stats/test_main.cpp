// The loop statistics: the longest pass, the sleep share, the share of passes that may sleep, where the work goes,
// and the half-hour window.

#include <unity.h>

#include <board/LoopStats.h>

using namespace board;

void setUp() {}
void tearDown() {}

void test_nothing_is_due_before_the_interval_and_before_the_first_pass() {
  LoopStats s;
  TEST_ASSERT_FALSE(s.due(1000000));                      // no pass yet
  s.pass(1000, 5000, 0, false);
  TEST_ASSERT_FALSE(s.due(1000 + kLoopStatsIntervalMs - 1));
  TEST_ASSERT_TRUE(s.due(1000 + kLoopStatsIntervalMs));
}

void test_the_line_has_the_longest_pass_the_count_and_the_sleep_shares() {
  LoopStats s;
  const uint32_t work[] = {10000, 11320000, 3000, 4000};   // four passes: 10, 11320, 3 and 4 ms
  for (uint32_t us : work) s.pass(0, us, 0, false);
  s.pass(100, 5000, 60000 * 9, true);                      // and a pass that sleeps for nine minutes of the thirty
  const std::string line = s.takeLine(kLoopStatsIntervalMs);
  TEST_ASSERT_EQUAL_STRING("Loop: longest pass 11320 ms, 5 passes in 1800 s, asleep 30 %, sleep allowed in 20 % of the passes", line.c_str());
}

void test_where_the_work_goes_is_told_per_pass_in_tenths_of_a_millisecond() {
  LoopStats s;
  for (int i = 0; i < 4; ++i) {
    s.section(LoopStats::Section::Console, 100);           // 0.1 ms
    s.section(LoopStats::Section::Keys, 1200);             // 1.2 ms
    s.section(LoopStats::Section::Modem, 400);             // 0.4 ms
    s.section(LoopStats::Section::Step, 50100);            // 50.1 ms
    s.section(LoopStats::Section::Housekeeping, 50);       // 0.05 ms: rounds to 0.1
    s.pass(0, 52000, 0, false);                            // the rest of the 52 ms is "other": 0.15 ms, which rounds to 0.2
  }
  const std::string line = s.takeLine(kLoopStatsIntervalMs);
  TEST_ASSERT_TRUE_MESSAGE(line.find("; per pass (ms) console 0.1, keys 1.2, modem 0.4, step 50.1, housekeeping 0.1, other 0.2") != std::string::npos,
                           line.c_str());
}

// A wake line that is low before the sleep ends it at once (D-33): the line counts the sleeps, what ended them and
// which lines were low before, so that a night says which line holds the processor awake.
void test_the_sleeps_say_what_ended_them_and_which_lines_were_low_before() {
  LoopStats s;
  s.pass(0, 5000, 0, true);
  for (int i = 0; i < 3; ++i) s.sleep(LoopStats::lineBit(LoopStats::WakeLine::Ring), LoopStats::WakeCause::Line);
  s.sleep(0, LoopStats::WakeCause::Timer);
  s.sleep(LoopStats::lineBit(LoopStats::WakeLine::Keyboard) | LoopStats::lineBit(LoopStats::WakeLine::Touch), LoopStats::WakeCause::Other);
  const std::string line = s.takeLine(kLoopStatsIntervalMs);
  TEST_ASSERT_TRUE_MESSAGE(line.find("; sleep: 5 times, ended by the timer 1, by a line 3, other 1; low before sleep: keyboard 1, touch 1, ring 3") != std::string::npos,
                           line.c_str());
  s.pass(kLoopStatsIntervalMs, 5000, 0, false);
  TEST_ASSERT_TRUE(s.takeLine(2 * kLoopStatsIntervalMs).find("sleep:") == std::string::npos);   // a new window, no sleep in it
}

void test_a_window_without_sections_has_no_per_pass_part() {
  LoopStats s;
  s.pass(0, 6000, 0, false);
  TEST_ASSERT_TRUE(s.takeLine(kLoopStatsIntervalMs).find("per pass") == std::string::npos);
}

void test_a_new_window_starts_after_the_line() {
  LoopStats s;
  s.pass(0, 900000, 100, true);
  s.takeLine(kLoopStatsIntervalMs);
  TEST_ASSERT_FALSE(s.due(kLoopStatsIntervalMs + 1));
  s.pass(kLoopStatsIntervalMs + 5, 7000, 0, false);
  const std::string line = s.takeLine(2 * kLoopStatsIntervalMs);
  TEST_ASSERT_EQUAL_STRING("Loop: longest pass 7 ms, 1 passes in 1800 s, asleep 0 %, sleep allowed in 0 % of the passes", line.c_str());
}

void test_the_sleep_share_never_passes_100_and_an_empty_window_is_safe() {
  LoopStats s;
  s.pass(0, 1000, kLoopStatsIntervalMs * 2, false);        // more sleep than the window holds (a coarse clock)
  TEST_ASSERT_TRUE(s.takeLine(kLoopStatsIntervalMs).find("asleep 100 %") != std::string::npos);
  LoopStats empty;
  empty.pass(500, 1000, 0, false);
  TEST_ASSERT_TRUE(empty.takeLine(500).find("asleep 0 %") != std::string::npos);   // a window of no length
}

void test_the_millisecond_counter_wrapping_is_no_problem() {
  LoopStats s;
  const uint32_t start = 0xFFFFFFFFu - 1000u;
  s.pass(start, 3000, 0, false);
  TEST_ASSERT_FALSE(s.due(start + kLoopStatsIntervalMs - 1));
  TEST_ASSERT_TRUE(s.due(start + kLoopStatsIntervalMs));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_nothing_is_due_before_the_interval_and_before_the_first_pass);
  RUN_TEST(test_the_line_has_the_longest_pass_the_count_and_the_sleep_shares);
  RUN_TEST(test_where_the_work_goes_is_told_per_pass_in_tenths_of_a_millisecond);
  RUN_TEST(test_the_sleeps_say_what_ended_them_and_which_lines_were_low_before);
  RUN_TEST(test_a_window_without_sections_has_no_per_pass_part);
  RUN_TEST(test_a_new_window_starts_after_the_line);
  RUN_TEST(test_the_sleep_share_never_passes_100_and_an_empty_window_is_safe);
  RUN_TEST(test_the_millisecond_counter_wrapping_is_no_problem);
  return UNITY_END();
}

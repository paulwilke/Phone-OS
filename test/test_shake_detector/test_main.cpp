// Host tests for the shake detector.

#include <unity.h>

#include <cmath>
#include <limits>

#include <app/ShakeDetector.h>

void setUp() {}
void tearDown() {}

// Lying still and tilting slowly never count: the magnitude stays near 1 g.
void test_resting_and_tilting_never_count() {
  ShakeDetector s;
  for (int i = 0; i < 100; ++i) s.feed(0.0f, 0.0f, 1.0f);  // flat
  for (int i = 0; i <= 90; ++i) {                          // tilted slowly by 90 degrees
    const float a = (float)i * 3.14159f / 180.0f;
    s.feed(std::sin(a), 0.0f, std::cos(a));
  }
  TEST_ASSERT_EQUAL(0u, s.count());
}

// Each burst counts once, however many samples it lasts.
void test_each_burst_counts_once() {
  ShakeDetector s;
  TEST_ASSERT_TRUE(s.feed(2.5f, 0.0f, 1.0f));    // begins
  TEST_ASSERT_FALSE(s.feed(2.7f, 0.0f, 1.0f));   // the same movement
  TEST_ASSERT_FALSE(s.feed(1.5f, 0.0f, 1.0f));   // fading, but not yet below the rest threshold
  TEST_ASSERT_FALSE(s.feed(2.5f, 0.0f, 1.0f));   // no new count: never at rest in between
  s.feed(0.0f, 0.0f, 1.0f);                      // at rest
  TEST_ASSERT_TRUE(s.feed(0.0f, 3.0f, 1.0f));    // second shake
  TEST_ASSERT_EQUAL(2u, s.count());
}

void test_reset_clears_the_count_and_the_burst() {
  ShakeDetector s;
  s.feed(3.0f, 0, 0);
  s.reset();
  TEST_ASSERT_EQUAL(0u, s.count());
  TEST_ASSERT_TRUE(s.feed(3.0f, 0, 0));  // a burst under way does not carry over
}

// The thresholds are on the magnitude, not on one axis, so the direction of the
// shake does not matter.
void test_any_axis_and_either_sign_counts() {
  const float samples[][3] = {{3, 0, 0}, {-3, 0, 0}, {0, 3, 0}, {0, -3, 0}, {0, 0, 3}, {0, 0, -3},
                              {1.2f, 1.2f, 1.2f}};
  for (const auto& v : samples) {
    ShakeDetector s;
    TEST_ASSERT_TRUE(s.feed(v[0], v[1], v[2]));
  }
}

void test_thresholds_are_inclusive() {
  ShakeDetector s;
  TEST_ASSERT_FALSE(s.feed(1.79f, 0, 0));
  TEST_ASSERT_TRUE(s.feed(1.8f, 0, 0));      // exactly the start threshold counts
  TEST_ASSERT_FALSE(s.feed(1.31f, 0, 0));    // still above the rest threshold
  TEST_ASSERT_FALSE(s.feed(1.8f, 0, 0));
  TEST_ASSERT_FALSE(s.feed(1.3f, 0, 0));     // exactly the rest threshold ends the burst
  TEST_ASSERT_TRUE(s.feed(1.8f, 0, 0));
  TEST_ASSERT_EQUAL(2u, s.count());
}

void test_custom_thresholds() {
  ShakeDetector::Config cfg;
  cfg.highG = 2.5f;
  cfg.lowG = 1.5f;
  ShakeDetector s(cfg);
  TEST_ASSERT_FALSE(s.feed(2.0f, 0, 0));
  TEST_ASSERT_TRUE(s.feed(2.6f, 0, 0));
}

// A rest threshold above the start threshold would end a burst after every sample
// and count one long shake again and again.
void test_an_inverted_configuration_still_counts_a_burst_once() {
  ShakeDetector::Config cfg;
  cfg.highG = 1.5f;
  cfg.lowG = 2.0f;
  ShakeDetector s(cfg);
  TEST_ASSERT_TRUE(s.feed(2.0f, 0, 0));
  TEST_ASSERT_FALSE(s.feed(2.0f, 0, 0));
  TEST_ASSERT_FALSE(s.feed(2.0f, 0, 0));
  TEST_ASSERT_EQUAL(1u, s.count());
  s.feed(1.0f, 0, 0);
  TEST_ASSERT_TRUE(s.feed(2.0f, 0, 0));
}

// A sample without a value (a failed read) is neither a shake nor the end of one.
void test_a_missing_value_changes_nothing() {
  const float nan = std::numeric_limits<float>::quiet_NaN();
  ShakeDetector s;
  TEST_ASSERT_FALSE(s.feed(nan, 0, 0));
  TEST_ASSERT_EQUAL(0u, s.count());
  TEST_ASSERT_TRUE(s.feed(3.0f, 0, 0));
  TEST_ASSERT_FALSE(s.feed(nan, nan, nan));
  TEST_ASSERT_FALSE(s.feed(3.0f, 0, 0));  // the burst is still under way
  TEST_ASSERT_EQUAL(1u, s.count());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_resting_and_tilting_never_count);
  RUN_TEST(test_each_burst_counts_once);
  RUN_TEST(test_reset_clears_the_count_and_the_burst);
  RUN_TEST(test_any_axis_and_either_sign_counts);
  RUN_TEST(test_thresholds_are_inclusive);
  RUN_TEST(test_custom_thresholds);
  RUN_TEST(test_an_inverted_configuration_still_counts_a_burst_once);
  RUN_TEST(test_a_missing_value_changes_nothing);
  return UNITY_END();
}

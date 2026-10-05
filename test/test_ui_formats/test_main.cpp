// Host tests for the number and age formats of the instrument pages.
// allow-non-english: the catalogs under test contain German and French.

#include <unity.h>

#include <cmath>

#include <Languages.h>
#include <ui/Formats.h>

using namespace ui;

void setUp() {}
void tearDown() {}

void test_a_coordinate_has_five_decimals_and_zeros_are_kept() {
  TEST_ASSERT_EQUAL_STRING("48.13745", format::degrees(48.13745).c_str());
  TEST_ASSERT_EQUAL_STRING("11.57550", format::degrees(11.5755).c_str());
  TEST_ASSERT_EQUAL_STRING("0.00000", format::degrees(0).c_str());
  TEST_ASSERT_EQUAL_STRING("180.00000", format::degrees(180).c_str());
  TEST_ASSERT_EQUAL_STRING("0.00100", format::degrees(0.001).c_str());       // leading zeros of the fraction
}

void test_a_negative_coordinate_keeps_its_sign() {
  TEST_ASSERT_EQUAL_STRING("-11.57550", format::degrees(-11.5755).c_str());
  TEST_ASSERT_EQUAL_STRING("-122.41942", format::degrees(-122.419416).c_str());
}

void test_rounding_carries_into_the_whole_part_and_zero_has_no_sign() {
  TEST_ASSERT_EQUAL_STRING("1.00000", format::degrees(0.999996).c_str());
  TEST_ASSERT_EQUAL_STRING("0.00000", format::degrees(-0.000004).c_str());    // not "-0.00000"
  TEST_ASSERT_EQUAL_STRING("-0.00001", format::degrees(-0.000006).c_str());
}

void test_a_number_that_is_none_is_two_dashes() {
  TEST_ASSERT_EQUAL_STRING("--", format::degrees(std::nan("")).c_str());
  TEST_ASSERT_EQUAL_STRING("--", format::degrees(INFINITY).c_str());
}

void test_tenths_have_one_decimal() {
  TEST_ASSERT_EQUAL_STRING("1.2", format::tenths(1.2f).c_str());
  TEST_ASSERT_EQUAL_STRING("1.0", format::tenths(1.04f).c_str());
  TEST_ASSERT_EQUAL_STRING("1.1", format::tenths(1.06f).c_str());
  TEST_ASSERT_EQUAL_STRING("99.0", format::tenths(99.0f).c_str());
  TEST_ASSERT_EQUAL_STRING("0.0", format::tenths(0).c_str());
  TEST_ASSERT_EQUAL_STRING("0.0", format::tenths(-0.04f).c_str());
  TEST_ASSERT_EQUAL_STRING("-1.3", format::tenths(-1.26f).c_str());
}

void test_an_age_is_told_in_the_largest_fitting_unit() {
  i18n::Translator t = languages::loaded();
  TEST_ASSERT_TRUE(t.select("en"));
  TEST_ASSERT_EQUAL_STRING("just now", format::age(0, t).c_str());
  TEST_ASSERT_EQUAL_STRING("just now", format::age(89, t).c_str());
  TEST_ASSERT_EQUAL_STRING("1 min ago", format::age(90, t).c_str());
  TEST_ASSERT_EQUAL_STRING("59 min ago", format::age(3599, t).c_str());
  TEST_ASSERT_EQUAL_STRING("1 h ago", format::age(3600, t).c_str());
  TEST_ASSERT_EQUAL_STRING("23 h ago", format::age(86399, t).c_str());
  TEST_ASSERT_EQUAL_STRING("1 day ago", format::age(86400, t).c_str());
  TEST_ASSERT_EQUAL_STRING("2 days ago", format::age(2 * 86400, t).c_str());
}

void test_an_age_in_german_and_french() {
  i18n::Translator t = languages::loaded();
  TEST_ASSERT_TRUE(t.select("de"));
  TEST_ASSERT_EQUAL_STRING("vor 5 Min.", format::age(300, t).c_str());
  TEST_ASSERT_EQUAL_STRING("vor 1 Tag", format::age(86400, t).c_str());
  TEST_ASSERT_EQUAL_STRING("vor 3 Tagen", format::age(3 * 86400, t).c_str());
  TEST_ASSERT_TRUE(t.select("fr"));
  TEST_ASSERT_EQUAL_STRING("il y a 5 min", format::age(300, t).c_str());
  TEST_ASSERT_EQUAL_STRING("il y a 3 jours", format::age(3 * 86400, t).c_str());
}

void test_an_age_between_two_times_and_when_one_is_unknown() {
  i18n::Translator t = languages::loaded();
  TEST_ASSERT_TRUE(t.select("en"));
  TEST_ASSERT_EQUAL_STRING("3 min ago", format::ageSince(1000, 1180, t).c_str());
  TEST_ASSERT_EQUAL_STRING("heard", format::ageSince(0, 1180, t).c_str());      // never reported a time
  TEST_ASSERT_EQUAL_STRING("heard", format::ageSince(1000, 0, t).c_str());      // our own time unknown
  TEST_ASSERT_EQUAL_STRING("heard", format::ageSince(2000, 1000, t).c_str());   // from the future: the clocks disagree
  TEST_ASSERT_TRUE(t.select("de"));
  TEST_ASSERT_EQUAL_STRING("geh\xC3\xB6rt", format::ageUnknown(t).c_str());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_a_coordinate_has_five_decimals_and_zeros_are_kept);
  RUN_TEST(test_a_negative_coordinate_keeps_its_sign);
  RUN_TEST(test_rounding_carries_into_the_whole_part_and_zero_has_no_sign);
  RUN_TEST(test_a_number_that_is_none_is_two_dashes);
  RUN_TEST(test_tenths_have_one_decimal);
  RUN_TEST(test_an_age_is_told_in_the_largest_fitting_unit);
  RUN_TEST(test_an_age_in_german_and_french);
  RUN_TEST(test_an_age_between_two_times_and_when_one_is_unknown);
  return UNITY_END();
}

// Host tests for the operator label. allow-non-english: one name has an umlaut.
//
// The previous code base had no tests for this module; the cases come from the
// shapes of names networks report.

#include <unity.h>

#include <cstring>
#include <string>

#include <app/OperatorLabel.h>

void setUp() {}
void tearDown() {}

namespace {

std::string label(const char* name, size_t size = 32) {
  char out[64];
  oplabel::shortName(name, out, size < sizeof(out) ? size : sizeof(out));
  return out;
}

}  // namespace

void test_plain_names_stay() {
  TEST_ASSERT_EQUAL_STRING("Provider", label("Provider").c_str());
  TEST_ASSERT_EQUAL_STRING("O2", label("O2").c_str());
  TEST_ASSERT_EQUAL_STRING("3", label("3").c_str());
}

void test_country_suffix_and_company_form_are_dropped() {
  TEST_ASSERT_EQUAL_STRING("Provider", label("Provider.de").c_str());
  TEST_ASSERT_EQUAL_STRING("Bouygues", label("Bouygues Telecom").c_str());
  TEST_ASSERT_EQUAL_STRING("Orange", label("Orange F").c_str());
}

void test_country_prefix_is_dropped() {
  TEST_ASSERT_EQUAL_STRING("Bouygues", label("F-Bouygues Telecom").c_str());
  TEST_ASSERT_EQUAL_STRING("Operator", label("D-Operator").c_str());
  TEST_ASSERT_EQUAL_STRING("KPN", label("NL-KPN").c_str());
}

// The previous code removed the "T-" of "T-Mobile" and showed "Mobile".
void test_a_brand_with_a_letter_prefix_keeps_it() {
  TEST_ASSERT_EQUAL_STRING("T-Mobile", label("T-Mobile").c_str());
  TEST_ASSERT_EQUAL_STRING("T-Mobile", label("T-Mobile.de").c_str());
  TEST_ASSERT_EQUAL_STRING("E-Plus", label("E-Plus").c_str());
}

void test_names_in_capitals_are_capitalised() {
  TEST_ASSERT_EQUAL_STRING("Operator", label("OPERATOR.DE").c_str());
  TEST_ASSERT_EQUAL_STRING("Operator", label("OPERATOR").c_str());
  // Short names are acronyms and stay in capitals.
  TEST_ASSERT_EQUAL_STRING("SFR", label("SFR").c_str());
  TEST_ASSERT_EQUAL_STRING("EE", label("EE").c_str());
  // Mixed case is left alone.
  TEST_ASSERT_EQUAL_STRING("iPhoneNet", label("iPhoneNet").c_str());
  TEST_ASSERT_EQUAL_STRING("Net1", label("NET1").c_str());
  // Digits are not letters: a network that reports its numeric code gets no change.
  TEST_ASSERT_EQUAL_STRING("26201", label("26201").c_str());
}

void test_long_names_are_cut_at_ten_characters() {
  TEST_ASSERT_EQUAL_STRING("Abcdefghij", label("Abcdefghijklmnop").c_str());
  TEST_ASSERT_EQUAL_STRING("Abcdefghij", label("Abcdefghij").c_str());
}

// The cut counts characters and never splits one: "Telefónica" has ten.
void test_the_cut_does_not_split_a_character() {
  TEST_ASSERT_EQUAL_STRING("Telef\xC3\xB3nica", label("Telef\xC3\xB3nica").c_str());
  TEST_ASSERT_EQUAL_STRING("Telef\xC3\xB3nica", label("Telef\xC3\xB3nica-Moviles").c_str());
  // A small buffer cuts on a boundary as well.
  TEST_ASSERT_EQUAL_STRING("Telef", label("Telef\xC3\xB3nica", 7).c_str());
  TEST_ASSERT_EQUAL_STRING("Telef\xC3\xB3", label("Telef\xC3\xB3nica", 8).c_str());
  TEST_ASSERT_EQUAL_STRING("\xC3\x96sterreich", label("\xC3\x96STERREICH").c_str());
}

void test_leading_blanks_are_ignored() {
  TEST_ASSERT_EQUAL_STRING("Provider", label("  Provider").c_str());
  TEST_ASSERT_EQUAL_STRING("Bouygues", label(" F-Bouygues").c_str());
}

void test_empty_and_degenerate_input() {
  TEST_ASSERT_EQUAL_STRING("", label(nullptr).c_str());
  TEST_ASSERT_EQUAL_STRING("", label("").c_str());
  TEST_ASSERT_EQUAL_STRING("", label("   ").c_str());
  TEST_ASSERT_EQUAL_STRING("", label(".de").c_str());
  TEST_ASSERT_EQUAL_STRING("", label("F-").c_str());
  char none[1] = {'x'};
  oplabel::shortName("Provider", none, 0);
  TEST_ASSERT_EQUAL_CHAR('x', none[0]);
  oplabel::shortName("Provider", nullptr, 4);
  char tiny[4];
  oplabel::shortName("Provider", tiny, sizeof(tiny));
  TEST_ASSERT_EQUAL_STRING("Pro", tiny);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_plain_names_stay);
  RUN_TEST(test_country_suffix_and_company_form_are_dropped);
  RUN_TEST(test_country_prefix_is_dropped);
  RUN_TEST(test_a_brand_with_a_letter_prefix_keeps_it);
  RUN_TEST(test_names_in_capitals_are_capitalised);
  RUN_TEST(test_long_names_are_cut_at_ten_characters);
  RUN_TEST(test_the_cut_does_not_split_a_character);
  RUN_TEST(test_leading_blanks_are_ignored);
  RUN_TEST(test_empty_and_degenerate_input);
  return UNITY_END();
}

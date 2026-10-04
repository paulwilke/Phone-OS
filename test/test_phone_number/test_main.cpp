// Host tests for phone number normalisation and comparison.

#include <unity.h>

#include <cstring>
#include <string>

#include <app/PhoneNumber.h>

void setUp() {}
void tearDown() {}

namespace {

// Returns the canonical form, or "<unreadable>" so a failing test names its input.
std::string norm(const char* in, const char* cc = "+49") {
  char buf[phone::kNormalizedSize];
  if (!phone::normalize(in, cc, buf, sizeof(buf))) return "<unreadable>";
  return buf;
}

}  // namespace

// ---------------------------------------------------------------------------
// Notations of the same number
// ---------------------------------------------------------------------------

void test_every_notation_of_a_number_gives_the_same_form() {
  TEST_ASSERT_EQUAL_STRING("+491701111111", norm("+491701111111").c_str());
  TEST_ASSERT_EQUAL_STRING("+491701111111", norm("+49 170 1111111").c_str());
  TEST_ASSERT_EQUAL_STRING("+491701111111", norm("0049 170 1111111").c_str());
  TEST_ASSERT_EQUAL_STRING("+491701111111", norm("0170-111 11 11").c_str());
  TEST_ASSERT_EQUAL_STRING("+491701111111", norm("(0170) 1111111").c_str());
  TEST_ASSERT_EQUAL_STRING("+491701111111", norm("0170/1111111").c_str());
  TEST_ASSERT_EQUAL_STRING("+491701111111", norm("0170.1111111").c_str());
}

void test_another_country_code_completes_national_numbers() {
  TEST_ASSERT_EQUAL_STRING("+34600123456", norm("0600123456", "+34").c_str());
  TEST_ASSERT_EQUAL_STRING("+34600123456", norm("0600123456", "34").c_str());
  TEST_ASSERT_EQUAL_STRING("+491701111111", norm("0170 1111111", nullptr).c_str());
  TEST_ASSERT_EQUAL_STRING("+491701111111", norm("0170 1111111", "").c_str());
}

void test_short_codes_stay_as_they_are() {
  TEST_ASSERT_EQUAL_STRING("110", norm("110").c_str());
  TEST_ASSERT_EQUAL_STRING("112", norm("112").c_str());
  TEST_ASSERT_EQUAL_STRING("116117", norm("116 117").c_str());
}

// A number written with the national prefix in brackets after the country code is
// common in letterheads and address books; the 0 is not part of the number.
void test_bracketed_national_prefix_after_the_country_code_is_dropped() {
  TEST_ASSERT_EQUAL_STRING("+491701111111", norm("+49 (0)170 1111111").c_str());
  TEST_ASSERT_EQUAL_STRING("+491701111111", norm("0049 (0)170 1111111").c_str());
  // Anywhere else the digits are what they are.
  TEST_ASSERT_EQUAL_STRING("+4917000000000", norm("+49 170 (0)0000000").c_str());
  TEST_ASSERT_EQUAL_STRING("+491701111111", norm("(0)170 1111111").c_str());
}

// ---------------------------------------------------------------------------
// Input that is not a number
// ---------------------------------------------------------------------------

void test_names_and_empty_input_are_not_numbers() {
  TEST_ASSERT_EQUAL_STRING("<unreadable>", norm("Provider").c_str());
  TEST_ASSERT_EQUAL_STRING("<unreadable>", norm("").c_str());
  TEST_ASSERT_EQUAL_STRING("<unreadable>", norm("   ").c_str());
  TEST_ASSERT_EQUAL_STRING("<unreadable>", norm(nullptr).c_str());
  TEST_ASSERT_EQUAL_STRING("<unreadable>", norm("0170 111 1111 ext").c_str());
  TEST_ASSERT_EQUAL_STRING("<unreadable>", norm("+").c_str());
}

void test_misplaced_plus_signs_are_rejected() {
  TEST_ASSERT_EQUAL_STRING("<unreadable>", norm("++491701111111").c_str());
  TEST_ASSERT_EQUAL_STRING("<unreadable>", norm("49+1701111111").c_str());
  TEST_ASSERT_EQUAL_STRING("<unreadable>", norm("0170+1111111").c_str());
}

// A country code never starts with 0 and a prefix without a number is no number.
void test_prefixes_without_a_number_are_rejected() {
  TEST_ASSERT_EQUAL_STRING("<unreadable>", norm("0").c_str());
  TEST_ASSERT_EQUAL_STRING("<unreadable>", norm("00").c_str());
  TEST_ASSERT_EQUAL_STRING("<unreadable>", norm("000 123").c_str());
  TEST_ASSERT_EQUAL_STRING("<unreadable>", norm("+0123456789").c_str());
}

// Cutting a long input short would make two different numbers equal.
void test_too_many_digits_are_rejected_not_cut_off() {
  const std::string fifteen = "+" + std::string(15, '1');
  TEST_ASSERT_EQUAL_STRING(fifteen.c_str(), norm(fifteen.c_str()).c_str());
  TEST_ASSERT_EQUAL_STRING("<unreadable>", norm((fifteen + "1").c_str()).c_str());
  TEST_ASSERT_EQUAL_STRING("<unreadable>", norm(("0" + std::string(15, '1')).c_str()).c_str());
  TEST_ASSERT_EQUAL_STRING("<unreadable>", norm(std::string(80, '1').c_str()).c_str());
  // Padding with separators does not hide the extra digit either.
  const std::string padded = "+" + std::string(15, '1') + std::string(30, ' ') + "1";
  TEST_ASSERT_EQUAL_STRING("<unreadable>", norm(padded.c_str()).c_str());
}

// The limit is on the digits, not on the size of the output buffer the caller happens to have.
void test_the_digit_limit_does_not_depend_on_the_buffer_size() {
  char big[64];
  TEST_ASSERT_TRUE(phone::normalize(("+" + std::string(15, '1')).c_str(), "+49", big, sizeof(big)));
  TEST_ASSERT_FALSE(phone::normalize(("+" + std::string(16, '1')).c_str(), "+49", big, sizeof(big)));
  TEST_ASSERT_FALSE(phone::normalize(("0" + std::string(14, '1')).c_str(), "+49", big, sizeof(big)));
  TEST_ASSERT_FALSE(phone::normalize(std::string(16, '1').c_str(), "+49", big, sizeof(big)));
  TEST_ASSERT_EQUAL_STRING("", big);
}

void test_a_country_code_that_is_none_makes_national_numbers_unreadable() {
  TEST_ASSERT_EQUAL_STRING("<unreadable>", norm("0170 1111111", "Germany").c_str());
  TEST_ASSERT_EQUAL_STRING("<unreadable>", norm("0170 1111111", "+0").c_str());
  TEST_ASSERT_EQUAL_STRING("<unreadable>", norm("0170 1111111", "+4949").c_str());
  // International numbers do not need it.
  TEST_ASSERT_EQUAL_STRING("+491701111111", norm("+49 170 1111111", "Germany").c_str());
}

void test_output_buffer_limits() {
  char small[5] = "xxxx";
  TEST_ASSERT_FALSE(phone::normalize("+491701111111", "+49", small, sizeof(small)));
  TEST_ASSERT_EQUAL_STRING("", small);
  char exact[14];
  TEST_ASSERT_TRUE(phone::normalize("+491701111111", "+49", exact, sizeof(exact)));
  char tooSmallByOne[13];
  TEST_ASSERT_FALSE(phone::normalize("+491701111111", "+49", tooSmallByOne, sizeof(tooSmallByOne)));
  char one[1] = {'x'};
  TEST_ASSERT_FALSE(phone::normalize("112", "+49", one, 1));
  TEST_ASSERT_EQUAL_CHAR('\0', one[0]);
  TEST_ASSERT_FALSE(phone::normalize("112", "+49", nullptr, 0));
}

// ---------------------------------------------------------------------------
// Comparison
// ---------------------------------------------------------------------------

void test_same_number_in_different_notations() {
  TEST_ASSERT_TRUE(phone::same("0170 1111111", "+491701111111", "+49"));
  TEST_ASSERT_TRUE(phone::same("0049 170 1111111", "(0170) 111 11 11", "+49"));
  TEST_ASSERT_FALSE(phone::same("+491701111111", "+491702222222", "+49"));
}

void test_unreadable_numbers_never_match_each_other() {
  TEST_ASSERT_FALSE(phone::same("Provider", "Provider", "+49"));
  TEST_ASSERT_FALSE(phone::same("", "", "+49"));
  TEST_ASSERT_FALSE(phone::same(nullptr, nullptr, "+49"));
  TEST_ASSERT_FALSE(phone::same("+491701111111", "Provider", "+49"));
}

// The same subscriber digits in another country are another number.
void test_the_country_code_takes_part_in_the_comparison() {
  TEST_ASSERT_TRUE(phone::same("0600123456", "+34600123456", "+34"));
  TEST_ASSERT_FALSE(phone::same("0600123456", "+34600123456", "+49"));
}

// A number without any prefix is not guessed to carry a country code.
void test_digits_without_a_prefix_do_not_match_the_international_form() {
  TEST_ASSERT_FALSE(phone::same("491701111111", "+491701111111", "+49"));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_every_notation_of_a_number_gives_the_same_form);
  RUN_TEST(test_another_country_code_completes_national_numbers);
  RUN_TEST(test_short_codes_stay_as_they_are);
  RUN_TEST(test_bracketed_national_prefix_after_the_country_code_is_dropped);
  RUN_TEST(test_names_and_empty_input_are_not_numbers);
  RUN_TEST(test_misplaced_plus_signs_are_rejected);
  RUN_TEST(test_prefixes_without_a_number_are_rejected);
  RUN_TEST(test_too_many_digits_are_rejected_not_cut_off);
  RUN_TEST(test_the_digit_limit_does_not_depend_on_the_buffer_size);
  RUN_TEST(test_a_country_code_that_is_none_makes_national_numbers_unreadable);
  RUN_TEST(test_output_buffer_limits);
  RUN_TEST(test_same_number_in_different_notations);
  RUN_TEST(test_unreadable_numbers_never_match_each_other);
  RUN_TEST(test_the_country_code_takes_part_in_the_comparison);
  RUN_TEST(test_digits_without_a_prefix_do_not_match_the_international_form);
  return UNITY_END();
}

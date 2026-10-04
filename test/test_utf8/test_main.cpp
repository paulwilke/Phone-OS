// Host tests for the UTF-8 helpers. allow-non-english: the text under test
// contains umlauts and other multi-byte characters.

#include <unity.h>

#include <cstring>
#include <string>

#include <app/Utf8.h>

void setUp() {}
void tearDown() {}

void test_fit_keeps_whole_characters() {
  const std::string s = "ab\xC3\xBC" "cd";  // "abücd": ü is bytes 2 and 3
  TEST_ASSERT_EQUAL(6, utf8::fit(s.data(), s.size(), 10));  // nothing to cut
  TEST_ASSERT_EQUAL(5, utf8::fit(s.data(), s.size(), 5));
  TEST_ASSERT_EQUAL(4, utf8::fit(s.data(), s.size(), 4));
  TEST_ASSERT_EQUAL(2, utf8::fit(s.data(), s.size(), 3));  // would end inside ü
  TEST_ASSERT_EQUAL(2, utf8::fit(s.data(), s.size(), 2));
  TEST_ASSERT_EQUAL(0, utf8::fit(s.data(), s.size(), 0));
}

void test_fit_handles_three_and_four_byte_characters() {
  const std::string euro = "\xE2\x82\xAC";           // €, three bytes
  const std::string smile = "\xF0\x9F\x99\x82";       // four bytes
  TEST_ASSERT_EQUAL(0, utf8::fit(euro.data(), euro.size(), 2));
  TEST_ASSERT_EQUAL(3, utf8::fit(euro.data(), euro.size(), 3));
  TEST_ASSERT_EQUAL(0, utf8::fit(smile.data(), smile.size(), 3));
  TEST_ASSERT_EQUAL(4, utf8::fit(smile.data(), smile.size(), 4));
}

void test_copy_terminates_and_never_overruns() {
  char out[5];
  const std::string s = "abc\xC3\xBC" "d";
  utf8::copy(out, sizeof(out), s.data(), s.data() + s.size());
  TEST_ASSERT_EQUAL_STRING("abc", out);  // ü would need bytes 3 and 4, the terminator needs 4
  char big[16];
  utf8::copy(big, sizeof(big), s.data(), s.data() + s.size());
  TEST_ASSERT_EQUAL_STRING(s.c_str(), big);
  utf8::copy(out, 0, s.data(), s.data() + s.size());  // no write at all
  char one[1] = {'x'};
  utf8::copy(one, 1, s.data(), s.data() + s.size());
  TEST_ASSERT_EQUAL_CHAR('\0', one[0]);
}

void test_count_and_prefix_bytes() {
  const std::string s = "\xC3\x96sterreich";  // "Österreich"
  TEST_ASSERT_EQUAL(10, utf8::count(s.data(), s.size()));
  TEST_ASSERT_EQUAL(11, s.size());
  TEST_ASSERT_EQUAL(0, utf8::prefixBytes(s.data(), s.size(), 0));
  TEST_ASSERT_EQUAL(2, utf8::prefixBytes(s.data(), s.size(), 1));
  TEST_ASSERT_EQUAL(5, utf8::prefixBytes(s.data(), s.size(), 4));
  TEST_ASSERT_EQUAL(11, utf8::prefixBytes(s.data(), s.size(), 10));
  TEST_ASSERT_EQUAL(11, utf8::prefixBytes(s.data(), s.size(), 99));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_fit_keeps_whole_characters);
  RUN_TEST(test_fit_handles_three_and_four_byte_characters);
  RUN_TEST(test_copy_terminates_and_never_overruns);
  RUN_TEST(test_count_and_prefix_bytes);
  return UNITY_END();
}

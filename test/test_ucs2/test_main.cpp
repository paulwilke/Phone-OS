// Host tests for the hex UCS-2 conversion of the modem driver: umlauts, emoji, the round trip, malformed
// hex, truncation, lone surrogates, and the edges of the conversion (NUL units, overlong UTF-8, small buffers).
// allow-non-english: texts with umlauts are the point of this module.

#include <unity.h>

#include <cstring>
#include <string>

#include <drivers/Ucs2.h>

void setUp() {}
void tearDown() {}

void test_umlauts_and_emoji_are_decoded() {
  char out[128];
  bool truncated = true;
  const char* hex = "0053006300680075006C00650020006600E4006C006C007400200061007500730020D83DDE00";
  TEST_ASSERT_TRUE(text::ucs2HexToUtf8(hex, strlen(hex), out, sizeof(out), &truncated));
  TEST_ASSERT_FALSE(truncated);
  TEST_ASSERT_EQUAL_STRING("Schule f\xC3\xA4llt aus \xF0\x9F\x98\x80", out);
}

void test_a_round_trip_returns_the_text() {
  const char* s = "Gr\xC3\xBC\xC3\x9F" "e, Papa! \xF0\x9F\x9A\x8C \xC3\x9F";
  char hex[256], back[128];
  TEST_ASSERT_TRUE(text::utf8ToUcs2Hex(s, hex, sizeof(hex)) > 0);
  TEST_ASSERT_TRUE(text::ucs2HexToUtf8(hex, strlen(hex), back, sizeof(back), nullptr));
  TEST_ASSERT_EQUAL_STRING(s, back);
}

void test_malformed_hex_is_refused_and_leaves_an_empty_string() {
  char out[16] = "x";
  TEST_ASSERT_FALSE(text::ucs2HexToUtf8("00410", 5, out, sizeof(out), nullptr));
  TEST_ASSERT_FALSE(text::ucs2HexToUtf8("00G1", 4, out, sizeof(out), nullptr));
  TEST_ASSERT_EQUAL_STRING("", out);
}

void test_a_text_that_does_not_fit_is_cut_at_a_character_boundary() {
  char out[4];  // room for one two-byte character and the end, not for two
  bool truncated = false;
  TEST_ASSERT_TRUE(text::ucs2HexToUtf8("00E400E4", 8, out, sizeof(out), &truncated));
  TEST_ASSERT_TRUE(truncated);
  TEST_ASSERT_EQUAL_STRING("\xC3\xA4", out);
}

void test_a_lone_surrogate_becomes_the_replacement_character() {
  char out[16];
  TEST_ASSERT_TRUE(text::ucs2HexToUtf8("D83D0041", 8, out, sizeof(out), nullptr));
  TEST_ASSERT_EQUAL_STRING("\xEF\xBF\xBD" "A", out);
  TEST_ASSERT_TRUE(text::ucs2HexToUtf8("DE00", 4, out, sizeof(out), nullptr));   // a low surrogate on its own
  TEST_ASSERT_EQUAL_STRING("\xEF\xBF\xBD", out);
  TEST_ASSERT_TRUE(text::ucs2HexToUtf8("D83D", 4, out, sizeof(out), nullptr));   // a high one at the very end
  TEST_ASSERT_EQUAL_STRING("\xEF\xBF\xBD", out);
}

// A NUL unit would end the C string, and with it everything after it, silently.
void test_a_nul_unit_is_dropped_and_the_rest_survives() {
  char out[16];
  TEST_ASSERT_TRUE(text::ucs2HexToUtf8("00410000" "0042", 12, out, sizeof(out), nullptr));
  TEST_ASSERT_EQUAL_STRING("AB", out);
}

void test_the_empty_string_is_valid_and_lower_case_hex_is_read() {
  char out[8];
  TEST_ASSERT_TRUE(text::isUcs2Hex("", 0));
  TEST_ASSERT_TRUE(text::ucs2HexToUtf8("", 0, out, sizeof(out), nullptr));
  TEST_ASSERT_EQUAL_STRING("", out);
  TEST_ASSERT_TRUE(text::ucs2HexToUtf8("00e4", 4, out, sizeof(out), nullptr));
  TEST_ASSERT_EQUAL_STRING("\xC3\xA4", out);
}

void test_encoding_writes_capitals_and_counts_units() {
  char hex[32];
  TEST_ASSERT_EQUAL(2, text::utf8ToUcs2Hex("\xC3\xA4z", hex, sizeof(hex)));
  TEST_ASSERT_EQUAL_STRING("00E4007A", hex);
  TEST_ASSERT_EQUAL(2, text::utf8ToUcs2Hex("\xF0\x9F\x98\x80", hex, sizeof(hex)));   // one emoji, two units
  TEST_ASSERT_EQUAL_STRING("D83DDE00", hex);
  TEST_ASSERT_EQUAL(0, text::utf8ToUcs2Hex("", hex, sizeof(hex)));
}

// An overlong form ("/" as two bytes) is a way to smuggle characters past a filter.
void test_invalid_utf8_is_refused_and_leaves_an_empty_string() {
  char hex[64] = "x";
  const char* bad[] = {"\xC0\xAF", "\xE0\x80\xAF", "\xF0\x80\x80\xAF", "\x80", "\xC3", "\xC3\x41", "\xED\xA0\x80", "\xF4\x90\x80\x80"};
  // "\xED\xA0\x80" is a surrogate, "\xF4\x90\x80\x80" lies beyond U+10FFFF.
  for (const char* s : bad) {
    TEST_ASSERT_EQUAL_MESSAGE(-1, text::utf8ToUcs2Hex(s, hex, sizeof(hex)), s);
    TEST_ASSERT_EQUAL_STRING("", hex);
  }
}

void test_a_buffer_that_is_too_small_is_refused_not_cut() {
  char hex[8];                                   // room for one unit (4) and the end, not for two
  TEST_ASSERT_EQUAL(-1, text::utf8ToUcs2Hex("ab", hex, sizeof(hex)));
  TEST_ASSERT_EQUAL_STRING("", hex);
  TEST_ASSERT_EQUAL(1, text::utf8ToUcs2Hex("a", hex, sizeof(hex)));
  char one[1];
  TEST_ASSERT_EQUAL(-1, text::utf8ToUcs2Hex("a", one, 1));
  TEST_ASSERT_EQUAL(-1, text::utf8ToUcs2Hex("a", one, 0));
  TEST_ASSERT_FALSE(text::ucs2HexToUtf8("0041", 4, one, 0, nullptr));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_umlauts_and_emoji_are_decoded);
  RUN_TEST(test_a_round_trip_returns_the_text);
  RUN_TEST(test_malformed_hex_is_refused_and_leaves_an_empty_string);
  RUN_TEST(test_a_text_that_does_not_fit_is_cut_at_a_character_boundary);
  RUN_TEST(test_a_lone_surrogate_becomes_the_replacement_character);
  RUN_TEST(test_a_nul_unit_is_dropped_and_the_rest_survives);
  RUN_TEST(test_the_empty_string_is_valid_and_lower_case_hex_is_read);
  RUN_TEST(test_encoding_writes_capitals_and_counts_units);
  RUN_TEST(test_invalid_utf8_is_refused_and_leaves_an_empty_string);
  RUN_TEST(test_a_buffer_that_is_too_small_is_refused_not_cut);
  return UNITY_END();
}

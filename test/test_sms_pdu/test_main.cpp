// The PDU of an outgoing SMS: the example every tutorial uses, the umlauts as 7-bit, the escape table, the
// parts of a long text with their header, UCS2 with emoji, the numbers, and a decoder written in the test
// from the specification (not from the encoder) that reads back what the encoder wrote.
// allow-non-english: the texts under test contain umlauts, Cyrillic letters and an emoji.

#include <unity.h>

#include <algorithm>
#include <string>
#include <vector>

#include <SmsPduDecoder.h>
#include <drivers/SmsPdu.h>

using namespace sms;
using namespace smsdecode;

void setUp() {}
void tearDown() {}

namespace {

const char* const kNumber = "+491701111111";

// Encodes, decodes every part, checks the header and the lengths, and gives back the joined places.
struct Round {
  Coding coding = Coding::Gsm7;
  std::vector<Part> parts;
  std::vector<uint8_t> places;
  std::vector<uint16_t> units;
  bool ok = false;
  explicit Round(const std::string& text, uint8_t reference = 7, const char* number = kNumber) {
    if (!encode(number, text, reference, coding, parts)) return;
    ok = true;
    for (size_t k = 0; k < parts.size(); ++k) {
      const Decoded d = decode(parts[k].hex);
      TEST_ASSERT_TRUE(d.ok);
      TEST_ASSERT_EQUAL_STRING(number, d.number.c_str());
      TEST_ASSERT_EQUAL(0xA7, d.validity);
      TEST_ASSERT_EQUAL(parts[k].tpduLength, d.tpduLength);
      TEST_ASSERT_EQUAL(coding == Coding::Gsm7 ? 0x00 : 0x08, d.dcs);
      TEST_ASSERT_EQUAL(parts.size() > 1, d.udhi);
      if (parts.size() > 1) {
        TEST_ASSERT_EQUAL(reference, d.reference);
        TEST_ASSERT_EQUAL((int)parts.size(), d.total);
        TEST_ASSERT_EQUAL((int)k + 1, d.sequence);
      }
      TEST_ASSERT_TRUE(d.udl <= (coding == Coding::Gsm7 ? 160 : 140));
      places.insert(places.end(), d.places.begin(), d.places.end());
      units.insert(units.end(), d.units.begin(), d.units.end());
    }
  }
};

std::string repeat(const std::string& s, size_t n) {
  std::string out;
  for (size_t i = 0; i < n; ++i) out += s;
  return out;
}

}  // namespace

// The example that every SMS tutorial gives for "hellohello": its user data is E8329BFD4697D9EC37 in all of them. The
// rest is built as the specification has it for an invented number: the service centre 00, first octet 11, reference
// 00, twelve digits (0C) of international type (91) with their pairs swapped, protocol 00, coding 00, validity A7
// (the tutorials have AA; the text mode was set to 167), ten places (0A).
void test_the_tutorial_user_data_is_reproduced() {
  Coding coding;
  std::vector<Part> parts;
  TEST_ASSERT_TRUE(encode("+491701111111", "hellohello", 1, coding, parts));
  TEST_ASSERT_TRUE(coding == Coding::Gsm7);
  TEST_ASSERT_EQUAL(1, parts.size());
  TEST_ASSERT_EQUAL_STRING("0011000C91947110111111" "0000A70AE8329BFD4697D9EC37", parts[0].hex.c_str());
  TEST_ASSERT_EQUAL(23, parts[0].tpduLength);
}

// Decision D-10: the word with the umlaut is no reason for UCS2 and no reason for three messages.
void test_an_sos_with_an_umlaut_is_one_7_bit_message() {
  const std::string text =
      "Jean m\xC3\xB6" "chte abgeholt werden. Koordinaten hier:\nhttps://maps.google.com/?q=48.13745,11.57550\n48.13745, 11.57550, auf 12 m genau";
  Round r(text);
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_TRUE(r.coding == Coding::Gsm7);
  TEST_ASSERT_EQUAL(1, r.parts.size());
  TEST_ASSERT_TRUE(r.places == expectedPlaces(text));
  TEST_ASSERT_TRUE(r.places.size() > 100 && r.places.size() <= 160);
  TEST_ASSERT_TRUE(std::find(r.places.begin(), r.places.end(), 0x7C) != r.places.end());   // the o-umlaut is septet 0x7C
}

void test_the_german_and_common_letters_are_in_the_alphabet() {
  const std::string text = "\xC3\xA4\xC3\xB6\xC3\xBC\xC3\x84\xC3\x96\xC3\x9C\xC3\x9F \xC3\xA9 @ $ _";
  Round r(text);
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_TRUE(r.coding == Coding::Gsm7);
  const std::vector<uint8_t> want = {0x7B, 0x7C, 0x7E, 0x5B, 0x5C, 0x5E, 0x1E, 0x20, 0x05, 0x20, 0x00, 0x20, 0x02, 0x20, 0x11};
  TEST_ASSERT_TRUE(r.places == want);
}

void test_the_extension_characters_take_two_places() {
  const std::string text = "[a]{b}|\\~^\xE2\x82\xAC";
  Round r(text);
  TEST_ASSERT_TRUE(r.ok && r.coding == Coding::Gsm7);
  TEST_ASSERT_TRUE(r.places == expectedPlaces(text));
  TEST_ASSERT_EQUAL(9 * 2 + 2, r.places.size());   // nine extension characters and the letters a and b
}

void test_the_length_where_one_message_becomes_two_is_160_places() {
  for (size_t n : {1u, 159u, 160u}) {
    Round r(repeat("a", n));
    TEST_ASSERT_EQUAL_MESSAGE(1, r.parts.size(), std::to_string(n).c_str());
    TEST_ASSERT_EQUAL(n, r.places.size());
  }
  Round two(repeat("a", 161));
  TEST_ASSERT_EQUAL(2, two.parts.size());
  TEST_ASSERT_EQUAL(161, two.places.size());
  Round full(repeat("a", 306));
  TEST_ASSERT_EQUAL(2, full.parts.size());
  Round three(repeat("a", 307));
  TEST_ASSERT_EQUAL(3, three.parts.size());
  TEST_ASSERT_TRUE(three.places == expectedPlaces(repeat("a", 307)));
}

// A pair of escape and code is one character and is never cut in two.
void test_a_long_text_is_not_cut_inside_an_escape_pair() {
  const std::string text = repeat("a", 152) + "\xE2\x82\xAC" + repeat("b", 10);   // the pair would start at place 153
  Round r(text);
  TEST_ASSERT_EQUAL(2, r.parts.size());
  TEST_ASSERT_TRUE(r.places == expectedPlaces(text));
  const Decoded first = decode(r.parts[0].hex);
  TEST_ASSERT_EQUAL(152, (int)first.places.size());
  const Decoded second = decode(r.parts[1].hex);
  TEST_ASSERT_EQUAL(0x1B, second.places[0]);
}

void test_the_parts_of_a_long_text_carry_the_reference_and_the_count() {
  Round r(repeat("word ", 90), 201);   // 450 places
  TEST_ASSERT_EQUAL(3, r.parts.size());
  TEST_ASSERT_EQUAL(450, r.places.size());
  for (const Part& part : r.parts) TEST_ASSERT_EQUAL((int)(part.hex.size() / 2) - 1, part.tpduLength);
}

void test_a_character_outside_the_alphabet_makes_the_whole_text_ucs2() {
  const std::string text = "Hallo \xD0\xB6 und \xF0\x9F\x98\x80";   // a Cyrillic letter and an emoji
  Round r(text);
  TEST_ASSERT_TRUE(r.ok && r.coding == Coding::Ucs2);
  const std::vector<uint16_t> want = {'H', 'a', 'l', 'l', 'o', ' ', 0x0436, ' ', 'u', 'n', 'd', ' ', 0xD83D, 0xDE00};
  TEST_ASSERT_TRUE(r.units == want);
  TEST_ASSERT_EQUAL(1, r.parts.size());
  Coding c; std::vector<uint8_t> p;
  TEST_ASSERT_FALSE(toGsm7(text, p));
  (void)c;
}

void test_ucs2_is_70_in_one_message_and_67_in_a_part() {
  const std::string z = "\xD0\xB6";
  Round one(repeat(z, 70));
  TEST_ASSERT_EQUAL(1, one.parts.size());
  Round two(repeat(z, 71));
  TEST_ASSERT_EQUAL(2, two.parts.size());
  TEST_ASSERT_EQUAL(71, two.units.size());
  Round two2(repeat(z, 134));
  TEST_ASSERT_EQUAL(2, two2.parts.size());
  Round three(repeat(z, 135));
  TEST_ASSERT_EQUAL(3, three.parts.size());
}

// An emoji is two units and is not cut between them.
void test_a_ucs2_part_does_not_split_a_surrogate_pair() {
  const std::string text = repeat("\xD0\xB6", 66) + "\xF0\x9F\x98\x80" + repeat("\xD0\xB6", 10);   // the pair would start at unit 67
  Round r(text);
  TEST_ASSERT_EQUAL(2, r.parts.size());
  TEST_ASSERT_EQUAL(66, (int)decode(r.parts[0].hex).units.size());
  TEST_ASSERT_EQUAL(0xD83D, decode(r.parts[1].hex).units[0]);
  TEST_ASSERT_EQUAL(78, r.units.size());
}

void test_the_number_is_written_as_the_specification_has_it() {
  Coding c;
  std::vector<Part> p;
  TEST_ASSERT_TRUE(encode("+491701111111", "x", 1, c, p));
  TEST_ASSERT_EQUAL_STRING("0C91947110111111", p[0].hex.substr(6, 16).c_str());   // 12 digits, international, swapped pairs
  TEST_ASSERT_EQUAL_STRING("+491701111111", decode(p[0].hex).number.c_str());
  TEST_ASSERT_TRUE(encode("017012345678", "x", 1, c, p));                       // no plus: unknown type
  TEST_ASSERT_EQUAL_STRING("017012345678", decode(p[0].hex).number.c_str());
  TEST_ASSERT_EQUAL_STRING("81", p[0].hex.substr(8, 2).c_str());
  TEST_ASSERT_TRUE(encode("*100#", "x", 1, c, p));                              // an odd count is padded with F
  TEST_ASSERT_EQUAL_STRING("*100#", decode(p[0].hex).number.c_str());
}

void test_a_number_is_never_shortened() {
  Coding c;
  std::vector<Part> p;
  TEST_ASSERT_TRUE(encode("+49170111111111111111", "x", 1, c, p));      // 20 digits: the most the PDU holds
  TEST_ASSERT_FALSE(encode("+491701111111111111111", "x", 1, c, p));    // 21 digits
  TEST_ASSERT_FALSE(encode("", "x", 1, c, p));
  TEST_ASSERT_FALSE(encode("+", "x", 1, c, p));
  TEST_ASSERT_FALSE(encode("0170 123", "x", 1, c, p));
  TEST_ASSERT_FALSE(encode("0170a123", "x", 1, c, p));
}

void test_bad_texts_are_refused() {
  Coding c;
  std::vector<Part> p;
  TEST_ASSERT_FALSE(encode(kNumber, "", 1, c, p));
  TEST_ASSERT_FALSE(encode(kNumber, std::string("a\0b", 3), 1, c, p));
  TEST_ASSERT_FALSE(encode(kNumber, "a\xC3(b", 1, c, p));                // a lead byte without its continuation
  TEST_ASSERT_FALSE(encode(kNumber, "a\xC0\x80" "b", 1, c, p));          // overlong NUL
  TEST_ASSERT_FALSE(encode(kNumber, "a\xED\xA0\x80" "b", 1, c, p));      // a surrogate
  TEST_ASSERT_FALSE(encode(kNumber, "a\xF4\x90\x80\x80", 1, c, p));      // beyond U+10FFFF
  TEST_ASSERT_FALSE(encode(kNumber, "a\xE2\x82", 1, c, p));              // cut short
  TEST_ASSERT_EQUAL(0, p.size());
}

void test_control_characters_that_were_a_problem_in_text_mode_are_just_characters() {
  Round r("a\x1A" "b\x1B" "c");   // Ctrl-Z and Escape would end or cancel a text in text mode; in a PDU they are data
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_TRUE(r.coding == Coding::Ucs2 || r.coding == Coding::Gsm7);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_the_tutorial_user_data_is_reproduced);
  RUN_TEST(test_an_sos_with_an_umlaut_is_one_7_bit_message);
  RUN_TEST(test_the_german_and_common_letters_are_in_the_alphabet);
  RUN_TEST(test_the_extension_characters_take_two_places);
  RUN_TEST(test_the_length_where_one_message_becomes_two_is_160_places);
  RUN_TEST(test_a_long_text_is_not_cut_inside_an_escape_pair);
  RUN_TEST(test_the_parts_of_a_long_text_carry_the_reference_and_the_count);
  RUN_TEST(test_a_character_outside_the_alphabet_makes_the_whole_text_ucs2);
  RUN_TEST(test_ucs2_is_70_in_one_message_and_67_in_a_part);
  RUN_TEST(test_a_ucs2_part_does_not_split_a_surrogate_pair);
  RUN_TEST(test_the_number_is_written_as_the_specification_has_it);
  RUN_TEST(test_a_number_is_never_shortened);
  RUN_TEST(test_bad_texts_are_refused);
  RUN_TEST(test_control_characters_that_were_a_problem_in_text_mode_are_just_characters);
  return UNITY_END();
}

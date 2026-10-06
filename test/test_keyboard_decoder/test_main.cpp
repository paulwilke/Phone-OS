// What the keys mean. The matrix and the layer symbols were checked on the device on 2026-09-30.

#include <unity.h>

#include <string>

#include <board/KeyboardDecoder.h>

using namespace board;

void setUp() {}
void tearDown() {}

namespace {
struct Rig {
  KeyboardDecoder decoder;

  void press(uint8_t code) { decoder.decode({code, true}, last); }
  // Presses and releases a key and returns what it produced.
  bool tap(uint8_t code, ui::Input& out) {
    const bool got = decoder.decode({code, true}, out);
    ui::Input ignored;
    decoder.decode({code, false}, ignored);
    return got;
  }
  // The character a tap produces, or 0 when it is not text.
  uint32_t typed(uint8_t code) {
    ui::Input in;
    if (!tap(code, in) || in.kind != ui::Input::Kind::Text) return 0;
    return in.character;
  }
  void hold(uint8_t code) { press(code); }
  void let(uint8_t code) {
    ui::Input ignored;
    decoder.decode({code, false}, ignored);
  }
  ui::Input last;
};

constexpr uint8_t kLeftShift = 35, kRightShift = 31, kAlt = 30, kSym = 32, kMic = 34, kDollar = 22;

std::string row(Rig& r, const uint8_t* codes, size_t n) {
  std::string s;
  for (size_t i = 0; i < n; ++i) s += (char)r.typed(codes[i]);
  return s;
}
}  // namespace

void test_the_three_letter_rows() {
  Rig r;
  const uint8_t top[] = {10, 9, 8, 7, 6, 5, 4, 3, 2, 1};
  const uint8_t home[] = {20, 19, 18, 17, 16, 15, 14, 13, 12};
  const uint8_t bottom[] = {29, 28, 27, 26, 25, 24, 23};
  TEST_ASSERT_EQUAL_STRING("qwertyuiop", row(r, top, sizeof(top)).c_str());
  TEST_ASSERT_EQUAL_STRING("asdfghjkl", row(r, home, sizeof(home)).c_str());
  TEST_ASSERT_EQUAL_STRING("zxcvbnm", row(r, bottom, sizeof(bottom)).c_str());
  TEST_ASSERT_EQUAL(' ', r.typed(33));
}

void test_enter_backspace_and_the_two_scroll_keys_are_keys_not_text() {
  Rig r;
  ui::Input in;
  TEST_ASSERT_TRUE(r.tap(21, in));
  TEST_ASSERT_TRUE(in.isKey(ui::Key::Select));
  TEST_ASSERT_TRUE(r.tap(11, in));
  TEST_ASSERT_TRUE(in.isKey(ui::Key::Back));
  TEST_ASSERT_TRUE(r.tap(kDollar, in));
  TEST_ASSERT_TRUE(in.isKey(ui::Key::Up));
  TEST_ASSERT_TRUE(r.tap(kMic, in));
  TEST_ASSERT_TRUE(in.isKey(ui::Key::Down));
}

// W and S are letters: a text field has to be able to type them, and a screen does not guess.
void test_w_and_s_are_text() {
  Rig r;
  TEST_ASSERT_EQUAL('w', r.typed(9));
  TEST_ASSERT_EQUAL('s', r.typed(19));
}

void test_a_tapped_shift_capitalises_exactly_one_letter() {
  Rig r;
  r.hold(kLeftShift);
  r.let(kLeftShift);
  TEST_ASSERT_EQUAL('A', r.typed(20));
  TEST_ASSERT_EQUAL('a', r.typed(20));
}

void test_a_held_shift_capitalises_every_letter_until_it_is_let_go() {
  Rig r;
  r.hold(kRightShift);
  TEST_ASSERT_EQUAL('Q', r.typed(10));
  TEST_ASSERT_EQUAL('W', r.typed(9));
  r.let(kRightShift);
  TEST_ASSERT_EQUAL('e', r.typed(8));
}

void test_a_shift_that_capitalised_while_held_does_not_linger() {
  Rig r;
  r.hold(kLeftShift);
  r.typed(10);
  r.let(kLeftShift);
  TEST_ASSERT_EQUAL('a', r.typed(20));
}

void test_both_shifts_together_capitalise_nothing() {
  Rig r;
  r.hold(kLeftShift);
  r.hold(kRightShift);
  TEST_ASSERT_EQUAL('q', r.typed(10));
  r.let(kRightShift);
  r.let(kLeftShift);
  TEST_ASSERT_EQUAL('q', r.typed(10));
}

void test_shift_does_not_change_a_symbol() {
  Rig r;
  r.hold(kLeftShift);
  r.let(kLeftShift);
  r.hold(kSym);
  TEST_ASSERT_EQUAL('1', r.typed(9));
}

// The symbols printed on the keys, one tap of the layer key before each.
void test_the_second_layer_gives_the_printed_symbols_and_digits() {
  struct Case {
    uint8_t code;
    char expected;
  };
  const Case cases[] = {{10, '#'}, {9, '1'}, {8, '2'}, {7, '3'}, {6, '('}, {5, ')'}, {4, '_'}, {3, '-'}, {2, '+'},
                        {1, '@'},  {20, '*'}, {19, '4'}, {18, '5'}, {17, '6'}, {16, '/'}, {15, ':'}, {14, ';'},
                        {13, '\''}, {12, '"'}, {29, '7'}, {28, '8'}, {27, '9'}, {26, '?'}, {25, '!'}, {24, ','},
                        {23, '.'}, {kMic, '0'}, {kDollar, '$'}};
  for (const Case& c : cases) {
    Rig r;
    r.hold(kSym);
    r.let(kSym);
    TEST_ASSERT_EQUAL_MESSAGE(c.expected, r.typed(c.code), std::string(1, c.expected).c_str());
  }
}

void test_alt_and_sym_are_the_same_layer() {
  Rig r;
  r.hold(kAlt);
  r.let(kAlt);
  TEST_ASSERT_EQUAL('1', r.typed(9));
}

void test_the_layer_key_works_for_one_key_only() {
  Rig r;
  r.hold(kSym);
  TEST_ASSERT_EQUAL('1', r.typed(9));
  TEST_ASSERT_EQUAL('w', r.typed(9));
}

void test_a_layer_key_before_a_key_without_a_symbol_still_types_the_key() {
  Rig r;
  r.hold(kSym);
  ui::Input in;
  TEST_ASSERT_TRUE(r.tap(21, in));  // Enter has no symbol
  TEST_ASSERT_TRUE(in.isKey(ui::Key::Select));
  TEST_ASSERT_EQUAL('w', r.typed(9));  // and the layer is used up
}

void test_releases_and_modifiers_alone_produce_nothing() {
  Rig r;
  ui::Input in;
  TEST_ASSERT_FALSE(r.decoder.decode({10, false}, in));
  TEST_ASSERT_FALSE(r.decoder.decode({kLeftShift, true}, in));
  TEST_ASSERT_FALSE(r.decoder.decode({kSym, true}, in));
}

void test_codes_nobody_knows_produce_nothing() {
  Rig r;
  ui::Input in;
  const uint8_t unknown[] = {0, 36, 50, 80, 127};
  for (uint8_t code : unknown) TEST_ASSERT_FALSE(r.tap(code, in));
}

void test_reset_forgets_an_armed_shift_and_layer() {
  Rig r;
  r.hold(kLeftShift);
  r.let(kLeftShift);
  r.hold(kSym);
  r.decoder.reset();
  TEST_ASSERT_EQUAL('w', r.typed(9));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_the_three_letter_rows);
  RUN_TEST(test_enter_backspace_and_the_two_scroll_keys_are_keys_not_text);
  RUN_TEST(test_w_and_s_are_text);
  RUN_TEST(test_a_tapped_shift_capitalises_exactly_one_letter);
  RUN_TEST(test_a_held_shift_capitalises_every_letter_until_it_is_let_go);
  RUN_TEST(test_a_shift_that_capitalised_while_held_does_not_linger);
  RUN_TEST(test_both_shifts_together_capitalise_nothing);
  RUN_TEST(test_shift_does_not_change_a_symbol);
  RUN_TEST(test_the_second_layer_gives_the_printed_symbols_and_digits);
  RUN_TEST(test_alt_and_sym_are_the_same_layer);
  RUN_TEST(test_the_layer_key_works_for_one_key_only);
  RUN_TEST(test_a_layer_key_before_a_key_without_a_symbol_still_types_the_key);
  RUN_TEST(test_releases_and_modifiers_alone_produce_nothing);
  RUN_TEST(test_codes_nobody_knows_produce_nothing);
  RUN_TEST(test_reset_forgets_an_armed_shift_and_layer);
  return UNITY_END();
}

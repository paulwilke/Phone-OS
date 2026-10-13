// Host tests for the fonts and the text engine: UTF-8 decoding, the character set, glyph
// geometry, measuring, fitting, ellipsizing, wrapping, drawing and clip detection.

#include <unity.h>

#include <fstream>
#include <set>
#include <sstream>
#include <string>

#include <Golden.h>
#include <ui/Fonts.h>
#include <ui/Text.h>

using namespace ui;
using namespace ui::text;

void setUp() {}
void tearDown() {}

namespace {

const FontSize kSizes[] = {FontSize::Small, FontSize::Text, FontSize::Tile, FontSize::Large};

const char* sizeName(FontSize s) {
  switch (s) {
    case FontSize::Small: return "small";
    case FontSize::Text: return "text";
    case FontSize::Tile: return "tile";
    case FontSize::Large: return "large";
  }
  return "?";
}

std::string at(FontSize s, const char* what) { return std::string(sizeName(s)) + ": " + what; }

// The character set, read from the one definition the generator and the catalog check read too.
std::set<uint32_t> charsetFromFile(std::set<uint32_t>* symbols = nullptr) {
  std::ifstream in("assets/fonts/charset.txt");
  TEST_ASSERT_TRUE_MESSAGE(in.good(), "assets/fonts/charset.txt: run the tests from the repository root");
  std::set<uint32_t> out;
  std::string line;
  while (std::getline(in, line)) {
    line = line.substr(0, line.find('#'));
    std::istringstream words(line);
    std::string kind;
    if (!(words >> kind)) continue;
    std::string a, b;
    words >> std::hex >> a >> b;
    if (kind == "range") {
      for (uint32_t c = std::stoul(a, nullptr, 16); c <= std::stoul(b, nullptr, 16); ++c) out.insert(c);
    } else if (kind == "char" || kind == "symbol") {
      out.insert(std::stoul(a, nullptr, 16));
      if (kind == "symbol" && symbols) symbols->insert(std::stoul(a, nullptr, 16));
    }
  }
  return out;
}

uint32_t decodeAll(const char* bytes, size_t length, size_t* consumed = nullptr) {
  size_t pos = 0;
  const uint32_t code = decode(std::string_view(bytes, length), pos);
  if (consumed) *consumed = pos;
  return code;
}

// The topmost and lowest rows with ink, relative to `y`, of what `fb` holds.
bool inkRows(const FrameBuffer& fb, int* top, int* bottom) {
  *top = *bottom = -1;
  for (int y = 0; y < fb.height(); ++y)
    for (int x = 0; x < fb.width(); ++x)
      if (fb.pixel(x, y)) {
        if (*top < 0) *top = y;
        *bottom = y;
      }
  return *top >= 0;
}

}  // namespace

// ---- UTF-8 decoding -----------------------------------------------------------------------

void test_decode_valid_sequences_of_every_length() {
  size_t used = 0;
  TEST_ASSERT_EQUAL_UINT32('A', decodeAll("A", 1, &used));
  TEST_ASSERT_EQUAL(1, used);
  TEST_ASSERT_EQUAL_UINT32(0xFC, decodeAll("\xC3\xBC", 2, &used));  // u with diaeresis
  TEST_ASSERT_EQUAL(2, used);
  TEST_ASSERT_EQUAL_UINT32(0x20AC, decodeAll("\xE2\x82\xAC", 3, &used));  // €
  TEST_ASSERT_EQUAL(3, used);
  TEST_ASSERT_EQUAL_UINT32(0x1F4CD, decodeAll("\xF0\x9F\x93\x8D", 4, &used));  // the pin
  TEST_ASSERT_EQUAL(4, used);
  // The ends of each length class.
  TEST_ASSERT_EQUAL_UINT32(0x7F, decodeAll("\x7F", 1));
  TEST_ASSERT_EQUAL_UINT32(0x80, decodeAll("\xC2\x80", 2));
  TEST_ASSERT_EQUAL_UINT32(0x7FF, decodeAll("\xDF\xBF", 2));
  TEST_ASSERT_EQUAL_UINT32(0x800, decodeAll("\xE0\xA0\x80", 3));
  TEST_ASSERT_EQUAL_UINT32(0xFFFF, decodeAll("\xEF\xBF\xBF", 3));
  TEST_ASSERT_EQUAL_UINT32(0x10000, decodeAll("\xF0\x90\x80\x80", 4));
  TEST_ASSERT_EQUAL_UINT32(0x10FFFF, decodeAll("\xF4\x8F\xBF\xBF", 4));
}

void test_decode_a_lone_continuation_or_invalid_leader_is_one_replacement() {
  for (const char* bad : {"\x80", "\xBF", "\xC0", "\xC1", "\xF5", "\xF8", "\xFF"}) {
    size_t used = 0;
    TEST_ASSERT_EQUAL_UINT32(kReplacementChar, decodeAll(bad, 1, &used));
    TEST_ASSERT_EQUAL(1, used);
  }
}

void test_decode_leaders_beyond_f4_are_invalid_even_with_continuations() {
  for (const char* bad : {"\xF5\x80\x80\x80", "\xF6\x80\x80\x80", "\xF7\x80\x80\x80", "\xF8\x80\x80\x80\x80"}) {
    size_t used = 0;
    TEST_ASSERT_EQUAL_UINT32(kReplacementChar, decodeAll(bad, 4, &used));
    TEST_ASSERT_EQUAL(1, used);
  }
}

void test_decode_truncated_sequence_consumes_only_what_belongs_to_it() {
  size_t used = 0;
  // Euro sign cut short, followed by an ordinary letter: one replacement, the letter survives.
  const char cut[] = "\xE2\x82" "A";
  TEST_ASSERT_EQUAL_UINT32(kReplacementChar, decodeAll(cut, 3, &used));
  TEST_ASSERT_EQUAL(2, used);
  // Cut at the end of the text.
  TEST_ASSERT_EQUAL_UINT32(kReplacementChar, decodeAll("\xE2\x82", 2, &used));
  TEST_ASSERT_EQUAL(2, used);
  TEST_ASSERT_EQUAL_UINT32(kReplacementChar, decodeAll("\xF0", 1, &used));
  TEST_ASSERT_EQUAL(1, used);
}

void test_decode_overlong_surrogate_and_out_of_range_are_rejected() {
  size_t used = 0;
  // Overlong "/" : C0 AF is two replacements (C0 is never valid, AF is a lone continuation).
  TEST_ASSERT_EQUAL_UINT32(kReplacementChar, decodeAll("\xC0\xAF", 2, &used));
  TEST_ASSERT_EQUAL(1, used);
  // E0 80 80 (overlong NUL): the leader alone is rejected, because 80 is below A0.
  TEST_ASSERT_EQUAL_UINT32(kReplacementChar, decodeAll("\xE0\x80\x80", 3, &used));
  TEST_ASSERT_EQUAL(1, used);
  // ED A0 80 (a surrogate half).
  TEST_ASSERT_EQUAL_UINT32(kReplacementChar, decodeAll("\xED\xA0\x80", 3, &used));
  TEST_ASSERT_EQUAL(1, used);
  // F0 80 80 80 (overlong) and F4 90 80 80 (beyond U+10FFFF).
  TEST_ASSERT_EQUAL_UINT32(kReplacementChar, decodeAll("\xF0\x80\x80\x80", 4, &used));
  TEST_ASSERT_EQUAL(1, used);
  TEST_ASSERT_EQUAL_UINT32(kReplacementChar, decodeAll("\xF4\x90\x80\x80", 4, &used));
  TEST_ASSERT_EQUAL(1, used);
}

void test_decode_always_makes_progress_on_every_byte_pair() {
  for (int a = 0; a < 256; ++a)
    for (int b = 0; b < 256; ++b) {
      const char bytes[2] = {(char)a, (char)b};
      size_t used = 0;
      decodeAll(bytes, 2, &used);
      TEST_ASSERT_TRUE(used >= 1 && used <= 2);
    }
}

// ---- the character set ----------------------------------------------------------------------

void test_every_font_has_exactly_the_characters_of_the_charset() {
  const std::set<uint32_t> wanted = charsetFromFile();
  TEST_ASSERT_EQUAL(206, wanted.size());   // 205 and the power symbol
  for (FontSize s : kSizes) {
    const Font& f = font(s);
    for (uint32_t c = 0; c < 0x20000; ++c) {
      const bool has = f.find(c) != nullptr;
      if (has != (wanted.count(c) == 1)) {
        char message[80];
        snprintf(message, sizeof message, "%s: U+%04X is %s", sizeName(s), c, has ? "extra" : "missing");
        TEST_FAIL_MESSAGE(message);
      }
    }
  }
}

void test_characters_outside_the_charset_get_the_replacement_glyph() {
  for (FontSize s : kSizes) {
    const Font& f = font(s);
    for (uint32_t c : {0x00u, 0x0Au, 0x1Fu, 0x7Fu, 0x80u, 0x9Fu, 0x100u, 0x2026u, 0xFFFDu, 0x1F600u}) {
      TEST_ASSERT_TRUE_MESSAGE(f.find(c) == nullptr, at(s, "has a glyph it must not have").c_str());
      TEST_ASSERT_TRUE(&f.glyph(c) == &f.glyphs[0]);
    }
    TEST_ASSERT_TRUE(&f.glyph('A') != &f.glyphs[0]);
    TEST_ASSERT_TRUE_MESSAGE(f.glyphs[0].width > 0, "the replacement glyph must be visible");
  }
}

void test_the_ink_of_every_glyph_stays_inside_its_cell() {
  // The cell is [pen, pen + advance) × [0, line height). Text laid out from these numbers (fit,
  // wrap, a region as wide as the width) then never paints outside what it measured.
  const std::set<uint32_t> chars = charsetFromFile();
  for (FontSize s : kSizes) {
    const Font& f = font(s);
    for (uint32_t c : chars) {
      const Glyph& g = f.glyph(c);
      if (g.width == 0) continue;
      char message[100];
      snprintf(message, sizeof message, "%s U+%04X: x %d..%d of advance %d, y %d..%d of %d", sizeName(s), c,
               g.xOffset, g.xOffset + g.width, g.advance, g.yOffset, g.yOffset + g.height, f.lineHeight());
      TEST_ASSERT_TRUE_MESSAGE(g.xOffset >= 0, message);
      TEST_ASSERT_TRUE_MESSAGE(g.xOffset + g.width <= g.advance, message);
      TEST_ASSERT_TRUE_MESSAGE(g.yOffset >= 0 && g.yOffset + g.height <= f.lineHeight(), message);
    }
  }
}

void test_line_heights_and_proportions() {
  struct Expect { FontSize s; int line, ascent, cap; };
  for (const Expect& e : {Expect{FontSize::Small, 17, 14, 11}, Expect{FontSize::Text, 22, 18, 14},
                          Expect{FontSize::Tile, 29, 24, 19}, Expect{FontSize::Large, 38, 31, 25}}) {
    const Font& f = font(e.s);
    TEST_ASSERT_EQUAL_MESSAGE(e.line, f.lineHeight(), sizeName(e.s));
    TEST_ASSERT_EQUAL_MESSAGE(e.ascent, f.ascent, sizeName(e.s));
    TEST_ASSERT_EQUAL_MESSAGE(e.cap, f.capHeight, sizeName(e.s));
  }
}

void test_a_font_lookup_does_not_confuse_neighbouring_ranges() {
  // The ranges join and part in several places (U+007E | U+00A0, U+00FF | U+0152, ...); the
  // characters on both sides of each seam must be their own glyphs.
  for (FontSize s : kSizes) {
    const Font& f = font(s);
    TEST_ASSERT_TRUE(f.find(0x7E) != nullptr);
    TEST_ASSERT_TRUE(f.find(0x7F) == nullptr);
    TEST_ASSERT_TRUE(f.find(0x9F) == nullptr);
    TEST_ASSERT_TRUE(f.find(0xA0) != nullptr);
    TEST_ASSERT_TRUE(f.find(0xFF) != nullptr);
    TEST_ASSERT_TRUE(f.find(0x100) == nullptr);
    TEST_ASSERT_TRUE(f.find(0x151) == nullptr);
    TEST_ASSERT_TRUE(f.find(0x152) != nullptr && f.find(0x153) != nullptr);
    TEST_ASSERT_TRUE(f.find(0x154) == nullptr);
    TEST_ASSERT_TRUE(f.find(0x177) == nullptr);
    TEST_ASSERT_TRUE(f.find(0x178) != nullptr);
    TEST_ASSERT_TRUE(f.find(0x179) == nullptr);
    TEST_ASSERT_TRUE(f.find(0x20AC) != nullptr);
    TEST_ASSERT_TRUE(f.find(0x20AB) == nullptr && f.find(0x20AD) == nullptr);
    TEST_ASSERT_TRUE(f.find('A') != f.find('B'));
    TEST_ASSERT_TRUE(f.find(0xE4) != f.find(0xE5));  // a with diaeresis, a with ring
  }
}

// ---- measuring, fitting ---------------------------------------------------------------------

void test_width_is_the_sum_of_the_advances() {
  const Font& f = font(FontSize::Text);
  TEST_ASSERT_EQUAL(0, width(f, ""));
  TEST_ASSERT_EQUAL(f.glyph('H').advance, width(f, "H"));
  TEST_ASSERT_EQUAL(f.glyph('H').advance + f.glyph('i').advance, width(f, "Hi"));
  TEST_ASSERT_EQUAL(f.glyph(0xFC).advance, width(f, "\xC3\xBC"));   // one u with diaeresis is one glyph
  TEST_ASSERT_EQUAL(f.glyphs[0].advance, width(f, "\xFF"));        // malformed: one replacement
  TEST_ASSERT_EQUAL(2 * f.glyphs[0].advance, width(f, "\xFF\xFE"));
  TEST_ASSERT_EQUAL(f.glyphs[0].advance, width(f, "\n"));          // no glyph for a control character
}

void test_fit_stops_at_the_exact_boundary() {
  const Font& f = font(FontSize::Text);
  const std::string_view hello = "Hello";
  const int full = width(f, hello);
  TEST_ASSERT_EQUAL(5, fit(f, hello, full));
  TEST_ASSERT_EQUAL(4, fit(f, hello, full - 1));
  TEST_ASSERT_EQUAL(5, fit(f, hello, full + 100));
  TEST_ASSERT_EQUAL(0, fit(f, hello, 0));
  TEST_ASSERT_EQUAL(0, fit(f, hello, -5));
  TEST_ASSERT_EQUAL(0, fit(f, "", 100));
  TEST_ASSERT_EQUAL(1, fit(f, hello, f.glyph('H').advance));
}

void test_fit_never_cuts_inside_a_character() {
  const Font& f = font(FontSize::Text);
  const std::string_view text = "a\xC3\xBC" "b";  // a, u with diaeresis, b
  TEST_ASSERT_EQUAL(1, fit(f, text, width(f, "a")));
  TEST_ASSERT_EQUAL(1, fit(f, text, width(f, "a") + f.glyph(0xFC).advance - 1));
  TEST_ASSERT_EQUAL(3, fit(f, text, width(f, "a") + f.glyph(0xFC).advance));
  const std::string_view euro = "\xE2\x82\xAC";
  TEST_ASSERT_EQUAL(0, fit(f, euro, f.glyph(0x20AC).advance - 1));
  TEST_ASSERT_EQUAL(3, fit(f, euro, f.glyph(0x20AC).advance));
}

// ---- ellipsize ------------------------------------------------------------------------------

void test_ellipsize_leaves_text_that_fits_alone() {
  const Font& f = font(FontSize::Text);
  TEST_ASSERT_EQUAL_STRING("Parent One", ellipsize(f, "Parent One", 240).c_str());
  TEST_ASSERT_EQUAL_STRING("Parent One", ellipsize(f, "Parent One", width(f, "Parent One")).c_str());  // exactly
  TEST_ASSERT_EQUAL_STRING("", ellipsize(f, "", 0).c_str());
}

void test_ellipsize_shortens_to_the_width_and_is_maximal() {
  const Font& f = font(FontSize::Text);
  const std::string text = "Alex Example from the football club on Tuesday afternoon";
  for (int limit = 40; limit <= 240; limit += 7) {
    const std::string out = ellipsize(f, text, limit);
    TEST_ASSERT_TRUE(width(f, out) <= limit);
    // The longest prefix that leaves room for the dots, found the slow way (the text is ASCII).
    size_t n = 0;
    while (n < text.size() && width(f, text.substr(0, n + 1)) + width(f, "...") <= limit) ++n;
    while (n > 0 && text[n - 1] == ' ') --n;
    TEST_ASSERT_EQUAL_STRING((text.substr(0, n) + "...").c_str(), out.c_str());
  }
}

void test_ellipsize_puts_no_space_before_the_dots() {
  const Font& f = font(FontSize::Text);
  // Room for "ab c" plus the dots, but the cut would fall right behind a space.
  const int limit = width(f, "ab ") + width(f, "...");
  TEST_ASSERT_EQUAL_STRING("ab...", ellipsize(f, "ab cdefghijkl", limit).c_str());
  TEST_ASSERT_EQUAL_STRING("ab c...", ellipsize(f, "ab cdefghijkl", limit + f.glyph('c').advance).c_str());
}

void test_ellipsize_gives_nothing_when_not_even_the_dots_fit() {
  const Font& f = font(FontSize::Text);
  const int dots = width(f, "...");
  TEST_ASSERT_EQUAL_STRING("", ellipsize(f, "Hello world", dots - 1).c_str());
  TEST_ASSERT_EQUAL_STRING("", ellipsize(f, "Hello world", 0).c_str());
  TEST_ASSERT_EQUAL_STRING("...", ellipsize(f, "Hello world", dots).c_str());  // just the dots
}

void test_ellipsize_does_not_break_a_multibyte_character() {
  const Font& f = font(FontSize::Text);
  const std::string text = "caf\xC3\xA9 na\xC3\xAFve r\xC3\xA9sum\xC3\xA9 and more words";  // cafe, naive, resume with accents
  for (int limit = 30; limit < 200; limit += 3) {
    const std::string out = ellipsize(f, text, limit);
    size_t pos = 0;
    while (pos < out.size()) TEST_ASSERT_NOT_EQUAL(kReplacementChar, decode(out, pos));
  }
}

// ---- wrap -----------------------------------------------------------------------------------

namespace {
std::string joined(const std::vector<std::string_view>& lines) {
  std::string out;
  for (size_t i = 0; i < lines.size(); ++i) out += (i ? "|" : "") + std::string(lines[i]);
  return out;
}
}  // namespace

void test_wrap_breaks_at_spaces_and_drops_them() {
  const Font& f = font(FontSize::Text);
  const int w = width(f, "aa bb");
  TEST_ASSERT_EQUAL_STRING("aa bb|cc dd", joined(wrap(f, "aa bb cc dd", w)).c_str());
  TEST_ASSERT_EQUAL_STRING("aa bb cc dd", joined(wrap(f, "aa bb cc dd", 1000)).c_str());
}

void test_wrap_line_exactly_as_wide_as_the_limit_stays_whole() {
  const Font& f = font(FontSize::Text);
  const int w = width(f, "aa aa");
  TEST_ASSERT_EQUAL_STRING("aa aa|aa", joined(wrap(f, "aa aa aa", w)).c_str());
  TEST_ASSERT_EQUAL_STRING("aa|aa|aa", joined(wrap(f, "aa aa aa", w - 1)).c_str());
}

void test_wrap_hard_breaks_blank_lines_and_empty_text() {
  const Font& f = font(FontSize::Text);
  TEST_ASSERT_EQUAL_STRING("a|b", joined(wrap(f, "a\nb", 200)).c_str());
  TEST_ASSERT_EQUAL_STRING("a||b", joined(wrap(f, "a\n\nb", 200)).c_str());
  TEST_ASSERT_EQUAL_STRING("a|", joined(wrap(f, "a\n", 200)).c_str());
  TEST_ASSERT_EQUAL(1, wrap(f, "", 200).size());  // one empty line, like "".split("\n")
  TEST_ASSERT_EQUAL(0, wrap(f, "", 200)[0].size());
}

void test_wrap_splits_a_word_that_is_wider_than_a_line() {
  const Font& f = font(FontSize::Text);
  const std::string word = "Donaudampfschifffahrt";
  const int limit = width(f, "Donaudampf");
  const std::string text = "a " + word + " b";  // the lines are views into it
  const auto lines = wrap(f, text, limit);
  TEST_ASSERT_TRUE(lines.size() >= 3);
  std::string rebuilt;
  for (auto l : lines) {
    TEST_ASSERT_TRUE(width(f, l) <= limit);
    for (char c : l) if (c != ' ') rebuilt += c;
  }
  const std::string expected = "a" + word + "b";
  TEST_ASSERT_EQUAL_STRING(expected.c_str(), rebuilt.c_str());
}

void test_wrap_collapses_spaces_only_at_the_break() {
  const Font& f = font(FontSize::Text);
  TEST_ASSERT_EQUAL_STRING("a  b", joined(wrap(f, "a  b", 1000)).c_str());                // inside a line: kept
  const int w = width(f, "aaa");
  TEST_ASSERT_EQUAL_STRING("aaa|aaa", joined(wrap(f, "aaa     aaa", w)).c_str());        // at the break: dropped
  TEST_ASSERT_EQUAL_STRING("aaa|aaa", joined(wrap(f, "aaa aaa   ", w)).c_str());         // trailing: dropped
  TEST_ASSERT_EQUAL_STRING("aaa", joined(wrap(f, "   aaa", 1000)).c_str());              // leading: dropped
  TEST_ASSERT_EQUAL_STRING("aaa", joined(wrap(f, "aaa   ", 1000)).c_str());              // trailing, last line
  // A break behind several spaces that still fit on the line: they are not part of the line.
  TEST_ASSERT_EQUAL_STRING("aaa|aaa", joined(wrap(f, "aaa     aaa", width(f, "aaa   "))).c_str());
}

void test_wrap_always_ends_even_if_a_character_is_wider_than_the_line() {
  const Font& f = font(FontSize::Large);
  const auto lines = wrap(f, "WW \xC3\xBC", 1);
  TEST_ASSERT_EQUAL_STRING("W|W|\xC3\xBC", joined(lines).c_str());
  TEST_ASSERT_EQUAL(0, wrap(f, "", 1)[0].size());
}

void test_wrap_keeps_all_text_and_respects_the_width_for_every_limit() {
  const Font& f = font(FontSize::Text);
  const std::string text = "Please call back when you can, it is not urgent.\nThe key is under the "
                           "flower pot \xE2\x82\xAC 5 for the ice cream.";
  for (int limit = 30; limit <= 240; ++limit) {
    std::string rebuilt;
    for (auto line : wrap(f, text, limit)) {
      TEST_ASSERT_TRUE(width(f, line) <= limit);
      TEST_ASSERT_TRUE(line.data() >= text.data() && line.data() + line.size() <= text.data() + text.size());
      for (char c : line) if (c != ' ') rebuilt += c;
    }
    std::string expected;
    for (char c : text) if (c != ' ' && c != '\n') expected += c;
    TEST_ASSERT_EQUAL_STRING(expected.c_str(), rebuilt.c_str());
  }
}

// ---- drawing --------------------------------------------------------------------------------

void test_the_top_of_a_capital_is_cap_height_below_the_ascent_line() {
  for (FontSize s : kSizes) {
    const Font& f = font(s);
    FrameBuffer fb(80, 60);
    draw(fb, f, 5, 7, "H");
    int top, bottom;
    TEST_ASSERT_TRUE(inkRows(fb, &top, &bottom));
    TEST_ASSERT_EQUAL_MESSAGE(7 + f.ascent - f.capHeight, top, at(s, "top of H").c_str());
    TEST_ASSERT_EQUAL_MESSAGE(7 + f.ascent - 1, bottom, at(s, "H sits on the baseline").c_str());
  }
}

void test_descenders_hang_below_the_baseline_but_inside_the_line() {
  for (FontSize s : kSizes) {
    const Font& f = font(s);
    FrameBuffer fb(80, 60);
    draw(fb, f, 5, 7, "gy");
    int top, bottom;
    TEST_ASSERT_TRUE(inkRows(fb, &top, &bottom));
    TEST_ASSERT_TRUE_MESSAGE(bottom >= 7 + f.ascent, at(s, "g reaches below the baseline").c_str());
    TEST_ASSERT_TRUE_MESSAGE(bottom < 7 + f.lineHeight(), at(s, "g stays inside the line").c_str());
  }
}

void test_draw_returns_the_width_and_draws_nothing_for_empty_text() {
  const Font& f = font(FontSize::Text);
  FrameBuffer fb(100, 40);
  TEST_ASSERT_EQUAL(width(f, "Hello"), draw(fb, f, 3, 3, "Hello"));
  const int before = fb.blackPixels();
  TEST_ASSERT_EQUAL(0, draw(fb, f, 3, 3, ""));
  TEST_ASSERT_EQUAL(before, fb.blackPixels());
}

void test_white_text_erases_black_text_and_leaves_the_rest() {
  const Font& f = font(FontSize::Text);
  FrameBuffer fb(100, 40);
  draw(fb, f, 3, 3, "Hello");
  draw(fb, f, 3, 3, "Hello", false);
  TEST_ASSERT_EQUAL(0, fb.blackPixels());
  fb.fillRect({0, 0, 100, 40}, true);
  draw(fb, f, 3, 3, "Hi", false);
  TEST_ASSERT_TRUE(fb.blackPixels() < 100 * 40);
  TEST_ASSERT_TRUE(fb.pixel(99, 39));
}

void test_a_space_draws_nothing_but_advances() {
  const Font& f = font(FontSize::Text);
  FrameBuffer fb(100, 40);
  TEST_ASSERT_EQUAL(f.glyph(' ').advance, draw(fb, f, 3, 3, " "));
  TEST_ASSERT_EQUAL(0, fb.blackPixels());
  FrameBuffer a(100, 40), b(100, 40);
  draw(a, f, 3, 3, "H");
  draw(b, f, 3 + f.glyph(' ').advance, 3, " H");
  draw(b, f, 3, 3, "H", false);  // erase the first H from b: only the shifted H is left
  FrameBuffer c(100, 40);
  draw(c, f, 3 + f.glyph(' ').advance, 3, "H");
  TEST_ASSERT_TRUE(!(a == c));   // the shift moved it
}

void test_centered_and_right_aligned_text_sit_where_the_arithmetic_says() {
  const Font& f = font(FontSize::Text);
  const int w = width(f, "OK");
  FrameBuffer centred(200, 40), plain(200, 40);
  const Rect box{20, 0, 101, 40};
  TEST_ASSERT_EQUAL(w, drawCentered(centred, f, box, 5, "OK"));
  draw(plain, f, 20 + (101 - w) / 2, 5, "OK");
  TEST_ASSERT_TRUE(centred == plain);

  FrameBuffer right(200, 40), plain2(200, 40);
  TEST_ASSERT_EQUAL(w, drawRight(right, f, 150, 5, "OK"));
  draw(plain2, f, 150 - w, 5, "OK");
  TEST_ASSERT_TRUE(right == plain2);
}

void test_centered_text_wider_than_the_box_starts_at_its_left_edge() {
  const Font& f = font(FontSize::Text);
  FrameBuffer centred(200, 40), plain(200, 40);
  const Rect box{20, 0, 10, 40};
  drawCentered(centred, f, box, 5, "wide text");
  draw(plain, f, 20, 5, "wide text");
  TEST_ASSERT_TRUE(centred == plain);
}

// ---- clipping -------------------------------------------------------------------------------

void test_text_that_runs_over_the_right_edge_of_its_region_is_reported() {
  const Font& f = font(FontSize::Text);
  FrameBuffer fb;
  const std::string text = "A long line of text that cannot fit";
  fb.beginRegion({10, 10, 100, f.lineHeight()});
  draw(fb, f, 10, 10, text);
  fb.endRegion();
  TEST_ASSERT_TRUE(fb.clipped());
}

void test_text_that_was_fitted_does_not_clip() {
  const Font& f = font(FontSize::Text);
  FrameBuffer fb;
  const Rect region{10, 10, 100, f.lineHeight()};
  const std::string text = "A long line of text that cannot fit";
  fb.beginRegion(region);
  draw(fb, f, region.x, region.y, ellipsize(f, text, region.w));
  fb.endRegion();
  TEST_ASSERT_FALSE_MESSAGE(fb.clipped(), fb.describeClips().c_str());
  // Every wrapped line fits too, one region per line.
  const auto lines = wrap(f, text, region.w);
  for (size_t i = 0; i < lines.size(); ++i) {
    const Rect line{10, 40 + (int)i * f.lineHeight(), region.w, f.lineHeight()};
    fb.beginRegion(line);
    draw(fb, f, line.x, line.y, lines[i]);
    fb.endRegion();
  }
  TEST_ASSERT_FALSE_MESSAGE(fb.clipped(), fb.describeClips().c_str());
}

void test_a_region_exactly_as_wide_as_the_text_is_enough_for_every_character() {
  // The cell test above gives the reason; this checks it end to end through the frame buffer.
  const std::set<uint32_t> chars = charsetFromFile();
  for (FontSize s : kSizes) {
    const Font& f = font(s);
    for (uint32_t c : chars) {
      std::string utf8;
      if (c < 0x80) utf8 = std::string(1, (char)c);
      else if (c < 0x800) utf8 = {(char)(0xC0 | c >> 6), (char)(0x80 | (c & 0x3F))};
      else if (c < 0x10000) utf8 = {(char)(0xE0 | c >> 12), (char)(0x80 | ((c >> 6) & 0x3F)), (char)(0x80 | (c & 0x3F))};
      else utf8 = {(char)(0xF0 | c >> 18), (char)(0x80 | ((c >> 12) & 0x3F)), (char)(0x80 | ((c >> 6) & 0x3F)), (char)(0x80 | (c & 0x3F))};
      FrameBuffer fb(100, 60);
      fb.beginRegion({10, 10, width(f, utf8), f.lineHeight()});
      draw(fb, f, 10, 10, utf8);
      if (fb.clipped()) {
        char message[100];
        snprintf(message, sizeof message, "%s U+%04X clips", sizeName(s), c);
        TEST_FAIL_MESSAGE(message);
      }
    }
  }
}

// ---- snapshots ------------------------------------------------------------------------------

void test_snapshot_latin1_text() {
  const Font& f = font(FontSize::Text);
  FrameBuffer fb(150, 60);
  draw(fb, f, 4, 4, "Gr\xC3\xBC\xC3\x9F" "e, \xC3\xA9t\xC3\xA9 \xC3\x85");     // umlaut, sharp s, acute, A with ring
  draw(fb, f, 4, 30, "\xC5\x93uvre \xC5\x92 \xC5\xB8 \xE2\x82\xAC 5");         // œuvre Œ Ÿ € 5
  golden::expect(fb, "text_latin1_text");
}

void test_snapshot_symbols_in_the_small_font() {
  FrameBuffer fb(150, 60);
  const Font& f = font(FontSize::Small);
  draw(fb, f, 4, 4, "\xE2\x86\xB5 \xE2\x8C\xAB \xE2\x96\xB2 \xE2\x96\xBC \xE2\x98\x8E \xE2\x9C\x89");
  draw(fb, f, 4, 30, "\xE2\x9C\x88 \xF0\x9F\x92\xAC \xF0\x9F\x93\xA1 \xF0\x9F\x93\x8D");
  golden::expect(fb, "text_symbols_small");
}

void test_snapshot_symbols_in_the_tile_font() {
  FrameBuffer fb(200, 80);
  const Font& f = font(FontSize::Tile);
  draw(fb, f, 4, 4, "\xE2\x98\x8E \xE2\x9C\x89 \xE2\x9C\x88 \xF0\x9F\x92\xAC");
  draw(fb, f, 4, 40, "\xF0\x9F\x93\xA1 \xF0\x9F\x93\x8D \xE2\x86\xB5 \xE2\x8C\xAB");
  golden::expect(fb, "text_symbols_tile");
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_decode_valid_sequences_of_every_length);
  RUN_TEST(test_decode_a_lone_continuation_or_invalid_leader_is_one_replacement);
  RUN_TEST(test_decode_leaders_beyond_f4_are_invalid_even_with_continuations);
  RUN_TEST(test_decode_truncated_sequence_consumes_only_what_belongs_to_it);
  RUN_TEST(test_decode_overlong_surrogate_and_out_of_range_are_rejected);
  RUN_TEST(test_decode_always_makes_progress_on_every_byte_pair);
  RUN_TEST(test_every_font_has_exactly_the_characters_of_the_charset);
  RUN_TEST(test_characters_outside_the_charset_get_the_replacement_glyph);
  RUN_TEST(test_the_ink_of_every_glyph_stays_inside_its_cell);
  RUN_TEST(test_line_heights_and_proportions);
  RUN_TEST(test_a_font_lookup_does_not_confuse_neighbouring_ranges);
  RUN_TEST(test_width_is_the_sum_of_the_advances);
  RUN_TEST(test_fit_stops_at_the_exact_boundary);
  RUN_TEST(test_fit_never_cuts_inside_a_character);
  RUN_TEST(test_ellipsize_leaves_text_that_fits_alone);
  RUN_TEST(test_ellipsize_shortens_to_the_width_and_is_maximal);
  RUN_TEST(test_ellipsize_puts_no_space_before_the_dots);
  RUN_TEST(test_ellipsize_gives_nothing_when_not_even_the_dots_fit);
  RUN_TEST(test_ellipsize_does_not_break_a_multibyte_character);
  RUN_TEST(test_wrap_breaks_at_spaces_and_drops_them);
  RUN_TEST(test_wrap_line_exactly_as_wide_as_the_limit_stays_whole);
  RUN_TEST(test_wrap_hard_breaks_blank_lines_and_empty_text);
  RUN_TEST(test_wrap_splits_a_word_that_is_wider_than_a_line);
  RUN_TEST(test_wrap_collapses_spaces_only_at_the_break);
  RUN_TEST(test_wrap_always_ends_even_if_a_character_is_wider_than_the_line);
  RUN_TEST(test_wrap_keeps_all_text_and_respects_the_width_for_every_limit);
  RUN_TEST(test_the_top_of_a_capital_is_cap_height_below_the_ascent_line);
  RUN_TEST(test_descenders_hang_below_the_baseline_but_inside_the_line);
  RUN_TEST(test_draw_returns_the_width_and_draws_nothing_for_empty_text);
  RUN_TEST(test_white_text_erases_black_text_and_leaves_the_rest);
  RUN_TEST(test_a_space_draws_nothing_but_advances);
  RUN_TEST(test_centered_and_right_aligned_text_sit_where_the_arithmetic_says);
  RUN_TEST(test_centered_text_wider_than_the_box_starts_at_its_left_edge);
  RUN_TEST(test_text_that_runs_over_the_right_edge_of_its_region_is_reported);
  RUN_TEST(test_text_that_was_fitted_does_not_clip);
  RUN_TEST(test_a_region_exactly_as_wide_as_the_text_is_enough_for_every_character);
  RUN_TEST(test_snapshot_latin1_text);
  RUN_TEST(test_snapshot_symbols_in_the_small_font);
  RUN_TEST(test_snapshot_symbols_in_the_tile_font);
  return UNITY_END();
}

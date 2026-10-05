#pragma once

// Finds a text in a picture: "does this screen say 'School time until 15:00'?" A screen draws its
// words with the text engine, so the words can be found by drawing the same text into a small
// buffer and looking for exactly that pattern in the picture, at any position. The test then asserts
// on what the child reads, in the language of the moment, not on how the code got there — and a
// word that is drawn in the wrong font, cut off or covered fails it.
//
//   TEST_ASSERT_TRUE(shows::text(fb, "Tuesday", FontSize::Text));
//   const Rect at = shows::where(fb, "Inbox", FontSize::Small).value();
//
// The match is exact over the whole box of the text (the width of the text, the height of the
// line), background included, and the three columns on either side must be background too, so
// "Test" is not found inside "Tests" and a word that another drawing overlaps is not found. Text on a black ground (an inverted tile, the notice line) is
// found with `onBlack`.

#include <optional>
#include <string>

#include <FrameBuffer.h>
#include <ui/Fonts.h>
#include <ui/Text.h>

namespace shows {

namespace detail {
constexpr int kMargin = 3;

inline bool matchesAt(const FrameBuffer& fb, const FrameBuffer& pattern, int x, int y, bool onBlack) {
  for (int j = 0; j < pattern.height(); ++j) {
    for (int i = 0; i < pattern.width(); ++i)
      if (fb.pixel(x + i, y + j) != (pattern.pixel(i, j) != onBlack)) return false;
    // Nothing of a longer word beside it: the background, where there is a picture to look at.
    for (int m = 1; m <= kMargin; ++m) {
      if (x - m >= 0 && fb.pixel(x - m, y + j) != onBlack) return false;
      if (x + pattern.width() - 1 + m < fb.width() && fb.pixel(x + pattern.width() - 1 + m, y + j) != onBlack) return false;
    }
  }
  return true;
}
}  // namespace detail

// The top-left corner and size of the first place (reading order) where `text` is drawn in `size`
// within `within` (the whole picture by default); nothing when it is not there.
inline std::optional<ui::Rect> where(const FrameBuffer& fb, const std::string& text, ui::FontSize size,
                                     ui::Rect within = ui::Rect{}, bool onBlack = false) {
  const ui::Font& font = ui::font(size);
  const int w = ui::text::width(font, text);
  if (text.empty() || w <= 0) return std::nullopt;
  FrameBuffer pattern(w, font.lineHeight());
  ui::text::draw(pattern, font, 0, 0, text);
  if (within.empty()) within = {0, 0, fb.width(), fb.height()};
  for (int y = within.y; y + pattern.height() <= within.bottom(); ++y)
    for (int x = within.x; x + w <= within.right(); ++x)
      if (detail::matchesAt(fb, pattern, x, y, onBlack)) return ui::Rect{x, y, w, pattern.height()};
  return std::nullopt;
}

inline bool text(const FrameBuffer& fb, const std::string& words, ui::FontSize size, ui::Rect within = ui::Rect{},
                 bool onBlack = false) {
  return where(fb, words, size, within, onBlack).has_value();
}

}  // namespace shows

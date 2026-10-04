#pragma once

// The text engine: measures, shortens, wraps and draws UTF-8 text with a ui::Font. Every
// text of the interface goes through here, so that what a test measures is what the panel shows.
//
// Conventions:
//  * `y` is the top of the line, not the baseline.
//  * Width is the sum of the advances (the pen position after the last character), not the
//    ink's bounding box.
//  * Nothing here wraps or clips by itself. draw() puts the pixels where the text belongs;
//    what runs over the right edge is the caller's mistake, which the frame buffer used in
//    tests reports as a clip (testing/FrameBuffer.h). Use fit(), ellipsize() or wrap() first.
//  * A byte sequence that is not valid UTF-8 and a character the font lacks are drawn as the
//    font's replacement glyph, one per malformed sequence, so the fault is visible and the
//    rest of the text is still there.
//  * Control characters, including '\n', have no glyph. Split text into lines with wrap().

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ui/Display.h"
#include "ui/Font.h"

namespace ui {
namespace text {

constexpr uint32_t kReplacementChar = 0xFFFD;

// The code point starting at text[pos], and `pos` moved behind it. Malformed input gives
// U+FFFD and consumes the longest prefix that could still have become valid (the rule of
// the WHATWG Encoding Standard), so one broken character is one replacement, and decoding
// always makes progress. `pos` must be below text.size().
uint32_t decode(std::string_view text, size_t& pos);

// Width in pixels.
int width(const Font& font, std::string_view text);

// The length in bytes of the longest prefix, ending between two characters, that is at most
// `maxWidth` pixels wide.
size_t fit(const Font& font, std::string_view text, int maxWidth);

// The text if it fits into `maxWidth`, otherwise as much of its start as leaves room for "..."
// (no space before the dots). Empty when not even the dots fit. **Only for text that comes
// from the user** (names, message bodies, contact labels); a fixed label that does not fit is a
// layout bug and is never shortened.
std::string ellipsize(const Font& font, std::string_view text, int maxWidth);

// Breaks the text into lines of at most `maxWidth` pixels: at '\n' always, otherwise at spaces,
// and inside a word only when the word alone is wider than a line. Spaces at the break are
// dropped; an empty paragraph is an empty line. The views point into `text`. A single
// character wider than `maxWidth` still gets a line of its own, so the result always ends.
std::vector<std::string_view> wrap(const Font& font, std::string_view text, int maxWidth);

// Draws the text with the left edge of its pen at `x`. Returns the width.
int draw(Display& display, const Font& font, int x, int y, std::string_view text, bool black = true);

// Centred in `box` (the text starts at box.x when it is wider). Returns the width.
int drawCentered(Display& display, const Font& font, const Rect& box, int y, std::string_view text,
                 bool black = true);

// Ending at `right` (the first column behind the text). Returns the width.
int drawRight(Display& display, const Font& font, int right, int y, std::string_view text,
              bool black = true);

}  // namespace text
}  // namespace ui

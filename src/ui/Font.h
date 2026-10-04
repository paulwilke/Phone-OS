#pragma once

// A bitmap font: the data format of the generated files in src/ui/fonts/ (written by
// scripts/make_font.py) and nothing else. Our own format, so that the text engine does not
// depend on a graphics library and the fonts can be tested on the host.
//
// Positions are measured from the top-left corner of the line: a glyph's bitmap is drawn at
// (x + xOffset, top + yOffset), and the pen then moves right by `advance`. The bitmaps use the
// layout of ui::Display::drawBitmap (rows padded to a byte, leftmost pixel in the top bit).

#include <cstddef>
#include <cstdint>

namespace ui {

struct Glyph {
  uint16_t offset;   // start of the bitmap in Font::bitmaps
  uint8_t width;
  uint8_t height;
  int8_t xOffset;    // bitmap's left edge relative to the pen
  int8_t yOffset;    // bitmap's top edge relative to the top of the line
  uint8_t advance;
};

// The code points first..last (both included) have the glyphs index.. in Font::glyphs, in
// order. The ranges are sorted and do not overlap.
struct CodeRange {
  uint32_t first;
  uint32_t last;
  uint16_t index;
};

struct Font {
  const uint8_t* bitmaps;
  const Glyph* glyphs;       // glyphs[0] is the replacement glyph, drawn for a missing character
  const CodeRange* ranges;
  uint8_t rangeCount;
  uint8_t ascent;            // top of the line to the baseline
  uint8_t descent;           // baseline to the bottom of the line
  uint8_t capHeight;

  int lineHeight() const { return ascent + descent; }

  // The glyph of a code point, or nullptr when the font has none.
  const Glyph* find(uint32_t codePoint) const {
    size_t low = 0, high = rangeCount;
    while (low < high) {
      size_t mid = (low + high) / 2;
      if (codePoint < ranges[mid].first) high = mid;
      else if (codePoint > ranges[mid].last) low = mid + 1;
      else return &glyphs[ranges[mid].index + (codePoint - ranges[mid].first)];
    }
    return nullptr;
  }

  // The glyph to draw for a code point: its own, or the replacement glyph.
  const Glyph& glyph(uint32_t codePoint) const {
    const Glyph* g = find(codePoint);
    return g ? *g : glyphs[0];
  }
};

}  // namespace ui

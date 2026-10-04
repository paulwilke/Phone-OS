#include "ui/Text.h"

namespace ui {
namespace text {

uint32_t decode(std::string_view text, size_t& pos) {
  const auto at = [&](size_t i) { return static_cast<uint8_t>(text[i]); };
  const uint8_t first = at(pos++);
  if (first < 0x80) return first;

  int needed;
  uint32_t code;
  uint8_t lower = 0x80, upper = 0xBF;  // the range of the next byte; narrower for a few leaders
  if (first >= 0xC2 && first <= 0xDF) {
    needed = 1;
    code = first & 0x1F;
  } else if (first >= 0xE0 && first <= 0xEF) {
    needed = 2;
    code = first & 0x0F;
    if (first == 0xE0) lower = 0xA0;      // else overlong
    if (first == 0xED) upper = 0x9F;      // else a surrogate
  } else if (first >= 0xF0 && first <= 0xF4) {
    needed = 3;
    code = first & 0x07;
    if (first == 0xF0) lower = 0x90;      // else overlong
    if (first == 0xF4) upper = 0x8F;      // else beyond U+10FFFF
  } else {
    return kReplacementChar;              // continuation byte, C0, C1, F5..FF
  }

  for (int i = 0; i < needed; ++i) {
    if (pos >= text.size() || at(pos) < lower || at(pos) > upper) return kReplacementChar;
    code = (code << 6) | (at(pos++) & 0x3F);
    lower = 0x80;
    upper = 0xBF;
  }
  return code;
}

int width(const Font& font, std::string_view text) {
  int total = 0;
  for (size_t pos = 0; pos < text.size();) total += font.glyph(decode(text, pos)).advance;
  return total;
}

size_t fit(const Font& font, std::string_view text, int maxWidth) {
  int used = 0;
  for (size_t pos = 0; pos < text.size();) {
    const size_t start = pos;
    used += font.glyph(decode(text, pos)).advance;
    if (used > maxWidth) return start;
  }
  return text.size();
}

namespace {

std::string_view trimEnd(std::string_view s) {
  while (!s.empty() && s.back() == ' ') s.remove_suffix(1);
  return s;
}

}  // namespace

std::string ellipsize(const Font& font, std::string_view text, int maxWidth) {
  if (width(font, text) <= maxWidth) return std::string(text);
  const char* dots = "...";
  const int dotsWidth = width(font, dots);
  if (dotsWidth > maxWidth) return std::string();
  std::string_view head = trimEnd(text.substr(0, fit(font, text, maxWidth - dotsWidth)));
  return std::string(head) + dots;
}

std::vector<std::string_view> wrap(const Font& font, std::string_view text, int maxWidth) {
  std::vector<std::string_view> lines;
  size_t paragraph = 0;
  while (true) {
    const size_t newline = text.find('\n', paragraph);
    const std::string_view rest =
        text.substr(paragraph, newline == std::string_view::npos ? newline : newline - paragraph);

    size_t pos = 0;
    if (rest.empty()) lines.push_back(rest);
    while (pos < rest.size()) {
      while (pos < rest.size() && rest[pos] == ' ') ++pos;
      if (pos == rest.size()) break;
      const std::string_view tail = rest.substr(pos);
      size_t take = fit(font, tail, maxWidth);
      if (take == tail.size()) {
        lines.push_back(trimEnd(tail));
        break;
      }
      // Break at the last space that lets the line end there: the space may be the first
      // character that no longer fits.
      size_t at = tail.rfind(' ', take);
      if (at != std::string_view::npos) {
        take = at;
      } else if (take == 0) {
        decode(tail, take);  // a character wider than the line: one per line, so wrapping ends
      }
      lines.push_back(trimEnd(tail.substr(0, take)));
      pos += take;
    }

    if (newline == std::string_view::npos) break;
    paragraph = newline + 1;
  }
  return lines;
}

int draw(Display& display, const Font& font, int x, int y, std::string_view text, bool black) {
  int pen = x;
  for (size_t pos = 0; pos < text.size();) {
    const Glyph& g = font.glyph(decode(text, pos));
    if (g.width > 0 && g.height > 0)
      display.drawBitmap(pen + g.xOffset, y + g.yOffset, font.bitmaps + g.offset, g.width, g.height, black);
    pen += g.advance;
  }
  return pen - x;
}

int drawCentered(Display& display, const Font& font, const Rect& box, int y, std::string_view text,
                 bool black) {
  const int w = width(font, text);
  return draw(display, font, w < box.w ? box.x + (box.w - w) / 2 : box.x, y, text, black);
}

int drawRight(Display& display, const Font& font, int right, int y, std::string_view text, bool black) {
  return draw(display, font, right - width(font, text), y, text, black);
}

}  // namespace text
}  // namespace ui

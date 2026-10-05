#include "ui/screens/ClockScreen.h"

#include <string>
#include <string_view>
#include <vector>

#include "ui/Fonts.h"
#include "ui/Text.h"
#include "ui/Wording.h"

namespace ui {

namespace {

// One part of the block in the middle of the page: its lines, wrapped to the width, in its font.
struct Part {
  const Font* font;
  std::vector<std::string> lines;
  int gapBefore;
};

Part part(const Font& font, const std::string& words, int width, int gapBefore) {
  Part p{&font, {}, gapBefore};
  for (std::string_view line : text::wrap(font, words, width)) p.lines.emplace_back(line);
  return p;
}

}  // namespace

void ClockScreen::draw(Display& display, const Rect& area) {
  const AppCore::State::WallClock& wall = state().wall;
  // "--:--" for a time that is not known: digits and punctuation, the same in every language.
  const std::string time = wall.known ? wording::clockTime(wall.minuteOfDay) : "--:--";
  const int width = area.w - 8;

  // The time, the weekday, the date, and a note on where the time comes from (or that there is
  // none). The words are wrapped rather than run over the edge in a language that needs more room.
  std::vector<Part> parts;
  parts.push_back(part(font(FontSize::Large), time, width, 0));
  if (wall.known) {
    parts.push_back(part(font(FontSize::Text), wording::weekdayName(wall, t()), width, 6));
    parts.push_back(part(font(FontSize::Text), wording::dateLine(wall, t()), width, 2));
  }
  parts.push_back(part(font(FontSize::Small), t().get(wall.known ? "clock.source" : "clock.no-time"), width, 8));

  int height = 0;
  for (const Part& p : parts) height += p.gapBefore + static_cast<int>(p.lines.size()) * p.font->lineHeight();
  int y = area.y + (area.h - height) / 2;
  for (const Part& p : parts) {
    y += p.gapBefore;
    for (const std::string& line : p.lines) {
      text::drawCentered(display, *p.font, area, y, line);
      y += p.font->lineHeight();
    }
  }
}

SoftKeys ClockScreen::keys() {
  SoftKeys keys;
  keys.back = t().get("key.back");
  return keys;
}

bool ClockScreen::onInput(const Input& input, const Rect&) {
  if (!input.isKey(Key::Back)) return false;
  nav().back();
  return true;
}

}  // namespace ui

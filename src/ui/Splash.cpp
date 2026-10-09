#include "ui/Splash.h"

#include "ui/Fonts.h"
#include "ui/Text.h"

namespace ui {

void drawSplash(Display& display, const std::string& name, const std::string& board, const std::string& version,
                const std::string& line) {
  const Rect screen{0, 0, kScreenWidth, kScreenHeight};
  const Font& large = font(FontSize::Large);
  const Font& body = font(FontSize::Text);
  const Font& small = font(FontSize::Small);
  // The name is wrapped, never shortened, if it is ever wider than a line ("Phone OS" is 156 px of the 240); the
  // picture then starts higher by half of each further line, so that it stays in the middle.
  const std::vector<std::string_view> nameLines = text::wrap(large, name, kScreenWidth - 24);
  int y = 110 - large.lineHeight() * ((int)nameLines.size() - 1) / 2;
  for (std::string_view part : nameLines) {
    text::drawCentered(display, large, screen, y, part);
    y += large.lineHeight();
  }
  y += 8;
  text::drawCentered(display, body, screen, y, board);
  y += body.lineHeight() + 4;
  text::drawCentered(display, small, screen, y, version);
  if (line.empty()) return;
  y += small.lineHeight() + 30;
  for (std::string_view part : text::wrap(body, line, kScreenWidth - 24)) {
    text::drawCentered(display, body, screen, y, part);
    y += body.lineHeight();
  }
}

}  // namespace ui

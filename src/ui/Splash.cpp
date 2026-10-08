#include "ui/Splash.h"

#include "ui/Fonts.h"
#include "ui/Text.h"

namespace ui {

void drawSplash(Display& display, const std::string& board, const std::string& version, const std::string& line) {
  const Rect screen{0, 0, kScreenWidth, kScreenHeight};
  const Font& large = font(FontSize::Large);
  const Font& body = font(FontSize::Text);
  const Font& small = font(FontSize::Small);
  int y = 110;
  text::drawCentered(display, large, screen, y, "PhonePlus");
  y += large.lineHeight() + 8;
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

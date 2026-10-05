#include "ui/PageTitle.h"

#include <string_view>

#include "ui/Fonts.h"
#include "ui/Text.h"

namespace ui {

namespace {
constexpr int kGap = 4;
constexpr int kInset = 4;
}  // namespace

int pageTitleHeight(const Rect& area, const std::string& title) {
  const Font& small = font(FontSize::Small);
  return static_cast<int>(text::wrap(small, title, area.w - 2 * kInset).size()) * small.lineHeight() + 2 * kGap + 1;
}

int drawPageTitle(Display& display, const Rect& area, const std::string& title) {
  const Font& small = font(FontSize::Small);
  int y = area.y;
  for (std::string_view line : text::wrap(small, title, area.w - 2 * kInset)) {
    text::draw(display, small, area.x + kInset, y, line);
    y += small.lineHeight();
  }
  y += kGap / 2;
  display.fillRect({area.x, y, area.w, 1}, true);
  return pageTitleHeight(area, title);
}

}  // namespace ui

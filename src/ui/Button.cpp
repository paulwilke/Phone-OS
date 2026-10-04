#include "ui/Button.h"

#include "ui/Fonts.h"
#include "ui/Shapes.h"
#include "ui/Text.h"

namespace ui {

void Button::draw(Display& display, const Rect& area) const {
  const Font& font = ui::font(FontSize::Text);
  if (focused) shapes::fillRoundRect(display, area, 4, true);
  else shapes::outlineRoundRect(display, area, 4, true);
  const int top = area.y + (area.h - font.lineHeight()) / 2;
  text::drawCentered(display, font, area, top, label, !focused);
}

}  // namespace ui

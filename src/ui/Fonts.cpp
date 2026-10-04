#include "ui/Fonts.h"

#include "ui/fonts/FontLarge.h"
#include "ui/fonts/FontSmall.h"
#include "ui/fonts/FontText.h"
#include "ui/fonts/FontTile.h"

namespace ui {

const Font& font(FontSize size) {
  switch (size) {
    case FontSize::Small: return fonts::kFontSmall;
    case FontSize::Tile: return fonts::kFontTile;
    case FontSize::Large: return fonts::kFontLarge;
    case FontSize::Text: break;
  }
  return fonts::kFontText;
}

}  // namespace ui

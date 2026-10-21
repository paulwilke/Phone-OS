#pragma once

// The four fonts of the interface. Sizes are named by their role; the pixel sizes are in
// docs/ui/ui-text.md.

#include "ui/Font.h"

namespace ui {

enum class FontSize {
  Small,  // bold: status bar, soft keys
  Text,   // lists, messages
  Tile,   // bold: tiles of the home screen
  Large,  // bold: clock, caller's name
};

const Font& font(FontSize size);

}  // namespace ui

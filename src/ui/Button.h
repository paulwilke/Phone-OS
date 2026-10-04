#pragma once

// A button in the content of a screen: an outlined rounded box with a centred label, drawn
// inverted (white on black) while it has the focus. The label is a fixed word from the
// catalog and is never shortened; a word that does not fit is a layout bug.

#include <string>

#include "ui/Display.h"

namespace ui {

struct Button {
  std::string label;
  bool focused = false;

  void draw(Display& display, const Rect& area) const;
};

}  // namespace ui

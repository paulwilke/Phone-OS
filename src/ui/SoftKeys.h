#pragma once

// The key bar at the bottom of every screen, like the soft keys of old phones. It answers the
// question nobody can answer without it: what does this key do here?
//
//   [ <backspace> Back ][ up ][ down ][ Send <enter> ]
//
// Back is always on the left and the main action always on the right, so a thumb finds them.
// Each button is also a touch target: a tap acts like the key (keyAt). A label is a fixed
// word from the catalog and is never shortened; if it does not fit its button, the layout or
// the word is wrong, and the layout tests in every language find it.

#include <optional>
#include <string>

#include "ui/Display.h"
#include "ui/Input.h"

namespace ui {

struct SoftKeys {
  std::string back;     // label of the back button; empty = no button
  bool scroll = false;  // show the up and down buttons
  std::string action;   // label of the main action; empty = no button
};

// Where the buttons of `keys` go in `bar`. A button that is not shown has an empty rectangle.
// The arrows get 26 pixels each; Back and the action share the rest, and when one of them is
// missing the other gets all of it (a call screen with only "Hang up" must not be squeezed by
// volume arrows).
struct SoftKeySlots {
  Rect back, up, down, action;
};
SoftKeySlots softKeySlots(const SoftKeys& keys, const Rect& bar);

void drawSoftKeys(Display& display, const Rect& bar, const SoftKeys& keys);

// One button in `slot`: its frame, and the word with the key symbol before it (back) or after
// it (action), centred. Public so that a test can draw a button alone and declare its slot as
// the region it may paint in.
void drawSoftKeyButton(Display& display, const Rect& slot, const std::string& word, const char* symbol,
                       bool symbolFirst);

// The key a tap at (x, y) stands for, if it hits a button.
std::optional<Key> softKeyAt(const SoftKeys& keys, const Rect& bar, int x, int y);

}  // namespace ui

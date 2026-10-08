#pragma once

// The key bar at the bottom of every screen, like the soft keys of old phones. It answers the
// question nobody can answer without it: what does this key do here?
//
//   [ <backspace> Back ][ up ][ down ][ Send <enter> ]
//
// Back is always on the left and the main action always on the right, so a thumb finds them. Back has
// a fixed place: the same on every screen, whether the arrows or an action are shown or not, and a
// key that comes or goes never moves it (owner, 2026-10-08: on the compose page Back moved to
// the left once something was typed, because it had the whole bar before).
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
  // The one button of the bar takes all of it: "Hang up" in a call, which must be easy to hit. Only
  // for a bar with exactly one button and no arrows; otherwise the places are the fixed ones.
  bool wide = false;
};

// Where the buttons of `keys` go in `bar`. A button that is not shown has an empty rectangle.
// The arrows get 26 pixels each in the middle; Back has the part left of them, whether the arrows
// are shown or not. The action ends at the right edge: right of the arrows, or from the middle
// when there are none. A `wide` bar with one button gives it all of the bar.
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

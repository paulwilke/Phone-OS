#pragma once

// What the keys of the keyboard mean: scan codes of the TCA8418 in, `ui::Input` out. The matrix is device
// specific and was checked on the device on 2026-09-30; the table is data in KeyboardDecoder.cpp and is
// pinned by a test.
//
//   letters      text; lower case, upper case after Shift
//   Shift        a tap arms the next key, held it stays while held; both Shifts together do nothing
//   Alt, Sym     one layer key each, and they do the same: the next key gives the symbol or digit printed on
//                it (W is 1, Q is #, the mic key is 0, the $ key is $)
//   Enter        Key::Select
//   Backspace    Key::Back (a text field deletes with it, an empty one goes back)
//   Space        text
//   Mic key      Key::Down, and with the layer key 0
//   $ key        Key::Up, and with the layer key $
//
// The device has no arrow keys, and W and S cannot scroll as they did in the earlier code: a letter is text
// here and the screens do not guess. The two keys that have no other use in this product, the $ key and
// the mic key, scroll. The three keys under the screen are not on the keyboard (they are keys of the touch
// chip, board/Cst66xx.h).

#include "../ui/Input.h"
#include "Tca8418.h"

namespace board {

class KeyboardDecoder {
public:
  // The input a key event produces, if any. Releases and modifiers produce none.
  bool decode(const KeyEvent& event, ui::Input& out);

  // Forgets every armed or held modifier (after a lock, so a stale Shift does not survive).
  void reset();

private:
  bool leftShift_ = false;
  bool rightShift_ = false;
  bool shiftArmed_ = false;
  bool shiftUsedWhileHeld_ = false;
  bool layerArmed_ = false;
};

}  // namespace board

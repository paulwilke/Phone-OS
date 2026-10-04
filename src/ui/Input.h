#pragma once

// What the user can do to the device, as the interface sees it. The platform turns key
// presses and touches into these; the screens never see a scancode or a touch controller.
//
//  * Up and Down scroll or move the selection, Select (Enter) does the main action, Back
//    (backspace) goes back. The device has no arrow keys and no Esc.
//  * Home, Inbox and Compose are the three keys under the screen. They act the same on every
//    screen, like the home key of a phone, so the ScreenStack owns them and a screen sees them
//    only if it asks for raw keys (a key-test screen).
//  * Text is a typed character (a Unicode code point), for screens with a text field.
//  * Touch is a tap at a point of the screen in pixels. Widgets turn a tap into the key it
//    stands for (SoftKeys::keyAt) or into a selection (TileGrid, ListView), so a screen needs
//    one input logic for keys and touches alike.

#include <cstdint>

namespace ui {

enum class Key : uint8_t {
  Up,
  Down,
  Select,
  Back,
  Home,
  Inbox,
  Compose,
};

struct Input {
  enum class Kind : uint8_t { Key, Text, Touch };

  Kind kind = Kind::Key;
  Key key = Key::Select;       // Kind::Key
  uint32_t character = 0;      // Kind::Text
  int x = 0, y = 0;            // Kind::Touch

  static Input pressed(Key k) {
    Input i;
    i.key = k;
    return i;
  }
  static Input text(uint32_t codePoint) {
    Input i;
    i.kind = Kind::Text;
    i.character = codePoint;
    return i;
  }
  static Input touch(int px, int py) {
    Input i;
    i.kind = Kind::Touch;
    i.x = px;
    i.y = py;
    return i;
  }

  bool isKey(Key k) const { return kind == Kind::Key && key == k; }
  bool isHardwareKey() const {
    return kind == Kind::Key && (key == Key::Home || key == Key::Inbox || key == Key::Compose);
  }
};

}  // namespace ui

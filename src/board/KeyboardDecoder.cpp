#include "KeyboardDecoder.h"

namespace board {

namespace {
constexpr uint8_t kBackspace = 11;
constexpr uint8_t kEnter = 21;
constexpr uint8_t kDollar = 22;
constexpr uint8_t kAlt = 30;
constexpr uint8_t kRightShift = 31;
constexpr uint8_t kSym = 32;
constexpr uint8_t kMic = 34;
constexpr uint8_t kLeftShift = 35;

// Scan code -> letter or space. Zero: not a text key.
char baseChar(uint8_t code) {
  static const char table[36] = {
      0,   'p', 'o', 'i', 'u', 'y', 't', 'r', 'e', 'w', 'q',   // 0..10
      0,   'l', 'k', 'j', 'h', 'g', 'f', 'd', 's', 'a',         // 11..20
      0,   0,   'm', 'n', 'b', 'v', 'c', 'x', 'z',             // 21..29
      0,   0,   0,   ' ', 0,   0};                              // 30..35
  return code < sizeof(table) ? table[code] : 0;
}

// Scan code -> what the layer key makes of it, printed on the keys. Zero: nothing.
char layerChar(uint8_t code) {
  static const char table[36] = {
      0,   '@', '+', '-', '_', ')', '(', '3', '2', '1', '#',    // 0..10
      0,   '"', '\'', ';', ':', '/', '6', '5', '4', '*',        // 11..20
      0,   '$', '.', ',', '!', '?', '9', '8', '7',              // 21..29
      0,   0,   0,   0,   '0', 0};                              // 30..35
  return code < sizeof(table) ? table[code] : 0;
}
}  // namespace

void KeyboardDecoder::reset() {
  leftShift_ = rightShift_ = shiftArmed_ = shiftUsedWhileHeld_ = layerArmed_ = false;
}

bool KeyboardDecoder::decode(const KeyEvent& e, ui::Input& out) {
  const bool isShift = e.code == kLeftShift || e.code == kRightShift;
  if (!e.pressed) {
    if (isShift) {
      (e.code == kLeftShift ? leftShift_ : rightShift_) = false;
      // A Shift that was used while it was held is spent; one that was only tapped stays armed for the
      // next key.
      if (shiftUsedWhileHeld_) shiftArmed_ = false;
      if (!leftShift_ && !rightShift_) shiftUsedWhileHeld_ = false;
    }
    return false;
  }

  if (isShift) {
    (e.code == kLeftShift ? leftShift_ : rightShift_) = true;
    if (leftShift_ && rightShift_) {
      shiftArmed_ = false;  // both together is no letter's Shift
      shiftUsedWhileHeld_ = true;
    } else {
      shiftArmed_ = true;
      shiftUsedWhileHeld_ = false;
    }
    return false;
  }
  if (e.code == kAlt || e.code == kSym) {
    layerArmed_ = true;
    return false;
  }

  const bool layer = layerArmed_;
  layerArmed_ = false;
  const bool held = leftShift_ || rightShift_;

  auto text = [&](char c) {
    out = ui::Input::text((uint32_t)(unsigned char)c);
    return true;
  };
  auto key = [&](ui::Key k) {
    out = ui::Input::pressed(k);
    return true;
  };
  auto spendShift = [&]() {
    if (held) shiftUsedWhileHeld_ = true;
    else shiftArmed_ = false;
  };

  if (layer) {
    const char c = layerChar(e.code);
    if (c != 0) return text(c);
  }

  switch (e.code) {
    case kBackspace: spendShift(); return key(ui::Key::Back);
    case kEnter: spendShift(); return key(ui::Key::Select);
    case kDollar: spendShift(); return key(ui::Key::Up);
    case kMic: spendShift(); return key(ui::Key::Down);
    default: break;
  }
  char c = baseChar(e.code);
  if (c == 0) return false;
  if (shiftArmed_) {
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    spendShift();
  }
  return text(c);
}

}  // namespace board

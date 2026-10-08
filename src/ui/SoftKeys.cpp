#include "ui/SoftKeys.h"

#include "ui/Fonts.h"
#include "ui/Symbols.h"
#include "ui/Text.h"

namespace ui {

namespace {

constexpr int kArrowWidth = 26;

}  // namespace

SoftKeySlots softKeySlots(const SoftKeys& keys, const Rect& bar) {
  const bool hasBack = !keys.back.empty(), hasAction = !keys.action.empty();
  SoftKeySlots s;
  if (keys.wide && !keys.scroll && hasBack != hasAction) {
    (hasBack ? s.back : s.action) = bar;
    return s;
  }
  const int side = (bar.w - 2 * kArrowWidth) / 2;
  const int middle = bar.x + side;
  if (hasBack) s.back = {bar.x, bar.y, side, bar.h};
  if (keys.scroll) {
    s.up = {middle, bar.y, kArrowWidth, bar.h};
    s.down = {middle + kArrowWidth, bar.y, kArrowWidth, bar.h};
  }
  // The action ends at the right edge; without arrows it also takes their place, because its words are the
  // long ones (the German word for "reset") and the place of Back does not depend on it.
  const int actionX = keys.scroll ? middle + 2 * kArrowWidth : middle;
  if (hasAction) s.action = {actionX, bar.y, bar.right() - actionX, bar.h};
  return s;
}

// The word and the symbol are separated by a space. When that does not fit, the symbol goes: the
// whole word matters more than the picture of the key, and a symbol pressed against a word (as
// 4.0 drew it) reads as part of it.
void drawSoftKeyButton(Display& display, const Rect& slot, const std::string& word, const char* symbol,
                       bool symbolFirst) {
  if (slot.empty()) return;
  const Font& font = ui::font(FontSize::Small);
  const Rect box{slot.x + 1, slot.y + 3, slot.w - 2, slot.h - 4};
  display.outlineRect(box, true);

  const int room = box.w - 4;
  const auto with = [&](const char* gap) {
    return symbolFirst ? std::string(symbol) + gap + word : word + gap + symbol;
  };
  std::string label = with(" ");
  if (text::width(font, label) > room && !word.empty()) label = word;
  const int top = box.y + (box.h - font.lineHeight()) / 2 + 1;
  text::drawCentered(display, font, {box.x + 2, box.y, room, box.h}, top, label);
}

void drawSoftKeys(Display& display, const Rect& bar, const SoftKeys& keys) {
  const SoftKeySlots s = softKeySlots(keys, bar);
  display.fillRect({bar.x, bar.y, bar.w, 1}, true);
  if (!keys.back.empty()) drawSoftKeyButton(display, s.back, keys.back, symbol::kBackspace, true);
  if (keys.scroll) {
    drawSoftKeyButton(display, s.up, "", symbol::kUp, true);
    drawSoftKeyButton(display, s.down, "", symbol::kDown, true);
  }
  if (!keys.action.empty()) drawSoftKeyButton(display, s.action, keys.action, symbol::kEnter, false);
}

std::optional<Key> softKeyAt(const SoftKeys& keys, const Rect& bar, int x, int y) {
  if (!bar.contains(x, y)) return std::nullopt;
  const SoftKeySlots s = softKeySlots(keys, bar);
  if (!keys.back.empty() && s.back.contains(x, y)) return Key::Back;
  if (keys.scroll && s.up.contains(x, y)) return Key::Up;
  if (keys.scroll && s.down.contains(x, y)) return Key::Down;
  if (!keys.action.empty() && s.action.contains(x, y)) return Key::Select;
  return std::nullopt;
}

}  // namespace ui

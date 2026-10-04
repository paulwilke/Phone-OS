#include "ui/SoftKeys.h"

#include "ui/Fonts.h"
#include "ui/Symbols.h"
#include "ui/Text.h"

namespace ui {

namespace {

constexpr int kArrowWidth = 26;

}  // namespace

SoftKeySlots softKeySlots(const SoftKeys& keys, const Rect& bar) {
  const int arrow = keys.scroll ? kArrowWidth : 0;
  const int rest = bar.w - 2 * arrow;
  const bool hasBack = !keys.back.empty(), hasAction = !keys.action.empty();
  const int left = hasBack ? (hasAction ? rest / 2 : rest) : 0;
  const int right = hasAction ? rest - left : 0;

  SoftKeySlots s;
  s.back = {bar.x, bar.y, left, bar.h};
  s.up = {bar.x + left, bar.y, arrow, bar.h};
  s.down = {bar.x + left + arrow, bar.y, arrow, bar.h};
  s.action = {bar.x + left + 2 * arrow, bar.y, right, bar.h};
  return s;
}

// The word and the symbol are separated by a space. When that does not fit, the space goes,
// and when that does not fit either, the symbol: a bar with scroll arrows leaves the buttons
// little room, and the whole word matters more than the picture of the key.
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
  if (text::width(font, label) > room) label = with("");
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

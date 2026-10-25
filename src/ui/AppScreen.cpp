#include "ui/AppScreen.h"

#include "ui/Wording.h"

namespace ui {

void AppScreen::render(Display& display) {
  state_ = app().state();
  layout_ = ScreenLayout::standard(display.width(), display.height());
  display.fillRect({0, 0, display.width(), display.height()}, false);

  drawStatusBar(display, layout_.status, statusModelOf(state_), t());
  notice_ = wording::notice(state_.notice, app().profile().contacts, t(), layout_.alert.w - kAlertBarMargin);
  drawAlertBar(display, layout_.alert, notice_);

  draw(display, layout_.content);

  keys_ = keys();
  drawSoftKeys(display, layout_.softKeys, keys_);
}

bool AppScreen::handle(const Input& input) {
  state_ = app().state();
  if (input.kind == Input::Kind::Touch) {
    if (layout_.softKeys.contains(input.x, input.y)) {
      const std::optional<Key> key = softKeyAt(keys_, layout_.softKeys, input.x, input.y);
      return key ? onInput(Input::pressed(*key), layout_.content) : false;
    }
    if (!layout_.content.contains(input.x, input.y)) return false;
  }
  return onInput(input, layout_.content);
}

std::vector<TouchTarget> AppScreen::touchTargets() {
  std::vector<TouchTarget> out;
  const SoftKeySlots slots = softKeySlots(keys_, layout_.softKeys);
  if (!keys_.back.empty()) out.push_back({slots.back, Key::Back});
  if (keys_.scroll) {
    out.push_back({slots.up, Key::Up});
    out.push_back({slots.down, Key::Down});
  }
  if (!keys_.action.empty()) out.push_back({slots.action, Key::Select});
  for (const Rect& rect : targets(layout_.content)) out.push_back({rect, std::nullopt});
  return out;
}

}  // namespace ui

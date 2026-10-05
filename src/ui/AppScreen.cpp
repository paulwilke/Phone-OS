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

}  // namespace ui

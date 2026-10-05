#include "ui/screens/LockScreen.h"

#include <string>

#include "ui/Fonts.h"
#include "ui/Symbols.h"
#include "ui/Text.h"
#include "ui/Wording.h"

namespace ui {

void LockScreen::onEnter() {
  armed_ = false;
  app().setFrontlight(false);
}

void LockScreen::draw(Display& display, const Rect& area) {
  const Font& big = font(FontSize::Tile);
  const Font& body = font(FontSize::Text);

  // What the device is connected to. The operator's name is the core's short label (ten characters
  // at most), which is far inside the width of the line in the large font.
  int y = area.y + 24;
  text::drawCentered(display, big, area, y, wording::networkLabel(state().network, t()));
  y += big.lineHeight() + 6;

  // The answer to "can they reach me": connected, only by SMS, or not.
  const bool anyNetwork = state().network.kind != Connectivity::NetworkLabel::Kind::None;
  const char* reachable = !anyNetwork ? "lock.not-connected"
                          : (state().network.kind == Connectivity::NetworkLabel::Kind::Operator && !state().voiceAvailable)
                              ? "status.sms-only"
                              : "lock.connected";
  text::drawCentered(display, body, area, y, t().get(reachable));
  y += body.lineHeight() + 22;

  if (state().unread > 0) {
    const std::string news = std::string(symbol::kEnvelope) + " " + t().plural("messages.unread", static_cast<uint32_t>(state().unread));
    text::drawCentered(display, body, area, y, news);
    y += body.lineHeight() + 12;
  }

  // The key is named by its symbol, as it is printed on the keyboard.
  const std::string hint = t().get(armed_ ? "lock.again" : "lock.unlock");
  for (std::string_view line : text::wrap(body, hint, area.w - 24)) {
    text::drawCentered(display, body, area, y, line);
    y += body.lineHeight();
  }
}

SoftKeys LockScreen::keys() { return SoftKeys(); }

bool LockScreen::onInput(const Input& input, const Rect&) {
  if (input.isKey(Key::Select)) {
    if (armed_) {
      armed_ = false;
      nav().unlock();
    } else {
      armed_ = true;
    }
    return true;
  }
  // Another key breaks the pair, and nothing gets through to the device. A touch does neither: it
  // is swallowed, so a pocket full of taps neither unlocks nor spoils a first Enter.
  if (input.kind != Input::Kind::Touch) armed_ = false;
  return true;
}

}  // namespace ui

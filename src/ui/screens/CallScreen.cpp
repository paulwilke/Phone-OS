#include "ui/screens/CallScreen.h"

#include <string>

#include "ui/Fonts.h"
#include "ui/SoftKeys.h"
#include "ui/Symbols.h"
#include "ui/Text.h"
#include "ui/Wording.h"

namespace ui {

namespace {

constexpr int kBoxWidth = 22, kBoxGap = 6, kBoxHeight = 18;
constexpr int kVolumeButtonHeight = 32;   // as tall as the soft keys: a finger finds them the same way

// The largest of the fonts that holds the whole text in `width`; a number must be there in full.
const Font& fontThatFits(const std::string& words, int width) {
  for (FontSize size : {FontSize::Large, FontSize::Tile, FontSize::Text})
    if (text::width(font(size), words) <= width) return font(size);
  return font(FontSize::Small);
}

}  // namespace

bool CallScreen::preventsAutoLock() const { return context().app.state().callActive; }

CallScreen::Phase CallScreen::phase() const {
  switch (state().callState) {
    case ModemState::Dialing: return Phase::Dialling;
    case ModemState::RingingIn: return Phase::Ringing;
    case ModemState::InCall: return Phase::Connected;
    default: break;
  }
  return wording::failedCallReason(state().notice).empty() ? Phase::None : Phase::Failed;
}

void CallScreen::draw(Display& display, const Rect& area) {
  const Phase now = phase();
  const char* titleKey = "call.title-none";
  switch (now) {
    case Phase::Dialling: titleKey = "call.title-dialing"; break;
    case Phase::Ringing: titleKey = "call.title-ringing"; break;
    case Phase::Connected: titleKey = "call.title-incall"; break;
    case Phase::Failed: titleKey = "call.title-failed"; break;
    case Phase::None: break;
  }

  const Font& body = font(FontSize::Text);
  const int width = area.w - 16;
  int y = area.y + 16;
  // The words are kept in a variable while their lines are drawn: the lines are views into them.
  const std::string title = t().get(titleKey);
  for (std::string_view line : text::wrap(body, title, width)) {
    text::drawCentered(display, body, area, y, line);
    y += body.lineHeight();
  }
  y += 10;

  // Who: the peer of the call, or, for a call that failed, the one the notice names. Nobody when
  // there is no call and nothing failed.
  std::string who;
  if (now == Phase::Failed) who = wording::peerName(state().notice.peer, context().app.profile().contacts, t());
  else if (now != Phase::None) who = wording::peerName(state().callPeer, context().app.profile().contacts, t());
  if (!who.empty()) {
    const Font& big = fontThatFits(who, width);
    text::drawCentered(display, big, area, y, who);
    y += big.lineHeight() + 12;
  }

  if (now == Phase::Failed) {
    const std::string why = wording::callReason(wording::failedCallReason(state().notice), t());
    for (std::string_view line : text::wrap(body, why, width)) {
      text::drawCentered(display, body, area, y, line);
      y += body.lineHeight();
    }
  }

  quieter_ = louder_ = Rect{};
  // The volume, during a call only: boxes, filled up to the level, and below them the two buttons that change it
  // (owner, 2026-10-08: by touch, in the middle of the page).
  if (now == Phase::Connected && state().volumeAdjustable) {
    const std::string label = t().get("call.volume");
    for (std::string_view line : text::wrap(body, label, width)) {
      text::drawCentered(display, body, area, y, line);
      y += body.lineHeight();
    }
    y += 8;
    const int total = Calls::kMaxVolume * kBoxWidth + (Calls::kMaxVolume - 1) * kBoxGap;
    int x = area.x + (area.w - total) / 2;
    for (int i = 0; i < Calls::kMaxVolume; ++i) {
      const Rect box{x, y, kBoxWidth, kBoxHeight};
      if (i < state().volume) display.fillRect(box, true);
      else display.outlineRect(box, true);
      x += kBoxWidth + kBoxGap;
    }
    y += kBoxHeight + 10;
    // The two buttons share the width of the page, wider than the row of boxes: a finger, and "Moins fort".
    const int left = area.x + 8, width = area.w - 16;
    const int half = (width - kBoxGap) / 2;
    quieter_ = {left, y, half, kVolumeButtonHeight};
    louder_ = {left + width - half, y, half, kVolumeButtonHeight};
    drawSoftKeyButton(display, quieter_, t().get("call.quieter"), symbol::kDown, true);
    drawSoftKeyButton(display, louder_, t().get("call.louder"), symbol::kUp, false);
  }
}

SoftKeys CallScreen::keys() {
  SoftKeys keys;
  switch (phase()) {
    // Hang up is the whole bar: it must be easy to hit. The volume has buttons of its own on the page, and
    // the keys Up and Down change it as before.
    case Phase::Dialling:
    case Phase::Connected:
      keys.back = t().get("call.hangup");
      keys.wide = true;
      break;
    case Phase::Ringing:
      keys.back = t().get("call.reject");
      keys.action = t().get("call.answer");
      break;
    default: keys.back = t().get("key.back"); break;
  }
  return keys;
}

bool CallScreen::onInput(const Input& input, const Rect&) {
  if (input.kind == Input::Kind::Touch && phase() == Phase::Connected) {
    if (quieter_.contains(input.x, input.y)) return app().changeVolume(-1);
    if (louder_.contains(input.x, input.y)) return app().changeVolume(+1);
  }
  if (input.isKey(Key::Select) && phase() == Phase::Ringing) {
    app().answer();
    return true;
  }
  // The arrows are the volume. The core changes it only during a call, and says no otherwise.
  if (input.isKey(Key::Up)) return app().changeVolume(+1);
  if (input.isKey(Key::Down)) return app().changeVolume(-1);
  if (input.isKey(Key::Back)) {
    const Phase now = phase();
    if (now == Phase::None || now == Phase::Failed) nav().back();
    else app().hangup();
    return true;
  }
  return false;
}

}  // namespace ui

#include "ui/screens/RadioScreen.h"

#include <algorithm>
#include <string_view>

#include "ui/Fonts.h"
#include "ui/Formats.h"
#include "ui/PageTitle.h"
#include "ui/Shapes.h"
#include "ui/Text.h"

namespace ui {

namespace {
constexpr int kGap = 4;
constexpr int kButtonPadding = 6;
constexpr int kPeerPadding = 2;
constexpr int kBarWidth = 4;
constexpr int kWordsInset = 6;   // between the edge of a row and its words
}  // namespace

std::string RadioScreen::head() {
  const AppCore::State& s = state();
  if (!context().app.profile().radioAllowed) return t().get("radio.not-allowed");
  if (!s.radioAttached) return t().get("radio.off");
  if (s.saveMode) return t().get("radio.save-mode");
  if (!s.radioReady) return t().get("radio.nobody");
  if (s.radioRssi != 0) return t().get("radio.ready-dbm", {{"dbm", s.radioRssi}});
  return t().get("radio.ready");
}

void RadioScreen::rebuild() {
  rows_.clear();
  const AppCore::State& s = state();
  const auto control = [&](Control which, std::string label) { rows_.push_back({true, which, std::move(label), "", ""}); };
  if (context().app.profile().radioAllowed) {
    if (!s.radioAttached) {
      control(Control::Start, t().get("radio.start"));
    } else if (!s.saveMode) {
      control(Control::SaveOn, t().get("radio.save-on"));
      control(Control::Stop, t().get("radio.stop"));
    } else {
      if (s.probeSecondsLeft > 0) {
        const std::string seconds = (s.probeSecondsLeft % 60 < 10 ? "0" : "") + std::to_string(s.probeSecondsLeft % 60);
        control(Control::Probe, t().get("radio.probe-running", {{"minutes", static_cast<int>(s.probeSecondsLeft / 60)}, {"seconds", seconds.c_str()}}));
      } else {
        control(Control::Probe, t().get("radio.probe"));
      }
      control(Control::SaveOff, t().get("radio.save-off"));
    }
    if (s.radioAttached) {
      for (size_t i = 0; i < context().app.radioPeerCount(); ++i) {
        RadioPeer peer;
        if (!context().app.radioPeer(i, peer)) break;
        const std::string route = t().get(peer.direct ? "radio.direct" : "radio.detour");
        const std::string age = format::ageSince(peer.lastSeen, s.epochUtc, t());
        rows_.push_back({false, Control::Start, peer.name, t().get("radio.peer-line", {{"route", route.c_str()}, {"age", age.c_str()}}), peer.name});
      }
    }
  }
  if (selected_ >= rows_.size()) selected_ = rows_.empty() ? 0 : rows_.size() - 1;
}

Rect RadioScreen::rowsArea(const Rect& area) {
  const int header = pageTitleHeight(area, head());
  return {area.x, area.y + header, area.w, area.h - header};
}

void RadioScreen::measure(const Rect& area) { wordsWidth_ = area.w - kBarWidth - 6 - 2 * kWordsInset; }

int RadioScreen::rowHeight(const Row& row) const {
  if (row.isControl) {
    const Font& body = font(FontSize::Text);
    return static_cast<int>(text::wrap(body, row.label, wordsWidth_).size()) * body.lineHeight() + 2 * kButtonPadding;
  }
  const Font& small = font(FontSize::Small);
  return font(FontSize::Text).lineHeight() + static_cast<int>(text::wrap(small, row.sub, wordsWidth_).size()) * small.lineHeight() + 2 * kPeerPadding;
}

size_t RadioScreen::fitting(size_t first, int room) const {
  size_t n = 0;
  for (size_t i = first; i < rows_.size(); ++i) {
    const int h = rowHeight(rows_[i]) + kGap;
    if (h > room + kGap) break;
    room -= h;
    ++n;
  }
  return n;
}

void RadioScreen::keepInView(const Rect& area) {
  if (selected_ < first_) first_ = selected_;
  while (first_ < selected_ && selected_ >= first_ + std::max<size_t>(1, fitting(first_, rowsArea(area).h))) ++first_;
}

bool RadioScreen::writable() const { return context().app.profile().freeText; }

void RadioScreen::draw(Display& display, const Rect& area) {
  measure(area);
  rebuild();
  keepInView(area);
  const std::string headline = head();
  drawPageTitle(display, area, headline);
  const Rect rect = rowsArea(area);
  const Font& body = font(FontSize::Text);
  const Font& small = font(FontSize::Small);

  const size_t shown = fitting(first_, rect.h);
  int y = rect.y;
  for (size_t i = first_; i < first_ + shown && i < rows_.size(); ++i) {
    const Row& row = rows_[i];
    const bool focused = i == selected_;
    const Rect box{rect.x + 2, y, rect.w - kBarWidth - 6, rowHeight(row)};
    if (row.isControl) {
      if (focused) shapes::fillRoundRect(display, box, 8, true);
      else shapes::outlineRoundRect(display, box, 8, true);
      int ty = box.y + kButtonPadding;
      for (std::string_view line : text::wrap(body, row.label, wordsWidth_)) {
        text::drawCentered(display, body, box, ty, line, !focused);
        ty += body.lineHeight();
      }
    } else {
      if (focused) display.fillRect(box, true);
      else display.fillRect({box.x, box.bottom() - 1, box.w, 1}, true);
      const int room = box.w - 8;
      text::draw(display, body, box.x + 4, box.y + kPeerPadding, text::ellipsize(body, row.label, room), !focused);
      int sy = box.y + kPeerPadding + body.lineHeight();
      for (std::string_view line : text::wrap(small, row.sub, wordsWidth_)) {
        text::draw(display, small, box.x + 4, sy, line, !focused);
        sy += small.lineHeight();
      }
    }
    y += box.h + kGap;
  }
  if (rows_.size() > shown)
    shapes::drawScrollBar(display, {rect.right() - kBarWidth, rect.y, kBarWidth, rect.h}, rows_.size(), first_, shown);

  // What is to be said where there is nothing to press or nobody to write to.
  const bool allowed = context().app.profile().radioAllowed;
  const bool peersEmpty = state().radioAttached && std::none_of(rows_.begin(), rows_.end(), [](const Row& r) { return !r.isControl; });
  const char* key = !allowed ? "radio.forbidden" : peersEmpty ? "radio.no-peers" : nullptr;
  if (key) {
    const std::string words = t().get(key);                             // the lines are views into it
    int ty = y + 4;
    for (std::string_view line : text::wrap(small, words, rect.w - 12)) {
      text::draw(display, small, rect.x + 6, ty, line);
      ty += small.lineHeight();
    }
  }
}

SoftKeys RadioScreen::keys() {
  SoftKeys keys;
  keys.back = t().get("key.back");
  keys.scroll = rows_.size() > 1;
  if (!rows_.empty()) {
    if (rows_[selected_].isControl) keys.action = t().get("key.choose");
    else if (writable()) keys.action = t().get("radio.write");
  }
  return keys;
}

bool RadioScreen::activate(const Row& row) {
  if (row.isControl) {
    switch (row.control) {
      case Control::Start: app().requestRadioStart(); break;
      case Control::SaveOn: app().setSaveMode(true); break;
      case Control::SaveOff: app().setSaveMode(false); break;
      case Control::Probe: app().probeCellular(); break;
      case Control::Stop: app().requestRestart(); break;
    }
    return true;
  }
  if (!writable()) return false;
  session().recipient = row.name;
  session().viaRadio = true;
  nav().open(screen::kCompose);
  return true;
}

bool RadioScreen::onInput(const Input& input, const Rect& area) {
  if (input.isKey(Key::Back)) {
    nav().back();
    return true;
  }
  measure(area);
  rebuild();
  keepInView(area);
  if (rows_.empty()) return false;
  if (input.isKey(Key::Up)) {
    if (selected_ > 0) --selected_;
    keepInView(area);
    return true;
  }
  if (input.isKey(Key::Down)) {
    if (selected_ + 1 < rows_.size()) ++selected_;
    keepInView(area);
    return true;
  }
  if (input.isKey(Key::Select)) return activate(rows_[selected_]);
  if (input.kind == Input::Kind::Touch) {
    const Rect rect = rowsArea(area);
    int y = rect.y;
    const size_t shown = fitting(first_, rect.h);
    for (size_t i = first_; i < first_ + shown && i < rows_.size(); ++i) {
      const int h = rowHeight(rows_[i]);
      if (input.y >= y && input.y < y + h && input.x < rect.right() - kBarWidth) {
        selected_ = i;
        activate(rows_[i]);   // a contact that may not be written to is only selected
        return true;
      }
      y += h + kGap;
    }
  }
  return false;
}

}  // namespace ui

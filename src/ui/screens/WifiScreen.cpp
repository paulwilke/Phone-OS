#include "ui/screens/WifiScreen.h"

#include <string_view>

#include "ui/Fonts.h"
#include "ui/PageTitle.h"
#include "ui/Text.h"
#include "ui/Wording.h"

namespace ui {

namespace {

constexpr WifiMode kModes[3] = {WifiMode::Station, WifiMode::AccessPoint, WifiMode::Off};
constexpr int kDetailGap = 4;

bool running(WifiPhase phase) { return phase != WifiPhase::Off && phase != WifiPhase::Failed; }

}  // namespace

void WifiScreen::rebuild() {
  const WifiPhase phase = state().wifiPhase;
  const bool station = phase == WifiPhase::Connecting || phase == WifiPhase::Connected;
  const bool accessPoint = phase == WifiPhase::AccessPoint;
  rows_.setRows({{t().get("wifi.home-network"), "", station},
                 {t().get("wifi.access-point"), "", accessPoint},
                 {t().get("wifi.off"), "", !running(phase)}});
}

Rect WifiScreen::rowsArea(const Rect& area) {
  const int header = pageTitleHeight(area, t().get("wifi.title"));
  return {area.x, area.y + header, area.w, area.h - header};
}

// What goes under the buttons, one paragraph per line.
std::string WifiScreen::details() {
  const AppCore::State& s = state();
  std::string text;
  const auto line = [&](const std::string& words) {
    if (!text.empty()) text += '\n';
    text += words;
  };
  const std::string reason = wording::wifiReason(s.wifiReason, t());
  if (s.wifiPhase == WifiPhase::Failed || (!running(s.wifiPhase) && !reason.empty())) {
    line(reason.empty() ? t().get("wifi.failed") : reason);
    if (s.wifiPhase == WifiPhase::Failed) line(t().get("wifi.try-access-point"));
  } else if (s.wifiPhase == WifiPhase::Connecting) {
    line(t().get("wifi.connecting", {{"ssid", s.wifiSsid.c_str()}}));
  } else if (running(s.wifiPhase)) {
    line(t().get("wifi.network", {{"ssid", s.wifiSsid.c_str()}}));
    if (s.wifiPhase == WifiPhase::AccessPoint) line(t().get("wifi.password", {{"password", s.wifiPassword.c_str()}}));
    if (!s.wifiAddress.empty()) line(t().get("wifi.page", {{"address", s.wifiAddress.c_str()}}));
    line(t().get("wifi.off-in", {{"n", static_cast<int>((s.wifiSecondsLeft + 59) / 60)}}));
  } else {
    line(t().get("wifi.is-off"));
  }
  return text;
}

void WifiScreen::draw(Display& display, const Rect& area) {
  rebuild();
  drawPageTitle(display, area, t().get("wifi.title"));
  const Rect rowsRect = rowsArea(area);
  rows_.draw(display, rowsRect);

  const Font& small = font(FontSize::Small);
  const std::string words = details();                                   // the lines are views into it
  int y = rowsRect.y + 3 * (rows_.rowHeight() + 4) + kDetailGap;
  for (std::string_view line : text::wrap(small, words, area.w - 12)) {
    text::draw(display, small, area.x + 6, y, line);
    y += small.lineHeight();
  }
}

SoftKeys WifiScreen::keys() {
  SoftKeys keys;
  keys.back = t().get("key.back");
  keys.action = t().get("key.choose");
  keys.scroll = true;
  return keys;
}

std::vector<Rect> WifiScreen::targets(const Rect& area) {
  rebuild();
  return rows_.targets(rowsArea(area));
}

bool WifiScreen::onInput(const Input& input, const Rect& area) {
  if (input.isKey(Key::Back)) {
    nav().back();
    return true;
  }
  rebuild();
  bool chosen = false;
  const bool used = rows_.handle(input, rowsArea(area), &chosen);
  if (chosen) activate(rows_.selected());
  return used;
}

void WifiScreen::activate(size_t index) {
  if (index < 3) app().setWifiMode(kModes[index]);
}

}  // namespace ui

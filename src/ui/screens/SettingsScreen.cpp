#include "ui/screens/SettingsScreen.h"

#include "ui/PageTitle.h"

namespace ui {

std::vector<SettingsScreen::Kind> SettingsScreen::kinds() {
  std::vector<Kind> kinds;
  if (state().volumeAdjustable) kinds.push_back(Kind::Volume);
  kinds.push_back(Kind::Vibration);
  kinds.push_back(Kind::SaveMode);
  if (context().app.profile().parentTools && context().app.wifi() != nullptr) kinds.push_back(Kind::Wifi);
  kinds.push_back(Kind::Language);
  return kinds;
}

void SettingsScreen::rebuild() {
  shown_ = kinds();
  const auto onOff = [&](bool on) { return t().get(on ? "option.on" : "option.off"); };
  std::vector<OptionRow> rows;
  for (Kind kind : shown_) {
    switch (kind) {
      case Kind::Volume:
        rows.push_back({t().get("settings.volume"),
                        t().get("settings.volume-value", {{"volume", state().volume}, {"max", Calls::kMaxVolume}})});
        break;
      case Kind::Vibration: rows.push_back({t().get("settings.vibration"), onOff(context().app.messageVibration())}); break;
      case Kind::SaveMode: rows.push_back({t().get("settings.save-mode"), onOff(state().saveMode)}); break;
      case Kind::Wifi:
        rows.push_back({t().get("wifi.title"),
                        onOff(state().wifiPhase != WifiPhase::Off && state().wifiPhase != WifiPhase::Failed)});
        break;
      case Kind::Language: {
        std::string name = t().language();
        for (const i18n::LanguageInfo& info : t().languages())
          if (info.code == name) name = info.native;
        rows.push_back({t().get("language.title"), name});
        break;
      }
    }
  }
  rows_.setRows(std::move(rows));
}

Rect SettingsScreen::rowsArea(const Rect& area) {
  const int header = pageTitleHeight(area, t().get("settings.title"));
  return {area.x, area.y + header, area.w, area.h - header};
}

void SettingsScreen::draw(Display& display, const Rect& area) {
  rebuild();
  drawPageTitle(display, area, t().get("settings.title"));
  rows_.draw(display, rowsArea(area));
}

SoftKeys SettingsScreen::keys() {
  SoftKeys keys;
  keys.back = t().get("key.back");
  keys.action = t().get("key.change");
  keys.scroll = shown_.size() > 1;
  return keys;
}

bool SettingsScreen::onInput(const Input& input, const Rect& area) {
  if (input.isKey(Key::Back)) {
    nav().back();
    return true;
  }
  rebuild();
  bool chosen = false;
  const bool used = rows_.handle(input, rowsArea(area), &chosen);
  if (chosen) activate(shown_[rows_.selected()]);
  return used;
}

void SettingsScreen::activate(Kind kind) {
  switch (kind) {
    case Kind::Volume: {
      // Round 1..max and not 0..max: a call that begins mute is a mistake, not a wish.
      int next = state().volume + 1;
      if (next > Calls::kMaxVolume) next = 1;
      app().setDefaultVolume(next);
      return;
    }
    case Kind::Vibration:
      app().setMessageVibration(!app().messageVibration());
      if (app().messageVibration()) app().playTestPattern(HapticPattern::Message);   // whoever turns it on wants to feel the real one
      return;
    case Kind::SaveMode: app().setSaveMode(!state().saveMode); return;
    case Kind::Wifi: nav().open(screen::kWifi); return;
    case Kind::Language: nav().open(screen::kLanguage); return;
  }
}

}  // namespace ui

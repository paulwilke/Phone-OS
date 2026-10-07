#include "ui/screens/StatusScreen.h"

#include "ui/PageTitle.h"

namespace ui {

void StatusScreen::onEnter() {
  block_.setText(std::string());
  text_.clear();
  offArmed_ = false;
}

std::string StatusScreen::words() {
  const AppCore::State& s = state();
  const Profile& profile = context().app.profile();
  std::string text;
  const auto line = [&](const std::string& words) {
    if (!text.empty()) text += '\n';
    text += words;
  };

  // The cable is the charger's power-good bit: a full battery on a cable reads "not charging", and the
  // board stays alive on USB either way.
  const bool cable = s.externalPower;
  if (s.batteryPercent < 0) line(t().get("status.battery-unknown"));
  else line(t().get(s.charge == ChargeState::Charging ? "status.battery-charging" : s.charge == ChargeState::Full ? "status.battery-full" : "status.battery",
                    {{"percent", s.batteryPercent}}));
  if (s.batteryMinutes < 0) line(t().get("status.remaining-unknown"));
  else line(t().get("status.remaining", {{"hours", s.batteryMinutes / 60}, {"minutes", s.batteryMinutes % 60}}));
  if (!profile.childName.empty()) line(t().get("status.child", {{"name", profile.childName.c_str()}}));
  line(t().get("status.contacts", {{"n", static_cast<int>(profile.contacts.count())}}));
  // From the core and not from the profile at the start: after a change with Enter the core's value counts.
  const CommandNotice notice = context().app.commandNotice();
  const std::string how = t().get(notice == CommandNotice::Silent ? "status.queries-silent"
                                  : notice == CommandNotice::Notify ? "status.queries-notice" : "status.queries-history");
  line(t().get("status.queries", {{"how", how.c_str()}}));
  if (!info_.problems.empty()) {
    line(t().plural("status.problems", static_cast<uint32_t>(info_.problems.size())));
    const ConfigProblem& first = info_.problems.front();
    switch (first.kind) {
      case ConfigProblem::Kind::File: line(t().get("status.problem-file", {{"file", first.file.c_str()}})); break;
      case ConfigProblem::Kind::Setting:
        line(t().get("status.problem-setting", {{"file", first.file.c_str()}, {"key", first.key.c_str()}}));
        break;
      case ConfigProblem::Kind::Entry: line(t().get("status.problem-entry")); break;
    }
  }
  line(t().get("status.firmware", {{"version", info_.firmware.c_str()}}));
  if (!info_.hardware.empty()) line(info_.hardware);
  line(t().get(cable ? "status.off-cable" : offArmed_ ? "status.off-again" : "status.off-press"));
  return text;
}

Rect StatusScreen::textArea(const Rect& area) {
  const int header = pageTitleHeight(area, t().get("status.title"));
  return {area.x, area.y + header, area.w, area.h - header};
}

void StatusScreen::draw(Display& display, const Rect& area) {
  drawPageTitle(display, area, t().get("status.title"));
  const std::string now = words();
  if (now != text_) {
    text_ = now;
    block_.setText(now);
  }
  block_.draw(display, textArea(area));
}

SoftKeys StatusScreen::keys() {
  SoftKeys keys;
  keys.back = t().get("key.back");
  keys.action = t().get("key.queries");
  const Rect text = textArea(ScreenLayout::standard().content);
  keys.scroll = block_.lineCount(text) > block_.visibleLines(text);
  return keys;
}

void StatusScreen::stepQueries() {
  switch (app().commandNotice()) {
    case CommandNotice::Silent: app().setCommandNotice(CommandNotice::History); break;
    case CommandNotice::History: app().setCommandNotice(CommandNotice::Notify); break;
    case CommandNotice::Notify: app().setCommandNotice(CommandNotice::Silent); break;
  }
}

bool StatusScreen::onInput(const Input& input, const Rect& area) {
  const bool x = input.kind == Input::Kind::Text && (input.character == 'x' || input.character == 'X');
  if (x) {
    if (!offArmed_) {
      offArmed_ = true;                // only the second press is meant
      return true;
    }
    offArmed_ = false;
    app().requestPowerOff();           // the application clears the display and then switches off
    return true;
  }
  offArmed_ = false;                   // any other input takes the first press back
  if (input.isKey(Key::Select)) {
    stepQueries();
    return true;
  }
  if (input.isKey(Key::Back)) {
    nav().back();
    return true;
  }
  return block_.handle(input, textArea(area));
}

}  // namespace ui

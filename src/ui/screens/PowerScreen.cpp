#include "ui/screens/PowerScreen.h"

#include "ui/PageTitle.h"

namespace ui {

std::string PowerScreen::title() { return t().get(asking_ ? "power.confirm" : "power.title"); }

void PowerScreen::rebuild() {
  std::vector<OptionRow> rows;
  if (asking_) {
    rows.push_back({t().get("power.cancel"), ""});          // first: the focus starts on the harmless one
    rows.push_back({t().get("power.yes-switch-off"), ""});
  } else if (state().externalPower) {
    rows.push_back({t().get("settings.power-off"), t().get("settings.power-off-cable")});
  } else {
    rows.push_back({t().get("settings.power-off"), ""});
  }
  rows_.setRows(std::move(rows));
}

Rect PowerScreen::rowsArea(const Rect& area) {
  const int header = pageTitleHeight(area, title());
  return {area.x, area.y + header, area.w, area.h - header};
}

void PowerScreen::draw(Display& display, const Rect& area) {
  rebuild();
  drawPageTitle(display, area, title());
  rows_.draw(display, rowsArea(area));
}

SoftKeys PowerScreen::keys() {
  SoftKeys keys;
  keys.back = t().get("key.back");
  keys.action = t().get("key.choose");
  keys.scroll = asking_;
  return keys;
}

bool PowerScreen::onInput(const Input& input, const Rect& area) {
  if (input.isKey(Key::Back)) {
    if (asking_) {
      asking_ = false;
      rows_.resetSelection();
    } else {
      nav().back();
    }
    return true;
  }
  rebuild();
  bool chosen = false;
  const bool used = rows_.handle(input, rowsArea(area), &chosen);
  if (!chosen) return used;
  if (!asking_) {
    if (state().externalPower) return used;   // the row says to unplug first
    asking_ = true;
    rows_.resetSelection();                   // on "Cancel"
    return true;
  }
  if (rows_.selected() == 0) {
    asking_ = false;
    rows_.resetSelection();
    return true;
  }
  asking_ = false;
  app().requestPowerOff();                    // the application clears the display and then switches off
  return true;
}

}  // namespace ui

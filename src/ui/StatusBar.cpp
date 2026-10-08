#include "ui/StatusBar.h"

#include <cstdio>

#include "ui/Fonts.h"
#include "ui/Text.h"

namespace ui {

namespace {

constexpr int kBars = 5, kBarWidth = 5, kBarGap = 1;
constexpr int kFrameWidth = kBars * kBarWidth + (kBars - 1) * kBarGap + 4;
constexpr int kFrameHeight = 12;

const char* const kWeekdayKeys[7] = {"weekday.sun", "weekday.mon", "weekday.tue", "weekday.wed",
                                     "weekday.thu", "weekday.fri", "weekday.sat"};

}  // namespace

int batteryBars(int percent) {
  if (percent < 0) return 0;
  if (percent >= 100) return kBars;
  const int bars = (percent + 19) / 20;
  return bars > kBars ? kBars : bars;
}

std::string dateText(const StatusModel& model, i18n::Translator& t, bool withWeekday) {
  if (!model.dateKnown) return t.get("status.date-unknown");
  char month[8], day[8];
  snprintf(month, sizeof month, "%02d", model.month);
  snprintf(day, sizeof day, "%02d", model.day);
  if (!withWeekday) return t.get("status.date-short", {{"month", month}, {"day", day}});
  const int index = ((model.weekday % 7) + 7) % 7;
  const std::string weekday = t.get(kWeekdayKeys[index]);
  return t.get("status.date", {{"weekday", weekday.c_str()}, {"month", month}, {"day", day}});
}

StatusLeft statusLeft(const StatusModel& model, i18n::Translator& t) {
  StatusLeft left;
  switch (model.sim) {
    case StatusModel::Sim::Missing: left.text = t.get("status.sim-missing"); return left;
    case StatusModel::Sim::PinRequired: left.text = t.get("status.sim-pin"); return left;
    case StatusModel::Sim::Locked: left.text = t.get("status.sim-locked"); return left;
    case StatusModel::Sim::Ready: break;
  }
  if (model.quietHours) {
    left.text = t.get("status.quiet");
  } else if (model.link == StatusModel::Link::Operator && model.voice == StatusModel::Voice::SmsOnly) {
    left.text = t.get("status.sms-only");
  } else if (model.link == StatusModel::Link::Operator && model.voice == StatusModel::Voice::NotReady) {
    left.text = t.get("status.telephony-not-ready");
  } else if (model.link == StatusModel::Link::Operator && !model.operatorName.empty()) {
    left.text = model.operatorName;
    left.isOperatorName = true;
  } else if (model.link == StatusModel::Link::Operator) {
    // A network is there and the modem did not say whose: better a general word than the
    // "no network" of a device that is in fact connected.
    left.text = t.get("status.network");
  } else if (model.link == StatusModel::Link::Radio) {
    left.text = t.get("status.radio");
  } else {
    left.text = t.get("status.no-network");
  }
  return left;
}

void drawStatusBar(Display& display, const Rect& area, const StatusModel& model, i18n::Translator& t) {
  const Font& font = ui::font(FontSize::Small);
  const int line = font.lineHeight();

  // Battery frame at the right edge, one filled bar for each fifth.
  const Rect frame{area.right() - 2 - kFrameWidth, area.y + (line - kFrameHeight) / 2, kFrameWidth, kFrameHeight};
  display.outlineRect(frame, true);
  for (int i = 0; i < batteryBars(model.batteryPercent); ++i)
    display.fillRect({frame.x + 2 + i * (kBarWidth + kBarGap), frame.y + 2, kBarWidth, kFrameHeight - 4}, true);

  const StatusLeft left = statusLeft(model, t);
  const std::string gps = model.locating ? "  " + t.get("status.gps") : std::string();
  const int gpsWidth = text::width(font, gps);

  // The weekday is the first thing to go when the sides meet.
  std::string date = dateText(model, t, true);
  if (text::width(font, left.text) + gpsWidth + text::width(font, date) + kFrameWidth + 14 > area.w)
    date = dateText(model, t, false);
  const int dateWidth = text::drawRight(display, font, frame.x - 6, area.y, date);

  const int leftRoom = frame.x - 12 - dateWidth - (area.x + 2);
  const std::string shown = left.isOperatorName ? text::ellipsize(font, left.text, leftRoom - gpsWidth) : left.text;
  const int shownWidth = text::draw(display, font, area.x + 2, area.y, shown);
  text::draw(display, font, area.x + 2 + shownWidth, area.y, gps);

  display.fillRect({area.x, area.y + line, area.w, 2}, true);
}

void drawAlertBar(Display& display, const Rect& area, const std::string& text) {
  if (text.empty()) return;
  const Font& font = ui::font(FontSize::Small);
  display.fillRect(area, true);
  const std::string shown = text::ellipsize(font, text, area.w - kAlertBarMargin);
  text::draw(display, font, area.x + 3, area.y + 2, shown, false);
}

}  // namespace ui

#include "ui/screens/GnssScreen.h"

#include "ui/Fonts.h"
#include "ui/Formats.h"
#include "ui/PageTitle.h"
#include "ui/Text.h"

namespace ui {

void GnssScreen::onEnter() {
  app().setPositionPageOpen(true);
  block_.setText(std::string());
  text_.clear();
}

void GnssScreen::onLeave() { app().setPositionPageOpen(false); }

bool GnssScreen::current(GnssFix& fix) {
  uint32_t age = 0;
  return state().positioningActive && app().lastKnownFix(fix, &age) && fix.valid && age <= kCurrentSeconds;
}

Rect GnssScreen::textArea(const Rect& area) {
  const int header = pageTitleHeight(area, t().get("gnss.title"));
  return {area.x, area.y + header, area.w, area.h - header};
}

// What the page says when there is no current position.
std::string GnssScreen::words() {
  std::string text;
  const auto line = [&](const std::string& words) {
    if (!text.empty()) text += '\n';
    text += words;
  };
  if (state().positioningActive) {
    line(t().get("gnss.searching"));
    line(std::string());
    line(t().get("gnss.indoors"));
  } else {
    line(t().get("gnss.off"));
  }
  GnssFix last;
  uint32_t age = 0;
  if (app().lastKnownFix(last, &age) && last.valid) {
    line(std::string());
    line(t().get("gnss.last-known"));
    line(format::age(age, t()));
    line(format::degrees(last.lat));
    line(format::degrees(last.lon));
  }
  return text;
}

void GnssScreen::draw(Display& display, const Rect& area) {
  drawPageTitle(display, area, t().get("gnss.title"));
  const Rect below = textArea(area);
  GnssFix fix;
  if (!current(fix)) {
    const std::string now = words();
    if (now != text_) {
      text_ = now;
      block_.setText(now);
    }
    block_.draw(display, below);
    return;
  }
  // Four values, each a label above a number.
  const Font& small = font(FontSize::Small);
  const Font& body = font(FontSize::Text);
  const std::string meters = t().get("gnss.accuracy-value", {{"meters", static_cast<int>(SosPing::accuracyMeters(fix.hdop))}});
  const std::string satellites = std::to_string(static_cast<int>(fix.sats));
  const std::string latitude = format::degrees(fix.lat), longitude = format::degrees(fix.lon);
  struct Pair {
    std::string label, value;
  };
  const Pair pairs[] = {{t().get("gnss.latitude"), latitude}, {t().get("gnss.longitude"), longitude},
                        {t().get("gnss.accuracy"), meters}, {t().get("gnss.satellites"), satellites}};
  int y = below.y + 4;
  for (const Pair& pair : pairs) {
    text::draw(display, small, below.x + 6, y, pair.label);
    y += small.lineHeight();
    text::draw(display, body, below.x + 6, y, pair.value);
    y += body.lineHeight() + 4;
  }
}

SoftKeys GnssScreen::keys() {
  SoftKeys keys;
  keys.back = t().get("key.back");
  GnssFix fix;
  if (!current(fix)) {
    const Rect text = textArea(ScreenLayout::standard().content);
    keys.scroll = block_.lineCount(text) > block_.visibleLines(text);
  }
  return keys;
}

bool GnssScreen::onInput(const Input& input, const Rect& area) {
  if (input.isKey(Key::Back)) {
    nav().back();
    return true;
  }
  GnssFix fix;
  if (current(fix)) return false;
  return block_.handle(input, textArea(area));
}

}  // namespace ui

#include "ui/screens/LanguageScreen.h"

#include <string_view>

#include "ui/Fonts.h"
#include "ui/PageTitle.h"
#include "ui/Text.h"

namespace ui {

namespace {
constexpr int kNowGap = 4;
}  // namespace

void LanguageScreen::onEnter() {
  list_.resetSelection();
  focusOnCurrent_ = true;
}

void LanguageScreen::rebuild() {
  codes_.clear();
  std::vector<ListItem> items;
  size_t current = 0;
  for (const i18n::LanguageInfo& info : t().languages()) {
    if (info.code == t().language()) current = codes_.size();
    codes_.push_back(info.code);
    // A name that a card's catalog brought may be of any length; it is shortened with "..." like a contact's.
    items.push_back({info.native.empty() ? info.code : info.native, true});
  }
  list_.setItems(std::move(items));
  if (focusOnCurrent_) {
    list_.select(current);
    focusOnCurrent_ = false;
  }
}

// The name of the language in use, as it calls itself.
std::string LanguageScreen::now() {
  for (const i18n::LanguageInfo& info : t().languages())
    if (info.code == t().language()) return t().get("language.now", {{"name", (info.native.empty() ? info.code : info.native).c_str()}});
  return t().get("language.now", {{"name", t().language().c_str()}});
}

Rect LanguageScreen::listArea(const Rect& area) {
  const std::string title = t().get("language.title");
  const std::string current = now();
  const int header = pageTitleHeight(area, title) + static_cast<int>(text::wrap(font(FontSize::Small), current, area.w - 8).size()) * font(FontSize::Small).lineHeight() + kNowGap;
  return {area.x, area.y + header, area.w, area.h - header};
}

void LanguageScreen::draw(Display& display, const Rect& area) {
  rebuild();
  const std::string title = t().get("language.title");
  int y = area.y + pageTitleHeight(area, title) - 1;   // just below the rule
  drawPageTitle(display, area, title);
  const Font& small = font(FontSize::Small);
  const std::string current = now();                    // the lines are views into it
  y += kNowGap / 2;
  for (std::string_view line : text::wrap(small, current, area.w - 8)) {
    text::draw(display, small, area.x + 4, y, line);
    y += small.lineHeight();
  }
  list_.draw(display, listArea(area));
}

SoftKeys LanguageScreen::keys() {
  SoftKeys keys;
  keys.back = t().get("key.back");
  keys.action = t().get("key.choose");
  keys.scroll = codes_.size() > 1;
  return keys;
}

std::vector<Rect> LanguageScreen::targets(const Rect& area) {
  rebuild();
  return list_.targets(listArea(area));
}

bool LanguageScreen::onInput(const Input& input, const Rect& area) {
  if (input.isKey(Key::Back)) {
    nav().back();
    return true;
  }
  rebuild();
  bool chosen = false;
  const bool used = list_.handle(input, listArea(area), &chosen);
  if (chosen) app().setInterfaceLanguage(codes_[list_.selected()]);
  return used;
}

}  // namespace ui

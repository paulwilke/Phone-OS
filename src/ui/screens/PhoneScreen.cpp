#include "ui/screens/PhoneScreen.h"

#include <string_view>

#include "ui/Fonts.h"
#include "ui/Symbols.h"
#include "ui/Text.h"
#include "ui/Wording.h"

namespace ui {

namespace {

constexpr int kHeaderGap = 4;

// The title may need two lines in a language that needs more room; it is a fixed sentence and is
// wrapped, never cut.
std::vector<std::string_view> titleLines(const std::string& title, int width) {
  return text::wrap(font(FontSize::Small), title, width - 8);
}

}  // namespace

void PhoneScreen::onEnter() {
  other_ = false;
  list_.resetSelection();
}

// Call targets first, in the order the parents arranged them, then the other contacts.
void PhoneScreen::rebuild() {
  const Profile& profile = context().app.profile();
  const phone::ContactBook& book = profile.contacts;
  struct Entry {
    std::string phone;
    std::string label;
    bool isNumber;
  };
  std::vector<Entry> targets, others;
  auto add = [&](std::vector<Entry>& into, const std::string& phoneNumber) {
    const bool inBook = book.find(phoneNumber.c_str()) != nullptr;
    into.push_back({phoneNumber, wording::peerName(phoneNumber, book, t()), !inBook});
  };
  for (const std::string& target : profile.callTargets) {
    if (target.empty()) continue;
    bool seen = false;
    for (const Entry& e : targets) seen = seen || phone::same(e.phone.c_str(), target.c_str(), book.countryCode());
    if (!seen) add(targets, target);
  }
  for (size_t i = 0; i < book.count(); ++i) {
    bool isTarget = false;
    for (const Entry& e : targets) isTarget = isTarget || phone::same(e.phone.c_str(), book.at(i).phone, book.countryCode());
    if (!isTarget) add(others, book.at(i).phone);
  }

  rows_.clear();
  // Without call targets everybody is on the short list: there is nothing to put "Other" behind.
  const std::vector<Entry>& shown = other_ ? others : (targets.empty() ? others : targets);
  for (const Entry& e : shown) rows_.push_back({Row::Kind::Contact, e.label, e.phone, e.isNumber});
  if (!other_) {
    if (!targets.empty() && !others.empty()) rows_.push_back({Row::Kind::Other, t().get("phone.other"), "", false});
    if (!rows_.empty()) rows_.push_back({Row::Kind::Cancel, t().get("phone.cancel"), "", false});
  }

  std::vector<ListItem> items;
  for (const Row& row : rows_) {
    if (row.kind == Row::Kind::Contact)
      items.push_back({std::string(symbol::kTelephone) + " " + row.label, !row.isNumber});
    else
      items.push_back({row.label, false});
  }
  list_.setItems(std::move(items));
}

Rect PhoneScreen::listArea(const Rect& area) {
  const std::string title = t().get(other_ ? "phone.title-other" : "phone.title");
  const int header = static_cast<int>(titleLines(title, area.w).size()) * font(FontSize::Small).lineHeight() + kHeaderGap + 1 + kHeaderGap;
  return {area.x, area.y + header, area.w, area.h - header};
}

void PhoneScreen::draw(Display& display, const Rect& area) {
  rebuild();
  const Font& small = font(FontSize::Small);
  const std::string title = t().get(other_ ? "phone.title-other" : "phone.title");
  int y = area.y;
  for (std::string_view line : titleLines(title, area.w)) {
    text::draw(display, small, area.x + 4, y, line);
    y += small.lineHeight();
  }
  y += kHeaderGap / 2;
  display.fillRect({area.x, y, area.w, 1}, true);

  const Rect listRect = listArea(area);
  if (rows_.empty()) {
    int ty = listRect.y + 8;
    const std::string none = t().get("phone.none");   // the lines are views into it
    for (std::string_view line : text::wrap(font(FontSize::Text), none, area.w - 16)) {
      text::draw(display, font(FontSize::Text), area.x + 8, ty, line);
      ty += font(FontSize::Text).lineHeight();
    }
    return;
  }
  list_.draw(display, listRect);
}

SoftKeys PhoneScreen::keys() {
  SoftKeys keys;
  keys.back = t().get("key.back");
  keys.scroll = rows_.size() > 1;
  if (!rows_.empty()) keys.action = t().get("key.choose");
  return keys;
}

std::vector<Rect> PhoneScreen::targets(const Rect& area) {
  rebuild();
  return list_.targets(listArea(area));
}

bool PhoneScreen::onInput(const Input& input, const Rect& area) {
  if (input.isKey(Key::Back)) {
    if (other_) {
      other_ = false;
      list_.resetSelection();
    } else {
      nav().back();
    }
    return true;
  }
  rebuild();
  bool chosen = false;
  const bool used = list_.handle(input, listArea(area), &chosen);
  if (chosen) activate(rows_[list_.selected()]);
  return used;
}

void PhoneScreen::activate(const Row& row) {
  switch (row.kind) {
    case Row::Kind::Contact:
      // The core refuses with a notice (school time, a number that is not allowed, cellular off, no
      // network) and the page stays; when it accepts, the call page takes over.
      if (app().dial(row.phone)) nav().open(screen::kCall);
      return;
    case Row::Kind::Other:
      other_ = true;
      list_.resetSelection();
      return;
    case Row::Kind::Cancel: nav().back(); return;
  }
}

}  // namespace ui

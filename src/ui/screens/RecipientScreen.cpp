#include "ui/screens/RecipientScreen.h"

#include <string_view>

#include "ui/Fonts.h"
#include "ui/Text.h"
#include "ui/Wording.h"

namespace ui {

namespace {

constexpr int kHeaderGap = 4;

std::vector<std::string_view> titleLines(const std::string& title, int width) {
  return text::wrap(font(FontSize::Small), title, width - 8);
}

}  // namespace

void RecipientScreen::onEnter() {
  other_ = false;
  list_.resetSelection();
}

void RecipientScreen::rebuild() {
  const Profile& profile = context().app.profile();
  const phone::ContactBook& book = profile.contacts;
  struct Entry {
    std::string phone, label;
    bool isNumber;
  };
  std::vector<Entry> targets, others;
  auto add = [&](std::vector<Entry>& into, const std::string& number) {
    into.push_back({number, wording::peerName(number, book, t()), book.find(number.c_str()) == nullptr});
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
  const std::vector<Entry>& shown = other_ ? others : (targets.empty() ? others : targets);
  for (const Entry& e : shown) rows_.push_back({Row::Kind::Contact, e.label, e.phone, e.isNumber});
  if (other_) {
    if (profile.freeText) rows_.push_back({Row::Kind::NewNumber, t().get("recipient.new-number"), "", false});
  } else {
    if (!targets.empty() && !others.empty()) rows_.push_back({Row::Kind::Other, t().get("recipient.other"), "", false});
    // With nobody on the short list a free number is still a way to write.
    if (profile.freeText && targets.empty() && others.empty())
      rows_.push_back({Row::Kind::NewNumber, t().get("recipient.new-number"), "", false});
    if (!rows_.empty()) rows_.push_back({Row::Kind::Cancel, t().get("recipient.cancel"), "", false});
  }

  std::vector<ListItem> items;
  for (const Row& row : rows_) items.push_back({row.label, row.kind == Row::Kind::Contact && !row.isNumber});
  list_.setItems(std::move(items));
}

Rect RecipientScreen::listArea(const Rect& area) {
  const std::string title = t().get(other_ ? "recipient.title-other" : "recipient.title");
  const int header = static_cast<int>(titleLines(title, area.w).size()) * font(FontSize::Small).lineHeight() + 2 * kHeaderGap + 1;
  return {area.x, area.y + header, area.w, area.h - header};
}

void RecipientScreen::draw(Display& display, const Rect& area) {
  rebuild();
  const Font& small = font(FontSize::Small);
  const std::string title = t().get(other_ ? "recipient.title-other" : "recipient.title");
  int y = area.y;
  for (std::string_view line : titleLines(title, area.w)) {
    text::draw(display, small, area.x + 4, y, line);
    y += small.lineHeight();
  }
  y += kHeaderGap / 2;
  display.fillRect({area.x, y, area.w, 1}, true);
  const Rect listRect = listArea(area);
  if (rows_.empty()) {
    const std::string none = t().get("recipient.none");   // the lines are views into it
    int ty = listRect.y + 8;
    for (std::string_view line : text::wrap(font(FontSize::Text), none, area.w - 16)) {
      text::draw(display, font(FontSize::Text), area.x + 8, ty, line);
      ty += font(FontSize::Text).lineHeight();
    }
    return;
  }
  list_.draw(display, listRect);
}

SoftKeys RecipientScreen::keys() {
  SoftKeys keys;
  keys.back = t().get("key.back");
  keys.scroll = rows_.size() > 1;
  if (!rows_.empty()) keys.action = t().get("key.choose");
  return keys;
}

std::vector<Rect> RecipientScreen::targets(const Rect& area) {
  rebuild();
  return list_.targets(listArea(area));
}

bool RecipientScreen::onInput(const Input& input, const Rect& area) {
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

void RecipientScreen::activate(const Row& row) {
  switch (row.kind) {
    case Row::Kind::Contact:
      session().recipient = row.phone;
      session().viaRadio = false;
      nav().open(screen::kCompose);
      return;
    case Row::Kind::Other:
      other_ = true;
      list_.resetSelection();
      return;
    case Row::Kind::NewNumber: nav().open(screen::kNumber); return;
    case Row::Kind::Cancel: nav().back(); return;
  }
}

}  // namespace ui

#include "ui/ListView.h"

#include <algorithm>

#include "ui/Fonts.h"
#include "ui/Shapes.h"
#include "ui/Text.h"

namespace ui {

namespace {
constexpr int kRowPadding = 8;
constexpr int kBarWidth = 4;
constexpr int kTextInset = 4;
}  // namespace

void ListView::setItems(std::vector<ListItem> items) {
  items_ = std::move(items);
  if (selected_ >= items_.size()) selected_ = items_.empty() ? 0 : items_.size() - 1;
  if (first_ > selected_) first_ = selected_;
}

int ListView::rowHeight() const { return font(FontSize::Text).lineHeight() + kRowPadding; }

size_t ListView::visibleRows(const Rect& area) const {
  return static_cast<size_t>(std::max(1, area.h / rowHeight()));
}

Rect ListView::rowRect(const Rect& area, size_t index) const {
  return {area.x, area.y + static_cast<int>(index) * rowHeight(), area.w - kBarWidth - 2, rowHeight()};
}

void ListView::scrollToSelection(const Rect& area) {
  const size_t rows = visibleRows(area);
  if (selected_ < first_) first_ = selected_;
  if (selected_ >= first_ + rows) first_ = selected_ + 1 - rows;
  if (items_.size() > rows && first_ > items_.size() - rows) first_ = items_.size() - rows;
}

std::vector<Rect> ListView::targets(const Rect& area) const {
  std::vector<Rect> out;
  const size_t rows = items_.size() > first_ ? std::min(visibleRows(area), items_.size() - first_) : 0;
  for (size_t i = 0; i < rows; ++i) out.push_back(rowRect(area, i));
  return out;
}

bool ListView::handle(const Input& input, const Rect& area, bool* chosen) {
  *chosen = false;
  if (items_.empty()) return false;
  if (input.isKey(Key::Up)) {
    if (selected_ > 0) --selected_;
    scrollToSelection(area);
    return true;
  }
  if (input.isKey(Key::Down)) {
    if (selected_ + 1 < items_.size()) ++selected_;
    scrollToSelection(area);
    return true;
  }
  if (input.isKey(Key::Select)) {
    *chosen = true;
    return true;
  }
  if (input.kind == Input::Kind::Touch) {
    scrollToSelection(area);
    const size_t rows = std::min(visibleRows(area), items_.size() - first_);
    for (size_t i = 0; i < rows; ++i) {
      if (rowRect(area, i).contains(input.x, input.y)) {
        selected_ = first_ + i;
        *chosen = true;
        return true;
      }
    }
  }
  return false;
}

void ListView::draw(Display& display, const Rect& area) {
  scrollToSelection(area);
  const Font& font = ui::font(FontSize::Text);
  const size_t rows = std::min(visibleRows(area), items_.size() - first_);
  for (size_t i = 0; i < rows; ++i) {
    const ListItem& item = items_[first_ + i];
    const Rect row = rowRect(area, i);
    const bool selected = first_ + i == selected_;
    if (selected) display.fillRect(row, true);
    const int room = row.w - 2 * kTextInset;
    const std::string shown = item.userText ? text::ellipsize(font, item.text, room) : item.text;
    text::draw(display, font, row.x + kTextInset, row.y + kRowPadding / 2, shown, !selected);
  }
  shapes::drawScrollBar(display, {area.right() - kBarWidth, area.y, kBarWidth, area.h}, items_.size(), first_,
                        visibleRows(area));
}

}  // namespace ui

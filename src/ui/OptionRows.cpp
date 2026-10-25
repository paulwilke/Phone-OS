#include "ui/OptionRows.h"

#include <algorithm>

#include "ui/Fonts.h"
#include "ui/Shapes.h"
#include "ui/Text.h"

namespace ui {

namespace {
constexpr int kGap = 4;
constexpr int kPadding = 3;
constexpr int kButtonPadding = 6;   // a button has only one line, and room for the second outline of the current one
constexpr int kBarWidth = 4;
constexpr int kRadius = 8;
constexpr int kMark = 3;   // how far inside the second outline of the current row lies
}  // namespace

void OptionRows::setRows(std::vector<OptionRow> rows) {
  rows_ = std::move(rows);
  if (selected_ >= rows_.size()) selected_ = rows_.empty() ? 0 : rows_.size() - 1;
  if (first_ > selected_) first_ = selected_;
}

int OptionRows::rowHeight() const {
  const bool values = std::any_of(rows_.begin(), rows_.end(), [](const OptionRow& r) { return !r.value.empty(); });
  const int text = font(FontSize::Text).lineHeight();
  return values ? 2 * kPadding + font(FontSize::Small).lineHeight() + text : 2 * kButtonPadding + text;
}

size_t OptionRows::visibleRows(const Rect& area) const {
  return static_cast<size_t>(std::max(1, (area.h + kGap) / (rowHeight() + kGap)));
}

Rect OptionRows::rowRect(const Rect& area, size_t index) const {
  return {area.x + 2, area.y + static_cast<int>(index) * (rowHeight() + kGap), area.w - kBarWidth - 6, rowHeight()};
}

void OptionRows::scrollToSelection(const Rect& area) {
  const size_t rows = visibleRows(area);
  if (selected_ < first_) first_ = selected_;
  if (selected_ >= first_ + rows) first_ = selected_ + 1 - rows;
  if (rows_.size() > rows && first_ > rows_.size() - rows) first_ = rows_.size() - rows;
}

std::vector<Rect> OptionRows::targets(const Rect& area) const {
  std::vector<Rect> out;
  const size_t rows = rows_.size() > first_ ? std::min(visibleRows(area), rows_.size() - first_) : 0;
  for (size_t i = 0; i < rows; ++i) out.push_back(rowRect(area, i));
  return out;
}

bool OptionRows::handle(const Input& input, const Rect& area, bool* chosen) {
  *chosen = false;
  if (rows_.empty()) return false;
  if (input.isKey(Key::Up)) {
    if (selected_ > 0) --selected_;
    scrollToSelection(area);
    return true;
  }
  if (input.isKey(Key::Down)) {
    if (selected_ + 1 < rows_.size()) ++selected_;
    scrollToSelection(area);
    return true;
  }
  if (input.isKey(Key::Select)) {
    *chosen = true;
    return true;
  }
  if (input.kind == Input::Kind::Touch) {
    scrollToSelection(area);
    const size_t shown = std::min(visibleRows(area), rows_.size() - first_);
    for (size_t i = 0; i < shown; ++i) {
      if (rowRect(area, i).contains(input.x, input.y)) {
        selected_ = first_ + i;
        *chosen = true;
        return true;
      }
    }
  }
  return false;
}

void OptionRows::draw(Display& display, const Rect& area) {
  scrollToSelection(area);
  const Font& small = font(FontSize::Small);
  const Font& body = font(FontSize::Text);
  const size_t shown = std::min(visibleRows(area), rows_.size() - first_);
  for (size_t i = 0; i < shown; ++i) {
    const OptionRow& row = rows_[first_ + i];
    const Rect box = rowRect(area, i);
    const bool focused = first_ + i == selected_;
    if (focused) shapes::fillRoundRect(display, box, kRadius, true);
    else shapes::outlineRoundRect(display, box, kRadius, true);
    if (row.current) shapes::outlineRoundRect(display, {box.x + kMark, box.y + kMark, box.w - 2 * kMark, box.h - 2 * kMark}, kRadius - kMark, !focused);
    if (row.value.empty()) {
      text::drawCentered(display, body, box, box.y + (box.h - body.lineHeight()) / 2, row.label, !focused);
    } else {
      text::drawCentered(display, small, box, box.y + kPadding, row.label, !focused);
      text::drawCentered(display, body, box, box.y + kPadding + small.lineHeight(), row.value, !focused);
    }
  }
  shapes::drawScrollBar(display, {area.right() - kBarWidth, area.y, kBarWidth, area.h}, rows_.size(), first_,
                        visibleRows(area));
}

}  // namespace ui

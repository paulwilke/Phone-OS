#include "ui/TextBlock.h"

#include <algorithm>

#include "ui/Shapes.h"
#include "ui/Text.h"

namespace ui {

namespace {
constexpr int kBarWidth = 4;
}  // namespace

void TextBlock::setText(std::string text) {
  text_ = std::move(text);
  first_ = 0;
}

int TextBlock::textWidth(const Rect& area) const { return area.w - kBarWidth - 4; }

size_t TextBlock::lineCount(const Rect& area) const {
  return text::wrap(font(size_), text_, textWidth(area)).size();
}

size_t TextBlock::visibleLines(const Rect& area) const {
  return static_cast<size_t>(std::max(1, area.h / font(size_).lineHeight()));
}

bool TextBlock::handle(const Input& input, const Rect& area) {
  const size_t total = lineCount(area), visible = visibleLines(area);
  const size_t last = total > visible ? total - visible : 0;
  if (input.isKey(Key::Up)) {
    if (first_ > 0) --first_;
    return true;
  }
  if (input.isKey(Key::Down)) {
    if (first_ < last) ++first_;
    return true;
  }
  return false;
}

void TextBlock::draw(Display& display, const Rect& area) {
  const Font& f = font(size_);
  const auto lines = text::wrap(f, text_, textWidth(area));
  const size_t visible = visibleLines(area);
  if (first_ + visible > lines.size() && lines.size() > visible) first_ = lines.size() - visible;
  for (size_t i = 0; i < visible && first_ + i < lines.size(); ++i)
    text::draw(display, f, area.x + 2, area.y + static_cast<int>(i) * f.lineHeight(), lines[first_ + i]);
  shapes::drawScrollBar(display, {area.right() - kBarWidth, area.y, kBarWidth, area.h}, lines.size(), first_, visible);
}

}  // namespace ui

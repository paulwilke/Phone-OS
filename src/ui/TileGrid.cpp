#include "ui/TileGrid.h"

#include <algorithm>

#include "ui/Fonts.h"
#include "ui/Shapes.h"
#include "ui/Text.h"

namespace ui {

namespace {

constexpr int kGap = 4;
constexpr int kRadius = 4;
constexpr int kDotsStrip = 18;  // at the bottom, for the page dots; always reserved so tiles do not move
constexpr int kDot = 6;

}  // namespace

void TileGrid::setPages(std::vector<std::vector<Tile>> pages) {
  pages_ = std::move(pages);
  // A page holds at most perPage() tiles; the rest would be unreachable.
  for (auto& p : pages_)
    if (p.size() > perPage()) p.resize(perPage());
  while (!pages_.empty() && pages_.back().empty()) pages_.pop_back();
  const size_t n = count();
  if (selected_ >= n) selected_ = n ? n - 1 : 0;
}

void TileGrid::select(size_t index) {
  const size_t n = count();
  selected_ = n == 0 ? 0 : (index < n ? index : n - 1);
}

size_t TileGrid::count() const {
  size_t n = 0;
  for (const auto& p : pages_) n += p.size();
  return n;
}

const Tile& TileGrid::selectedTile() const {
  size_t index = selected_;
  for (const auto& p : pages_) {
    if (index < p.size()) return p[index];
    index -= p.size();
  }
  static const Tile none;
  return none;
}

size_t TileGrid::page() const {
  size_t index = selected_;
  for (size_t i = 0; i < pages_.size(); ++i) {
    if (index < pages_[i].size()) return i;
    index -= pages_[i].size();
  }
  return 0;
}

size_t TileGrid::firstOfPage(size_t page) const {
  size_t first = 0;
  for (size_t i = 0; i < page && i < pages_.size(); ++i) first += pages_[i].size();
  return first;
}

Rect TileGrid::tileRect(const Rect& area, size_t index) const {
  const int cellW = (area.w - kGap * (columns_ + 1)) / columns_;
  const int cellH = (area.h - kDotsStrip - kGap * (rows_ + 1)) / rows_;
  size_t pageIndex = 0;
  while (pageIndex + 1 < pages_.size() && index >= firstOfPage(pageIndex + 1)) ++pageIndex;
  const size_t slot = index - firstOfPage(pageIndex);
  const int col = static_cast<int>(slot % columns_), row = static_cast<int>(slot / columns_);
  return {area.x + kGap + col * (cellW + kGap), area.y + kGap + row * (cellH + kGap), cellW, cellH};
}

Rect TileGrid::dotTarget(const Rect& area, size_t page) const {
  if (pages_.size() < 2 || page >= pages_.size()) return Rect{};
  const int total = static_cast<int>(pages_.size()) * kDot + (static_cast<int>(pages_.size()) - 1) * kDot;
  const int x0 = area.x + (area.w - total) / 2;
  return {x0 + static_cast<int>(page) * 2 * kDot - kDot / 2, area.bottom() - kDotsStrip, 2 * kDot, kDotsStrip};
}

bool TileGrid::handle(const Input& input, const Rect& area, bool* chosen) {
  *chosen = false;
  const size_t n = count();
  if (n == 0) return false;
  if (input.isKey(Key::Up)) {
    selected_ = selected_ ? selected_ - 1 : n - 1;
    return true;
  }
  if (input.isKey(Key::Down)) {
    selected_ = (selected_ + 1) % n;
    return true;
  }
  if (input.isKey(Key::Select)) {
    *chosen = true;
    return true;
  }
  if (input.kind == Input::Kind::Touch) {
    for (size_t p = 0; p < pages_.size(); ++p) {
      if (dotTarget(area, p).contains(input.x, input.y)) {
        selected_ = firstOfPage(p);
        return true;
      }
    }
    // Only the tiles of the page on show can be hit.
    const size_t shown = page();
    const size_t first = firstOfPage(shown);
    for (size_t i = 0; i < pages_[shown].size(); ++i) {
      if (tileRect(area, first + i).contains(input.x, input.y)) {
        selected_ = first + i;
        *chosen = true;
        return true;
      }
    }
  }
  return false;
}

void TileGrid::drawTile(Display& display, const Rect& cell, const Tile& tile, bool selected) {
  const Font& small = font(FontSize::Small);
  const Font& symbolFont = font(FontSize::Tile);
  const bool ink = !selected;  // text colour: white on the inverted tile

  if (selected) {
    shapes::fillRoundRect(display, cell, kRadius, true);
  } else {
    shapes::outlineRoundRect(display, cell, kRadius, true);
    display.fillRect({cell.x + kRadius + 1, cell.bottom(), cell.w - 2 * kRadius - 1, 1}, true);   // shadow below
    display.fillRect({cell.right(), cell.y + kRadius + 1, 1, cell.h - 2 * kRadius - 1}, true);    // and to the right
  }

  const Rect inner{cell.x + 3, cell.y, cell.w - 6, cell.h};
  if (tile.symbol) {
    const int block = symbolFont.lineHeight() + small.lineHeight();
    const int top = cell.y + (cell.h - block) / 2;
    text::drawCentered(display, symbolFont, inner, top, tile.symbol, ink);
    text::drawCentered(display, small, inner, top + symbolFont.lineHeight(), tile.label, ink);
  } else {
    const auto lines = text::wrap(small, tile.label, inner.w);
    const int top = cell.y + (cell.h - small.lineHeight() * static_cast<int>(lines.size())) / 2;
    for (size_t l = 0; l < lines.size(); ++l)
      text::drawCentered(display, small, inner, top + static_cast<int>(l) * small.lineHeight(), lines[l], ink);
  }
  if (!tile.badge.empty()) text::drawRight(display, small, cell.right() - 3, cell.y + 2, tile.badge, ink);
}

void TileGrid::draw(Display& display, const Rect& area) const {
  if (pages_.empty()) return;
  const size_t shown = page();
  const size_t first = firstOfPage(shown);
  for (size_t i = 0; i < pages_[shown].size(); ++i)
    drawTile(display, tileRect(area, first + i), pages_[shown][i], first + i == selected_);

  if (pages_.size() > 1) {
    const int total = static_cast<int>(pages_.size()) * kDot + (static_cast<int>(pages_.size()) - 1) * kDot;
    const int x0 = area.x + (area.w - total) / 2;
    const int y = area.bottom() - (kDotsStrip + kDot) / 2;
    for (size_t p = 0; p < pages_.size(); ++p) {
      const Rect dot{x0 + static_cast<int>(p) * 2 * kDot, y, kDot, kDot};
      if (p == shown) display.fillRect(dot, true);
      else display.outlineRect(dot, true);
    }
  }
}

}  // namespace ui

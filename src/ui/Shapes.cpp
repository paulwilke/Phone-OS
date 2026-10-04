#include "ui/Shapes.h"

#include <algorithm>
#include <cmath>

namespace ui {
namespace shapes {

namespace {

// How many pixels the corner cuts off the row `dy` rows from the top or bottom edge.
int inset(int radius, int dy) {
  if (dy >= radius) return 0;
  const double fromCentre = radius - dy - 0.5;
  const double across = std::sqrt(std::max(0.0, double(radius) * radius - fromCentre * fromCentre));
  return radius - static_cast<int>(std::lround(across));
}

int clampRadius(const Rect& r, int radius) { return std::max(0, std::min(radius, std::min(r.w, r.h) / 2)); }

// The columns [first, last) of row `y` that belong to the rounded rectangle, or an empty span.
bool span(const Rect& r, int radius, int y, int* first, int* last) {
  if (y < r.y || y >= r.bottom() || r.empty()) return false;
  const int fromEdge = std::min(y - r.y, r.bottom() - 1 - y);
  const int cut = inset(radius, fromEdge);
  *first = r.x + cut;
  *last = r.right() - cut;
  return *first < *last;
}

}  // namespace

void fillRoundRect(Display& display, const Rect& r, int radius, bool black) {
  radius = clampRadius(r, radius);
  for (int y = r.y; y < r.bottom(); ++y) {
    int a, b;
    if (span(r, radius, y, &a, &b)) display.fillRect({a, y, b - a, 1}, black);
  }
}

void outlineRoundRect(Display& display, const Rect& r, int radius, bool black) {
  radius = clampRadius(r, radius);
  const Rect inner{r.x + 1, r.y + 1, r.w - 2, r.h - 2};
  const int innerRadius = std::max(0, radius - 1);
  for (int y = r.y; y < r.bottom(); ++y) {
    int a, b, c, d;
    if (!span(r, radius, y, &a, &b)) continue;
    if (!span(inner, innerRadius, y, &c, &d)) {
      display.fillRect({a, y, b - a, 1}, black);  // a border row, or a rectangle too thin for an inside
    } else {
      if (c > a) display.fillRect({a, y, c - a, 1}, black);
      if (b > d) display.fillRect({d, y, b - d, 1}, black);
    }
  }
}

void drawScrollBar(Display& display, const Rect& track, size_t total, size_t first, size_t visible) {
  if (track.empty() || visible >= total || total == 0) return;
  display.fillRect({track.x + track.w / 2, track.y, 1, track.h}, true);
  const int thumb = std::max(4, static_cast<int>(static_cast<long long>(track.h) * visible / total));
  const int room = track.h - thumb;
  const size_t last = total - visible;
  const int offset = last ? static_cast<int>(static_cast<long long>(room) * std::min(first, last) / last) : 0;
  display.fillRect({track.x, track.y + offset, track.w, thumb}, true);
}

}  // namespace shapes
}  // namespace ui

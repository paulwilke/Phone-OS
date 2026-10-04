#pragma once

// The few shapes the widgets share: rounded rectangles and a scroll bar. Everything draws
// inside the rectangle it is given and nothing outside it.

#include <cstddef>

#include "ui/Display.h"

namespace ui {
namespace shapes {

// A rectangle with its corners cut by a quarter circle of `radius` pixels. A radius of 0
// is a plain rectangle; one too large for the rectangle is reduced to fit.
void fillRoundRect(Display& display, const Rect& r, int radius, bool black = true);
// The same shape, one pixel wide, drawn inside `r`.
void outlineRoundRect(Display& display, const Rect& r, int radius, bool black = true);

// A thin scroll bar in `track` for `total` rows of which `visible` are shown starting at row
// `first`. Draws nothing when everything is visible. The thumb is never shorter than 4 pixels.
void drawScrollBar(Display& display, const Rect& track, size_t total, size_t first, size_t visible);

}  // namespace shapes
}  // namespace ui

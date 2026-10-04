#pragma once

// What the user interface draws on. The panel is 240 × 320 pixels, one bit per pixel
// (black or white), origin top-left; the e-ink panel cannot do more, so there is no
// grey and no colour in this interface.
//
// Drawing is **always clipped by the caller**: the display does not scale, wrap or
// shorten anything. A pixel outside the screen is dropped, and a widget that draws
// outside the rectangle it owns is a bug that the frame buffer used in tests reports
// (see testing/FrameBuffer.h).
//
// A frame is drawn between beginFrame() and endFrame(); the driver may hold the whole
// image back until endFrame() and decide there how to refresh the panel.

#include <cstdint>

namespace ui {

constexpr int kScreenWidth = 240;
constexpr int kScreenHeight = 320;

struct Rect {
  int x = 0, y = 0, w = 0, h = 0;

  int right() const { return x + w; }    // first column outside
  int bottom() const { return y + h; }   // first row outside
  bool empty() const { return w <= 0 || h <= 0; }
  bool contains(int px, int py) const { return px >= x && px < right() && py >= y && py < bottom(); }
  bool operator==(const Rect& o) const { return x == o.x && y == o.y && w == o.w && h == o.h; }
};

class Display {
public:
  virtual ~Display() = default;

  virtual int width() const = 0;
  virtual int height() const = 0;

  virtual void beginFrame() = 0;
  virtual void endFrame() = 0;

  // Sets one pixel; `black` false paints white. Outside the screen: ignored.
  virtual void setPixel(int x, int y, bool black) = 0;
  // Fills the rectangle, and draws its one-pixel outline inside it.
  virtual void fillRect(const Rect& r, bool black) = 0;
  virtual void outlineRect(const Rect& r, bool black) = 0;
  // Draws a packed bitmap with its top-left corner at (x, y). Rows follow each other,
  // each starting at a byte boundary (`(w + 7) / 8` bytes per row), the leftmost pixel
  // in the most significant bit. **Only the set bits are drawn**, in the given colour;
  // the clear bits leave the background as it is, so glyphs can be drawn on anything.
  virtual void drawBitmap(int x, int y, const uint8_t* bits, int w, int h, bool black) = 0;
};

}  // namespace ui

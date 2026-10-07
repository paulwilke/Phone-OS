#pragma once

// A picture of one bit per pixel: the frame buffer of the e-ink panel and the copy of what the panel shows.
// Rows are packed from the left, the leftmost pixel in the most significant bit, each row starting at a byte
// boundary, as ui::Display::drawBitmap takes them. A set bit is black. Drawing outside is dropped.

#include <algorithm>
#include <cstdint>
#include <vector>

#include "../ui/Display.h"

namespace board {

class Bitmap1 {
public:
  Bitmap1(int width, int height) : w_(width), h_(height), stride_((width + 7) / 8), bits_((size_t)stride_ * (size_t)height, 0) {}

  int width() const { return w_; }
  int height() const { return h_; }
  int stride() const { return stride_; }
  const uint8_t* data() const { return bits_.data(); }
  size_t size() const { return bits_.size(); }

  void clear(bool black = false) { std::fill(bits_.begin(), bits_.end(), black ? 0xFF : 0x00); }
  bool pixel(int x, int y) const;
  void setPixel(int x, int y, bool black);
  void fillRect(const ui::Rect& r, bool black);
  void outlineRect(const ui::Rect& r, bool black);
  void drawBitmap(int x, int y, const uint8_t* bits, int w, int h, bool black);

  bool operator==(const Bitmap1& o) const { return w_ == o.w_ && h_ == o.h_ && bits_ == o.bits_; }

  // The smallest rectangle that holds every pixel in which this picture differs from `other`, widened to
  // whole bytes on the x axis (the panel addresses columns in groups of eight). False when they are equal.
  bool differences(const Bitmap1& other, ui::Rect& out) const;

  // The bytes of the rectangle (x and width in multiples of 8), row after row, as the panel takes them. With
  // `invert` a black pixel is a clear bit.
  std::vector<uint8_t> window(const ui::Rect& r, bool invert) const;

private:
  int w_, h_, stride_;
  std::vector<uint8_t> bits_;
};

}  // namespace board

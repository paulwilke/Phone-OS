#include "Bitmap1.h"

#include <algorithm>

namespace board {

bool Bitmap1::pixel(int x, int y) const {
  if (x < 0 || y < 0 || x >= w_ || y >= h_) return false;
  return (bits_[(size_t)y * stride_ + (size_t)(x / 8)] & (0x80 >> (x % 8))) != 0;
}

void Bitmap1::setPixel(int x, int y, bool black) {
  if (x < 0 || y < 0 || x >= w_ || y >= h_) return;
  uint8_t& b = bits_[(size_t)y * stride_ + (size_t)(x / 8)];
  const uint8_t mask = (uint8_t)(0x80 >> (x % 8));
  if (black) b |= mask;
  else b &= (uint8_t)~mask;
}

void Bitmap1::fillRect(const ui::Rect& r, bool black) {
  for (int y = std::max(0, r.y); y < std::min(h_, r.bottom()); ++y)
    for (int x = std::max(0, r.x); x < std::min(w_, r.right()); ++x) setPixel(x, y, black);
}

void Bitmap1::outlineRect(const ui::Rect& r, bool black) {
  if (r.empty()) return;
  fillRect({r.x, r.y, r.w, 1}, black);
  fillRect({r.x, r.bottom() - 1, r.w, 1}, black);
  fillRect({r.x, r.y, 1, r.h}, black);
  fillRect({r.right() - 1, r.y, 1, r.h}, black);
}

void Bitmap1::drawBitmap(int x, int y, const uint8_t* bits, int w, int h, bool black) {
  const int rowBytes = (w + 7) / 8;
  for (int row = 0; row < h; ++row)
    for (int col = 0; col < w; ++col)
      if (bits[row * rowBytes + col / 8] & (0x80 >> (col % 8))) setPixel(x + col, y + row, black);
}

bool Bitmap1::differences(const Bitmap1& other, ui::Rect& out) const {
  if (w_ != other.w_ || h_ != other.h_) {
    out = {0, 0, w_, h_};
    return true;
  }
  int top = h_, bottom = -1, left = stride_, right = -1;
  for (int y = 0; y < h_; ++y)
    for (int b = 0; b < stride_; ++b)
      if (bits_[(size_t)y * stride_ + b] != other.bits_[(size_t)y * stride_ + b]) {
        top = std::min(top, y);
        bottom = std::max(bottom, y);
        left = std::min(left, b);
        right = std::max(right, b);
      }
  if (bottom < 0) return false;
  out = {left * 8, top, (right - left + 1) * 8, bottom - top + 1};
  return true;
}

std::vector<uint8_t> Bitmap1::window(const ui::Rect& r, bool invert) const {
  std::vector<uint8_t> out;
  out.reserve((size_t)(r.w / 8) * (size_t)r.h);
  for (int y = r.y; y < r.bottom(); ++y)
    for (int b = r.x / 8; b < (r.x + r.w) / 8; ++b) {
      const uint8_t v = bits_[(size_t)y * stride_ + (size_t)b];
      out.push_back(invert ? (uint8_t)~v : v);
    }
  return out;
}

}  // namespace board

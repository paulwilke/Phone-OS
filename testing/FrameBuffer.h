#pragma once

// An in-memory 1-bit display for tests and, later, the simulator.
//
// Beyond remembering the pixels it keeps the layout honest:
//
//   * **Clip detection.** A widget declares the rectangle it owns with beginRegion() /
//     endRegion(). Every drawing call that touches a pixel outside that rectangle is
//     recorded (clipEvents()), and so is every pixel outside the screen. A test of a
//     widget asserts `!clipped()`: text that runs over its right edge, a bitmap that
//     sticks out of its tile, a fill that is too tall. Without a region only the
//     screen edge is checked. Regions nest; the innermost one is the rule.
//   * **ASCII art.** toAscii() writes the buffer (or a rectangle of it) as lines of
//     `#` (black) and `.` (white), fromAscii() reads it back; these are the golden
//     files of the widget tests.
//   * **CRC.** crc() is the checksum of the content, as the e-ink refresh logic uses it.
//
// Only the pixels a call would actually change count as "touched": a bitmap's clear bits
// do not, a fill touches every pixel of its rectangle, white or black.

#include <algorithm>
#include <string>
#include <vector>

#include <app/Crc32.h>
#include <ui/Display.h>

class FrameBuffer : public ui::Display {
public:
  struct ClipEvent {
    std::string call;  // "setPixel", "fillRect", "outlineRect", "drawBitmap"
    bool hasRegion = false;  // a region was declared; otherwise only the screen edge counts
    ui::Rect region;   // the declared rectangle
    int outside = 0;   // how many pixels of this call fell outside it
    int offScreen = 0; // how many fell outside the screen
    int firstX = 0, firstY = 0;  // the first offender
  };

  explicit FrameBuffer(int w = ui::kScreenWidth, int h = ui::kScreenHeight)
      : w_(w), h_(h), bits_((size_t)w * (size_t)h, 0) {}

  // ---- ui::Display -------------------------------------------------------------------
  int width() const override { return w_; }
  int height() const override { return h_; }
  void beginFrame() override { ++frames_; }
  void endFrame() override { ++endedFrames_; }

  void setPixel(int x, int y, bool black) override {
    Call call("setPixel", this);
    call.plot(x, y, black);
  }
  void fillRect(const ui::Rect& r, bool black) override {
    Call call("fillRect", this);
    for (int y = r.y; y < r.bottom(); ++y) {
      for (int x = r.x; x < r.right(); ++x) call.plot(x, y, black);
    }
  }
  void outlineRect(const ui::Rect& r, bool black) override {
    if (r.empty()) return;
    Call call("outlineRect", this);
    for (int x = r.x; x < r.right(); ++x) {
      call.plot(x, r.y, black);
      if (r.h > 1) call.plot(x, r.bottom() - 1, black);
    }
    for (int y = r.y + 1; y < r.bottom() - 1; ++y) {
      call.plot(r.x, y, black);
      if (r.w > 1) call.plot(r.right() - 1, y, black);
    }
  }
  void drawBitmap(int x, int y, const uint8_t* bits, int w, int h, bool black) override {
    Call call("drawBitmap", this);
    const int stride = (w + 7) / 8;
    for (int row = 0; row < h; ++row) {
      for (int col = 0; col < w; ++col) {
        if (bits[row * stride + col / 8] & (0x80 >> (col % 8))) call.plot(x + col, y + row, black);
      }
    }
  }

  // ---- reading -------------------------------------------------------------------------
  bool pixel(int x, int y) const { return inScreen(x, y) && bits_[index(x, y)]; }

  // Black pixels in the rectangle (the part inside the screen).
  int blackPixels(const ui::Rect& r) const {
    int n = 0;
    for (int y = r.y; y < r.bottom(); ++y) {
      for (int x = r.x; x < r.right(); ++x) n += pixel(x, y);
    }
    return n;
  }
  int blackPixels() const { return blackPixels({0, 0, w_, h_}); }

  void clear() { std::fill(bits_.begin(), bits_.end(), 0); }

  int frames() const { return frames_; }
  int endedFrames() const { return endedFrames_; }

  // The content packed like a bitmap (rows padded to bytes, leftmost pixel in the top bit),
  // and its checksum. The size is part of the checksum, so a 240 × 320 and a 320 × 240
  // buffer with the same bits differ.
  std::vector<uint8_t> packed() const {
    const int stride = (w_ + 7) / 8;
    std::vector<uint8_t> out((size_t)stride * (size_t)h_, 0);
    for (int y = 0; y < h_; ++y) {
      for (int x = 0; x < w_; ++x) {
        if (bits_[index(x, y)]) out[(size_t)y * stride + x / 8] |= (uint8_t)(0x80 >> (x % 8));
      }
    }
    return out;
  }
  uint32_t crc() const {
    const std::vector<uint8_t> data = packed();
    const uint8_t size[4] = {(uint8_t)(w_ >> 8), (uint8_t)w_, (uint8_t)(h_ >> 8), (uint8_t)h_};
    return checksum::crc32(data.data(), data.size(), checksum::crc32(size, 4));
  }
  bool operator==(const FrameBuffer& o) const { return w_ == o.w_ && h_ == o.h_ && bits_ == o.bits_; }

  // ---- ASCII art -------------------------------------------------------------------------
  // Lines of '#' and '.', each ending in a line break. A rectangle outside the screen reads as white.
  std::string toAscii(const ui::Rect& r) const {
    std::string out;
    for (int y = r.y; y < r.bottom(); ++y) {
      for (int x = r.x; x < r.right(); ++x) out += pixel(x, y) ? '#' : '.';
      out += '\n';
    }
    return out;
  }
  std::string toAscii() const { return toAscii({0, 0, w_, h_}); }

  // Reads ASCII art into a buffer of the size of the art. False for an empty text, lines of
  // different lengths, or any character but '#', '.' and line breaks.
  static bool fromAscii(const std::string& art, FrameBuffer& out) {
    std::vector<std::string> lines;
    std::string line;
    for (char c : art) {
      if (c == '\n') {
        lines.push_back(line);
        line.clear();
      } else if (c == '\r') {
        continue;
      } else if (c == '#' || c == '.') {
        line += c;
      } else {
        return false;
      }
    }
    if (!line.empty()) lines.push_back(line);
    if (lines.empty() || lines[0].empty()) return false;
    for (const std::string& l : lines) {
      if (l.size() != lines[0].size()) return false;
    }
    FrameBuffer fb((int)lines[0].size(), (int)lines.size());
    for (size_t y = 0; y < lines.size(); ++y) {
      for (size_t x = 0; x < lines[y].size(); ++x) fb.bits_[fb.index((int)x, (int)y)] = lines[y][x] == '#';
    }
    out = fb;
    return true;
  }

  // ---- clip detection ------------------------------------------------------------------------
  // The widget declares the rectangle it owns. Regions nest.
  void beginRegion(const ui::Rect& r) { regions_.push_back(r); }
  void endRegion() {
    if (!regions_.empty()) regions_.pop_back();
  }
  bool clipped() const { return !clipEvents_.empty(); }
  const std::vector<ClipEvent>& clipEvents() const { return clipEvents_; }
  void clearClipEvents() { clipEvents_.clear(); }
  // One line per event, for a failure message.
  std::string describeClips() const {
    std::string out;
    for (const ClipEvent& e : clipEvents_) {
      out += e.call + ": " + std::to_string(e.outside) + " pixel(s) outside ";
      out += !e.hasRegion ? "the screen" : "region " + std::to_string(e.region.x) + "," + std::to_string(e.region.y) +
                                               " " + std::to_string(e.region.w) + "x" + std::to_string(e.region.h);
      out += ", first at " + std::to_string(e.firstX) + "," + std::to_string(e.firstY) + "\n";
    }
    return out;
  }

private:
  // One drawing call: collects what it touched outside, records one event at the end.
  struct Call {
    Call(const char* name, FrameBuffer* fb) : fb(fb) {
      event.call = name;
      if (!fb->regions_.empty()) {
        event.hasRegion = true;
        event.region = fb->regions_.back();
      }
    }
    ~Call() {
      if (event.outside > 0 || event.offScreen > 0) fb->clipEvents_.push_back(event);
    }
    void plot(int x, int y, bool black) {
      const bool onScreen = fb->inScreen(x, y);
      const bool inRegion = event.hasRegion ? event.region.contains(x, y) : onScreen;
      if (!onScreen) ++event.offScreen;
      if (!inRegion || !onScreen) {
        if (event.outside == 0) {
          event.firstX = x;
          event.firstY = y;
        }
        ++event.outside;
      }
      if (onScreen) fb->bits_[fb->index(x, y)] = black;
    }
    FrameBuffer* fb;
    ClipEvent event;
  };

  bool inScreen(int x, int y) const { return x >= 0 && y >= 0 && x < w_ && y < h_; }
  size_t index(int x, int y) const { return (size_t)y * (size_t)w_ + (size_t)x; }

  int w_, h_;
  std::vector<uint8_t> bits_;
  int frames_ = 0, endedFrames_ = 0;
  std::vector<ui::Rect> regions_;
  std::vector<ClipEvent> clipEvents_;
};

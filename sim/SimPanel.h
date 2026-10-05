#pragma once

// The e-ink panel of the simulator, without a window: what the device's driver does, minus the
// hardware. The interface draws on it like on any ui::Display; at the end of the frame the picture
// goes through the real RefreshController (ui/RefreshController.h), so the panel is refreshed
// partially, in full or not at all exactly as the firmware will decide, and what it shows is the
// picture of the last refresh — not of the last frame.
//
// A refresh takes time on a real panel and the simulator says how long (kPartialMs, kFullMs: round
// figures for a 3-inch panel, to make the delay visible, not a measurement). busy() tells a window
// whether to show the flash; a screenshot is of the settled picture.
//
// Not simulated: ghosting, the true duration, the temperature. See docs/simulator.md.

#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include <FrameBuffer.h>
#include <Png.h>
#include <ui/RefreshController.h>

namespace sim {

class SimPanel : public ui::Display, private ui::Panel {
public:
  static constexpr uint32_t kPartialMs = 300;
  static constexpr uint32_t kFullMs = 1200;

  struct Refresh {
    uint32_t frame;  // which frame (counted from 0) caused it
    uint32_t atMs;
    bool full;
  };

  // `now` is the simulator's millisecond counter, asked at the end of every frame.
  explicit SimPanel(std::function<uint32_t()> now, ui::RefreshConfig config = ui::RefreshConfig())
      : now_(std::move(now)), controller_(*this, config, now_()) {}

  // ---- ui::Display: the picture being drawn --------------------------------------------
  int width() const override { return drawn_.width(); }
  int height() const override { return drawn_.height(); }
  void beginFrame() override { drawn_.beginFrame(); }
  void endFrame() override {
    drawn_.endFrame();
    controller_.present(drawn_.crc(), now_());
    ++frames_;
  }
  void setPixel(int x, int y, bool black) override { drawn_.setPixel(x, y, black); }
  void fillRect(const ui::Rect& r, bool black) override { drawn_.fillRect(r, black); }
  void outlineRect(const ui::Rect& r, bool black) override { drawn_.outlineRect(r, black); }
  void drawBitmap(int x, int y, const uint8_t* bits, int w, int h, bool black) override {
    drawn_.drawBitmap(x, y, bits, w, h, black);
  }

  // ---- the panel --------------------------------------------------------------------------
  // What the panel shows once the last refresh is done; blank before the first one.
  const FrameBuffer& shown() const { return shown_; }
  // What it showed before the last refresh, which a window keeps on show while that refresh runs.
  const FrameBuffer& previous() const { return previous_; }
  // The last refresh is still running at `nowMs`: a window shows it as a flash.
  bool busy(uint32_t nowMs) const {
    if (refreshes_.empty()) return false;
    const Refresh& last = refreshes_.back();
    return nowMs - last.atMs < (last.full ? kFullMs : kPartialMs);
  }
  const std::vector<Refresh>& refreshes() const { return refreshes_; }
  const ui::RefreshController::Stats& stats() const { return controller_.stats(); }

  // The panel as a PNG (Png.h), and into a file; false when the file cannot be written.
  std::vector<uint8_t> png() const { return encodePng(shown_.packed(), shown_.width(), shown_.height()); }
  bool writePng(const std::string& path) const {
    const std::vector<uint8_t> bytes = png();
    FILE* file = std::fopen(path.c_str(), "wb");
    if (!file) return false;
    const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    return std::fclose(file) == 0 && ok;
  }

private:
  void refreshPartial() override { refreshed(false); }
  void refreshFull() override { refreshed(true); }
  void refreshed(bool full) {
    previous_ = shown_;
    shown_ = drawn_;
    refreshes_.push_back({frames_, now_(), full});
  }

  std::function<uint32_t()> now_;
  FrameBuffer drawn_;
  FrameBuffer shown_;
  FrameBuffer previous_;
  ui::RefreshController controller_;
  std::vector<Refresh> refreshes_;
  uint32_t frames_ = 0;
};

}  // namespace sim

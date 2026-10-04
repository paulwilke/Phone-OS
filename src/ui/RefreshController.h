#pragma once

// Ties the pieces together at the end of a frame: the checksum of the picture that was just
// drawn goes to the FrameTracker, the answer to the RefreshPolicy, and the decision to the
// panel. The e-ink driver implements `Panel` and calls present() from Display::endFrame().
// The first frame is always refreshed in full, because nothing is known about what the panel
// shows after start-up.

#include <cstdint>

#include "ui/FrameTracker.h"
#include "ui/RefreshPolicy.h"

namespace ui {

// What the panel can be told to do once the picture is in its memory.
class Panel {
public:
  virtual ~Panel() = default;
  virtual void refreshPartial() = 0;
  virtual void refreshFull() = 0;
};

class RefreshController {
public:
  struct Stats {
    uint32_t frames = 0;     // frames presented
    uint32_t skipped = 0;    // no refresh: the picture was the one on the panel
    uint32_t partial = 0;
    uint32_t full = 0;
  };

  RefreshController(Panel& panel, RefreshConfig config = RefreshConfig(), uint32_t startMs = 0)
      : panel_(panel), policy_(config, startMs) {}

  // The frame with this checksum was drawn at `nowMs`. Returns what was done to the panel.
  RefreshPolicy::Action present(uint32_t crc, uint32_t nowMs);

  // The panel's content is unknown (it was powered off): the next frame is refreshed in full,
  // changed or not. Frames after it are compared with that frame as before.
  void invalidate();

  const Stats& stats() const { return stats_; }
  const FrameTracker& tracker() const { return tracker_; }

private:
  Panel& panel_;
  RefreshPolicy policy_;
  FrameTracker tracker_;
  Stats stats_;
  bool forceFull_ = true;  // what the panel shows at start-up is not known: the first frame is a full refresh
};

}  // namespace ui

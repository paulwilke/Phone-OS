#pragma once

// A panel that only counts, and a rig that draws frames into a FrameBuffer and presents them
// to a RefreshController, so that a test can state how often the panel may be refreshed in a
// scenario (a frame budget) and fail, with the history, when the interface makes it work more.
//
//   RefreshRig rig;
//   rig.frame(0, [&](ui::Display& d) { drawHome(d, state); });      // boot: one full refresh
//   for (...) rig.frame(1000 * i, [&](ui::Display& d) { drawHome(d, state); });
//   rig.expect("an idle hour", /*partial*/ 0, /*full*/ 1);
//
// Time is the rig's own millisecond counter, given with every frame.

#include <unity.h>

#include <string>
#include <vector>

#include <FrameBuffer.h>
#include <ui/RefreshController.h>

class RecordingPanel : public ui::Panel {
public:
  struct Event {
    uint32_t frame;
    uint32_t nowMs;
    bool full;
  };
  void refreshPartial() override { events.push_back({frame, nowMs, false}); }
  void refreshFull() override { events.push_back({frame, nowMs, true}); }

  uint32_t frame = 0, nowMs = 0;  // set by the rig before each frame, for the history
  std::vector<Event> events;
};

class RefreshRig {
public:
  explicit RefreshRig(ui::RefreshConfig config = ui::RefreshConfig(), uint32_t startMs = 0)
      : controller(panel, config, startMs) {}

  // Clears the buffer, lets `draw` paint the picture, presents it. Returns what the panel was told.
  template <class Draw>
  ui::RefreshPolicy::Action frame(uint32_t nowMs, Draw&& draw) {
    fb.clear();
    fb.beginFrame();
    draw(fb);
    fb.endFrame();
    panel.frame = frames_++;
    panel.nowMs = nowMs;
    return controller.present(fb.crc(), nowMs);
  }

  int partial() const { return static_cast<int>(controller.stats().partial); }
  int full() const { return static_cast<int>(controller.stats().full); }

  // The refreshes so far, one per line, for a failure message.
  std::string history() const {
    std::string out = "refreshes (frame, ms):";
    for (const auto& e : panel.events)
      out += "\n  frame " + std::to_string(e.frame) + " at " + std::to_string(e.nowMs) + " ms: " + (e.full ? "full" : "partial");
    return out;
  }

  // Fails the test unless the panel was refreshed exactly this often since the rig was made
  // (or since the last mark()).
  void expect(const char* what, int partialCount, int fullCount) const {
    const std::string message = std::string(what) + ": expected " + std::to_string(partialCount) + " partial and " +
                                std::to_string(fullCount) + " full, got " + std::to_string(partial() - markPartial_) +
                                " and " + std::to_string(full() - markFull_) + "\n" + history();
    TEST_ASSERT_TRUE_MESSAGE(partial() - markPartial_ == partialCount && full() - markFull_ == fullCount, message.c_str());
  }

  // Starts counting from here, e.g. after the boot frame.
  void mark() {
    markPartial_ = partial();
    markFull_ = full();
  }

  FrameBuffer fb;
  RecordingPanel panel;
  ui::RefreshController controller;

private:
  uint32_t frames_ = 0;
  int markPartial_ = 0, markFull_ = 0;
};

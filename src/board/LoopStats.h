#pragma once

// What the main loop did in the last half hour, as one line for the system log (decision D-20): the longest
// pass of work (what blocks a key press or a touch), how many passes there were, and how much of the time the
// processor slept. Two questions of the owner are answered by it: whether anything blocks the loop for seconds
// (D-8: the sensor start, the e-ink wait, the card), and whether the processor really goes into light sleep when the
// power policy lets it (the energy log showed about 105 mA with the modem asleep).
//
// A pass is measured without the sleep or the pause that ends it: `pass(workMs, sleptMs)`. Pure arithmetic on
// 32-bit millisecond counters that wrap; the caller reads the clock.

#include <cstdint>
#include <string>

namespace board {

constexpr uint32_t kLoopStatsIntervalMs = 30 * 60 * 1000;   // a line every half hour: 48 a day, not a flood in the system log

class LoopStats {
public:
  void pass(uint32_t nowMs, uint32_t workMs, uint32_t sleptMs) {
    if (!started_) {
      started_ = true;
      windowStart_ = nowMs;
    }
    ++passes_;
    if (workMs > longestMs_) longestMs_ = workMs;
    sleptMs_ += sleptMs;
  }

  bool due(uint32_t nowMs) const { return started_ && (uint32_t)(nowMs - windowStart_) >= kLoopStatsIntervalMs; }

  // The line for the window that ends now, and a new window.
  std::string takeLine(uint32_t nowMs) {
    const uint32_t window = (uint32_t)(nowMs - windowStart_);
    uint64_t asleep = window ? (uint64_t)sleptMs_ * 100 / window : 0;
    if (asleep > 100) asleep = 100;
    const std::string line = "Loop: longest pass " + std::to_string(longestMs_) + " ms, " + std::to_string(passes_) + " passes in " +
                             std::to_string((window + 500) / 1000) + " s, asleep " + std::to_string((unsigned)asleep) + " %";
    windowStart_ = nowMs;
    passes_ = 0;
    longestMs_ = 0;
    sleptMs_ = 0;
    return line;
  }

private:
  bool started_ = false;
  uint32_t windowStart_ = 0;
  uint32_t passes_ = 0;
  uint32_t longestMs_ = 0;
  uint32_t sleptMs_ = 0;
};

}  // namespace board

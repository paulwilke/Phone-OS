#pragma once

// What the main loop did in the last half hour, as one line for the system log (decisions D-20 and D-25): the
// longest pass of work (what blocks a key press or a touch), how many passes there were, how much of the time the
// processor slept, in how many passes the power policy allowed it to sleep, and where the time of a pass goes.
// Questions of the owner are answered by it: whether anything blocks the loop for seconds (D-8: the sensor start, the
// e-ink wait, the card), and why the processor sleeps 1 % of the time and draws about 105 mA with the modem asleep:
// the policy not allowing it, or a pass that takes about 50 ms of work.
//
// A pass is measured without the sleep or the pause that ends it: `pass(...)`. The parts of the work are timed by
// the caller in microseconds (`section`); what is left over is "other". Pure arithmetic on 32-bit millisecond
// counters that wrap; the caller reads the clock.

#include <cstdint>
#include <string>

namespace board {

constexpr uint32_t kLoopStatsIntervalMs = 30 * 60 * 1000;   // a line every half hour: 48 a day, not a flood in the system log

class LoopStats {
public:
  // The parts of a pass that are timed on their own.
  enum class Section : uint8_t { Console, Keys, Modem, Step, Housekeeping, kCount };

  void section(Section which, uint32_t micros) { sectionUs_[(size_t)which] += micros; }

  // One pass: its work in microseconds (without the sleep or the pause), the milliseconds slept at its end, and
  // whether the power policy allowed the processor to sleep.
  void pass(uint32_t nowMs, uint32_t workUs, uint32_t sleptMs, bool maySleep) {
    if (!started_) {
      started_ = true;
      windowStart_ = nowMs;
    }
    ++passes_;
    if (workUs / 1000u > longestMs_) longestMs_ = workUs / 1000u;
    workUs_ += workUs;
    sleptMs_ += sleptMs;
    if (maySleep) ++maySleep_;
  }

  bool due(uint32_t nowMs) const { return started_ && (uint32_t)(nowMs - windowStart_) >= kLoopStatsIntervalMs; }

  // The line for the window that ends now, and a new window.
  std::string takeLine(uint32_t nowMs) {
    const uint32_t window = (uint32_t)(nowMs - windowStart_);
    uint64_t asleep = window ? (uint64_t)sleptMs_ * 100 / window : 0;
    if (asleep > 100) asleep = 100;
    const uint64_t may = passes_ ? (uint64_t)maySleep_ * 100 / passes_ : 0;
    std::string line = "Loop: longest pass " + std::to_string(longestMs_) + " ms, " + std::to_string(passes_) + " passes in " +
                       std::to_string((window + 500) / 1000) + " s, asleep " + std::to_string((unsigned)asleep) +
                       " %, sleep allowed in " + std::to_string((unsigned)may) + " % of the passes";
    if (passes_ > 0 && sectionTotal() > 0) {
      static const char* const names[] = {"console", "keys", "modem", "step", "housekeeping"};
      line += "; per pass (ms)";
      uint64_t sections = 0;
      for (size_t i = 0; i < (size_t)Section::kCount; ++i) {
        line += std::string(i ? ", " : " ") + names[i] + " " + tenths(sectionUs_[i], passes_);
        sections += sectionUs_[i];
      }
      const uint64_t other = workUs_ > sections ? workUs_ - sections : 0;
      line += ", other " + tenths(other, passes_);
    }
    windowStart_ = nowMs;
    passes_ = 0;
    longestMs_ = 0;
    sleptMs_ = 0;
    maySleep_ = 0;
    workUs_ = 0;
    for (auto& us : sectionUs_) us = 0;
    return line;
  }

private:
  uint64_t sectionTotal() const {
    uint64_t total = 0;
    for (uint64_t us : sectionUs_) total += us;
    return total;
  }
  // microseconds over a number of passes, in tenths of a millisecond: "50.1"
  static std::string tenths(uint64_t micros, uint32_t passes) {
    const uint64_t t = passes ? (micros * 10 / passes + 500) / 1000 : 0;   // rounded to a tenth of a millisecond
    return std::to_string((unsigned)(t / 10)) + "." + std::to_string((unsigned)(t % 10));
  }

  bool started_ = false;
  uint32_t windowStart_ = 0;
  uint32_t passes_ = 0;
  uint32_t longestMs_ = 0;
  uint32_t sleptMs_ = 0;
  uint32_t maySleep_ = 0;
  uint64_t workUs_ = 0;
  uint64_t sectionUs_[(size_t)Section::kCount] = {};
};

}  // namespace board

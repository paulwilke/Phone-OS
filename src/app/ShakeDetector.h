#pragma once

// Recognises shaking from acceleration samples. A shake is a burst well above 1 g;
// each burst counts once, on its rising edge, and the magnitude must fall below a
// rest threshold before the next burst can count. A device lying still or tilted
// slowly stays near 1 g and never counts, whatever its orientation.
//
// The detector keeps no clock and does not decide what a number of shakes means;
// whoever feeds it samples decides how many shakes within how much time are meant.

#include <cstdint>

class ShakeDetector {
public:
  struct Config {
    float highG = 1.8f;  // at or above this a burst begins
    float lowG = 1.3f;   // at or below this the burst is over
  };

  ShakeDetector() : ShakeDetector(Config{}) {}
  // A rest threshold above the start threshold is lowered to it: otherwise one
  // long burst would end after every sample and count again and again.
  explicit ShakeDetector(const Config& cfg)
      : high2_(cfg.highG * cfg.highG),
        low2_(cfg.lowG > cfg.highG ? cfg.highG * cfg.highG : cfg.lowG * cfg.lowG) {}

  // One sample in g on the three axes. True when it begins a new shake. The
  // comparison is on the squared magnitude, which needs no square root and treats
  // a sample exactly on a threshold the same on every platform.
  bool feed(float ax, float ay, float az) {
    const float g2 = ax * ax + ay * ay + az * az;
    if (!inShake_ && g2 >= high2_) {
      inShake_ = true;
      ++count_;
      return true;
    }
    if (inShake_ && g2 <= low2_) inShake_ = false;
    return false;
  }

  uint32_t count() const { return count_; }
  void reset() {
    count_ = 0;
    inShake_ = false;
  }

private:
  float high2_;
  float low2_;
  uint32_t count_ = 0;
  bool inShake_ = false;
};

#pragma once

// A motion sensor driven by the test.
//
// Scripting: setAvailable(bool), failNextStart(), tilt(x, y, z) sets the resting value
// (it starts as lying flat), clearSample() makes "no value yet", shake() makes the next
// two readings a strong movement (a burst), then it rests again.

#include <hal/MotionSensor.h>

class FakeMotion : public MotionSensor {
public:
  bool available() override { return available_; }
  bool start() override {
    if (!available_ || failStart_) {
      failStart_ = false;
      return false;
    }
    running_ = true;
    return true;
  }
  void stop() override { running_ = false; }
  bool running() const override { return running_; }

  bool read(MotionSample& out) override {
    if (!running_) return false;
    if (shakeLeft_ > 0) {
      --shakeLeft_;
      out = {2.8f, -1.5f, 1.2f, true};
      return true;
    }
    if (!hasSample_) return false;
    out = {ax_, ay_, az_, true};
    return true;
  }

  // Scripting.
  void setAvailable(bool available) { available_ = available; }
  void failNextStart() { failStart_ = true; }
  void tilt(float x, float y, float z) {
    ax_ = x;
    ay_ = y;
    az_ = z;
    hasSample_ = true;
  }
  void clearSample() { hasSample_ = false; }
  void shake() { shakeLeft_ = 2; }

private:
  bool available_ = true;
  bool failStart_ = false;
  bool running_ = false;
  bool hasSample_ = true;
  float ax_ = 0, ay_ = 0, az_ = 1;
  int shakeLeft_ = 0;
};

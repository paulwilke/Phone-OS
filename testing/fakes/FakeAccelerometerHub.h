#pragma once

// The sensor hub chip, scripted. It starts as lying flat on a table, which on this device reads about
// -0.95 g on z (the sign is the sensor's own).
//   failNextStart()    the next start() fails
//   tilt(x, y, z)      the resting value
//   clearSample()      no value yet
//   watchRail(expander, leg)   at start() record whether the rail was on, as the chip needs it
// Reading back: starts(), stops(), railWasOnAtStart().

#include <board/AccelerometerHub.h>
#include <board/Xl9555.h>

class FakeAccelerometerHub : public board::AccelerometerHub {
public:
  bool start() override {
    ++starts_;
    if (rail_) railOn_ = rail_->level(railLeg_);
    if (failStart_) {
      failStart_ = false;
      return false;
    }
    running_ = true;
    return true;
  }
  void stop() override {
    ++stops_;
    running_ = false;
  }
  bool read(float& x, float& y, float& z) override {
    if (!running_ || !hasSample_) return false;
    x = x_;
    y = y_;
    z = z_;
    return true;
  }

  void failNextStart() { failStart_ = true; }
  void tilt(float x, float y, float z) {
    x_ = x;
    y_ = y;
    z_ = z;
    hasSample_ = true;
  }
  void clearSample() { hasSample_ = false; }
  void watchRail(const board::Xl9555& expander, uint8_t leg) {
    rail_ = &expander;
    railLeg_ = leg;
  }
  int starts() const { return starts_; }
  int stops() const { return stops_; }
  bool railWasOnAtStart() const { return railOn_; }

private:
  bool failStart_ = false;
  bool running_ = false;
  bool hasSample_ = true;
  float x_ = 0, y_ = 0, z_ = -0.95f;
  int starts_ = 0;
  int stops_ = 0;
  const board::Xl9555* rail_ = nullptr;
  uint8_t railLeg_ = 0;
  bool railOn_ = false;
};

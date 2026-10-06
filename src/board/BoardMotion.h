#pragma once

// The MotionSensor capability on the board. The sensor's 1.8 V rail hangs on the expander and the chip needs
// its firmware at every power-up, so it runs only while a screen needs it: start() switches the rail on and
// starts the hub, stop() stops it and cuts the rail.
//
// **The samples are handed on as the sensor reports them.** Lying flat on a table the device reads about
// -0.95 on z (measured on 2026-10-03; the library's field is called m/s² but holds g), and the motion
// screen's mapping from the sensor's axes to the picture was checked with exactly these values. A
// "correction" of the sign here would flip every direction on the screen.

#include "../hal/MotionSensor.h"
#include "AccelerometerHub.h"
#include "Delay.h"
#include "Xl9555.h"

namespace board {

// The rail needs a moment before the chip answers.
constexpr uint32_t kMotionRailSettleMs = 30;

class BoardMotion : public MotionSensor {
public:
  BoardMotion(Xl9555& expander, AccelerometerHub& hub, Delay& delay) : expander_(expander), hub_(hub), delay_(delay) {}

  // Puts the rail into its idle state (off): the expander's boot state powers it, and nobody has asked yet.
  void begin();

  // The hardware is on the board whenever the expander that powers it answers; whether the chip itself
  // answers shows only start().
  bool available() override { return expander_.ready(); }
  bool start() override;
  void stop() override;
  bool running() const override { return running_; }
  bool read(MotionSample& out) override;

private:
  Xl9555& expander_;
  AccelerometerHub& hub_;
  Delay& delay_;
  bool running_ = false;
};

}  // namespace board

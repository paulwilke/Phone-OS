#pragma once

// The seam below the motion sensor: what the sensor hub chip (BHI260AP) has to be told and asked, in raw
// numbers. The chip is a small computer of its own: it gets its firmware loaded into RAM at every power-up
// (a blob that is delivered inside a library and is not part of this repository), which takes seconds. The
// device implements it with that library; the tests with a scripted hub.

namespace board {

class AccelerometerHub {
public:
  virtual ~AccelerometerHub() = default;

  // Loads the firmware and starts the acceleration stream; blocks for seconds (about 11 on the device).
  // False when the chip does not answer.
  virtual bool start() = 0;
  virtual void stop() = 0;
  // The newest acceleration in g, in the sensor's own axes; false when there is none yet.
  virtual bool read(float& x, float& y, float& z) = 0;
};

}  // namespace board

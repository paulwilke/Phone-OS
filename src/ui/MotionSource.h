#pragma once

// What the motion page needs of the acceleration sensor, in the interface's own terms: a screen may
// not know the capability of the hardware layer (src/hal/MotionSensor.h), so the application hands it
// something that does what this says. The sensor is a small computer of its own: **starting it takes
// seconds and blocks** (about 11 s on the device), and it costs current, so the page starts it only once
// it has said so on the panel, and stops it when it is left.
//
// A sample is in g and in the **axes of the sensor**. On the T-Deck Max the sensor is turned 90 degrees
// against the screen; the page's mapping to the screen is in MotionScreen (see screen-motion.md).

namespace ui {

struct Acceleration {
  float x = 0, y = 0, z = 0;  // in g
  bool valid = false;
};

class MotionSource {
public:
  virtual ~MotionSource() = default;
  // Is there a sensor at all?
  virtual bool available() = 0;
  // Switches it on; blocks until it is ready. False when it does not answer; it is not running then.
  virtual bool start() = 0;
  virtual void stop() = 0;
  virtual bool running() const = 0;
  // The newest value; false while it is not running or has none yet.
  virtual bool read(Acceleration& out) = 0;
};

}  // namespace ui

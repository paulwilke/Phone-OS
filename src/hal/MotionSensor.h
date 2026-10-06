#pragma once

// Capability: motion sensor. The device's acceleration sensor is a small computer of
// its own: it needs firmware at power-up and a supply rail of its own, so it costs power
// and time. It therefore runs **only while a screen needs it** (start/stop).
//
// Values are in g (1.0 = gravity) and in the axes of the sensor itself, not of the screen: the sensor of the
// T-Deck Pro MAX is turned against the screen, and lying flat on a table it reads about -0.95 on z. Whoever
// hands samples on hands them on unchanged (the motion page's mapping was checked with exactly these values).

struct MotionSample {
  float ax = 0, ay = 0, az = 0;  // in g
  bool valid = false;
};

class MotionSensor {
public:
  virtual ~MotionSensor() = default;
  // Is there a sensor at all (hardware present, driver reported)?
  virtual bool available() = 0;
  // Switches it on. False when it is not available or does not answer; it is not running then.
  virtual bool start() = 0;
  virtual void stop() = 0;
  virtual bool running() const = 0;
  // The newest value. False while the sensor is not running or has no value yet; `out`
  // is left alone then.
  virtual bool read(MotionSample& out) = 0;
};

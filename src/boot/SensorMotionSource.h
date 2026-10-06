#pragma once

// The motion sensor of the hardware layer (src/hal/MotionSensor.h) seen as the interface's MotionSource
// (src/ui/MotionSource.h): what the runtime's wiring does between the two layers, because a screen may
// not know a hardware capability. The sample is handed on unchanged, in the sensor's own axes. Without a
// sensor the page still exists: it says there is none.

#include "../hal/MotionSensor.h"
#include "../ui/MotionSource.h"

namespace boot {

class SensorMotionSource : public ui::MotionSource {
public:
  explicit SensorMotionSource(MotionSensor* sensor) : sensor_(sensor) {}

  bool available() override { return sensor_ && sensor_->available(); }
  bool start() override { return sensor_ && sensor_->start(); }
  void stop() override {
    if (sensor_) sensor_->stop();
  }
  bool running() const override { return sensor_ && sensor_->running(); }
  bool read(ui::Acceleration& out) override {
    MotionSample sample;
    if (!sensor_ || !sensor_->read(sample)) return false;
    out = {sample.ax, sample.ay, sample.az, sample.valid};
    return true;
  }

private:
  MotionSensor* sensor_;
};

}  // namespace boot

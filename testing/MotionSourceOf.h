#pragma once

// A hardware motion sensor (a FakeMotion) seen as the interface's MotionSource: what the application's
// wiring does between the two layers, here for the tests of the motion page. The sensor's value is
// handed on unchanged, in the sensor's own axes.

#include <hal/MotionSensor.h>
#include <ui/MotionSource.h>

class MotionSourceOf : public ui::MotionSource {
public:
  explicit MotionSourceOf(MotionSensor& sensor) : sensor_(sensor) {}
  bool available() override { return sensor_.available(); }
  bool start() override {
    ++starts;
    return sensor_.start();
  }
  void stop() override {
    ++stops;
    sensor_.stop();
  }
  bool running() const override { return sensor_.running(); }
  bool read(ui::Acceleration& out) override {
    MotionSample sample;
    if (!sensor_.read(sample)) return false;
    out = {sample.ax, sample.ay, sample.az, sample.valid};
    return true;
  }

  int starts = 0, stops = 0;

private:
  MotionSensor& sensor_;
};

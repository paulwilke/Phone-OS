#pragma once

// The sensor hub chip BHI260AP through SensorLib (MIT). The chip gets its firmware loaded into RAM at every
// power-up: the "shuttle" firmware that comes with the library, acceleration only. **The firmware is the
// manufacturer's (Bosch) and its redistribution terms are not verified** (THIRD_PARTY.md), so it is not part of
// this repository: the library delivers it at build time, and a binary that contains it is for the
// owner's own device.
//
// Facts from the device (2026-10-03): starting takes about 11 s, a transfer block of 64 bytes works and
// 256 does not, no reset line is wired, the interrupt line (GPIO 21) is not used (the stream is polled), and
// the library's field called m/s² holds g.

#define BOSCH_APP30_SHUTTLE_BHI260_FW
#include <BoschFirmware.h>
#include <SensorBHI260AP.hpp>
#include <Wire.h>
#include <bosch/BoschSensorDataHelper.hpp>

#include <board/AccelerometerHub.h>
#include <board/PinMap.h>

class Esp32Bhi260Hub : public board::AccelerometerHub {
public:
  Esp32Bhi260Hub() : accel_(hub_) {}

  bool start() override {
    hub_.setPins(-1);
    hub_.setMaxiTransferSize(64);  // the default of 32 bytes makes the firmware load slow
    hub_.setFirmware(bosch_firmware_image, bosch_firmware_size, bosch_firmware_type, false);
    if (!hub_.begin(Wire, BHI260AP_SLAVE_ADDRESS_L, board::pin::kI2cSda, board::pin::kI2cScl)) return false;
    accel_.enable(25.0f, 0);  // 25 Hz is enough for a tilt and a shake
    return true;
  }

  void stop() override { accel_.disable(); }

  bool read(float& x, float& y, float& z) override {
    hub_.update();
    AccelerometerData d;
    if (!accel_.readData(d)) return false;
    x = d.mps2.x;
    y = d.mps2.y;
    z = d.mps2.z;
    return true;
  }

private:
  SensorBHI260AP hub_;
  SensorAcceleration accel_;
};

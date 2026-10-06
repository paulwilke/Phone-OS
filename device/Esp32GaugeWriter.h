#pragma once

// Writes the battery size into the fuel gauge with SensorLib's procedure (unseal, enter the configuration
// update, write the data memory with its checksum, leave, seal again). That procedure is the library's
// (MIT); the decision whether to write at all is board::ensureCapacity()'s and is tested on the host.

#include <GaugeDrv.hpp>
#include <Wire.h>

#include <board/FuelGauge.h>

class Esp32GaugeWriter : public board::GaugeCapacityWriter {
public:
  bool setCapacities(uint16_t designMah, uint16_t fullChargeMah) override {
    // The bus is already started by the caller; no pins are passed so the library does not start it again.
    if (!gauge_.begin(Wire)) return false;
    return gauge_.setNewCapacity(designMah, fullChargeMah);
  }

private:
  GaugeBQ27220 gauge_;
};

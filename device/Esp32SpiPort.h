#pragma once

// The shared SPI bus behind the SpiPort seam, for the display: mode 0, most significant bit first. The clock of
// the panel's transfers is the bus's own setting for this device, 4 MHz, which the card also stands.

#include <board/SpiPort.h>

#include "Esp32Spi.h"

class Esp32SpiPort : public SpiPort {
public:
  void write(const uint8_t* data, size_t len) override {
    SPIClass& spi = boardSpi();
    spi.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    spi.writeBytes(data, len);
    spi.endTransaction();
  }
};

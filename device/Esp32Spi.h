#pragma once

// The one SPI bus of the board. The display, the SD card and the radio share the same three lines (clock,
// MOSI, MISO) and have a chip select each; **exactly one `SPIClass` may drive them**: two objects on the
// same pins (HSPI and FSPI) fight over the legs of the ESP32-S3, and the second participant stays silent
// or returns garbage. Whoever adds a fourth participant uses this object and its own chip select.

#include <SPI.h>

#include <board/PinMap.h>

inline SPIClass& boardSpi() {
  static SPIClass spi(FSPI);
  static bool started = false;
  if (!started) {
    spi.begin(board::pin::kSpiSck, board::pin::kSpiMiso, board::pin::kSpiMosi, -1);
    started = true;
  }
  return spi;
}

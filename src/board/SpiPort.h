#pragma once

// The seam to the shared SPI bus for a device that is written to: bytes out, in mode 0, most significant bit
// first. The chip select is the caller's (several devices share the bus, each with its own).

#include <cstddef>
#include <cstdint>

class SpiPort {
public:
  virtual ~SpiPort() = default;
  virtual void write(const uint8_t* data, size_t len) = 0;
};

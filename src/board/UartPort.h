#pragma once

// The seam to a serial port (the satellite receiver, the modem). Bytes in, bytes out; the pins and the baud
// rate are the implementation's business (the device) and are not on this interface.

#include <cstddef>
#include <cstdint>

class UartPort {
public:
  virtual ~UartPort() = default;

  // Returns the number of bytes accepted.
  virtual size_t write(const uint8_t* data, size_t len) = 0;
  // The next received byte, or -1 when none is waiting. Never blocks.
  virtual int read() = 0;
  // Whether anybody is listening. A port with a fixed peer (the modem, the receiver) is always "connected"; the
  // USB console is not: with no computer attached, output has nowhere to go and nobody may wait for it.
  virtual bool hostConnected() { return true; }
};

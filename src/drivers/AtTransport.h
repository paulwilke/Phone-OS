#pragma once

// The only seam between the modem driver and the UART: bytes in, bytes out, and the
// time. Everything above it (lines, answers, jobs, calls, messages) is plain C++ and is
// tested on the host against FakeAtTransport, a scripted modem. What stands below it
// (the serial port, the power and key lines of the board) comes with the device drivers.

#include <cstddef>
#include <cstdint>

class AtTransport {
public:
  virtual ~AtTransport() = default;

  virtual void write(const char* data, size_t len) = 0;
  // The next received byte, or -1 when none is waiting. Never blocks.
  virtual int read() = 0;
  // Milliseconds since an arbitrary start. It wraps after about 49 days, so a caller
  // compares differences (see at::Deadline), never absolute values.
  virtual uint32_t now() = 0;
};

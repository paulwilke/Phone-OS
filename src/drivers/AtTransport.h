#pragma once

// The only seam between the modem driver and the UART: bytes in, bytes out, the time and
// the two lines that switch the modem on. Everything above it (lines, answers, jobs, calls, messages) is plain C++ and is
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

  // The supply of the modem, and the pulse of its power key. A running modem is switched OFF
  // by the pulse, and after a reset of the processor it still runs (its supply hangs on an
  // expander, not on the processor), so the driver asks first and presses only when the
  // modem stays silent.
  virtual void power(bool on) = 0;
  virtual void pwrkeyPulse() = 0;
};

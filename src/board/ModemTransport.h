#pragma once

// The modem driver's transport on the board: the bytes of the modem's UART and the lines around it, which the
// driver reaches only through AtTransport (src/drivers/AtTransport.h).
//
//   supply     expander leg 0 (the modem's regulator). The modem hangs on the expander, not on the processor, so
//              after a reset of the processor it still runs: switching the supply never presses the key.
//   power key  expander leg 8, a pulse of the shape the device was brought up with: high for 100 ms, low for 1200
//              ms, high again. The pulse **switches a running modem off**, so the driver presses it only when
//              the modem has stayed silent (the driver's tests pin that); the transport does nothing of its own.
//   DTR        a pin of the processor: high lets the modem sleep (after AT+CSCLK=1), low wakes it.
//   audio      the speaker hangs on a switch between the modem and the codec, and its amplifier must be off
//              while no call runs. On: the switch to the modem first, then the amplifier. Off: the amplifier
//              first, then the switch, so that nothing pops.
//
// An expander that refuses a switch is said in the log, once per refusal, because the driver cannot be told.

#include "../drivers/AtTransport.h"
#include "../hal/EventLog.h"
#include "Delay.h"
#include "Gpio.h"
#include "MillisSource.h"
#include "UartPort.h"
#include "Xl9555.h"

namespace board {

constexpr uint32_t kPowerKeyHighMs = 100;
constexpr uint32_t kPowerKeyLowMs = 1200;

class ModemTransport : public AtTransport {
public:
  ModemTransport(UartPort& uart, Xl9555& expander, Gpio& gpio, Delay& delay, MillisSource& millis, EventLog& log)
      : uart_(uart), expander_(expander), gpio_(gpio), delay_(delay), millis_(millis), log_(log) {}

  // Makes the DTR pin an output, low: the modem is awake.
  void begin();

  void write(const char* data, size_t len) override;
  int read() override { return uart_.read(); }
  uint32_t now() override { return millis_.millis32(); }
  void power(bool on) override;
  void pwrkeyPulse() override;
  void setAudioToModem(bool on) override;
  void setDtr(bool high) override;

private:
  void expanderLeg(uint8_t leg, bool high, const char* what);

  UartPort& uart_;
  Xl9555& expander_;
  Gpio& gpio_;
  Delay& delay_;
  MillisSource& millis_;
  EventLog& log_;
};

}  // namespace board

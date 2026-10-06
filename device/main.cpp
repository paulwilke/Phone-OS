// The firmware's entry point. It starts the board in the documented order and says what it found; the
// capabilities, the runtime and the console are wired in the later parts of the device work (docs/device.md).
//
// The USB console is the processor's own USB port: opening the serial port from a computer resets the
// ESP32, and nothing here waits for a computer to open it.

#include <Arduino.h>
#include <Wire.h>

#include <board/PinMap.h>
#include <board/PowerUp.h>

#include "Esp32Delay.h"
#include "Esp32Gpio.h"
#include "Esp32I2cBus.h"

namespace {
Esp32I2cBus i2cBus;
Esp32Gpio gpio;
Esp32Delay delays;
board::Xl9555 expander(i2cBus);
}  // namespace

void setup() {
  Serial.begin(115200);
  Wire.begin(board::pin::kI2cSda, board::pin::kI2cScl);
  Wire.setClock(100000);
  const bool up = board::powerUp(expander, gpio, delays);
  Serial.printf("[boot] expander %s, board %s\n", expander.ready() ? "answers" : "silent: modem, radio, receiver and motor stay off",
                up ? "started" : "started with errors");
}

void loop() { delay(1000); }

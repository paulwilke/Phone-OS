// The firmware's entry point. It starts the board in the documented order and says what it found; the
// capabilities, the runtime and the console are wired in the later parts of the device work (docs/device.md).
//
// The USB console is the processor's own USB port: opening the serial port from a computer resets the
// ESP32, and nothing here waits for a computer to open it.

#include <Arduino.h>
#include <Wire.h>

#include <board/BatteryDiagnostic.h>
#include <board/Charger.h>
#include <board/FuelGauge.h>
#include <board/PinMap.h>
#include <board/PowerUp.h>

#include "Esp32Delay.h"
#include "Esp32GaugeWriter.h"
#include "Esp32Gpio.h"
#include "Esp32I2cBus.h"
#include "SerialLog.h"

namespace {
Esp32I2cBus i2cBus;
Esp32Gpio gpio;
Esp32Delay delays;
SerialLog eventLog;
board::Xl9555 expander(i2cBus);
board::Charger charger(i2cBus);
board::FuelGauge gauge(i2cBus);
Esp32GaugeWriter gaugeWriter;
}  // namespace

void setup() {
  Serial.begin(115200);
  Wire.begin(board::pin::kI2cSda, board::pin::kI2cScl);
  Wire.setClock(100000);
  const bool up = board::powerUp(expander, gpio, delays);
  Serial.printf("[boot] expander %s, board %s\n", expander.ready() ? "answers" : "silent: modem, radio, receiver and motor stay off",
                up ? "started" : "started with errors");

  // The battery size of the profile's default until the configuration is read from the card.
  const int batteryMah = board::kDefaultBatteryMah;
  charger.detect();
  eventLog.log(board::describeCapacityCheck(board::ensureCapacity(gauge, gaugeWriter, (uint16_t)batteryMah)));
  for (const std::string& line : board::formatBatteryDiagnostic(board::readBatteryDiagnostic(gauge, charger), batteryMah))
    eventLog.log(line);
}

void loop() { delay(1000); }

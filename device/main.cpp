// The firmware's entry point. It starts the board in the documented order and says what it found; the
// capabilities, the runtime and the console are wired in the later parts of the device work (docs/device.md).
// Until then the input the keyboard and the touch chip produce is written to the console, so the first
// start on the device shows at once whether the keys, the touch and the three keys under the screen work.
//
// The USB console is the processor's own USB port: opening the serial port from a computer resets the
// ESP32, and nothing here waits for a computer to open it.

#include <Arduino.h>
#include <Wire.h>

#include <board/BatteryDiagnostic.h>
#include <board/BoardMotion.h>
#include <board/Charger.h>
#include <board/Cst66xx.h>
#include <board/FuelGauge.h>
#include <board/KeyboardDecoder.h>
#include <board/PinMap.h>
#include <board/PowerUp.h>
#include <board/Tca8418.h>

#include "Esp32Bhi260Hub.h"
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
board::Tca8418 keyboard(i2cBus, delays);
board::KeyboardDecoder keyboardDecoder;
board::Cst66xxTouch touch(i2cBus, delays);
board::TouchDecoder touchDecoder;
Esp32Bhi260Hub motionHub;
board::BoardMotion motion(expander, motionHub, delays);

void printInput(const char* source, const ui::Input& in) {
  switch (in.kind) {
    case ui::Input::Kind::Key: Serial.printf("[input] %s key %d\n", source, (int)in.key); break;
    case ui::Input::Kind::Text: Serial.printf("[input] %s text U+%04X\n", source, (unsigned)in.character); break;
    case ui::Input::Kind::Touch: Serial.printf("[input] %s touch %d,%d\n", source, in.x, in.y); break;
  }
}
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

  Serial.printf("[boot] keyboard %s\n", keyboard.begin() ? "answers" : "does not answer");
  Serial.printf("[boot] touch %s\n", touch.begin() ? "CST66xx" : "not found or not a CST66xx");
  motion.begin();
  Serial.printf("[boot] motion sensor %s\n", motion.available() ? "on the board, idle" : "unavailable");
}

void loop() {
  board::KeyEvent key;
  ui::Input in;
  while (keyboard.next(key))
    if (keyboardDecoder.decode(key, in)) printInput("keyboard", in);
  board::cst66xx::Report report;
  if (touch.poll(report) && touchDecoder.decode(report, in)) printInput("touch", in);
  delay(10);
}

// The firmware's entry point. It starts the board in the documented order and says what it found; the
// capabilities, the runtime and the console are wired in the later parts of the device work (docs/device.md).
// Until then the input the keyboard and the touch chip produce is written to the console, so the first
// start on the device shows at once whether the keys, the touch and the three keys under the screen work,
// and the modem and the receiver run for the bring-up: the modem is started, the receiver is on, and what they
// report is written to the console.
//
// The USB console is the processor's own USB port: opening the serial port from a computer resets the
// ESP32, and nothing here waits for a computer to open it.

#include <Arduino.h>
#include <Wire.h>

#include <drivers/A7682EDriver.h>

#include <board/BatteryDiagnostic.h>
#include <board/BoardGnss.h>
#include <board/BoardMotion.h>
#include <board/BoardPower.h>
#include <board/Charger.h>
#include <board/Cst66xx.h>
#include <board/DeviceClock.h>
#include <board/FuelGauge.h>
#include <board/BoardTcpServer.h>
#include <board/BoardWifi.h>
#include <board/KeyboardDecoder.h>
#include <board/ModemTransport.h>
#include <board/SdCard.h>
#include <board/SdConfigStore.h>
#include <board/SdEventLog.h>
#include <board/SdMessageLog.h>
#include <board/PinMap.h>
#include <board/PowerUp.h>
#include <board/Tca8418.h>
#include <board/TimeSync.h>

#include "Esp32Bhi260Hub.h"
#include "Esp32Delay.h"
#include "Esp32GaugeWriter.h"
#include "Esp32Gpio.h"
#include "Esp32I2cBus.h"
#include "Esp32Millis.h"
#include "Esp32SdVolume.h"
#include "Esp32TcpListener.h"
#include "Esp32UartPort.h"
#include "Esp32WifiRadio.h"
#include "FlashAssets.h"
#include "SerialLog.h"

namespace {
Esp32I2cBus i2cBus;
Esp32Gpio gpio;
Esp32Delay delays;
SerialLog serialLog;
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

// The pins are given from the processor's side: it receives on the line the modem transmits on.
Esp32UartPort modemUart(Serial1, 115200, board::pin::kModemUartRx, board::pin::kModemUartTx);
Esp32UartPort gnssUart(Serial2, 38400, board::pin::kGnssUartRx, board::pin::kGnssUartTx);
Esp32Millis millisSource;
board::DeviceClock deviceClock(millisSource);

// The card and what lives on it. Without a card the device runs on its defaults and says so; the log goes to
// the console in every case and waits in memory for the card.
Esp32SdVolume sdVolume;
board::SdCard sdCard(sdVolume, gpio, delays);
board::SdEventLog eventLog(sdCard, deviceClock, &serialLog);
board::SdConfigStore configStore(sdCard);
board::SdMessageLog messageLog(sdCard);

Esp32WifiRadio wifiRadio;
board::BoardWifi wifi(wifiRadio);
Esp32TcpListener tcpListener;
board::BoardTcpServer tcpServer(tcpListener);
FlashAssets flashAssets;

board::TimeSync timeSync(deviceClock, eventLog);
board::ModemTransport modemTransport(modemUart, expander, gpio, delays, millisSource, eventLog);
A7682EDriver::Config modemConfig;  // the APN and the PIN come from the configuration once it is read
A7682EDriver modemDriver(modemTransport, modemConfig);
board::BoardPower power(expander, gpio, charger, gauge, modemDriver);
board::BoardGnss gnss(power, gnssUart, deviceClock);
ModemState lastModemState = ModemState::Off;
uint32_t lastReportMs = 0;

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
  power.begin((uint16_t)batteryMah, gaugeWriter, eventLog);
  for (const std::string& line : board::formatBatteryDiagnostic(board::readBatteryDiagnostic(gauge, charger), batteryMah))
    eventLog.log(line);

  // The card first: what the other parts say about themselves is then written to it.
  boardSpi();  // starts the one SPI bus before anything on it is asked
  sdCard.begin(eventLog);
  const int recovered = configStore.recover();
  if (recovered) Serial.printf("[boot] %d scratch files of an interrupted write dealt with\n", recovered);
  std::vector<FileEntry> config;
  if (configStore.list("/config", config)) Serial.printf("[boot] /config holds %u files\n", (unsigned)config.size());
  Serial.printf("[boot] web page in the firmware: %u bytes, Wi-Fi %s\n", (unsigned)[] { size_t n = 0; flashAssets.indexHtml(n); return n; }(),
                wifi.state() == WifiState::Off ? "off" : "on");

  Serial.printf("[boot] keyboard %s\n", keyboard.begin() ? "answers" : "does not answer");
  Serial.printf("[boot] touch %s\n", touch.begin() ? "CST66xx" : "not found or not a CST66xx");
  motion.begin();
  Serial.printf("[boot] motion sensor %s\n", motion.available() ? "on the board, idle" : "unavailable");

  modemUart.begin();
  gnssUart.begin();
  modemTransport.begin();
  power.setEnabled(PowerDomain::Cellular, true);
  power.setEnabled(PowerDomain::Positioning, true);
}

void loop() {
  board::KeyEvent key;
  ui::Input in;
  while (keyboard.next(key))
    if (keyboardDecoder.decode(key, in)) printInput("keyboard", in);
  board::cst66xx::Report report;
  if (touch.poll(report) && touchDecoder.decode(report, in)) printInput("touch", in);

  sdCard.poll(millis(), eventLog);
  eventLog.flush();
  modemDriver.poll();
  std::string line;
  while (modemDriver.pollLog(line)) eventLog.log(line);
  if (modemDriver.state() != lastModemState) {
    lastModemState = modemDriver.state();
    Serial.printf("[modem] state %d\n", (int)lastModemState);
  }
  at::NetworkTime networkTime;
  if (modemDriver.networkTime(networkTime)) timeSync.offerNetwork(networkTime.epochUtc, networkTime.tzOffsetMinutes);

  GnssFix fix;
  if (gnss.readFix(fix) && fix.valid && fix.utc) timeSync.offerSatellite(fix.utc);
  if (millis() - lastReportMs >= 5000) {
    lastReportMs = millis();
    if (fix.valid) Serial.printf("[gnss] fix, %u satellites, hdop %.1f\n", (unsigned)fix.sats, (double)fix.hdop);
    else Serial.println("[gnss] no fix");
    Serial.printf("[clock] epoch %u\n", (unsigned)deviceClock.epochUtc());
  }
  delay(10);
}

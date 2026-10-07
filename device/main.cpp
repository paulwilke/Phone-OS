// The firmware: the capabilities of the board wired into boot::Runtime exactly as sim/main.cpp wires the
// fakes into it for the simulator (docs/device.md). The loop is the host's duties that only hardware can do:
// read the keys and the touch, run the modem and the receiver, feed the clock, carry out the power logic's
// decision on sleeping, restart and switch off when asked, and answer a request to start the radio with
// "not available": this firmware has no radio.
//
// The USB console is the processor's own USB port. Opening the serial port from a computer resets the ESP32,
// and nothing here waits for a computer to open it: output that nobody takes is dropped.

#include <Arduino.h>
#include <Wire.h>
#include <esp_system.h>

#include <memory>

#include <boot/Runtime.h>
#include <board/BatteryDiagnostic.h>
#include <board/BoardGnss.h>
#include <board/BoardHaptic.h>
#include <board/BoardMotion.h>
#include <board/BoardPower.h>
#include <board/BoardTcpServer.h>
#include <board/BoardWifi.h>
#include <board/Console.h>
#include <board/Cst66xx.h>
#include <board/DeviceClock.h>
#include <board/EinkDisplay.h>
#include <board/EpdWire.h>
#include <board/KeyboardDecoder.h>
#include <board/ModemTransport.h>
#include <board/PinMap.h>
#include <board/PowerUp.h>
#include <board/SdCard.h>
#include <board/SdConfigStore.h>
#include <board/SdEventLog.h>
#include <board/SdMessageLog.h>
#include <board/Tca8418.h>
#include <board/TimeSync.h>
#include <config/ConfigLoader.h>
#include <drivers/A7682EDriver.h>

#include "Esp32Bhi260Hub.h"
#include "Esp32Delay.h"
#include "Esp32GaugeWriter.h"
#include "Esp32Gpio.h"
#include "Esp32I2cBus.h"
#include "Esp32Millis.h"
#include "Esp32SdVolume.h"
#include "Esp32SpiPort.h"
#include "Esp32TcpListener.h"
#include "Esp32UartPort.h"
#include "Esp32UsbPort.h"
#include "Esp32WifiRadio.h"
#include "FlashAssets.h"
#include "SerialLog.h"
#include "ShippedCatalogs.h"
#include "Sleep.h"

namespace {

// ---- what needs no configuration ----------------------------------------------------------------------------
Esp32I2cBus i2cBus;
Esp32Gpio gpio;
Esp32Delay delays;
Esp32Millis millisSource;
SerialLog serialLog;
board::DeviceClock deviceClock(millisSource);

board::Xl9555 expander(i2cBus);
board::Charger charger(i2cBus);
board::FuelGauge gauge(i2cBus);
Esp32GaugeWriter gaugeWriter;

// The card and what lives on it. Without a card the device runs on its defaults and says so; the log goes to
// the console in every case and waits in memory for the card.
Esp32SdVolume sdVolume;
board::SdCard sdCard(sdVolume, gpio, delays);
board::SdEventLog eventLog(sdCard, deviceClock, &serialLog);
board::SdConfigStore configStore(sdCard);
board::SdMessageLog messageLog(sdCard);

board::Tca8418 keyboard(i2cBus, delays);
board::KeyboardDecoder keyboardDecoder;
board::Cst66xxTouch touch(i2cBus, delays);
board::TouchDecoder touchDecoder;
board::Drv2605 hapticDriver(i2cBus);
board::BoardHaptic haptic(hapticDriver, expander, deviceClock, delays);
Esp32Bhi260Hub motionHub;
board::BoardMotion motion(expander, motionHub, delays);

// The pins are given from the processor's side: it receives on the line the modem transmits on.
Esp32UartPort modemUart(Serial1, 115200, board::pin::kModemUartRx, board::pin::kModemUartTx);
Esp32UartPort gnssUart(Serial2, 38400, board::pin::kGnssUartRx, board::pin::kGnssUartTx);
board::ModemTransport modemTransport(modemUart, expander, gpio, delays, millisSource, eventLog);
board::TimeSync timeSync(deviceClock, eventLog);

// The panel. The values of its datasheet are not in this build (docs/device-display.md): without a profile
// the display says so at start and is not driven, and everything else runs.
Esp32SpiPort displaySpi;
board::EpdWire displayWire(displaySpi, gpio, delays);
board::EinkDisplay display(displayWire, nullptr, millisSource, eventLog);

Esp32WifiRadio wifiRadio;
board::BoardWifi wifi(wifiRadio);
Esp32TcpListener tcpListener;
board::BoardTcpServer tcpServer(tcpListener);
FlashAssets flashAssets;

i18n::Translator translator;
std::string firmwareVersion;

// ---- what needs the configuration: made in setup() once the card has been read ---------------------------------
std::unique_ptr<A7682EDriver> modem;
std::unique_ptr<board::BoardPower> power;
std::unique_ptr<board::BoardGnss> gnss;
std::unique_ptr<boot::Runtime> runtime;
int batteryMah = board::kDefaultBatteryMah;
bool powerOffLogged = false;

// What the console asks of the device.
class Host : public board::ConsoleHost {
public:
  void restart() override {
    Serial.flush();
    delay(200);
    ESP.restart();
  }
  bool sendText(const std::string& phone, const std::string& text) override { return runtime && runtime->core().send(phone, text); }
  bool handleWeb(const std::vector<uint8_t>& request, std::string& response) override {
    if (!runtime) return false;
    char head[512];
    const char* body = nullptr;
    size_t bodyLength = 0;
    const size_t headLength = runtime->web().handleRaw(request.data(), request.size(), millis() / 1000, head, sizeof(head), &body, &bodyLength);
    if (headLength == 0) return false;
    response.assign(head, headLength);
    if (bodyLength) response.append(body, bodyLength);
    return true;
  }
  std::vector<std::string> batteryReport() override {
    return board::formatBatteryDiagnostic(board::readBatteryDiagnostic(gauge, charger), batteryMah);
  }
};
Host consoleHost;
Esp32UsbPort usbPort;
board::Console console(usbPort, configStore, consoleHost, millisSource);

void drainModemLog() {
  std::string line;
  while (modem && modem->pollLog(line)) eventLog.log(line);
}

void offerTimes() {
  at::NetworkTime networkTime;
  if (modem && modem->networkTime(networkTime)) timeSync.offerNetwork(networkTime.epochUtc, networkTime.tzOffsetMinutes);
  GnssFix fix;
  if (gnss && gnss->readFix(fix) && fix.valid && fix.utc) timeSync.offerSatellite(fix.utc);
}

void readKeys() {
  board::KeyEvent key;
  ui::Input in;
  while (keyboard.next(key))
    if (keyboardDecoder.decode(key, in)) runtime->input(in, millis());
  board::cst66xx::Report report;
  if (touch.poll(report) && touchDecoder.decode(report, in)) runtime->input(in, millis());
}

// The power logic asked to switch off: the modem says goodbye to the network first, then the rails and the battery.
void powerOff() {
  if (!powerOffLogged) {
    powerOffLogged = true;
    eventLog.log("Power: switching off");
  }
  modem->shutdown();
  for (int i = 0; i < 200 && modem->state() != ModemState::Off; ++i) {
    modem->poll();
    drainModemLog();
    delay(50);
  }
  power->powerOff();  // false while a cable feeds the board: the device stays on, and the log says so
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);  // never wait for a computer to read the console
  Wire.begin(board::pin::kI2cSda, board::pin::kI2cScl);
  Wire.setClock(100000);
  const bool up = board::powerUp(expander, gpio, delays);
  serialLog.log(std::string("Board: expander ") + (expander.ready() ? "answers" : "silent: modem, radio, receiver and motor stay off") +
                (up ? ", started" : ", started with errors"));

  // The card first: what the other parts say about themselves is then written to it.
  boardSpi();  // starts the one SPI bus before anything on it is asked
  sdCard.begin(eventLog);
  configStore.recover();
  const config::ConfigLoader::Result loaded = config::ConfigLoader(configStore).load();
  batteryMah = loaded.profile.batteryCapacityMah;

  A7682EDriver::Config modemConfig;
  modemConfig.apn = loaded.profile.modemApn;
  modemConfig.simPin = loaded.simPin;
  modem.reset(new A7682EDriver(modemTransport, modemConfig));
  power.reset(new board::BoardPower(expander, gpio, charger, gauge, *modem));
  gnss.reset(new board::BoardGnss(*power, gnssUart, deviceClock));

  power->begin((uint16_t)batteryMah, gaugeWriter, eventLog);
  eventLog.log(keyboard.begin() ? "Keyboard: answers" : "Keyboard: does not answer");
  eventLog.log(touch.begin() ? "Touch: CST66xx found" : "Touch: not found or not a CST66xx");
  motion.begin();
  modemUart.begin();
  gnssUart.begin();
  modemTransport.begin();
  display.begin();

  shipped::load(translator);
  firmwareVersion = shipped::version();
  runtime.reset(new boot::Runtime(
      boot::Capabilities{deviceClock, *power, *modem, *gnss, haptic, eventLog, configStore, wifi, tcpServer, flashAssets, display,
                         [] { return (uint32_t)esp_random(); }, firmwareVersion.c_str(), nullptr, &motion, "T-Deck Pro MAX", &messageLog},
      translator, millis()));
  power->setEnabled(PowerDomain::Cellular, true);
  eventLog.log("Boot: the runtime runs, firmware " + firmwareVersion);
}

void loop() {
  const uint32_t now = millis();
  sdCard.poll(now, eventLog);
  eventLog.flush();
  console.poll();
  readKeys();
  modem->poll();
  drainModemLog();
  offerTimes();
  haptic.service();

  const PowerDecision decision = runtime->step(now);

  // The radio is not part of this firmware: a request to start it is answered with "not available".
  if (runtime->core().takeRadioStartRequest()) eventLog.log("Radio: not available in this firmware");
  if (runtime->core().powerOffRequested()) powerOff();
  if (runtime->restartRequested()) consoleHost.restart();

  if (decision.cpuMaySleep && !haptic.busy() && decision.cpuSleepMs > 0) lightSleep(decision.cpuSleepMs);
  else delay(5);
}

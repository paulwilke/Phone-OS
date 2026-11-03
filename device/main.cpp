// The firmware: the capabilities of the board wired into boot::Runtime exactly as sim/main.cpp wires the
// fakes into it for the simulator (docs/hardware/device.md). The loop is the host's duties that only hardware can do:
// read the keys and the touch, run the modem and the receiver, feed the clock, carry out the power logic's
// decision on sleeping, restart and switch off when asked, and start and stop the radio when the application asks.
// The radio (MeshCore, device/MeshCoreRadio) is created only on the application's request: the SOS without a
// mobile network (D-62), or the radio page where the profile allows it. It is destroyed again when the emergency
// ends, and its chip loses power with it (the power coordination cuts the rail once nothing is attached).
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
#include <board/GDEQ031T10Profile.h>
#include <board/EpdWire.h>
#include <board/KeyboardDecoder.h>
#include <board/LoopStats.h>
#include <board/LowVoltageGuard.h>
#include <board/ModemTransport.h>
#include <board/PinMap.h>
#include <board/PowerUp.h>
#include <board/SdCard.h>
#include <board/SdConfigStore.h>
#include <board/EnergyLog.h>
#include <board/SdEventLog.h>
#include <board/SdMessageLog.h>
#include <board/Tca8418.h>
#include <board/TimeSync.h>
#include <config/ConfigLoader.h>
#include <drivers/A7682EDriver.h>
#include <ui/Splash.h>

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
#include "MeshCoreRadio.h"
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
board::EnergyLog energyLog(sdCard, gauge, deviceClock);
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
Esp32UartPort modemUart(Serial1, 115200, board::pin::kModemUartRx, board::pin::kModemUartTx, 4096);  // D-41
Esp32UartPort gnssUart(Serial2, 38400, board::pin::kGnssUartRx, board::pin::kGnssUartTx);
board::ModemTransport modemTransport(modemUart, expander, gpio, delays, millisSource, eventLog);
board::TimeSync timeSync(deviceClock, eventLog);

// The panel, driven with the GDEQ031T10 profile (docs/hardware/device-display.md). A build without a profile says so at
// start and does not drive the panel.
Esp32SpiPort displaySpi;
board::EpdWire displayWire(displaySpi, gpio, delays);
board::EinkDisplay display(displayWire, &board::gdeq031t10Profile(), millisSource, eventLog);

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
bool lowVoltageOff = false;           // the switch-off was asked by the guard, not by a person
board::LowVoltageGuard lowVoltageGuard;
board::LoopStats loopStats;
uint32_t lastVoltageCheckMs = 0;
const char* const kBoardName = "T-Deck Pro MAX";   // on the status page and the splash picture
const char* const kProjectName = "Phone OS";   // on the splash picture; never shortened, it wraps

// ---- the radio ---------------------------------------------------------------------------------------------
std::unique_ptr<MeshCoreRadio> meshRadio;
uint32_t lastAdvertiseMs = 0;
// The key pair of this node, in plain text as everything on the card is (D-55): without it the node would be a
// stranger to everybody again at every start. In the root, where the parents' page does not reach.
const char* const kRadioIdentityPath = "/radio-identity.txt";
// A node is heard as a contact only after its advertisement, and strangers write to it only then. Once at the
// start and then every ten minutes while the emergency lasts: a few bytes of the name, within the airtime limit.
constexpr uint32_t kAdvertiseEveryMs = 10UL * 60UL * 1000UL;

std::string toHex(const std::string& bytes) {
  static const char digits[] = "0123456789abcdef";
  std::string out;
  for (unsigned char c : bytes) {
    out += digits[c >> 4];
    out += digits[c & 15];
  }
  return out;
}

bool fromHex(const std::string& text, std::string& bytes) {
  bytes.clear();
  std::string clean;
  for (char c : text)
    if (c != '\n' && c != '\r' && c != ' ') clean += c;
  if (clean.size() % 2 != 0) return false;
  auto nibble = [](char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; };
  for (size_t i = 0; i < clean.size(); i += 2) {
    const int hi = nibble(clean[i]);
    const int lo = nibble(clean[i + 1]);
    if (hi < 0 || lo < 0) return false;
    bytes += static_cast<char>(hi * 16 + lo);
  }
  return true;
}

// The application asked for the radio. The rail (leg kRadioPower) and the antenna switch (leg kRadioAntenna: high
// is the internal antenna, which is the one that comes with the device, D-61) are the expander's legs of
// src/board/PinMap.h; the boot state of board::powerUp already has both, and the rail is cut again at the end of
// board power's begin(), so both are set here explicitly. The chip needs a moment after its supply before its reset
// is released (done by RadioLib in begin()).
void startRadio() {
  if (meshRadio) return;
  if (!power->setEnabled(PowerDomain::Radio, true) || !expander.set(board::leg::kRadioAntenna, true)) {
    eventLog.log("Radio: the rail or the antenna switch did not answer");
    runtime->core().radioStartFailed();
    return;
  }
  delay(20);
  meshRadio.reset(new MeshCoreRadio(deviceClock));
  std::string hex, blob;
  if (configStore.read(kRadioIdentityPath, hex, 512) && fromHex(hex, blob) && meshRadio->setIdentity(blob))
    eventLog.log("Radio: the stored identity is used");
  if (!meshRadio->begin(runtime->core().profile().radioName)) {
    eventLog.log("Radio: the chip does not answer");
    meshRadio.reset();
    power->setEnabled(PowerDomain::Radio, false);
    runtime->core().radioStartFailed();
    return;
  }
  const std::string identity = meshRadio->identity();
  if (!identity.empty() && identity != blob) {
    if (configStore.write(kRadioIdentityPath, toHex(identity) + "\n")) eventLog.log("Radio: a new identity was made and stored");
    else eventLog.log("Radio: a new identity was made and could not be stored");
  }
  runtime->core().setRadio(meshRadio.get());
  meshRadio->advertise();
  lastAdvertiseMs = millis();
  eventLog.log("Radio: started");
}

// The emergency is over: nothing may hold the object when it goes, so the application lets go first.
void stopRadio() {
  runtime->core().setRadio(nullptr);
  meshRadio.reset();
  eventLog.log("Radio: stopped");
}

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
board::Console console(usbPort, configStore, consoleHost, millisSource, delays);

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
// Seen on the device 2026-10-08 (4.1.0): the picture of the page stayed after switching off (e-ink keeps it), so a
// device that was off looked on; and a switch-off the cable refuses was asked again at every pass of the loop, the
// modem shut down each time, and nobody was told. Fix host-tested where it can be (ui::drawSplash, the core's
// refusal); the order below is to be verified on the device.
void powerOff() {
  const char* const reason = board::switchOffWordKey(lowVoltageOff);
  // A cable keeps the board alive whatever the battery switch does: refuse before anything is shut down.
  if (power->externalPower()) {
    runtime->core().notePowerOffFailed();   // the notice says "only without cable", the log says why
    return;
  }
  if (!powerOffLogged) {
    powerOffLogged = true;
    eventLog.log("Power: switching off");
  }
  display.beginFrame();
  ui::drawSplash(display, kProjectName, kBoardName, "Version " + firmwareVersion,
                 runtime->translator().get(reason));
  display.endFrame();
  modem->shutdown();
  for (int i = 0; i < 200 && modem->state() != ModemState::Off; ++i) {
    modem->poll();
    drainModemLog();
    delay(50);
  }
  if (!power->powerOff()) {
    // The charger did not take the command: the modem is down, so the device starts again rather than stay on
    // without a network behind a picture that says it is off.
    eventLog.log("Power: the charger did not switch off; restarting");
    consoleHost.restart();
  }
}

}  // namespace

void setup() {
  // The default transmit buffer (256 bytes) is what a long console answer runs into; a larger one lets most
  // answers go out in one piece. Must come before begin(), which only creates the default one when none exists.
  Serial.setTxBufferSize(4096);
  Serial.setRxBufferSize(4096);   // the default of 256 bytes overflowed when a computer wrote a file: the loop reads the port once in about 56 ms
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
  // The idle level of the three wake lines, with the pull-ups of board::powerUp in place: a line that idles low
  // is armed to wake on high (device/Sleep.h, D-34), and this line says which it is, on the cable too.
  eventLog.log(std::string("Wake lines: keyboard ") + (gpio.read(board::pin::kKeyboardInterrupt) ? "high" : "low") +
               ", touch " + (gpio.read(board::pin::kTouchInterrupt) ? "high" : "low") +
               ", ring " + (gpio.read(board::pin::kModemRing) ? "high" : "low"));
  configStore.recover();
  const config::ConfigLoader::Result loaded = config::ConfigLoader(configStore).load();
  batteryMah = loaded.profile.batteryCapacityMah;

  A7682EDriver::Config modemConfig;
  modemConfig.apn = loaded.profile.modemApn;
  modemConfig.simPin = loaded.simPin;
  modem.reset(new A7682EDriver(modemTransport, modemConfig));
  power.reset(new board::BoardPower(expander, gpio, charger, gauge, *modem));
  power->setClock(millisSource);   // one reading of the battery chips a second serves every question (D-31)
  gnss.reset(new board::BoardGnss(*power, gnssUart, deviceClock));

  power->begin((uint16_t)batteryMah, gaugeWriter, eventLog);
  eventLog.log(keyboard.begin() ? "Keyboard: answers" : "Keyboard: does not answer");
  eventLog.log(touch.begin() ? "Touch: CST66xx found" : "Touch: not found or not a CST66xx");
  motion.begin();
  modemUart.begin();
  gnssUart.begin();
  modemTransport.begin();
  display.begin();
  // The device's own picture while the rest starts, as 3.x showed it; no words, the catalogs come next.
  display.beginFrame();
  ui::drawSplash(display, kProjectName, kBoardName, "Version " + shipped::version());
  display.endFrame();

  shipped::load(translator, [](const std::string& line) { eventLog.log(line); });
  firmwareVersion = shipped::version();
  runtime.reset(new boot::Runtime(
      boot::Capabilities{deviceClock, *power, *modem, *gnss, haptic, eventLog, configStore, wifi, tcpServer, flashAssets, display,
                         [] { return (uint32_t)esp_random(); }, firmwareVersion.c_str(), nullptr, &motion, kBoardName, &messageLog},
      translator, millis()));
  // The named zone of settings.toml is in force when the time sync writes its line about a new time.
  timeSync.setZoneRule([](uint32_t epoch, int fallback) { return runtime ? runtime->core().zoneOffsetMinutes(epoch, fallback) : fallback; });
  power->setEnabled(PowerDomain::Cellular, true);
  eventLog.log("Boot: the runtime runs, firmware " + firmwareVersion);
}

// Times a part of the loop pass for the statistics line (decisions D-20, D-25).
struct SectionTimer {
  board::LoopStats::Section id;
  uint32_t start;
  explicit SectionTimer(board::LoopStats::Section section) : id(section), start(micros()) {}
  ~SectionTimer() { loopStats.section(id, micros() - start); }
};

void loop() {
  using Section = board::LoopStats::Section;
  const uint32_t now = millis();
  const uint32_t passStartUs = micros();
  {
    SectionTimer timer(Section::Console);
    sdCard.poll(now, eventLog);
    eventLog.flush();
    console.poll();
  }
  {
    SectionTimer timer(Section::Keys);
    readKeys();
  }
  {
    SectionTimer timer(Section::Modem);
    modem->poll();
    drainModemLog();
    offerTimes();
    haptic.service();
  }

  {
    SectionTimer timer(Section::Housekeeping);
    // The cell is protected by the firmware (D-5): a minute below the limit on the battery switches off.
    if (now - lastVoltageCheckMs >= 5000) {
      lastVoltageCheckMs = now;
      const int mv = power->batteryMillivolts();
      const bool onBattery = !power->externalPower();
      if (!onBattery) lowVoltageOff = false;   // a switch-off the cable refused is asked again after the next discharge
      if (lowVoltageGuard.update(now, mv, onBattery) && !lowVoltageOff) {
        lowVoltageOff = true;
        eventLog.log("Battery: " + std::to_string(mv) + " mV for a minute, switching off to protect the cell");
        runtime->core().requestPowerOff();
      }
    }
  }

  PowerDecision decision;
  {
    SectionTimer timer(Section::Step);
    decision = runtime->step(now);
  }

  {
    SectionTimer timer(Section::Housekeeping);
    // One reading of the battery every ten minutes (docs/hardware/energy-log.md). The cable is asked only when a line is due.
    if (energyLog.due(now))
      energyLog.record(now, power->externalPower() ? board::EnergyState::Cable
                            : decision.modemShouldSleep ? board::EnergyState::ModemSleep
                                                        : board::EnergyState::Awake);

    if (runtime->core().takeRadioStartRequest()) startRadio();
    if (runtime->core().takeRadioStopRequest()) stopRadio();
    if (meshRadio) {
      meshRadio->loop();
      if (now - lastAdvertiseMs >= kAdvertiseEveryMs) {
        meshRadio->advertise();
        lastAdvertiseMs = now;
      }
    }
    if (runtime->core().powerOffRequested()) powerOff();
    if (runtime->restartRequested()) consoleHost.restart();
  }

  // The pass is timed without the sleep or the pause that ends it (decisions D-20, D-25; docs/hardware/device-console.md).
  const uint32_t workUs = micros() - passStartUs;
  uint32_t sleptMs = 0;
  if (decision.cpuMaySleep && !haptic.busy() && decision.cpuSleepMs > 0) {
    const uint32_t before = millis();
    const SleepReport report = lightSleep(decision.cpuSleepMs);
    // A sleep that nothing ended did not happen: the processor went down and came up at once (a wake line already
    // at its level), which the night of 2026-10-14 counted as 23 % asleep at 105 mA. Only the timer or a line
    // ending it is sleep.
    sleptMs = report.cause == board::LoopStats::WakeCause::Other ? 0 : millis() - before;
    loopStats.sleep(report.lowBefore, report.cause);
  } else {
    delay(5);
  }
  loopStats.pass(now, workUs, sleptMs, decision.cpuMaySleep);
  if (loopStats.due(millis())) eventLog.log(loopStats.takeLine(millis()));
  // Bytes from the modem lost in the receive queue (D-41): at most one line a minute, with the count. None is
  // expected with the larger queue; the line is what tells whether it is large enough.
  static uint32_t uartOverflows = 0;
  static uint32_t uartReportedMs = 0;
  uartOverflows += modemUart.takeOverflows();
  if (uartOverflows > 0 && millis() - uartReportedMs >= 60000) {
    eventLog.log("Modem UART: receiving failed " + std::to_string(uartOverflows) + " times, the buffer overflowed and bytes were lost");
    uartOverflows = 0;
    uartReportedMs = millis();
  }
}

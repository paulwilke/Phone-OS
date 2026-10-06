#pragma once

// The application core: what the device does, without a screen. A thin facade that owns the
// components, runs them in one fixed order per `poll()`, and offers the interface two things: the
// **entry points** it calls (dial, send, begin the SOS, switch cellular, ...) and a **read-only
// state** of plain data it draws from. The facade decides nothing itself; every rule lives in
// the component that owns it (see docs/app-behaviour.md).
//
// The core contains no sentence. Notices and system entries are codes with arguments, the network
// label is data, and the only text it produces is the body of an SMS, worded from the catalogs in
// the profile's SMS language. The interface words everything else when it draws.
//
// **The profile** is the core's own copy. Two settings may change while the device runs, because
// the child may change them in the options (message vibration, how visible a remote query is, the
// language of the screen);
// they last until the next start, since writing the configuration back is the configuration's job.
//
// **The time zone** of the profile sets the clock's offset from the epoch on every pass, so school
// time stays on the wall clock across the change of the clocks.
//
// The host calls `start()` once, `poll()` in the main loop, and `leaveSos()` when the SOS screen
// closes. It does the things only hardware can do: it sleeps the processor as the returned
// decision says, starts the radio when `takeRadioStartRequest()` says so, restarts the device when
// `restartRequested()` says so, and switches the device off when `powerOffRequested()` says so.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../hal/Clock.h"
#include "../hal/EventLog.h"
#include "../hal/GnssReceiver.h"
#include "../hal/Haptic.h"
#include "../hal/Modem.h"
#include "../hal/Power.h"
#include "../hal/RadioLink.h"
#include "../hal/Wifi.h"
#include "../i18n/Translator.h"
#include "Calls.h"
#include "Connectivity.h"
#include "Emergency.h"
#include "Logger.h"
#include "MessageStore.h"
#include "Messaging.h"
#include "Notices.h"
#include "Positioning.h"
#include "PowerCoordination.h"
#include "Profile.h"
#include "SchoolTime.h"
#include "WifiController.h"

class AppCore : private Messaging::Hooks {
public:
  // Everything the interface draws that comes from the core.
  struct State {
    // The wall clock as plain numbers: the interface draws a date and a time of day and has no
    // calendar of its own. Unknown until the network or the receiver has told the time.
    struct WallClock {
      bool known = false;
      int year = 0;
      int month = 0, day = 0;  // 1..12, 1..31
      int weekday = 0;         // 0 = Sunday .. 6 = Saturday
      int minuteOfDay = 0;     // 0 .. 1439
    };
    WallClock wall;
    // network
    Connectivity::NetworkLabel network;
    bool registered = false;
    int signalBars = 0;
    SimStatus sim = SimStatus::Unknown;
    bool voiceAvailable = false;
    bool cellularEnabled = false;
    bool saveMode = false;
    uint32_t probeSecondsLeft = 0;
    bool radioAttached = false;
    WifiPhase wifiPhase = WifiPhase::Off;
    WifiReason wifiReason = WifiReason::None;
    // What somebody needs to join: the network (or the device's own), the access point's password
    // (only for the device's screen), the address of the device and the minutes it stays on.
    std::string wifiSsid, wifiPassword, wifiAddress;
    uint32_t wifiSecondsLeft = 0;
    // messages and the notice line
    size_t unread = 0;
    size_t queued = 0;
    Notice notice;
    // calls
    ModemState callState = ModemState::Off;
    bool callActive = false;
    std::string callPeer;
    int volume = -1;
    bool volumeAdjustable = false;
    // emergency
    SosFlow::Phase sosPhase = SosFlow::Phase::Idle;
    uint32_t sosSecondsLeft = 0;
    int sosTapsLeft = 0;
    bool emergencyRunning = false;
    SosPing::Stage pingStage = SosPing::Stage::Idle;
    SosPing::Kind pingKind = SosPing::Kind::Emergency;
    int pingProgress = 0;
    uint32_t pingElapsedSeconds = 0;  // since the press, while the ping searches
    bool pingHasFix = false;          // the best position so far is valid
    unsigned pingAccuracyMeters = 0;  // of that position, by the one rule everywhere
    std::string pingPeer;             // the number the text goes to
    // school time and positioning
    bool schoolTime = false;
    int schoolTimeEndMinute = 0;
    bool positioningActive = false;
    // power
    int batteryPercent = -1;
    int batteryMinutes = -1;
    ChargeState charge = ChargeState::Unknown;
    bool modemAsleep = false;
    bool frontlight = false;
    bool powerOffRequested = false;
  };

  AppCore(Clock& clock, Power& power, Modem& modem, GnssReceiver& gnss, Haptic& haptic, EventLog& eventLog,
          i18n::Translator& translator, Profile profile);

  // Optional capabilities, before start(). The radio can also come later, once the host has
  // started it.
  void setRadio(RadioLink* radio);
  void setWifi(Wifi& wifi, WifiController::Random random);
  void attachMessageLog(MessageLog* log) { store_.attach(log); }

  // Restores the history, reads the trigger words, asks for the start-up fix.
  void start();
  // One pass of the main loop. Returns the decision on sleeping for the host.
  PowerDecision poll(bool displayOn, uint32_t nextRenderInMs);
  State state();
  // A time as the wall clock shows it in the clock's zone, for the time of a stored message; not
  // known for epoch 0 (a message that arrived before the time was known).
  State::WallClock wallClockOf(uint32_t epochUtc);

  // --- input ---
  void noteInput() { powerCoordination_.noteInput(); }

  // --- calls ---
  bool dial(const std::string& number) { return calls_.call(number); }
  bool answer() { return calls_.answer(); }
  bool hangup() { return calls_.hangup(); }
  bool changeVolume(int delta) { return calls_.changeVolume(delta); }
  bool setDefaultVolume(int level) { return calls_.setDefaultVolume(level); }
  // Says "school time until ..." and returns true when school time is running.
  bool refuseInSchoolTime() { return calls_.refuseInSchoolTime(); }

  // --- messages ---
  bool send(const std::string& peer, const std::string& text) { return messaging_.sendText(peer, text); }
  bool sendRadio(const std::string& peer, const std::string& text) { return messaging_.sendRadioText(peer, text); }
  bool deleteMessage(uint32_t seq) { return messaging_.remove(seq); }
  void markAllRead() { store_.markAllRead(); }
  const MessageStore& history() const { return store_; }
  size_t radioPeerCount();
  bool radioPeer(size_t index, RadioPeer& out);

  // --- SOS ---
  bool beginSos(const std::string& target = std::string()) { return emergency_.begin(target); }
  bool chooseSos(SosPing::Kind kind) { return emergency_.choose(kind); }
  bool tapSos() { return emergency_.tap(); }
  void leaveSos() { emergency_.leave(); }

  // --- positioning ---
  void setPositionPageOpen(bool open) { positioning_.want(Positioning::Reason::PositionPage, open); }
  void setTestPageOpen(bool open) { positioning_.want(Positioning::Reason::TestPage, open); }
  bool lastKnownFix(GnssFix& out, uint32_t* ageSeconds = nullptr) { return positioning_.lastKnownFix(out, ageSeconds); }

  // --- connectivity ---
  bool setCellularEnabled(bool on) { return connectivity_.setCellularEnabled(on); }
  bool setSaveMode(bool on) { return connectivity_.setSaveMode(on); }
  bool probeCellular() { return connectivity_.probeCellular(); }
  bool setWifiMode(WifiMode mode) { return connectivity_.setWifiMode(mode); }
  // The one Wi-Fi controller of the device: the screens switch it, the web service tells it about use and
  // reads its state. Null on a device without Wi-Fi.
  const WifiController* wifi() const { return wifi_.get(); }
  WifiController* wifiController() { return wifi_.get(); }
  bool requestRadioStart() { return connectivity_.requestRadioStart(); }
  bool takeRadioStartRequest() { return connectivity_.takeRadioStartRequest(); }
  void requestRestart() { connectivity_.requestRestart(); }
  bool restartRequested() const { return connectivity_.restartRequested(); }

  // --- power ---
  bool toggleFrontlight() { return powerCoordination_.toggleFrontlight(); }
  // The device locks: the light draws current and goes off.
  void setFrontlight(bool on) { powerCoordination_.setFrontlight(on); }
  void requestPowerOff() { powerCoordination_.requestPowerOff(); }
  bool powerOffRequested() const { return powerCoordination_.powerOffRequested(); }
  void notePowerOffFailed() { powerCoordination_.notePowerOffFailed(); }

  // --- settings the child may change at run time ---
  // Both methods set settingsNeedWrite() so the host can persist the change.
  void setMessageVibration(bool on);
  // Switches the language of the screen at once. False, and nothing changes, for a code that no
  // catalog has. The code is kept in the profile and applied again by start(); writing it to the
  // card is the configuration's job. The language of the SMS is not touched: it is the parents'.
  bool setInterfaceLanguage(const std::string& code);
  void setCommandNotice(CommandNotice notice);
  bool messageVibration() const { return profile_.messageVibration; }
  CommandNotice commandNotice() const { return profile_.commandNotice; }
  // True when setMessageVibration() or setCommandNotice() was called since the last
  // clearSettingsWrite(). The host calls ConfigWriter::writeSettings() then clears it.
  bool settingsNeedWrite() const { return settingsNeedWrite_; }
  void clearSettingsWrite() { settingsNeedWrite_ = false; }

  // --- self test ---
  void playTestPattern(HapticPattern pattern) { haptic_.play(pattern); }

  const Profile& profile() const { return profile_; }

  // --- web interface ---
  // Writes a line to the log (via the Logger, which applies the log level).
  void log(const char* line) { logger_.log(line); }
  // The firmware version string, set by the host at startup.
  const std::string& firmware() const { return firmware_; }
  void setFirmware(const char* v) { firmware_ = v; }
  // Clock values for the state response.
  uint32_t epochUtc() { return clock_.epochUtc(); }
  int tzOffsetMinutes() { return clock_.tzOffsetMinutes(); }

private:
  // Messaging::Hooks
  bool startLocationReply(const std::string& phone) override { return emergency_.startLocationReply(phone); }
  bool smsSendFailed(const std::string& phone) override { return emergency_.noteSendFailed(phone); }

  void applyTimeZone();
  void configureTriggers();

  Clock& clock_;
  Modem& modem_;
  Haptic& haptic_;
  i18n::Translator& translator_;
  Profile profile_;
  bool settingsNeedWrite_ = false;
  std::string firmware_;
  Logger logger_;
  MessageStore store_;
  Notices notices_;
  SchoolTime school_;
  Positioning positioning_;
  Messaging messaging_;
  Calls calls_;
  Emergency emergency_;
  PowerCoordination powerCoordination_;
  std::unique_ptr<WifiController> wifi_;
  Connectivity connectivity_;
  RadioLink* radio_ = nullptr;
};

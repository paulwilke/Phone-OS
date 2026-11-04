#pragma once

// How the device reaches the world, and the switches that decide it: the mobile network, the
// long-range radio and Wi-Fi.
//
// **Cellular** can be switched off and on. Either way the modem is no longer "asleep" (a modem
// that is off or has just started is not), which is told to PowerCoordination.
//
// **Radio.** Starting the radio software is the host's job; this component only keeps the request
// (and refuses it where the profile does not allow the radio). The host reads and clears the
// request with takeRadioStartRequest() and attaches the radio with setRadio().
//
// **Save mode** (D-1, D-62, D-67) is the emergency radio's battery saving: Wi-Fi off, and the modem off
// except for searches. It is never asked for by a person; EmergencyRadio switches it on with the
// emergency and off again at its end. Entering switches the modem off at once. A search switches it on for
// at most kSaveSearchWindowSeconds and off again if it has no network by then. A search runs on demand
// (searchNow(), the radio page's control) and every `[radio] cellular_retry` minutes (0 = never). Once
// registered the modem stays on for as long as it keeps the network (the queued SOS text leaves by SMS,
// calls and texts work); a network lost again starts a new search window, and the timer runs again after
// the modem has been switched off. While it holds, switching cellular on or off by hand and switching Wi-Fi
// on are refused (logged), and the end puts cellular back as it was before (off again if it was off) and
// leaves no timer. While the modem is off the processor may sleep (PowerPolicy).
//
// **Wi-Fi** is switched through the WifiController, with one rule in front of it: not on a nearly
// empty battery unless a cable feeds the device (the biggest consumer after the modem must not
// take the last percent). Switching it off always works.
//
// **The network label** the status line shows is data, not text: the interface words the
// case of no network.

#include <cstdint>
#include <string>

#include "../hal/Clock.h"
#include "../hal/Modem.h"
#include "../hal/Power.h"
#include "../hal/RadioLink.h"
#include "Logger.h"
#include "Notices.h"
#include "PowerCoordination.h"
#include "Profile.h"
#include "WifiController.h"

class Connectivity {
public:
  static constexpr int kMinWifiBattery = 20;
  // Save mode (D-67; the agent's proposal, open point 9): a search lasts at most this long before the modem
  // is switched off again. The interval of the timer is the setting `[radio] cellular_retry`.
  static constexpr uint32_t kSaveSearchWindowSeconds = 90;

  struct NetworkLabel {
    enum class Kind : uint8_t {
      None,        // no operator and no radio
      Operator,    // `name` is the operator, shortened for the status line
      Radio,       // no operator, the radio runs
      Connecting,  // the modem is on its way to a network: searching for the first one since it was
                   // switched on, or registered while the network's name has not come yet
    };
    Kind kind = Kind::None;
    std::string name;  // only for Operator
  };

  Connectivity(Clock& clock, Power& power, Modem& modem, const Profile& profile, Notices& notices,
               PowerCoordination& powerCoordination, Logger& log)
      : clock_(clock), power_(power), modem_(modem), profile_(profile), notices_(notices),
        powerCoordination_(powerCoordination), log_(log) {}

  // Both are optional. The radio is attached once the host has started it.
  void setRadio(RadioLink* radio) { radio_ = radio; }
  void setWifi(WifiController* wifi) { wifi_ = wifi; }

  // Keeps Wi-Fi going and, in save mode, the modem's search cycle.
  void poll();

  // --- cellular ---
  // True when it is as asked afterwards (also when it was already). A switch the hardware refuses
  // is logged and returns false.
  bool setCellularEnabled(bool on);
  bool cellularEnabled() { return power_.isEnabled(PowerDomain::Cellular); }
  bool registered();
  NetworkLabel networkLabel();

  // --- radio ---
  bool radioAttached() const { return radio_ != nullptr; }
  // Asks the host to start the radio. False when the profile does not allow it or it runs
  // already.
  bool requestRadioStart();
  // True once per request: the host starts the radio.
  bool takeRadioStartRequest();
  // Asks the host to stop the radio (the emergency is over). False when it is not attached.
  bool requestRadioStop();
  // True once per request: the host stops the radio and detaches it with setRadio(nullptr).
  bool takeRadioStopRequest();

  // --- save mode ---
  bool saveMode() const { return saveMode_; }
  // Wi-Fi off, the modem off until a search; remembers whether cellular was on.
  void enterSaveMode();
  // Cellular as it was before (off again if it was off); the search timer is gone.
  void leaveSaveMode();
  // Save mode only: the modem is switched on (searching or registered).
  bool saveSearching() const { return saveMode_ && savePhase_ != SavePhase::Off; }
  // Save mode only: a search now (the modem is switched on for at most the window). False when save mode is not
  // on or the modem is on already (searching or registered).
  bool searchNow();

  // --- restart ---
  void requestRestart() { restartRequested_ = true; }
  bool restartRequested() const { return restartRequested_; }

  // --- Wi-Fi ---
  // False when refused (see the top) or when there is no Wi-Fi.
  bool setWifiMode(WifiMode mode);
  bool hasWifi() const { return wifi_ != nullptr; }
  WifiController* wifi() { return wifi_; }
  bool wifiActive() const { return wifi_ && wifi_->active(); }

private:
  Clock& clock_;
  Power& power_;
  Modem& modem_;
  const Profile& profile_;
  Notices& notices_;
  PowerCoordination& powerCoordination_;
  Logger& log_;
  RadioLink* radio_ = nullptr;
  WifiController* wifi_ = nullptr;
  bool radioStartRequested_ = false;
  bool radioStopRequested_ = false;
  bool saveMode_ = false;
  bool cellularWasOn_ = false;
  // Save mode: Off (the modem is off, the timer runs), Searching (a window runs), Online (registered, stays on
  // while it keeps the network).
  enum class SavePhase : uint8_t { Off, Searching, Online };
  SavePhase savePhase_ = SavePhase::Off;
  uint32_t savePhaseSince_ = 0;  // uptime in seconds when the phase began
  void pollSaveMode();
  void startSearch(const char* why);
  void switchModem(bool on);  // the search's own switch: no notice, not refused by save mode
  bool restartRequested_ = false;
  // The modem has named a network since it was last switched on: a network lost after that is "no
  // network", not "connecting" (a child must see that nobody can reach the device).
  bool namedSinceOn_ = false;
  void noteModem();
};

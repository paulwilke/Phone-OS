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

  // Keeps Wi-Fi going.
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
  bool restartRequested_ = false;
  // The modem has named a network since it was last switched on: a network lost after that is "no
  // network", not "connecting" (a child must see that nobody can reach the device).
  bool namedSinceOn_ = false;
};

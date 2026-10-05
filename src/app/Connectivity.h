#pragma once

// How the device reaches the world, and the switches that decide it: the mobile network, the
// long-range radio, Wi-Fi and the save mode that ties them together.
//
// **Cellular** can be switched off and on. Either way the modem is no longer "asleep" (a modem
// that is off or has just started is not), which is told to PowerCoordination.
//
// **Save mode** is "radio only": cellular and Wi-Fi are off. It needs a running radio, because
// without one the device would have no way out at all, so it is refused with a notice then.
// Because a parent's SMS cannot arrive while cellular is off, the child (or a parent) can look
// for the mobile network for a short while, the *cell probe*: cellular goes on for
// kProbeSeconds and off again by itself, and save mode stays.
//
// **Radio.** Starting the radio software is the host's job; this component only keeps the request
// (and refuses it where the profile does not allow the radio). The host reads and clears the
// request with takeRadioStartRequest() and attaches the radio with setRadio().
//
// **Wi-Fi** is switched through the WifiController, with two rules in front of it: not in save
// mode, and not on a nearly empty battery unless a cable feeds the device (the biggest consumer
// after the modem must not take the last percent). Switching it off always works.
//
// **The network label** the status line shows is data, not text: the interface words the
// "radio only" and "none" cases.

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
  static constexpr uint32_t kProbeSeconds = 180;
  static constexpr int kMinWifiBattery = 20;

  struct NetworkLabel {
    enum class Kind : uint8_t {
      None,       // no operator and no radio
      Operator,   // `name` is the operator, shortened for the status line
      Radio,      // no operator, the radio runs
      RadioOnly,  // save mode, no operator
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

  // Ends the cell probe when its time is up, and keeps Wi-Fi going.
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

  // --- save mode ---
  bool setSaveMode(bool on);
  bool saveMode() const { return saveMode_; }
  // Looks for the mobile network for kProbeSeconds. Only in save mode.
  bool probeCellular();
  // Seconds until the probe ends; 0 when none runs.
  uint32_t probeSecondsLeft() const;

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
  bool saveMode_ = false;
  uint32_t probeUntil_ = 0;
  bool radioStartRequested_ = false;
  bool restartRequested_ = false;
};

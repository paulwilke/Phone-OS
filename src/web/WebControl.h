#pragma once

// What the application and the interface may know about the Wi-Fi / web part.
// The interface switches Wi-Fi on and off and shows its status; how Wi-Fi and
// the web server carry that out is WebService's business.

#include <cstdint>
#include <string>

#include "../app/WifiController.h"

struct WifiStatus {
  WifiMode mode = WifiMode::Off;
  WifiPhase phase = WifiPhase::Off;
  std::string ssid;
  std::string apPassword;  // access point only, for the device screen
  std::string address;
  WifiReason reason = WifiReason::None;
  uint32_t secondsLeft = 0;
  bool serverRunning = false;
};

class WebControl {
public:
  virtual ~WebControl() = default;
  virtual bool setWifiMode(WifiMode mode, uint32_t nowSeconds) = 0;
  virtual WifiStatus wifiStatus(uint32_t nowSeconds) = 0;
};

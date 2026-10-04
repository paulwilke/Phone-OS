#pragma once

// WLAN that the test drives. Starting a station leaves it Connecting until the test
// decides how it ends.
//
// Scripting: finishConnecting(true|false) ends a station attempt (Connected or Failed),
// failAccessPoint(bool) makes the next access point fail to start,
// setStationAddress(a). Reading back: ssid(), password().

#include <hal/Wifi.h>

class FakeWifi : public Wifi {
public:
  bool startStation(const std::string& ssid, const std::string& password) override {
    if (ssid.empty()) return false;
    stop();
    state_ = WifiState::Connecting;
    ssid_ = ssid;
    password_ = password;
    return true;
  }
  bool startAccessPoint(const std::string& ssid, const std::string& password) override {
    if (ssid.empty() || password.size() < 8 || password.size() > 63) return false;
    stop();
    if (apFails_) return false;
    state_ = WifiState::AccessPoint;
    ssid_ = ssid;
    password_ = password;
    return true;
  }
  void stop() override {
    state_ = WifiState::Off;
    ssid_.clear();
    password_.clear();
  }

  WifiState state() override { return state_; }
  std::string address() override {
    if (state_ == WifiState::AccessPoint) return "192.168.4.1";
    if (state_ == WifiState::Connected) return stationAddress_;
    return "";
  }

  // Scripting.
  void finishConnecting(bool success) {
    if (state_ == WifiState::Connecting) state_ = success ? WifiState::Connected : WifiState::Failed;
  }
  void failAccessPoint(bool fails) { apFails_ = fails; }
  void setStationAddress(const std::string& address) { stationAddress_ = address; }
  const std::string& ssid() const { return ssid_; }
  const std::string& password() const { return password_; }

private:
  WifiState state_ = WifiState::Off;
  std::string ssid_, password_;
  std::string stationAddress_ = "192.168.1.50";
  bool apFails_ = false;
};

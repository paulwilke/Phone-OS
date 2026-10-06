#pragma once

// The Wi-Fi radio of the processor, scripted.
//   finishConnecting(bool)        the station attempt ends: signed in (with an address) or failed
//   failAccessPoint(bool)         an access point that is valid does not start
//   setStationAddress(string)     what the home network hands out (default 192.168.1.50)
// Reading back: beginStationCalls(), beginAccessPointCalls(), shutdowns(), ssid(), password(), running().

#include <string>

#include <board/WifiRadio.h>

class FakeWifiRadio : public board::WifiRadio {
public:
  void beginStation(const std::string& ssid, const std::string& password) override {
    ++stationCalls_;
    ssid_ = ssid;
    password_ = password;
    link_ = board::StationLink::Connecting;
    running_ = true;
  }
  bool beginAccessPoint(const std::string& ssid, const std::string& password) override {
    ++apCalls_;
    ssid_ = ssid;
    password_ = password;
    running_ = !failAp_;
    return !failAp_;
  }
  void shutdown() override {
    ++shutdowns_;
    running_ = false;
    link_ = board::StationLink::Connecting;
  }
  board::StationLink stationLink() override { return link_; }
  std::string stationAddress() override { return link_ == board::StationLink::Connected ? stationAddress_ : std::string(); }
  std::string accessPointAddress() override { return running_ ? "192.168.4.1" : std::string(); }

  void finishConnecting(bool success) { link_ = success ? board::StationLink::Connected : board::StationLink::Failed; }
  void failAccessPoint(bool fails) { failAp_ = fails; }
  void setStationAddress(const std::string& address) { stationAddress_ = address; }

  int beginStationCalls() const { return stationCalls_; }
  int beginAccessPointCalls() const { return apCalls_; }
  int shutdowns() const { return shutdowns_; }
  const std::string& ssid() const { return ssid_; }
  const std::string& password() const { return password_; }
  bool running() const { return running_; }

private:
  board::StationLink link_ = board::StationLink::Connecting;
  std::string stationAddress_ = "192.168.1.50";
  std::string ssid_, password_;
  bool failAp_ = false;
  bool running_ = false;
  int stationCalls_ = 0, apCalls_ = 0, shutdowns_ = 0;
};

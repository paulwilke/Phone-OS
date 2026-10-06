#include "BoardWifi.h"

namespace board {

bool BoardWifi::startStation(const std::string& ssid, const std::string& password) {
  if (ssid.empty() || ssid.size() > kWifiNameMax || password.size() > kWifiPasswordMax) return false;  // refused: nothing changes
  stop();
  radio_.beginStation(ssid, password);  // an empty password means an open network
  mode_ = Mode::Station;
  return true;
}

bool BoardWifi::startAccessPoint(const std::string& ssid, const std::string& password) {
  if (ssid.empty() || ssid.size() > kWifiNameMax || password.size() < kAccessPointPasswordMin || password.size() > kWifiPasswordMax)
    return false;
  stop();
  if (!radio_.beginAccessPoint(ssid, password)) {
    radio_.shutdown();
    mode_ = Mode::Off;
    return false;
  }
  mode_ = Mode::AccessPoint;
  return true;
}

void BoardWifi::stop() {
  if (mode_ == Mode::Off) return;
  radio_.shutdown();
  mode_ = Mode::Off;
}

WifiState BoardWifi::state() {
  switch (mode_) {
    case Mode::AccessPoint: return WifiState::AccessPoint;
    case Mode::Station:
      switch (radio_.stationLink()) {
        case StationLink::Connected: return WifiState::Connected;
        case StationLink::Failed: return WifiState::Failed;
        case StationLink::Connecting: break;
      }
      return WifiState::Connecting;
    case Mode::Off: break;
  }
  return WifiState::Off;
}

std::string BoardWifi::address() {
  switch (mode_) {
    case Mode::AccessPoint: return radio_.accessPointAddress();
    case Mode::Station: return radio_.stationLink() == StationLink::Connected ? radio_.stationAddress() : std::string();
    case Mode::Off: break;
  }
  return std::string();
}

}  // namespace board

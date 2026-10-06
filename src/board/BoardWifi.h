#pragma once

// The Wifi capability on the board: the rules of the capability on top of a WifiRadio. Non-blocking, the
// access point's password has 8 to 63 characters, a name has 1 to 32, starting one mode stops the other,
// and a start that is refused changes nothing: what ran keeps running. An access point that is valid but
// does not start leaves the state Off. Whoever starts Wi-Fi stops it again (the controller does that): it is
// the biggest consumer after the modem.

#include "../hal/Wifi.h"
#include "WifiRadio.h"

namespace board {

constexpr size_t kWifiNameMax = 32;
constexpr size_t kWifiPasswordMax = 63;
constexpr size_t kAccessPointPasswordMin = 8;

class BoardWifi : public Wifi {
public:
  explicit BoardWifi(WifiRadio& radio) : radio_(radio) {}

  bool startStation(const std::string& ssid, const std::string& password) override;
  bool startAccessPoint(const std::string& ssid, const std::string& password) override;
  void stop() override;
  WifiState state() override;
  std::string address() override;

private:
  enum class Mode : unsigned char { Off, Station, AccessPoint };
  WifiRadio& radio_;
  Mode mode_ = Mode::Off;
};

}  // namespace board

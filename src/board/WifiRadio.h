#pragma once

// The seam below the Wi-Fi capability: what the radio of the processor is told and asked, in its own terms.
// The device implements it with the Wi-Fi stack of the ESP32 (device/Esp32WifiRadio.h); the tests with a
// scripted one (testing/fakes/FakeWifiRadio.h). Everything that makes it the capability — what is refused,
// what stops what, what state it reports — is BoardWifi's and is tested on the host.

#include <string>

namespace board {

enum class StationLink : unsigned char { Connecting, Connected, Failed };

class WifiRadio {
public:
  virtual ~WifiRadio() = default;

  // Starts signing in; returns at once. Nothing is stored in the flash of the chip: credentials live on the
  // card and nowhere else, and a device that remembers strange networks would be hard to understand.
  virtual void beginStation(const std::string& ssid, const std::string& password) = 0;
  // Opens an access point of its own. False when it did not start; the radio is off then.
  virtual bool beginAccessPoint(const std::string& ssid, const std::string& password) = 0;
  // Stops everything; the chip is without power afterwards.
  virtual void shutdown() = 0;

  virtual StationLink stationLink() = 0;
  virtual std::string stationAddress() = 0;
  virtual std::string accessPointAddress() = 0;
};

}  // namespace board

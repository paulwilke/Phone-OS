#pragma once

// Capability: WLAN. Two modes: as a station in the home network, or as an access point
// of its own (so the device can be set up where there is no home network). The
// application knows only this interface; whether the chip of the processor or a
// network of the simulator stands behind it, only the implementation knows.
//
// Everything is non-blocking: start*() sets things going, state() says how far it
// is. WLAN is the biggest consumer after the modem: whoever starts it must stop it
// again (a controller does that).

#include <cstdint>
#include <string>

enum class WifiState : uint8_t {
  Off,
  Connecting,   // station: signing in
  Connected,    // station: signed in and has an address
  AccessPoint,  // the access point runs
  Failed        // sign-in failed (wrong password, network not there)
};

class Wifi {
public:
  virtual ~Wifi() = default;

  // Signs in to the home network. An empty password means an open network; an empty
  // name is refused. Starting a mode while the other runs stops the other first. True
  // when the attempt was started (not that it succeeded: state() tells).
  virtual bool startStation(const std::string& ssid, const std::string& password) = 0;
  // Opens an access point. The password must have 8 to 63 characters; anything else is
  // refused (false, and nothing runs). Start-up that fails leaves the state Off.
  virtual bool startAccessPoint(const std::string& ssid, const std::string& password) = 0;
  // Stops either mode; the chip is without power afterwards. Stopping when Off is fine.
  virtual void stop() = 0;

  virtual WifiState state() = 0;
  // The device's own address ("192.168.4.1"); empty as long as there is none (Off,
  // Connecting, Failed).
  virtual std::string address() = 0;
};

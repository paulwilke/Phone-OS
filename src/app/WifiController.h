#pragma once

// Wi-Fi is off unless somebody asks for it, and it switches itself off after a while (decided
// 2026-10-01). After the modem it is the biggest consumer; a device that transmits on the side
// does not last a day. All of it runs through the capability `Wifi`.
//
// Two modes, both only on request:
//   Station      sign in to a home network; the stored networks in turn, each for at most
//                kAttemptSeconds
//   AccessPoint  the device's own access point, so that it can be set up without a home network.
//                The password is the stored one or a random eight-digit one that only the
//                device's screen reveals, and it is forgotten when Wi-Fi goes off.
//
// The switch-off counts from the last use (noteActivity), not from switching on: somebody who is
// just editing the contacts must not be thrown out in the middle.
//
// Why it stopped is a code, not a sentence (`reason()`): the interface words it.

#include <cstddef>
#include <cstdint>
#include <string>

#include "../hal/Wifi.h"
#include "WifiSettings.h"

enum class WifiMode : uint8_t { Off, Station, AccessPoint };
enum class WifiPhase : uint8_t { Off, Connecting, Connected, AccessPoint, Failed };
// Why Wi-Fi is off or failed.
enum class WifiReason : uint8_t {
  None,
  NoHomeNetwork,         // station mode, and no network is stored
  HomeNetworkUnreachable,  // every stored network was tried
  AccessPointFailed,     // the access point would not start
  AccessPointLost,       // it was running and stopped
  SwitchedOffByItself,   // the idle time ran out
};

class WifiController {
public:
  static constexpr uint32_t kAttemptSeconds = 20;
  using Random = uint32_t (*)();

  WifiController(Wifi& wifi, Random random) : wifi_(wifi), random_(random) {}

  void configure(const WifiSettings& settings) { settings_ = settings; }

  // Switches on (or over). False when it cannot; reason() says why.
  bool request(WifiMode mode, uint32_t nowSeconds);
  // Switches off. `reason` is shown when it was not the user's wish.
  void stop(WifiReason reason = WifiReason::None);
  void poll(uint32_t nowSeconds);
  // The device is in use (a page was served): the switch-off timer starts again.
  void noteActivity(uint32_t nowSeconds) { lastActivity_ = nowSeconds; }

  WifiMode mode() const { return mode_; }
  WifiPhase phase() const { return phase_; }
  bool active() const { return phase_ != WifiPhase::Off && phase_ != WifiPhase::Failed; }
  // The network (station) or the device's own name (access point).
  const std::string& ssid() const { return ssid_; }
  // Only for the access point, only for the device's screen.
  const std::string& apPassword() const { return apPassword_; }
  std::string address() { return wifi_.address(); }
  WifiReason reason() const { return reason_; }
  // Seconds until it switches itself off; 0 when it is off.
  uint32_t secondsLeft(uint32_t nowSeconds) const;

private:
  void startNetwork(size_t index, uint32_t now);
  void fail(WifiReason reason);

  Wifi& wifi_;
  Random random_;
  WifiSettings settings_;

  WifiMode mode_ = WifiMode::Off;
  WifiPhase phase_ = WifiPhase::Off;
  size_t netIndex_ = 0;
  uint32_t attemptStarted_ = 0;
  uint32_t lastActivity_ = 0;
  std::string ssid_, apPassword_;
  WifiReason reason_ = WifiReason::None;
};

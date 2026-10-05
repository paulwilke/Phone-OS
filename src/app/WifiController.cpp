#include "WifiController.h"

#include <cstdio>

void WifiController::fail(WifiReason reason) {
  wifi_.stop();
  phase_ = WifiPhase::Failed;
  mode_ = WifiMode::Off;
  reason_ = reason;
}

void WifiController::stop(WifiReason reason) {
  wifi_.stop();
  phase_ = WifiPhase::Off;
  mode_ = WifiMode::Off;
  apPassword_.clear();  // the password belongs on the screen and nowhere else
  reason_ = reason;
}

void WifiController::startNetwork(size_t index, uint32_t now) {
  netIndex_ = index;
  attemptStarted_ = now;
  const WifiSettings::Network& n = settings_.networks[index];
  ssid_ = n.ssid;
  phase_ = WifiPhase::Connecting;
  if (!wifi_.startStation(n.ssid, n.password)) phase_ = WifiPhase::Failed;
}

bool WifiController::request(WifiMode mode, uint32_t now) {
  if (mode == WifiMode::Off) {
    stop();
    return true;
  }
  reason_ = WifiReason::None;
  lastActivity_ = now;
  mode_ = mode;

  if (mode == WifiMode::Station) {
    if (settings_.networks.empty()) {
      fail(WifiReason::NoHomeNetwork);
      return false;
    }
    startNetwork(0, now);
    return true;
  }

  // The access point: the stored password, else a fresh one. Eight digits can be typed from the
  // screen and are enough for an access point that disappears after minutes.
  if (!settings_.apPassword.empty()) {
    apPassword_ = settings_.apPassword;
  } else {
    char digits[16];
    snprintf(digits, sizeof digits, "%08u", static_cast<unsigned>(random_() % 100000000u));
    apPassword_ = digits;
  }
  ssid_ = settings_.apSsid;
  if (!wifi_.startAccessPoint(ssid_, apPassword_)) {
    fail(WifiReason::AccessPointFailed);
    return false;
  }
  phase_ = WifiPhase::AccessPoint;
  return true;
}

uint32_t WifiController::secondsLeft(uint32_t now) const {
  if (!active()) return 0;
  const uint32_t limit = settings_.autoOffMinutes * 60u;
  const uint32_t used = now - lastActivity_;
  return used >= limit ? 0 : limit - used;
}

void WifiController::poll(uint32_t now) {
  if (!active()) return;

  // The switch-off timer applies in every phase.
  if (now - lastActivity_ >= settings_.autoOffMinutes * 60u) {
    stop(WifiReason::SwitchedOffByItself);
    return;
  }

  if (mode_ == WifiMode::AccessPoint) {
    if (wifi_.state() != WifiState::AccessPoint) fail(WifiReason::AccessPointLost);
    return;
  }

  const WifiState s = wifi_.state();
  if (phase_ == WifiPhase::Connected) {
    // The connection was lost: start again from the first network.
    if (s != WifiState::Connected) startNetwork(0, now);
    return;
  }
  if (s == WifiState::Connected) {
    phase_ = WifiPhase::Connected;
    // Somebody who has just connected wants to use it.
    lastActivity_ = now;
    return;
  }
  if (s == WifiState::Failed || now - attemptStarted_ >= kAttemptSeconds) {
    if (netIndex_ + 1 < settings_.networks.size()) startNetwork(netIndex_ + 1, now);
    else fail(WifiReason::HomeNetworkUnreachable);
  }
}

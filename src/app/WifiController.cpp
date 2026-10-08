#include "WifiController.h"

#include <cstdio>

void WifiController::fail(WifiReason reason) {
  inGrace_ = false;
  wifi_.stop();
  phase_ = WifiPhase::Failed;
  mode_ = WifiMode::Off;
  reason_ = reason;
}

void WifiController::stop(WifiReason reason) {
  inGrace_ = false;
  wifi_.stop();
  phase_ = WifiPhase::Off;
  mode_ = WifiMode::Off;
  apPassword_.clear();  // the password belongs on the screen and nowhere else
  reason_ = reason;
}

void WifiController::startNetwork(size_t index, uint32_t now) {
  inGrace_ = false;
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
  const uint32_t used = idleSeconds(now);
  return used >= limit ? 0 : limit - used;
}

void WifiController::poll(uint32_t now) {
  if (!active()) return;

  // The switch-off timer applies in every phase.
  if (idleSeconds(now) >= settings_.autoOffMinutes * 60u) {
    stop(WifiReason::SwitchedOffByItself);
    return;
  }

  if (mode_ == WifiMode::AccessPoint) {
    if (wifi_.state() != WifiState::AccessPoint) fail(WifiReason::AccessPointLost);
    return;
  }

  const WifiState s = wifi_.state();
  if (phase_ == WifiPhase::Connected) {
    if (s == WifiState::Connected) {
      if (inGrace_) endGrace(now);
      return;
    }
    // One reading is not a lost link: the phase stays Connected (so the server keeps its answer going)
    // until the link has been gone for kLinkGraceSeconds, and only then it starts again from the first network.
    if (!inGrace_) {
      inGrace_ = true;
      graceStarted_ = now;
      graceLogged_ = mayLogGlitch(now);
      if (graceLogged_) {
        const std::string detail = wifi_.linkDetail();
        std::string line = "Wi-Fi: link not connected";
        if (!detail.empty()) line += " (" + detail + ")";
        line += "; waiting " + std::to_string(kLinkGraceSeconds) + " s before reconnecting";
        if (suppressed_ > 0) line += " [" + std::to_string(suppressed_) + " earlier ones not logged]";
        suppressed_ = 0;
        note(line);
      }
      return;
    }
    if (now - graceStarted_ >= kLinkGraceSeconds) {
      inGrace_ = false;
      const std::string detail = wifi_.linkDetail();
      note("Wi-Fi: link still not connected after " + std::to_string(now - graceStarted_) + " s" +
           (detail.empty() ? std::string() : " (" + detail + ")") + "; reconnecting");
      startNetwork(0, now);
    }
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

// The link came back inside the grace period: nothing was lost, and the log says how long it took.
void WifiController::endGrace(uint32_t now) {
  inGrace_ = false;
  if (!graceLogged_) return;
  const uint32_t waited = now - graceStarted_;
  note(waited == 0 ? std::string("Wi-Fi: link back within 1 s")
                   : "Wi-Fi: link back after " + std::to_string(waited) + " s");
}

// A flapping link must not fill the card: at most kGlitchLogsPerMinute opening lines a minute, and the
// next line that is written says how many were left out.
bool WifiController::mayLogGlitch(uint32_t now) {
  if (logsInWindow_ == 0 || now - logWindow_ >= 60) {
    logWindow_ = now;
    logsInWindow_ = 0;
  }
  if (logsInWindow_ >= kGlitchLogsPerMinute) {
    ++suppressed_;
    return false;
  }
  ++logsInWindow_;
  return true;
}

// The page notes its activity with the time of the host's loop, the controller is polled with the clock's
// uptime; the two may stand a second apart either way (seen on the device 2026-10-07: the clock's uptime
// starts when the clock is made, the loop's at boot). An activity "in the future" is an activity now: in
// unsigned arithmetic it read as about 136 years without use and switched Wi-Fi off at the first request
// after the login.
uint32_t WifiController::idleSeconds(uint32_t now) const {
  return now > lastActivity_ ? now - lastActivity_ : 0;
}

#include "Connectivity.h"

#include "Cellular.h"
#include "OperatorLabel.h"

void Connectivity::poll() {
  if (wifi_) wifi_->poll(clock_.uptimeSeconds());
  noteModem();
  if (saveMode_) pollSaveMode();
}

void Connectivity::switchModem(bool on) {
  if (power_.isEnabled(PowerDomain::Cellular) == on) return;
  if (!power_.setEnabled(PowerDomain::Cellular, on)) {
    log_.log(on ? "Cellular cannot be switched on" : "Cellular cannot be switched off");
    return;
  }
  powerCoordination_.noteModemAwake();   // off or just started: not asleep
}

void Connectivity::startSearch(const char* why) {
  switchModem(true);
  savePhase_ = SavePhase::Searching;
  savePhaseSince_ = clock_.uptimeSeconds();
  log_.log(std::string("Save mode: the modem searches for a network (") + why + ")");
}

bool Connectivity::searchNow() {
  if (!saveMode_ || savePhase_ != SavePhase::Off) return false;
  startSearch("asked for");
  return true;
}

// The modem's searches in save mode (D-67).
void Connectivity::pollSaveMode() {
  const uint32_t now = clock_.uptimeSeconds();
  const uint32_t since = now > savePhaseSince_ ? now - savePhaseSince_ : 0;
  switch (savePhase_) {
    case SavePhase::Off:
      if (profile_.radioCellularRetryMinutes == 0 || since < profile_.radioCellularRetryMinutes * 60u) return;
      startSearch("timer");
      return;
    case SavePhase::Searching:
      if (registered()) {
        savePhase_ = SavePhase::Online;
        savePhaseSince_ = now;
        log_.log("Save mode: network found, the modem stays on");
      } else if (since >= kSaveSearchWindowSeconds) {
        switchModem(false);
        savePhase_ = SavePhase::Off;
        savePhaseSince_ = now;
        log_.log("Save mode: no network, the modem is off");
      }
      return;
    case SavePhase::Online:
      if (registered()) return;
      savePhase_ = SavePhase::Searching;   // a new window; the modem is on already
      savePhaseSince_ = now;
      log_.log("Save mode: network lost, the modem searches again");
      return;
  }
}

// Whether the modem has named a network since it was last switched on. Watched at every step, not only when
// the label is drawn, so that a restart of the modem between two pictures is not missed.
void Connectivity::noteModem() {
  const ModemState state = modem_.state();
  if (state == ModemState::Off || state == ModemState::PoweringOn || state == ModemState::Initializing) namedSinceOn_ = false;
  if (!modem_.operatorName().empty()) namedSinceOn_ = true;
}

bool Connectivity::setCellularEnabled(bool on) {
  if (saveMode_) {
    log_.log(on ? "Cellular on refused: the emergency radio decides when the modem searches"
                : "Cellular off refused: the emergency radio decides when the modem searches");
    return false;
  }
  if (power_.isEnabled(PowerDomain::Cellular) == on) return true;
  if (!power_.setEnabled(PowerDomain::Cellular, on)) {
    log_.log(on ? "Cellular cannot be switched on" : "Cellular cannot be switched off");
    return false;
  }
  // Off or just started: both are the opposite of asleep.
  powerCoordination_.noteModemAwake();
  log_.log(on ? "Cellular switched on" : "Cellular switched off");
  notices_.set(on ? NoticeKind::CellularSwitchedOn : NoticeKind::CellularSwitchedOff);
  return true;
}

bool Connectivity::registered() { return cellular::registered(power_, modem_); }

Connectivity::NetworkLabel Connectivity::networkLabel() {
  NetworkLabel label;
  const std::string op = modem_.operatorName();
  const ModemState state = modem_.state();
  noteModem();
  // Owner, 2026-10-08: after switching on, the status line said "Network" and "No calls yet" before the
  // operator's name. While the modem is on its way it says "Connecting".
  const bool modemOn = cellularEnabled() && state != ModemState::Off && state != ModemState::Error;
  if (op.empty() && modemOn && (!namedSinceOn_ || state != ModemState::Registering)) {
    label.kind = NetworkLabel::Kind::Connecting;
  } else if (!op.empty()) {
    char buffer[24];
    oplabel::shortName(op.c_str(), buffer, sizeof buffer);
    label.kind = NetworkLabel::Kind::Operator;
    label.name = buffer;
  } else if (radio_) {
    label.kind = NetworkLabel::Kind::Radio;
  }
  return label;
}

bool Connectivity::requestRadioStart() {
  if (!profile_.radioAllowed || radio_) return false;
  radioStartRequested_ = true;
  return true;
}

bool Connectivity::takeRadioStartRequest() {
  const bool requested = radioStartRequested_;
  radioStartRequested_ = false;
  return requested;
}

bool Connectivity::requestRadioStop() {
  if (!radio_) return false;
  radioStopRequested_ = true;
  return true;
}

bool Connectivity::takeRadioStopRequest() {
  const bool requested = radioStopRequested_;
  radioStopRequested_ = false;
  return requested;
}

void Connectivity::enterSaveMode() {
  if (saveMode_) return;
  cellularWasOn_ = cellularEnabled();
  // The modem is off between the searches (D-67); the timer starts now.
  switchModem(false);
  setWifiMode(WifiMode::Off);
  saveMode_ = true;
  savePhase_ = SavePhase::Off;
  savePhaseSince_ = clock_.uptimeSeconds();
  log_.log("Save mode on: the modem is off and searches on demand and by the timer, Wi-Fi off");
}

void Connectivity::leaveSaveMode() {
  if (!saveMode_) return;
  saveMode_ = false;
  savePhase_ = SavePhase::Off;
  savePhaseSince_ = 0;
  setCellularEnabled(cellularWasOn_);
  log_.log("Save mode off");
}

bool Connectivity::setWifiMode(WifiMode mode) {
  if (!wifi_) return false;
  if (mode != WifiMode::Off && saveMode_) {
    log_.log("Wi-Fi refused: the emergency radio keeps it off");
    return false;
  }
  const int battery = power_.batteryPercent();
  if (mode != WifiMode::Off && !power_.externalPower() && battery >= 0 && battery < kMinWifiBattery) {
    notices_.set(NoticeKind::WifiBatteryLow);
    log_.log("Wi-Fi refused: battery below 20 percent");
    return false;
  }
  const bool ok = wifi_->request(mode, clock_.uptimeSeconds());
  const char* name = mode == WifiMode::Station ? "home network" : mode == WifiMode::AccessPoint ? "access point" : "off";
  log_.log(std::string("Wi-Fi ") + name + (ok ? "" : " failed"));
  return ok;
}

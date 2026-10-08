#include "Connectivity.h"

#include "Cellular.h"
#include "OperatorLabel.h"

void Connectivity::poll() {
  if (wifi_) wifi_->poll(clock_.uptimeSeconds());
  noteModem();
}

// Whether the modem has named a network since it was last switched on. Watched at every step, not only when
// the label is drawn, so that a restart of the modem between two pictures is not missed.
void Connectivity::noteModem() {
  const ModemState state = modem_.state();
  if (state == ModemState::Off || state == ModemState::PoweringOn || state == ModemState::Initializing) namedSinceOn_ = false;
  if (!modem_.operatorName().empty()) namedSinceOn_ = true;
}

bool Connectivity::setCellularEnabled(bool on) {
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

bool Connectivity::setWifiMode(WifiMode mode) {
  if (!wifi_) return false;
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

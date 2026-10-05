#include "Connectivity.h"

#include "Cellular.h"
#include "OperatorLabel.h"

void Connectivity::poll() {
  if (saveMode_ && probeUntil_ && clock_.uptimeSeconds() >= probeUntil_) {
    probeUntil_ = 0;
    setCellularEnabled(false);
    log_.log("Save mode: cell probe ended");
  }
  if (wifi_) wifi_->poll(clock_.uptimeSeconds());
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
  if (!op.empty()) {
    char buffer[24];
    oplabel::shortName(op.c_str(), buffer, sizeof buffer);
    label.kind = NetworkLabel::Kind::Operator;
    label.name = buffer;
  } else if (saveMode_) {
    label.kind = NetworkLabel::Kind::RadioOnly;
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

bool Connectivity::setSaveMode(bool on) {
  if (on == saveMode_) return true;
  probeUntil_ = 0;
  if (on) {
    // Without a running radio there would be no way out of the device any more.
    if (!radio_) {
      notices_.set(NoticeKind::SaveModeNeedsRadio);
      return false;
    }
    saveMode_ = true;
    if (wifi_) wifi_->request(WifiMode::Off, clock_.uptimeSeconds());
    setCellularEnabled(false);
    log_.log("Save mode on: radio only, cellular and Wi-Fi off");
    notices_.set(NoticeKind::SaveModeOn);
    return true;
  }
  saveMode_ = false;
  setCellularEnabled(true);
  log_.log("Save mode off: cellular on again");
  return true;
}

bool Connectivity::probeCellular() {
  if (!saveMode_) return false;
  probeUntil_ = clock_.uptimeSeconds() + kProbeSeconds;
  setCellularEnabled(true);
  log_.log("Save mode: looking for the mobile network");
  return true;
}

uint32_t Connectivity::probeSecondsLeft() const {
  const uint32_t now = clock_.uptimeSeconds();
  return probeUntil_ > now ? probeUntil_ - now : 0;
}

bool Connectivity::setWifiMode(WifiMode mode) {
  if (!wifi_) return false;
  if (mode != WifiMode::Off && saveMode_) {
    notices_.set(NoticeKind::SaveModeNoWifi);
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

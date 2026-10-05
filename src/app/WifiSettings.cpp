#include "WifiSettings.h"

bool WifiSettings::validPassword(const std::string& password, bool allowEmpty) {
  if (password.empty()) return allowEmpty;
  return password.size() >= 8 && password.size() <= 63;
}

WifiSettings::AddResult WifiSettings::addNetwork(const std::string& ssid, const std::string& password) {
  if (ssid.empty() || ssid.size() > kMaxSsidBytes) return AddResult::BadName;
  if (!validPassword(password, true)) return AddResult::BadPassword;
  if (networks.size() >= kMaxNetworks) return AddResult::TooMany;
  networks.push_back({ssid, password});
  return AddResult::Added;
}

bool WifiSettings::setApPassword(const std::string& password) {
  if (!validPassword(password, true)) return false;
  apPassword = password;
  return true;
}

bool WifiSettings::setApSsid(const std::string& ssid) {
  if (ssid.empty() || ssid.size() > kMaxSsidBytes) return false;
  apSsid = ssid;
  return true;
}

bool WifiSettings::setAutoOffMinutes(uint32_t minutes) {
  autoOffMinutes = minutes < kMinAutoOffMinutes ? kMinAutoOffMinutes
                   : minutes > kMaxAutoOffMinutes ? kMaxAutoOffMinutes : minutes;
  return autoOffMinutes == minutes;
}

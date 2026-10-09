#pragma once

// The Wi-Fi settings: the home networks the device may sign in to, and its own access point. Plain
// data with the rules that make an entry usable; src/config/ConfigLoader reads them from the
// settings file.
//
// Passwords are kept in clear on the card, like every other secret there — whoever has the card
// has them — and are never shown on the web page, only set anew. Without an access point password
// the device invents one every time it starts the access point and shows it on its screen: an
// open or built-in password would be a back door to the contacts.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct WifiSettings {
  static constexpr size_t kMaxNetworks = 4;
  static constexpr size_t kMaxSsidBytes = 32;
  static constexpr uint32_t kMinAutoOffMinutes = 1;
  static constexpr uint32_t kMaxAutoOffMinutes = 120;

  struct Network {
    std::string ssid, password;
  };
  enum class AddResult {
    Added,
    BadName,      // empty or longer than 32 bytes
    BadPassword,  // not empty and not 8 to 63 characters (an empty one is an open network)
    TooMany,      // more than kMaxNetworks
  };

  std::vector<Network> networks;
  std::string apSsid = "phone-os";
  // Empty: a new random password at every start of the access point.
  std::string apPassword;
  uint32_t autoOffMinutes = 10;

  AddResult addNetwork(const std::string& ssid, const std::string& password);
  // False, and unchanged, for a password outside 8 … 63 characters: the access point would not
  // start at all, so the random one stays in force. An empty password clears it.
  bool setApPassword(const std::string& password);
  // False, and unchanged, for a name that is empty or too long.
  bool setApSsid(const std::string& ssid);
  // Clamped to 1 … 120. True when the value was taken as given.
  bool setAutoOffMinutes(uint32_t minutes);

  // WPA2 wants 8 to 63 characters; an empty password means an open network when `allowEmpty`.
  static bool validPassword(const std::string& password, bool allowEmpty);
};

#pragma once

// Writes configuration back to the SD card, atomically through ConfigStore. Used
// when the child changes a setting at run time (message vibration, remote-query
// notice) and by the web interface after a form submit.
//
// Each write method renders a fresh TOML file from the current values in the
// Profile (or the separate secrets); comments are not preserved across writes.
// A failed write (full or missing card) leaves the old file intact and returns false.

#include <string>

#include "../app/Profile.h"
#include "../hal/ConfigStore.h"

namespace config {

class ConfigWriter {
public:
  explicit ConfigWriter(ConfigStore& store);

  // Writes /config/settings.toml from the profile.
  bool writeSettings(const Profile& profile) const;

  // Writes /config/contacts.toml from profile.contacts.
  bool writeContacts(const Profile& profile) const;

  // Writes /config/secrets.toml from the wifi settings, simPin and webPassword.
  // The passwords must be passed by the caller, not stored in Profile.
  bool writeSecrets(const Profile& profile, const std::string& simPin,
                    const std::string& webPassword) const;

private:
  ConfigStore& store_;
};

}  // namespace config

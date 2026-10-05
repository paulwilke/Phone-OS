#pragma once

// Reads the three configuration files from the SD card into a Profile.
//
// Every problem is a Diagnostic, never a silent default: an unknown key (a typo in the
// parents' file), a wrong type, a value outside the allowed range, a contact without a
// number, a quiet-hours window the QuietHours module refuses, a TOML parse error with
// its line number. A missing file means defaults and one MissingFile diagnostic; a file
// larger than kMaxFileBytes means one OversizedFile diagnostic and no load for that file.
//
// The loader does not validate language codes against the installed catalogs; the host
// calls translator.select() after loading and handles an unknown code there.
//
// User catalogs: loadUserCatalogs() reads /lang/*.toml from the card and feeds each one
// to Translator::addUser(). A catalog that fails validation is skipped with a
// UserCatalogError diagnostic; loading continues with the next file.

#include <string>
#include <vector>

#include "../app/Profile.h"
#include "../hal/ConfigStore.h"
#include "../i18n/Translator.h"

#include "Diagnostic.h"

namespace config {

class ConfigLoader {
public:
  // Maximum file size read from each config file. Files larger than this are skipped.
  static constexpr size_t kMaxFileBytes = 4096;
  // Maximum size of a user catalog file.
  static constexpr size_t kMaxCatalogBytes = 16384;

  struct Result {
    Profile profile;
    std::string simPin;       // secret; not stored in Profile
    std::string webPassword;  // secret; not stored in Profile
    std::vector<Diagnostic> diagnostics;
  };

  explicit ConfigLoader(ConfigStore& store);

  // Reads settings.toml, contacts.toml and secrets.toml from /config/ into a Profile.
  // Missing files give defaults. A file that is present but cannot be parsed still gives
  // defaults for that file; other files are loaded normally.
  Result load() const;

  // Reads user-provided catalogs from /lang/ on the card and adds them to the translator.
  // Call this before load() so that catalog codes can be validated later by the host.
  static void loadUserCatalogs(ConfigStore& store, i18n::Translator& translator,
                                std::vector<Diagnostic>& diagnostics);

private:
  ConfigStore& store_;
};

}  // namespace config

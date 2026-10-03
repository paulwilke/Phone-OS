#pragma once

// Looks messages up in the selected language and fills in their placeholders.
//
// Languages come from two places and are layered:
//   1. the catalogs built into the firmware (English is the reference and the last
//      resort; German and French ship with it), and
//   2. catalogs the user puts on the SD card. A user catalog with a *new* code adds a
//      language to the picker; one with an existing code overrides single messages
//      of that language (a parent who prefers "Hol mich ab" over "Abholen").
//
// Lookup order for a message: the selected language, then English. A key that is in
// neither is returned as the key itself and remembered in `missingKeys()` — visible
// to tests and the catalog check, and (as "home.phone") plainly wrong on screen
// instead of silently empty.
//
// Strings are UTF-8. Turning them into glyphs is the display layer's job.

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "Catalog.h"
#include "Format.h"

namespace i18n {

struct LanguageInfo {
  std::string code;
  std::string name;
  std::string native;
};

class Translator {
public:
  static constexpr const char* kReference = "en";

  // Adds a catalog (see the layering above). Returns false when the catalog was
  // empty.
  bool add(const Catalog& catalog);

  // Parses and adds a user catalog from text. Returns false and fills `errors` when the
  // text is not a valid catalog; nothing is added then. A catalog that differs from the
  // English reference in its placeholders is rejected as well, because a wrong
  // placeholder is a message that prints garbage.
  bool addUser(const char* text, size_t length, std::vector<std::string>* errors);

  // Selects a language by code. "fr-CA" falls back to "fr". Returns false (and keeps
  // the current language) for an unknown code.
  bool select(const std::string& code);
  const std::string& language() const { return current_; }
  std::vector<LanguageInfo> languages() const;

  // The message as stored (no placeholders filled). For a message with plural forms,
  // the first form.
  const char* text(const char* key);

  // The message with placeholders filled in.
  std::string get(const char* key, const Arg* args = nullptr, size_t argCount = 0);
  std::string get(const char* key, std::initializer_list<Arg> args) { return get(key, args.begin(), args.size()); }

  // A message with a count: picks the plural form for `n` (the message should use
  // `{n}`; the count is also passed as the argument "n").
  std::string plural(const char* key, uint32_t n, const Arg* args = nullptr, size_t argCount = 0);

  // Keys that were asked for and found in no language, and how often a lookup had to
  // fall back from the selected language to English.
  const std::set<std::string>& missingKeys() const { return missing_; }
  size_t fallbackCount() const { return fallbacks_; }

private:
  const Entry* find(const char* key, PluralRule* rule);

  std::map<std::string, Catalog> languages_;
  std::string current_ = kReference;
  std::set<std::string> missing_;
  size_t fallbacks_ = 0;
};

}  // namespace i18n

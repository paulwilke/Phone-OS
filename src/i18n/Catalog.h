#pragma once

// A language catalog: the translated messages of one language, read from a small
// TOML file. The same format serves the three languages that ship with the
// firmware (lang/en.toml, de.toml, fr.toml) and translations that users add on the
// SD card, so one parser and one set of rules covers both.
//
//   [meta]
//   code   = "fr"            # language tag: "fr", "pt-BR"
//   name   = "French"        # English name, for parents who read the web page
//   native = "Français"      # the language's own name, for the language picker
//   plural = "zero-one-other"
//
//   [strings]
//   home.phone      = "Téléphone"
//   call.busy       = "{name} : occupé"
//   messages.unread = ["{n} nouveau message", "{n} nouveaux messages"]
//
// A message with a count has one form per plural category (see Plural.h). Keys are
// lowercase words joined by dots; the dotted key is the *whole* key — the parser
// does not build nested tables.
//
// The parser is strict on purpose. A catalog with a typo is rejected with the line
// number of every problem, instead of loading half of it and showing English or raw
// keys on the child's screen weeks later.

#include <map>
#include <set>
#include <string>
#include <vector>

#include "Plural.h"

namespace i18n {

struct Entry {
  std::vector<std::string> forms;  // one per plural category; a single form for plain messages
};

struct Issue {
  std::string key;   // empty for problems with the catalog as a whole
  std::string what;
};

class Catalog {
public:
  struct Error {
    int line;
    std::string message;
  };

  // Parses `text`. Returns true when there was no error; `errors()` lists all problems
  // otherwise (the catalog is left empty then).
  bool parse(const char* text, size_t length);

  const std::vector<Error>& errors() const { return errors_; }

  const std::string& code() const { return code_; }
  const std::string& name() const { return name_; }
  const std::string& native() const { return native_; }
  PluralRule plural() const { return plural_; }

  const Entry* find(const std::string& key) const {
    auto it = entries_.find(key);
    return it == entries_.end() ? nullptr : &it->second;
  }
  const std::map<std::string, Entry>& entries() const { return entries_; }
  size_t size() const { return entries_.size(); }

  // Adds the entries of `other` (they win over existing ones). Used for user
  // translations that override single messages of a shipped language.
  void merge(const Catalog& other);

  // Compares this catalog with the reference (English): messages the reference has
  // and this one lacks, messages that exist only here (a leftover from a renamed key),
  // a different set of `{placeholders}`, and a wrong number of plural forms.
  std::vector<Issue> compare(const Catalog& reference) const;

  // The names of the `{placeholders}` of a message form, in order of first appearance.
  static std::vector<std::string> placeholders(const std::string& form);

private:
  friend class Translator;
  std::string code_, name_, native_;
  PluralRule plural_ = PluralRule::OneOther;
  std::map<std::string, Entry> entries_;
  std::vector<Error> errors_;
};

}  // namespace i18n

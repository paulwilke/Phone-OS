#pragma once

// Plural rules. A translated message with a count has one form per plural
// category of the language; the rule maps a count to the form's index.
//
// Only the rules the shipped languages need exist. A user translation names its
// rule in the catalog header (`plural = "..."`); an unknown name is rejected when
// the catalog is loaded, never guessed.

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace i18n {

enum class PluralRule : uint8_t {
  // One form for everything ("1 Nachricht", "2 Nachrichten" need two; this is for
  // languages without plural, e.g. Japanese — and for messages without a count).
  Single,
  // Form 0 for exactly 1, form 1 otherwise. English, German, Spanish, Italian, ...
  OneOther,
  // Form 0 for 0 and 1, form 1 otherwise. French, Portuguese.
  ZeroOneOther,
};

// Number of forms a message needs under the rule.
inline size_t pluralFormCount(PluralRule rule) {
  return rule == PluralRule::Single ? 1 : 2;
}

// Index of the form to use for `n`.
inline size_t pluralIndex(PluralRule rule, uint32_t n) {
  switch (rule) {
    case PluralRule::Single: return 0;
    case PluralRule::OneOther: return n == 1 ? 0 : 1;
    case PluralRule::ZeroOneOther: return n <= 1 ? 0 : 1;
  }
  return 0;
}

// Parses the rule name used in catalog headers. Returns false for unknown names.
inline bool parsePluralRule(const char* name, PluralRule* out) {
  if (strcmp(name, "single") == 0) *out = PluralRule::Single;
  else if (strcmp(name, "one-other") == 0) *out = PluralRule::OneOther;
  else if (strcmp(name, "zero-one-other") == 0) *out = PluralRule::ZeroOneOther;
  else return false;
  return true;
}

}  // namespace i18n

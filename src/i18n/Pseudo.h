#pragma once

// allow-non-english: the accented letters it produces are shown in comments.

// A pseudo-language for testing layouts: the English catalog, transformed so that a layout
// that survives it survives real translations. It is not shipped and no user can select it;
// the host tests add it next to English, German and French (testing/Languages.h).
//
// What the transformation does to a message, and why:
//   * Vowels and a few consonants become accented letters of Latin-1 (a -> á, e -> é,
//     o -> ö, A -> Å, ...). Accents are taller than plain letters, so a line that is too
//     tight at the top or bottom shows up, and a character the fonts lack shows up as a box.
//   * Every word grows by about 40 % of its letters, because German and French run that much
//     longer than English (words like "Eingang" or "Nachrichten" are not rare). Spaces and
//     everything that is not a letter stay as they are.
//   * The message is put in square brackets. A closing bracket missing on screen means the
//     text was cut off, which is easy to spot in a golden picture.
//   * {placeholders}, {{ and }} are left alone: they are filled in at run time with whatever
//     the caller passes, and a catalog check compares them across languages.
//
// Only characters of assets/fonts/charset.txt are produced, and a test checks it.

#include <string>

#include "Catalog.h"

namespace i18n {

// The language tag of the pseudo-language: a private-use tag, which no real language has.
constexpr const char* kPseudoCode = "qps-ploc";

// One message form transformed as described above.
std::string pseudoLocalize(const std::string& message);

// `reference` with every message form transformed; the plural rule is the reference's.
Catalog pseudoCatalog(const Catalog& reference);

}  // namespace i18n

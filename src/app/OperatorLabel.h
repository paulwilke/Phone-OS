#pragma once

// The mobile operator's name for the status line: short enough for the line and
// without the additions networks hang on their names ("Provider.de",
// "F-Operator Telecom", "OPERATOR.DE").

#include <cstddef>

namespace oplabel {

// The longest label, in characters.
constexpr size_t kMaxCharacters = 10;

// Shortens `name` into `out` (at most `size - 1` bytes):
//   * leading blanks are dropped;
//   * a country prefix of one or two letters and a hyphen ("F-", "NL-") is removed,
//     except where the letter belongs to the brand ("T-Mobile", "E-Plus");
//   * the label ends at the first blank or dot, which removes ".de" and the
//     company form that follows the name;
//   * it is cut to kMaxCharacters characters, never inside a UTF-8 character;
//   * a name written entirely in capitals becomes "Capitalised" (names of up to
//     three characters such as "EE" or "SFR" stay as they are).
// A null name gives an empty label.
void shortName(const char* name, char* out, size_t size);

}  // namespace oplabel

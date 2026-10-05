#pragma once

// Numbers and ages the pages of the instruments write, in one place so that two pages never write the
// same thing in two ways. Digits only where they can be, so that a number reads the same in every
// language; ages are catalog words.

#include <cstdint>
#include <string>

#include "i18n/Translator.h"

namespace ui {
namespace format {

// A coordinate with five decimals ("48.13745", "-11.57550"): about a metre. Done in whole numbers,
// so that the rounding is the same on every platform and a value that rounds to zero has no minus.
std::string degrees(double value);

// One decimal ("1.2"), for a quality figure such as HDOP. Negative values keep their sign.
std::string tenths(float value);

// How long ago, in words: "just now" up to a minute and a half, then minutes, hours and days. One
// that cannot be known (`seconds` is the age; use `unknown` for no time) is the word for "heard".
std::string age(uint32_t seconds, i18n::Translator& t);
std::string ageUnknown(i18n::Translator& t);

// The age of something last seen at the Unix time `lastSeen`, as of the Unix time `now`; either 0
// (unknown) or `lastSeen` after `now` gives the word for "heard".
std::string ageSince(uint32_t lastSeen, uint32_t now, i18n::Translator& t);

}  // namespace format
}  // namespace ui

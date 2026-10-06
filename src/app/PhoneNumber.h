#pragma once

// Phone numbers made comparable. The same number reaches the device as
// "+491701111111", "0049 170 1111111" or "0170-1111111" depending on the network
// and on who typed it; without one canonical form the whitelist would reject a
// parent who is calling from the number on file.
//
// The canonical form is "+" followed by the country code and the subscriber
// digits (E.164), or the bare digits of a short code such as "112".

#include <cstddef>
#include <string>

namespace phone {

// Country code used when none is configured. The first deployment is German;
// anyone else sets their own through ContactBook::setCountryCode().
constexpr const char* kDefaultCountryCode = "+49";

// E.164 limits a number to 15 digits. Longer input is rejected instead of being
// cut off: two different long numbers must never turn into the same one.
constexpr size_t kMaxDigits = 15;

// Room for the longest canonical number: "+", kMaxDigits digits, terminator.
constexpr size_t kNormalizedSize = kMaxDigits + 2;

// Writes the canonical form of `input` to `out`. Returns false, with `out` empty,
// when `input` is not a phone number (a sender name such as "Provider", an empty
// string, too many digits, a misplaced "+") or `out` is too small.
//
// `countryCode` ("+49", or "49") completes numbers written with a national
// leading 0; null or empty selects kDefaultCountryCode, and a value that is not
// a country code makes every national number unreadable, so a misconfiguration
// can never turn into a match.
bool normalize(const char* input, const char* countryCode, char* out, size_t outSize);

// The canonical form of `input`, or an empty string when it is not a phone number: for a log line
// that may name a number and nothing else about a sender.
std::string canonical(const char* input, const char* countryCode);

// True when both are phone numbers and denote the same one. Two unreadable
// inputs are never "the same".
bool same(const char* a, const char* b, const char* countryCode);

}  // namespace phone

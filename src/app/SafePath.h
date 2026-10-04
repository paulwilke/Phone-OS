#pragma once

// Paths that arrive from outside (the parents' web page, the USB console) must not
// leave the SD card and must not address anything odd. The check lives in one
// place and every storage implementation (device, simulator) uses it, instead of
// each one remembering "..", "//" and control characters for itself.
//
// A valid path is absolute and made of letters, digits and `. _ -` separated by
// single slashes. A blank is not allowed: no file on the card needs one and every
// special case is a trap.
//
// The card is FAT, which does not tell upper from lower case, so everything that
// compares names (the scratch-file suffix, the configuration directory, file
// endings) ignores case too. Otherwise `/CONFIG/x` would be the configuration
// directory for the card and not for the check.

#include <cstddef>

namespace safepath {

// Longest path, and longest single name within it, in bytes.
constexpr size_t kMaxLength = 64;
constexpr size_t kMaxSegment = 40;

// Suffix of the scratch file a safe write goes through. It belongs to the
// storage code, not to whoever sends a path, so a path ending in it is refused.
constexpr const char* kTempSuffix = ".tmp";

enum class Result {
  Ok,
  NotAbsolute,     // empty, null or not starting with "/"
  TooLong,         // more than kMaxLength bytes
  EmptySegment,    // "//" inside the path
  DotSegment,      // a name of dots only: ".", "..", "..."
  BadCharacter,    // anything outside letters, digits and . _ -
  SegmentTooLong,  // a name longer than kMaxSegment
  Reserved,        // a name ending in kTempSuffix
};

Result check(const char* path);
inline bool valid(const char* path) { return check(path) == Result::Ok; }

// True for paths below "/config/" (case-insensitive).
bool inConfigDir(const char* path);

// True when `s` ends with `suffix` (case-insensitive).
bool endsWith(const char* s, const char* suffix);

}  // namespace safepath

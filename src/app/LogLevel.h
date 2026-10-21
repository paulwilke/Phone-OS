#pragma once

// How much of the log is written to the SD card. After the development phase the
// card should be written less to make it last, but some lines must never be
// dropped: that somebody queried the device, or sent an emergency, has to be
// provable afterwards, and no log setting may switch that off.
//
// Three levels, chosen by the parents on the web page:
//   All     everything, including modem wake-ups, GPS fixes and radio traffic
//   Events  calls, messages, SOS, remote queries, quiet hours, restarts, errors
//   Errors  only errors, and what is always kept
//
// A line carries no explicit category at the call site; its importance follows
// from the words in it (the vocabulary is in docs/core/log-level.md). Matching is
// case-insensitive and a keyword must start a word, so "reasons" is not "sos".

#include <cstdint>

namespace loglevel {

enum class Level : uint8_t { All, Events, Errors };

// Ordered by importance: a lower value is more important.
enum class Importance : uint8_t {
  Always,  // never dropped
  Error,
  Event,
  Detail,
};

// Importance of a log line. When a line fits several classes the most important
// one wins: "remote query failed" is Always, not Error. A null or empty line is
// a Detail.
Importance classify(const char* line);

bool shouldWrite(Level level, Importance importance);
inline bool shouldWrite(Level level, const char* line) { return shouldWrite(level, classify(line)); }

// Reads the level from the settings file. Anything unknown, including a different
// spelling of a known name, selects All: writing too much is the safe mistake.
Level parse(const char* text);

}  // namespace loglevel

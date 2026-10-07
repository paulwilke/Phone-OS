#include "LogLevel.h"

#include <cstring>

namespace loglevel {
namespace {

// Lines that must stay on every level. "web:" marks actions on the parents' page; "wi-fi: link" is the explanation of a link that
// dropped, which is only worth anything if it is still there when somebody looks.
const char* const kAlways[] = {"command", "remote query", "sos",      "quiet hours", "restart",
                               "firmware", "shutdown",     "config",   "web:",
                               "not in the contact book", "number withheld",
                               "not the configured radio peer", "wi-fi: link"};

const char* const kError[] = {"failed", "rejected",        "not found", "not responding", "error",
                              "not established", "crash", "blocked",   "cannot"};

const char* const kEvent[] = {"call", "sent",  "received", "power saving", "wifi",
                              "cellular", "message", "mesh running", "sim:"};

char lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c; }
bool isAlnum(char c) { return (c >= '0' && c <= '9') || (lower(c) >= 'a' && lower(c) <= 'z'); }

// True when `keyword` (lower case) occurs in `line`, starting at the beginning of
// a word.
bool hasKeyword(const char* line, const char* keyword) {
  const size_t len = strlen(keyword);
  for (const char* p = line; *p; ++p) {
    if (p > line && isAlnum(p[-1])) continue;
    size_t i = 0;
    while (i < len && p[i] && lower(p[i]) == keyword[i]) ++i;
    if (i == len) return true;
  }
  return false;
}

template <size_t N>
bool hasAny(const char* line, const char* const (&keywords)[N]) {
  for (const char* k : keywords) {
    if (hasKeyword(line, k)) return true;
  }
  return false;
}

}  // namespace

Importance classify(const char* line) {
  if (!line) return Importance::Detail;
  if (hasAny(line, kAlways)) return Importance::Always;
  if (hasAny(line, kError)) return Importance::Error;
  if (hasAny(line, kEvent)) return Importance::Event;
  return Importance::Detail;
}

bool shouldWrite(Level level, Importance importance) {
  if (importance == Importance::Always) return true;
  switch (level) {
    case Level::All: return true;
    case Level::Events: return importance != Importance::Detail;
    case Level::Errors: return importance == Importance::Error;
  }
  return true;
}

Level parse(const char* text) {
  if (text && strcmp(text, "errors") == 0) return Level::Errors;
  if (text && strcmp(text, "events") == 0) return Level::Events;
  return Level::All;
}

}  // namespace loglevel

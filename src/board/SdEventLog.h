#pragma once

// The EventLog on the SD card: /system.log, one line for every call, each stamped with the time the clock knew.
// The log cannot be switched off, and a line that must be provable (a remote query, an SOS) must not be lost
// because the card was busy or missing, so:
//
//   * every line is first handed to the console (the mirror), so that something can be read at the desk
//     without a card;
//   * a line that cannot be written (no card, a full card, a card that fails) waits in memory, up to 64
//     lines, and is written, in order, as soon as the card allows; when more than 64 had to be thrown away, one
//     line says how many;
//   * the log does not eat the card: below 32 KB free the lines wait instead of taking the last room that a
//     configuration write needs, and at 256 KB the file is renamed to /system.old.log (replacing the older
//     one) and a new one begins, so the two together never exceed 512 KB;
//   * a line is one record: line breaks inside it become '|'.
//
// A line is "<epoch> <HH:MM> <text>" with the local time of day, or "- --:-- <text>" while the time is not
// known: never 1970.

#include <cstdint>
#include <deque>
#include <string>
#include <string_view>

#include "../hal/Clock.h"
#include "../hal/EventLog.h"
#include "SdCard.h"

namespace board {

constexpr const char* kSystemLogPath = "/system.log";
constexpr const char* kSystemLogOldPath = "/system.old.log";
constexpr size_t kLogPendingMax = 64;
constexpr int64_t kLogRotateBytes = 256 * 1024;

class SdEventLog : public EventLog {
public:
  SdEventLog(SdCard& card, Clock& clock, EventLog* mirror = nullptr) : card_(card), clock_(clock), mirror_(mirror) {}

  void log(std::string_view line) override;
  // Tries to write what is waiting; called by log() and by the main loop when the card came back.
  void flush();
  size_t pending() const { return pending_.size(); }

private:
  std::string stamp(const std::string& text);
  bool writeLine(const std::string& stamped);

  SdCard& card_;
  Clock& clock_;
  EventLog* mirror_;
  std::deque<std::string> pending_;
  uint32_t dropped_ = 0;
  bool lastFailed_ = false;  // the file may end in a piece of a line
};

}  // namespace board

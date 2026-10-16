#pragma once

// The MessageLog on the SD card: /messages.log, appended to and never rewritten, so that a power loss in the
// middle of a write costs at worst the last line. A line that cannot be written (no card, a full card, below
// 32 KB free so that the configuration keeps its room) is reported as false: the store counts it and keeps the
// message on the screen. Reading hands every line to the visitor, oldest first, without its line break; a
// line of more than 511 bytes (damage, never a message) is handed over cut, and so fails the store's parse and
// is counted as damaged instead of vanishing. A block that cannot be read is tried once more before the read
// is reported as stopped early.

#include "../hal/MessageLog.h"
#include "SdCard.h"

namespace board {

constexpr const char* kMessageLogPath = "/messages.log";
constexpr size_t kMessageLineMax = 511;

class SdMessageLog : public MessageLog {
public:
  explicit SdMessageLog(SdCard& card) : card_(card) {}

  bool append(const char* line) override;
  Read readAll(Visitor& visitor) override;

private:
  SdCard& card_;
  bool lastFailed_ = false;  // the file may end in a piece of a line
};

}  // namespace board

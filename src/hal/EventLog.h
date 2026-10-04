#pragma once

// Capability: the log. Calls, sent messages and every remote query end up here. That a
// query was made must always be verifiable, so the log cannot be switched off. On the
// device it is a file on the SD card, in the simulator the terminal.
//
// The level of detail (see LogLevel) is applied by whoever calls: the caller asks
// `loglevel::shouldWrite` and logs only what passes, so a line that must never be
// dropped (a remote query, an SOS) cannot be lost in an implementation. An
// implementation therefore writes every line it is given.
//
// A line is text without a line break and without a time stamp; the implementation
// adds the time when the clock knows it. A line break inside a line is replaced so
// that one call remains one record.

#include <string_view>

class EventLog {
public:
  virtual ~EventLog() = default;
  virtual void log(std::string_view line) = 0;
};

#pragma once

// A log that remembers what it was given.
//
// Reading back: entries() (each with the epoch the clock knew, 0 when unknown) and
// lines(). Like the real log it keeps one record per call and replaces line breaks.

#include <string>
#include <string_view>
#include <vector>

#include <hal/Clock.h>
#include <hal/EventLog.h>

class FakeEventLog : public EventLog {
public:
  struct Entry {
    uint32_t epoch;
    std::string line;
  };

  explicit FakeEventLog(Clock* clock = nullptr) : clock_(clock) {}

  void log(std::string_view line) override {
    std::string text(line);
    for (char& c : text) {
      if (c == '\n' || c == '\r') c = '|';
    }
    entries_.push_back({clock_ ? clock_->epochUtc() : 0, std::move(text)});
  }

  const std::vector<Entry>& entries() const { return entries_; }
  std::vector<std::string> lines() const {
    std::vector<std::string> out;
    for (const Entry& e : entries_) out.push_back(e.line);
    return out;
  }
  bool contains(const std::string& part) const {
    for (const Entry& e : entries_) {
      if (e.line.find(part) != std::string::npos) return true;
    }
    return false;
  }

private:
  Clock* clock_;
  std::vector<Entry> entries_;
};

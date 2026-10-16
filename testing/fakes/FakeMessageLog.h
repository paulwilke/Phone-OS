#pragma once

// A message log in memory: the lines, in order. failNextAppend() and failAllAppends() make an append report false
// and write nothing. stopReadingAfter(n) makes every read hand over the first n lines and report that it stopped
// early, like a card whose block could not be read (n < 0: read to the end again). It does not exist until the
// first line is appended, like a file; lines() reads it back without going through readAll().

#include <string>
#include <vector>

#include <hal/MessageLog.h>

class FakeMessageLog : public MessageLog {
public:
  bool append(const char* line) override {
    if (failAll_ || failNext_) {
      failNext_ = false;
      return false;
    }
    std::string text(line);
    for (char& c : text)
      if (c == '\n' || c == '\r') c = '|';
    lines_.push_back(text);
    exists_ = true;
    return true;
  }
  Read readAll(Visitor& visitor) override {
    if (!exists_) return Read::Missing;
    for (size_t i = 0; i < lines_.size(); ++i) {
      if (stopAfter_ >= 0 && i >= (size_t)stopAfter_) return Read::StoppedEarly;
      if (!lines_[i].empty()) visitor.line(lines_[i].c_str());
    }
    return Read::Complete;
  }

  void failNextAppend() { failNext_ = true; }
  void failAllAppends(bool fail) { failAll_ = fail; }
  void stopReadingAfter(int lines) { stopAfter_ = lines; }
  const std::vector<std::string>& lines() const { return lines_; }

private:
  std::vector<std::string> lines_;
  bool exists_ = false;
  bool failNext_ = false;
  bool failAll_ = false;
  int stopAfter_ = -1;
};

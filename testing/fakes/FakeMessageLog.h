#pragma once

// A message log in memory: the lines, in order. failNextAppend() and failAllAppends() make an append report false
// and write nothing. It does not exist until the first line is appended, like a file.

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
  bool readAll(Visitor& visitor) override {
    if (!exists_) return false;
    for (const std::string& l : lines_)
      if (!l.empty()) visitor.line(l.c_str());
    return true;
  }

  void failNextAppend() { failNext_ = true; }
  void failAllAppends(bool fail) { failAll_ = fail; }

private:
  std::vector<std::string> lines_;
  bool exists_ = false;
  bool failNext_ = false;
  bool failAll_ = false;
};

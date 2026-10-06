#pragma once

// A serial port with a scripted line: bytes the code will receive (feed), bytes it sent (sent()), and a port
// that accepts only part of a write (limitWritesTo) for the code that has to cope with it.

#include <cstdint>
#include <deque>
#include <string>

#include <board/UartPort.h>

class FakeUartPort : public UartPort {
public:
  size_t write(const uint8_t* data, size_t len) override {
    const size_t n = len < limit_ ? len : limit_;
    sent_.append((const char*)data, n);
    return n;
  }
  int read() override {
    if (in_.empty()) return -1;
    const uint8_t b = in_.front();
    in_.pop_front();
    return b;
  }

  void feed(const std::string& bytes) {
    for (char c : bytes) in_.push_back((uint8_t)c);
  }
  void limitWritesTo(size_t n) { limit_ = n; }
  const std::string& sent() const { return sent_; }
  size_t pending() const { return in_.size(); }

private:
  std::deque<uint8_t> in_;
  std::string sent_;
  size_t limit_ = (size_t)-1;
};

#pragma once

// A millisecond counter the test sets and advances, 32 bit like the processor's: setTo() starts it anywhere
// (near the wrap, to test it), advance() moves it and wraps.

#include <board/MillisSource.h>

class FakeMillis : public MillisSource {
public:
  uint32_t millis32() override { return ms_; }
  void setTo(uint32_t ms) { ms_ = ms; }
  void advance(uint32_t ms) { ms_ += ms; }

private:
  uint32_t ms_ = 0;
};

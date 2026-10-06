#pragma once

// A delay that does not wait: it adds up. A start-up sequence runs in no time and the test reads how long it
// would have held a line (elapsedMs()). The other board fakes take a FakeDelay to stamp what they record.

#include <board/Delay.h>

class FakeDelay : public Delay {
public:
  void delayMs(uint32_t ms) override { elapsedUs_ += (uint64_t)ms * 1000; }
  void delayUs(uint32_t us) override { elapsedUs_ += us; }
  uint32_t elapsedMs() const { return (uint32_t)(elapsedUs_ / 1000); }
  uint64_t elapsedUs() const { return elapsedUs_; }

private:
  uint64_t elapsedUs_ = 0;
};

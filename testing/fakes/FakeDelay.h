#pragma once

// A delay that does not wait: it adds up. A start-up sequence runs in no time and the test reads how long it
// would have held a line (elapsedMs()). The other board fakes take a FakeDelay to stamp what they record.

#include <board/Delay.h>

class FakeDelay : public Delay {
public:
  void delayMs(uint32_t ms) override { elapsed_ += ms; }
  uint32_t elapsedMs() const { return elapsed_; }

private:
  uint32_t elapsed_ = 0;
};

#include "BoardGnss.h"

namespace board {

bool BoardGnss::readFix(GnssFix& out) {
  if (!power_.isEnabled(PowerDomain::Positioning)) {
    wasOn_ = false;
    return false;
  }
  const uint64_t now = clock_.uptimeMillis();
  const uint32_t ons = power_.switchOns(PowerDomain::Positioning);
  if (!wasOn_ || ons != seenSwitchOns_) {
    // The first switch-on of the run has nothing before it: whatever waits at the UART is its own. Any later
    // one, seen or not, may have left sentences of the earlier run behind.
    const bool earlierRun = ons != seenSwitchOns_ && !(seenSwitchOns_ == 0 && ons == 1);
    if (earlierRun) {
      parser_.reset();
      while (uart_.read() >= 0) {
      }
    }
    wasOn_ = true;
    seenSwitchOns_ = ons;
    seenSentences_ = parser_.positionSentences();
    lastSentenceAtMs_ = now;  // the receiver is given the stale time to say something after it was switched on
  }

  for (int i = 0; i < kMaxBytesPerRead; ++i) {
    const int c = uart_.read();
    if (c < 0) break;
    parser_.feed((char)c);
  }
  if (parser_.positionSentences() != seenSentences_) {
    seenSentences_ = parser_.positionSentences();
    lastSentenceAtMs_ = now;
  }

  const bool stale = now - lastSentenceAtMs_ > kFixStaleMs;
  out = stale ? GnssFix() : parser_.fix();
  return true;
}

}  // namespace board

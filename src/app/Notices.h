#pragma once

// The notice line: what the device tells the user for a few seconds under the status bar.
// There is at most one; a newer notice replaces the older, because the newest is what the
// child needs to know ("Message is out" replaces "Message waits for network"). A notice is a
// code (Notice.h), never a sentence; it expires on its own after kSeconds of uptime.

#include <cstdint>
#include <string>

#include "../hal/Clock.h"
#include "Notice.h"

class Notices {
public:
  static constexpr uint32_t kSeconds = 8;

  explicit Notices(Clock& clock) : clock_(clock) {}

  void set(NoticeKind kind, const std::string& peer = std::string(), int minute = -1);
  // The current notice; none() once it has expired.
  const Notice& current();
  void clear() { notice_ = Notice(); }

private:
  Clock& clock_;
  Notice notice_;
  uint32_t until_ = 0;
};

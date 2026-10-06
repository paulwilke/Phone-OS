#pragma once

// The rig that runs the Clock contract against the device's clock on a scripted millisecond counter.

#include <board/DeviceClock.h>
#include <fakes/FakeMillis.h>

struct DeviceClockRig {
  FakeMillis millis;
  board::DeviceClock impl{millis};
  Clock& clock() { return impl; }
  void advance(uint32_t ms) { millis.advance(ms); }
};

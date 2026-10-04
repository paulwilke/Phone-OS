#pragma once

// Capability: satellite positioning. Read only; power comes through Power
// (PowerDomain::Positioning). What chip sits underneath, the software does not know.

#include "GnssFix.h"

class GnssReceiver {
public:
  virtual ~GnssReceiver() = default;

  // The current state.
  //   false                the receiver is switched off; `out` is left alone
  //   true, !out.valid     it runs but has no position yet
  //   true, out.valid      a position
  // **After the receiver has been switched on again, a position from the earlier run
  // must not be returned**: the device would report a fix from yesterday as if it
  // were current.
  virtual bool readFix(GnssFix& out) = 0;
};

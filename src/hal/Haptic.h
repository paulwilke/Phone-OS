#pragma once

// Capability: vibration. The device does not ring with a sound but vibrates: a child
// usually has it in a pocket, and a ring tone in the middle of a lesson is what nobody
// wants. A pulse is short (about 100 ms); the pattern — how often, how long — is
// decided by the application (HapticPattern.h), not by the driver.

#include "HapticPattern.h"

class Haptic {
public:
  virtual ~Haptic() = default;

  // Starts a pattern and returns at once; the pulses follow at the rhythm of the
  // pattern, fired by service(). A pattern started while another is playing replaces
  // it (the newest is what the child needs to know); the pulses of the old one that
  // are still to come are not fired. Costs power: the application calls sparingly.
  virtual void play(HapticPattern pattern) = 0;

  // Fires the pulses that are due. Called from the main loop, often.
  virtual void service() = 0;

  // Is something still to come? True from play() until the last pulse has been fired
  // by service(). While true, the processor must not sleep.
  virtual bool busy() const = 0;
};

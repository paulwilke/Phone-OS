#pragma once

// Is the device registered in a mobile network? One question asked in several places (routing
// a message, dialling, the power decision), so one definition.

#include "../hal/Modem.h"
#include "../hal/Power.h"

namespace cellular {

// Registered means: the modem is powered, in a state that is past the search for a network, and
// has named an operator. A modem that is still starting, searching or in error is not.
inline bool registered(Power& power, const Modem& modem) {
  if (!power.isEnabled(PowerDomain::Cellular)) return false;
  const ModemState s = modem.state();
  return s != ModemState::Off && s != ModemState::Registering && s != ModemState::Error &&
         s != ModemState::Initializing && s != ModemState::PoweringOn && !modem.operatorName().empty();
}

}  // namespace cellular

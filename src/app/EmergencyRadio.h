#pragma once

// The radio's emergency (D-59, D-62). In normal use the long-range radio does not run. It becomes active when
// the SOS is used while there is no mobile network, and it stays active until the SOS is ended on the device;
// then it stops.
//
// While it is active:
//   * the radio is started (Connectivity's start request; starting the software is the host's job),
//   * save mode holds: Wi-Fi off, the modem on and searching (Connectivity, D-64),
//   * Messaging accepts a radio message from anybody and shows it; outside the emergency only `[radio] peer`
//     may write.
//
// The SOS itself sends nothing on the radio (D-61): the radio is for a conversation, and Messaging keeps the
// SOS text off it. The text goes by SMS as soon as the modem is registered, while the radio keeps running.
//
// With `features.radio` off nothing of this happens.

#include "Connectivity.h"
#include "Logger.h"
#include "Profile.h"

class EmergencyRadio {
public:
  EmergencyRadio(const Profile& profile, Connectivity& connectivity, Logger& log)
      : profile_(profile), connectivity_(connectivity), log_(log) {}

  // An emergency or a pickup was started (not a reply to a parent's query). `registered` says whether the
  // device is registered in a mobile network at that moment; only without a network the emergency begins.
  void sosStarted(bool registered);
  // The SOS is ended on the device. False when no emergency was on.
  bool end();
  // The host could not start the radio (the chip does not answer): the emergency ends, so that the device
  // does not sit in save mode without a radio.
  void startFailed();

  bool active() const { return active_; }

private:
  const Profile& profile_;
  Connectivity& connectivity_;
  Logger& log_;
  bool active_ = false;
};

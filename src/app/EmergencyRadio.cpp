#include "EmergencyRadio.h"

void EmergencyRadio::sosStarted(bool registered) {
  if (!profile_.radioAllowed || registered || active_) return;
  active_ = true;
  log_.log("Emergency radio on: the SOS was used without a mobile network");
  connectivity_.enterSaveMode();
  connectivity_.requestRadioStart();
}

bool EmergencyRadio::end() {
  if (!active_) return false;
  active_ = false;
  connectivity_.leaveSaveMode();
  connectivity_.requestRadioStop();
  log_.log("Emergency radio off: the SOS was ended on the device");
  return true;
}

void EmergencyRadio::startFailed() {
  if (!active_) return;
  active_ = false;
  connectivity_.leaveSaveMode();
  log_.log("Emergency radio off: the radio did not start");
}

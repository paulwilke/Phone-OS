// Runs the contract suites of testing/contracts/ against the test doubles. When the
// device or the simulator gets an implementation of a capability, its rig is added
// here (or in a test of its own) with the same one line per suite.

#include <unity.h>

#include <contracts/ClockContract.h>
#include <contracts/ConfigStoreContract.h>
#include <contracts/EventLogContract.h>
#include <contracts/GnssContract.h>
#include <contracts/HapticContract.h>
#include <contracts/ModemContract.h>
#include <contracts/MotionSensorContract.h>
#include <contracts/PowerContract.h>
#include <contracts/RadioLinkContract.h>
#include <contracts/TcpServerContract.h>
#include <contracts/WifiContract.h>

#include <BoardPowerRig.h>

#include "DriverRig.h"
#include "Rigs.h"

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_CLOCK_CONTRACT(FakeClockRig);
  RUN_POWER_CONTRACT(FakePowerRig);
  RUN_POWER_CONTRACT(BoardPowerRig);
  RUN_GNSS_CONTRACT(FakeGnssRig);
  RUN_MODEM_CONTRACT(FakeModemRig);
  RUN_MODEM_CONTRACT(DriverModemRig);
  RUN_EVENT_LOG_CONTRACT(FakeEventLogRig);
  RUN_HAPTIC_CONTRACT(FakeHapticRig);
  RUN_CONFIG_STORE_CONTRACT(FakeConfigStoreRig);
  RUN_WIFI_CONTRACT(FakeWifiRig);
  RUN_TCP_SERVER_CONTRACT(FakeTcpServerRig);
  RUN_MOTION_SENSOR_CONTRACT(FakeMotionRig);
  RUN_RADIO_LINK_CONTRACT(FakeRadioLinkRig);
  return UNITY_END();
}

// Runs the contract suites of testing/contracts/ against the test doubles. When the
// device or the simulator gets an implementation of a capability, its rig is added
// here (or in a test of its own) with the same one line per suite.

#include <unity.h>

#include <contracts/ClockContract.h>
#include <contracts/ConfigStoreContract.h>
#include <contracts/EventLogContract.h>
#include <contracts/GnssContract.h>
#include <contracts/HapticContract.h>
#include <contracts/MessageLogContract.h>
#include <contracts/ModemContract.h>
#include <contracts/MotionSensorContract.h>
#include <contracts/PowerContract.h>
#include <contracts/RadioLinkContract.h>
#include <contracts/TcpServerContract.h>
#include <contracts/WifiContract.h>

#include <BoardClockRig.h>
#include <BoardInputRigs.h>
#include <BoardNetworkRigs.h>
#include <BoardPowerRig.h>
#include <BoardStorageRigs.h>

#include "DriverRig.h"
#include "Rigs.h"

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_CLOCK_CONTRACT(FakeClockRig);
  RUN_CLOCK_CONTRACT(DeviceClockRig);
  RUN_POWER_CONTRACT(FakePowerRig);
  RUN_POWER_CONTRACT(BoardPowerRig);
  RUN_GNSS_CONTRACT(FakeGnssRig);
  RUN_GNSS_CONTRACT(BoardGnssRig);
  RUN_MODEM_CONTRACT(FakeModemRig);
  RUN_MODEM_CONTRACT(DriverModemRig);
  RUN_EVENT_LOG_CONTRACT(FakeEventLogRig);
  RUN_EVENT_LOG_CONTRACT(SdEventLogRig);
  RUN_HAPTIC_CONTRACT(FakeHapticRig);
  RUN_HAPTIC_CONTRACT(BoardHapticRig);
  RUN_CONFIG_STORE_CONTRACT(FakeConfigStoreRig);
  RUN_CONFIG_STORE_CONTRACT(SdConfigStoreRig);
  RUN_MESSAGE_LOG_CONTRACT(FakeMessageLogRig);
  RUN_MESSAGE_LOG_CONTRACT(SdMessageLogRig);
  RUN_WIFI_CONTRACT(FakeWifiRig);
  RUN_WIFI_CONTRACT(BoardWifiRig);
  RUN_TCP_SERVER_CONTRACT(FakeTcpServerRig);
  RUN_TCP_SERVER_CONTRACT(BoardTcpServerRig);
  RUN_MOTION_SENSOR_CONTRACT(FakeMotionRig);
  RUN_MOTION_SENSOR_CONTRACT(BoardMotionRig);
  RUN_RADIO_LINK_CONTRACT(FakeRadioLinkRig);
  return UNITY_END();
}

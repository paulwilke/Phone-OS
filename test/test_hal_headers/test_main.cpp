// The capability interfaces are pure: each header compiles on its own (the
// include_*.cpp files in this directory), each interface is abstract, can be deleted
// through a base pointer, and cannot be copied by accident.

#include <unity.h>

#include <type_traits>

#include <hal/Clock.h>
#include <hal/ConfigStore.h>
#include <hal/EventLog.h>
#include <hal/GnssReceiver.h>
#include <hal/Haptic.h>
#include <hal/Modem.h>
#include <hal/MotionSensor.h>
#include <hal/Power.h>
#include <hal/RadioLink.h>
#include <hal/TcpServer.h>
#include <hal/Wifi.h>

namespace {

template <class T>
constexpr bool isInterface() {
  return std::is_abstract<T>::value && std::has_virtual_destructor<T>::value;
}

static_assert(isInterface<Clock>(), "Clock");
static_assert(isInterface<ConfigStore>(), "ConfigStore");
static_assert(isInterface<EventLog>(), "EventLog");
static_assert(isInterface<GnssReceiver>(), "GnssReceiver");
static_assert(isInterface<Haptic>(), "Haptic");
static_assert(isInterface<Modem>(), "Modem");
static_assert(isInterface<MotionSensor>(), "MotionSensor");
static_assert(isInterface<Power>(), "Power");
static_assert(isInterface<RadioLink>(), "RadioLink");
static_assert(isInterface<TcpServer>(), "TcpServer");
static_assert(isInterface<TcpConnection>(), "TcpConnection");
static_assert(isInterface<Wifi>(), "Wifi");

}  // namespace

void setUp() {}
void tearDown() {}

// The constants of the clock contract are what the documentation says.
void test_clock_constants() {
  TEST_ASSERT_EQUAL_UINT32(1577836800u, Clock::kEarliestEpoch);  // 2020-01-01 00:00 UTC
  TEST_ASSERT_EQUAL(-720, Clock::kMinOffsetMinutes);
  TEST_ASSERT_EQUAL(840, Clock::kMaxOffsetMinutes);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_clock_constants);
  return UNITY_END();
}

#include "TimeSync.h"

#include <string>

namespace board {

namespace {
int64_t seconds(uint32_t later, uint32_t earlier) { return (int64_t)later - (int64_t)earlier; }
int64_t absolute(int64_t v) { return v < 0 ? -v : v; }
}  // namespace

void TimeSync::adopt(uint32_t epoch, Source source, const char* why) {
  const uint32_t before = clock_.epochUtc();
  if (!clock_.setEpochUtc(epoch)) return;
  source_ = source;
  if (source == Source::Satellite) satelliteAtMs_ = clock_.uptimeMillis();
  std::string line = std::string("Clock: set from the ") + (source == Source::Satellite ? "satellites" : "network");
  if (before != 0) line += ", " + std::to_string(seconds(epoch, before)) + " s from the time before";
  else line += ", the time was unknown";
  line += std::string(" (") + why + ")";
  log_.log(line);
}

void TimeSync::offerSatellite(uint32_t epoch) {
  if (epoch == lastSatelliteEpoch_) return;
  lastSatelliteEpoch_ = epoch;
  if (!plausible(epoch)) {
    if (!saidImplausibleSatellite_) log_.log("Clock: a time from the satellites is not plausible and is ignored");
    saidImplausibleSatellite_ = true;
    return;
  }
  const uint32_t now = clock_.epochUtc();
  if (now == 0) {
    adopt(epoch, Source::Satellite, "first time");
    return;
  }
  if (absolute(seconds(epoch, now)) > (int64_t)kSatelliteStepSeconds) adopt(epoch, Source::Satellite, "corrected");
  else if (source_ != Source::Satellite) {
    source_ = Source::Satellite;  // it agrees: the clock now rests on the better witness
    satelliteAtMs_ = clock_.uptimeMillis();
  } else {
    satelliteAtMs_ = clock_.uptimeMillis();
  }
}

void TimeSync::offerNetwork(uint32_t epoch, int tz) {
  if (epoch == lastNetworkEpoch_ && tz == lastNetworkTz_) return;
  lastNetworkEpoch_ = epoch;
  lastNetworkTz_ = tz;
  if (!plausible(epoch)) {
    if (!saidImplausibleNetwork_) log_.log("Clock: a time from the network is not plausible and is ignored");
    saidImplausibleNetwork_ = true;
    return;
  }
  const uint64_t uptime = clock_.uptimeMillis();
  const uint32_t now = clock_.epochUtc();

  if (now == 0) {
    adopt(epoch, Source::Network, "first time");
    clock_.setTzOffsetMinutes(tz);
    return;
  }
  if (source_ == Source::Satellite && uptime - satelliteAtMs_ < kSatellitePrecedenceMs) return;

  const int64_t off = seconds(epoch, now);
  bool take = absolute(off) <= (int64_t)kNetworkAgreeSeconds;
  const char* why = "corrected";
  if (!take && held_) {
    // The earlier suspect, moved on by the time that has passed, is where this reading should be.
    const int64_t expected = (int64_t)heldEpoch_ + (int64_t)((uptime - heldAtMs_) / 1000);
    if (absolute((int64_t)epoch - expected) <= (int64_t)kNetworkAgreeSeconds) {
      take = true;
      why = "confirmed by a second reading";
    }
  }
  if (take) {
    held_ = false;
    if (absolute(off) > (int64_t)kSatelliteStepSeconds) adopt(epoch, Source::Network, why);
    else if (source_ == Source::None) source_ = Source::Network;
    clock_.setTzOffsetMinutes(tz);
    return;
  }
  held_ = true;
  heldEpoch_ = epoch;
  heldAtMs_ = uptime;
  log_.log("Clock: a time from the network is " + std::to_string(off) + " s from the clock and is held back until a second reading agrees");
}

}  // namespace board

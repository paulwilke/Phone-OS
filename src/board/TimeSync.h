#pragma once

// Which time to believe. The device learns the time from two sources that do not always tell the truth: the
// network (the modem's AT+CCLK, which is what the operator says and can be hours off or stale) and the
// satellites (a position's UTC time, exact when the receiver has a fix). TimeSync takes what they offer and
// sets the Clock by these rules:
//
//   1. A time before 2020 or from 2070 on is no time (a modem that never synchronised, a receiver after a
//      week-number rollover) and is ignored.
//   2. The same reading offered again is ignored: the modem hands out its last reading until it reads
//      again, and the receiver sends one position a second.
//   3. While the clock is unknown, any time is taken.
//   4. A time from the satellites corrects the clock whenever it differs by more than 2 seconds, and a
//      clock that was set from them is not overruled by the network for a day.
//   5. A time from the network that is within 30 seconds of the clock is taken (it corrects a drift, a
//      small step back included). One that is further away, ahead or **behind**, is held back as suspect and
//      taken only when the next, different reading of the network agrees with it as much as the time between
//      the two readings predicts. A source that lies once does not move the clock; one that is right twice does.
//   6. The zone offset of the network follows the time: it is taken when the time is taken or agrees.
//
//   7. The zone is in force before the line about a change is written. The log stamps a line with the
//      local time of day, so a line written between "the time is set" and "the zone follows" would show
//      the time of the old zone (UTC on a device that has just started: two hours behind the other lines
//      in summer in Germany). The zone is the network's, or, when the host has a named zone configured
//      (setZoneRule), that one, which has precedence.
//
// Every change of the clock is one line in the log; a time that is refused is said once for each reason.

#include <cstdint>
#include <functional>

#include "../hal/Clock.h"
#include "../hal/EventLog.h"

namespace board {

constexpr uint32_t kSatelliteStepSeconds = 2;
constexpr uint32_t kNetworkAgreeSeconds = 30;
constexpr uint64_t kSatellitePrecedenceMs = 24ull * 3600 * 1000;

class TimeSync {
public:
  enum class Source : uint8_t { None, Network, Satellite };

  TimeSync(Clock& clock, EventLog& log) : clock_(clock), log_(log) {}

  // The offset in force at `epochUtc` when the zone the clock would take is `fallbackMinutes`: the host's named
  // zone if it has one, else the fallback. Without a rule the fallback is used.
  using ZoneRule = std::function<int(uint32_t epochUtc, int fallbackMinutes)>;
  void setZoneRule(ZoneRule rule) { zone_ = std::move(rule); }

  void offerNetwork(uint32_t epochUtc, int tzOffsetMinutes);
  void offerSatellite(uint32_t epochUtc);

  // Where the clock's time came from, or None while it is unknown.
  Source source() const { return source_; }

private:
  bool plausible(uint32_t epoch) const { return epoch >= Clock::kEarliestEpoch && epoch < Clock::kLatestEpoch; }
  void adopt(uint32_t epoch, Source source, const char* why, const int* networkZone = nullptr);
  void applyZone(const int* networkZone);

  Clock& clock_;
  EventLog& log_;
  ZoneRule zone_;
  Source source_ = Source::None;
  uint64_t satelliteAtMs_ = 0;

  uint32_t lastNetworkEpoch_ = 0;
  int lastNetworkTz_ = 0;
  uint32_t lastSatelliteEpoch_ = 0;

  bool held_ = false;
  uint32_t heldEpoch_ = 0;
  uint64_t heldAtMs_ = 0;

  bool saidImplausibleNetwork_ = false;
  bool saidImplausibleSatellite_ = false;
};

}  // namespace board

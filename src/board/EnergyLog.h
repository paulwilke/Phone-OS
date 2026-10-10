#pragma once

// The energy log: /energy.log on the card, one line every ten minutes, so that a battery that ran down fast
// can be explained afterwards. The system log says what happened; only a series of readings says how fast the
// charge went while it happened, and whether the modem and the processor were asleep at the time.
//
// A line is "<epoch> <uptime s> <percent> <mV> <mA> <mAh> <state>":
//
//   epoch    Unix time (UTC), or "-" while the clock does not know it; the uptime restarts with every boot, so
//            on its own it cannot order lines across a restart
//   percent, mV, mA, mAh  the fuel gauge's own readings, **not** the plausibility-checked percentage the
//            screens show: a gauge that is wrong has to be visible here. mA is negative while discharging, mAh is
//            the charge the gauge says is left. "-" for all four when the gauge does not answer.
//   state    "cable" (a supply is attached, nothing sleeps), "modem-sleep" (the power policy lets the modem
//            sleep, and with it the processor) or "awake"
//
// Like the system log it does not eat the card: below 32 KB free nothing is written, and at 64 KB the file is
// renamed to /energy.old.log (replacing the older one), so the two together hold about a month. A line that
// cannot be written is lost, not queued: a reading is only worth something at its own time.

#include <cstdint>
#include <string>

#include "../hal/Clock.h"
#include "FuelGauge.h"
#include "SdCard.h"

namespace board {

constexpr const char* kEnergyLogPath = "/energy.log";
constexpr const char* kEnergyLogOldPath = "/energy.old.log";
constexpr int64_t kEnergyLogRotateBytes = 64 * 1024;
constexpr uint32_t kEnergyIntervalMs = 10 * 60 * 1000;

enum class EnergyState : uint8_t { Awake, ModemSleep, Cable };

class EnergyLog {
public:
  EnergyLog(SdCard& card, FuelGauge& gauge, Clock& clock) : card_(card), gauge_(gauge), clock_(clock) {}

  // True when a line is due: at the first question after the start and then every ten minutes. Cheap, so the
  // main loop may ask on every pass and read the state (which can cost a bus transaction) only when it is.
  bool due(uint32_t nowMs) const { return !started_ || (uint32_t)(nowMs - lastMs_) >= kEnergyIntervalMs; }

  // Reads the gauge and appends one line. False when nothing was written (no card, too little room, a write
  // that failed); the next line is due after the interval all the same.
  bool record(uint32_t nowMs, EnergyState state);

  // The line for the given readings, without the line end; public so that the format is pinned by a test.
  static std::string format(uint32_t epoch, uint32_t uptimeSeconds, const GaugeReading* gauge, EnergyState state);

private:
  SdCard& card_;
  FuelGauge& gauge_;
  Clock& clock_;
  bool started_ = false;
  uint32_t lastMs_ = 0;
};

}  // namespace board

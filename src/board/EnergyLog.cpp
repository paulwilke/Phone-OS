#include "EnergyLog.h"

#include <cstdio>

namespace board {

namespace {
const char* word(EnergyState state) {
  switch (state) {
    case EnergyState::Cable: return "cable";
    case EnergyState::ModemSleep: return "modem-sleep";
    case EnergyState::Awake: break;
  }
  return "awake";
}
}  // namespace

std::string EnergyLog::format(uint32_t epoch, uint32_t uptimeSeconds, const GaugeReading* gauge, EnergyState state) {
  char head[24];
  if (epoch == 0) std::snprintf(head, sizeof(head), "- %u", (unsigned)uptimeSeconds);
  else std::snprintf(head, sizeof(head), "%u %u", (unsigned)epoch, (unsigned)uptimeSeconds);
  char readings[64];
  if (gauge) std::snprintf(readings, sizeof(readings), "%d %d %d %d", gauge->percent, gauge->millivolts, gauge->currentMa, gauge->remainingMah);
  else std::snprintf(readings, sizeof(readings), "- - - -");
  return std::string(head) + " " + readings + " " + word(state);
}

bool EnergyLog::record(uint32_t nowMs, EnergyState state) {
  started_ = true;
  lastMs_ = nowMs;
  if (!card_.ready()) return false;
  SdVolume& v = card_.volume();
  const int64_t free = v.freeBytes();
  if (free >= 0 && free < kLogReserveBytes) return false;  // the configuration needs the last room
  if (v.fileSize(kEnergyLogPath) >= kEnergyLogRotateBytes) {
    v.removeFile(kEnergyLogOldPath);
    v.rename(kEnergyLogPath, kEnergyLogOldPath);
  }
  GaugeReading reading;
  const bool gaugeOk = gauge_.read(reading);
  const std::string line = format(clock_.epochUtc(), clock_.uptimeSeconds(), gaugeOk ? &reading : nullptr, state) + "\n";
  if (!v.appendFile(kEnergyLogPath, reinterpret_cast<const uint8_t*>(line.data()), line.size())) {
    card_.operationFailed();
    return false;
  }
  card_.operationSucceeded();
  return true;
}

}  // namespace board

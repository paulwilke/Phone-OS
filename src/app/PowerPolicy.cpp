#include "PowerPolicy.h"

PowerDecision PowerPolicy::decide(const PowerInputs& in) {
  PowerDecision out;

  // Rule 5 first, because it does not depend on quiet or calls, and because the
  // function returns early below. Placed lower, the radio would lose power in the
  // middle of a call (a test pins this).
  out.radioShouldBePowered = in.radioRunning;

  // Rule 6.
  if (in.externalPower) return out;

  // Rule 1.
  if (in.callActive || in.pendingWork) return out;

  // A last input in the future (the clock jumped) counts as no quiet at all: when in doubt, stay awake.
  const uint32_t idle = in.nowSeconds > in.lastInputSeconds ? in.nowSeconds - in.lastInputSeconds : 0;
  const bool quiet = idle >= kIdleSeconds;

  // Rules 2, 3, 7 and 9. A modem that is switched off is not put to sleep.
  const bool imsWaitOver = in.voiceSettled || in.registeredSeconds >= kImsWaitSeconds;
  out.modemShouldSleep = in.cellularEnabled && in.modemSleepAllowed && quiet && in.modemRegistered && imsWaitOver;

  // Rules 4 and 8. A switched-off modem cannot lose a message, so the processor may sleep.
  const bool modemSilent = out.modemShouldSleep || !in.cellularEnabled;
  if (quiet && !in.displayOn && modemSilent && !in.wifiActive) {
    out.cpuMaySleep = true;
    out.cpuSleepMs = in.nextRenderInMs < kMaxCpuSleepMs ? in.nextRenderInMs : kMaxCpuSleepMs;
    // A redraw that is due already must not turn into a sleep of zero length.
    if (out.cpuSleepMs == 0) out.cpuSleepMs = kMaxCpuSleepMs;
  }
  return out;
}

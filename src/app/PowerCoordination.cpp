#include "PowerCoordination.h"

PowerDecision PowerCoordination::apply(const Needs& needs, bool displayOn, uint32_t nextRenderInMs) {
  PowerInputs in;
  in.nowSeconds = clock_.uptimeSeconds();
  in.lastInputSeconds = lastInput_;
  in.cellularEnabled = power_.isEnabled(PowerDomain::Cellular);
  in.callActive = needs.callActive;
  in.pendingWork = needs.pendingWork;
  in.modemSleepAllowed = profile_.modemSleep;
  in.displayOn = displayOn;
  in.nextRenderInMs = nextRenderInMs;
  in.radioRunning = needs.radioUsable;
  // On the cable saving has no point. The question is whether a supply is connected, not whether
  // it is charging: a full battery is not charged any more, and the device once slept on the cable
  // and took the console with it.
  in.externalPower = power_.externalPower();
  in.wifiActive = needs.wifiActive;
  // A modem without a network must not sleep (rule 7). Ready means registered; the call states
  // presuppose it.
  const ModemState s = modem_.state();
  in.modemRegistered = s == ModemState::Ready || s == ModemState::Dialing || s == ModemState::RingingIn ||
                       s == ModemState::InCall;
  // Rule 9 counts from the registration, and every new one (after a lost network, a restart of the modem,
  // switching it on) starts the wait for the IMS again: the IMS registers after the network, each time.
  if (in.modemRegistered && !wasRegistered_) registeredAt_ = in.nowSeconds;
  wasRegistered_ = in.modemRegistered;
  in.registeredSeconds = in.modemRegistered && in.nowSeconds > registeredAt_ ? in.nowSeconds - registeredAt_ : 0;
  const VoiceStatus voice = modem_.voiceStatus();
  in.voiceSettled = voice == VoiceStatus::Ready || voice == VoiceStatus::SmsOnly;

  const PowerDecision out = PowerPolicy::decide(in);

  // The radio chip follows the need. This is the place that ends the round-the-clock reception on
  // a device that never uses the radio.
  if (out.radioShouldBePowered != power_.isEnabled(PowerDomain::Radio)) {
    if (power_.setEnabled(PowerDomain::Radio, out.radioShouldBePowered))
      log_.log(out.radioShouldBePowered ? "Radio: power on" : "Radio: power off");
  }

  // A modem that is switched off is neither put to sleep nor woken.
  if (!in.cellularEnabled) return out;

  if (out.modemShouldSleep && !modemAsleep_) {
    modem_.sleep();
    modemAsleep_ = true;
    log_.log("Modem sleeps");
  } else if (!out.modemShouldSleep && modemAsleep_) {
    modem_.wake();
    modemAsleep_ = false;
    log_.log("Modem woken");
  }
  return out;
}

bool PowerCoordination::toggleFrontlight() {
  const bool on = !power_.isEnabled(PowerDomain::Frontlight);
  if (!power_.setEnabled(PowerDomain::Frontlight, on)) return false;
  notices_.set(on ? NoticeKind::LightOn : NoticeKind::LightOff);
  return true;
}

void PowerCoordination::notePowerOffFailed() {
  powerOffRequested_ = false;
  notices_.set(NoticeKind::PowerOffRefused);
  log_.log("Shutdown refused: a cable feeds the board");
}

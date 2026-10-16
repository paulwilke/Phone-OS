#pragma once

// What may sleep when. Pure decision logic, so that it can be tested without hardware;
// putting things to sleep is the job of the code that calls it.
//
// The trade-off: a modem that stays registered costs about a day of battery life, a
// modem that is switched off costs reachability. The compromise: the modem stays
// registered but sleeps between events. It is woken by the ring-indicator line, that
// is by an incoming call or SMS, or by us when there is something to do.
//
// The rules. Rule 5 is answered first and independently of the others (see below);
// the rest are applied in the order of the code:
//   6. On external power nothing sleeps. Saving only matters on battery; on the cable
//      sleep would cost only reachability and the USB console, because in light sleep
//      the ESP32-S3 switches its USB bridge off and the connection to the computer
//      drops (seen on the device).
//   1. During a call, or while something waits to be sent, nothing sleeps.
//   2. If the settings do not allow modem sleep, the modem stays awake.
//   3. After kIdleSeconds without input the modem may sleep.
//   7. A modem without a network must not sleep: asleep it no longer answers AT
//      commands and registration never happens. That is exactly what happened on the
//      device: 60 s after start it went to sleep in the middle of the operator search.
//   9. A registered modem stays awake until its IMS is registered or the network carries
//      SMS only, and at most kImsWaitSeconds after the registration. Asleep it is not asked
//      about the IMS again: on the device (4.2.5) it went to sleep 14 s after registering,
//      before the IMS, and the screens said "connecting" and a call was refused for 40
//      minutes, until something woke it. The bound keeps a network without VoLTE from
//      keeping the modem awake for good.
//   4. The processor may sleep only with the display off, after the idle time, and
//      with the modem asleep or switched off, and only until the next render is due.
//      In light sleep the ESP32 stops the clock of the serial port; an awake modem
//      could send a message in that time (a new-SMS notice, say) and it would be lost.
//      A sleeping modem wakes us through the ring line before it sends anything.
//   8. While WLAN runs the processor does not sleep: the connection drops in light
//      sleep and the web server stops answering. WLAN therefore runs only on request
//      and switches itself off.
//   5. The radio chip is powered only while somebody uses it. Its boot state would
//      power it, and with no radio software running nobody reads it: it received
//      around the clock on devices that never use radio.

#include <cstdint>

struct PowerInputs {
  uint32_t nowSeconds = 0;
  uint32_t lastInputSeconds = 0;  // last key press
  bool callActive = false;
  bool pendingWork = false;       // a queue, a running location search, an open reply
  bool modemSleepAllowed = true;  // the setting
  bool cellularEnabled = true;    // the modem is powered at all
  bool displayOn = true;
  uint32_t nextRenderInMs = 1000;  // until the next planned redraw
  bool radioRunning = false;       // the radio software uses the chip
  bool externalPower = false;
  bool modemRegistered = false;    // registered in the network
  bool voiceSettled = false;       // the IMS is registered, or the network carries SMS only: nothing to wait for
  uint32_t registeredSeconds = 0;  // since the modem registered in the network
  bool wifiActive = false;
};

struct PowerDecision {
  bool modemShouldSleep = false;
  bool cpuMaySleep = false;
  uint32_t cpuSleepMs = 0;  // at most this long, then something is due again
  // Power for the radio chip: true only while the radio software really uses it.
  // Cutting power while it is in use leaves the radio library confused.
  bool radioShouldBePowered = false;
};

class PowerPolicy {
public:
  // Quiet time before the modem may sleep. Shorter saves more but wakes more often.
  static constexpr uint32_t kIdleSeconds = 60;
  // How long a registered modem waits awake for its IMS (rule 9). On the device the IMS came
  // about 30 s after the network; two minutes are four of the driver's 30-second questions,
  // and a network without VoLTE costs two minutes awake per registration, not more.
  static constexpr uint32_t kImsWaitSeconds = 120;
  // Upper limit of one light-sleep stretch: the clock stays accurate enough and
  // incoming events are noticed promptly.
  static constexpr uint32_t kMaxCpuSleepMs = 2000;

  static PowerDecision decide(const PowerInputs& in);
};

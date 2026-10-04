#pragma once

// The escalation in front of the SOS ping: what happens between the child pressing
// SOS and the ping starting. A clock-driven state machine with a small interface to
// the outside, so every transition can be tested without a screen or a receiver.
//
// Pressing SOS starts the location search at once, not after a choice: by the time
// somebody has decided, the receiver is already looking. Then:
//
//   outside school time   a choice opens: *emergency*, *pick me up* or *cancel*.
//                         If nobody chooses, the emergency goes out by itself after
//                         kAutoSeconds. Choosing starts the ping; cancelling stops the
//                         search and sends nothing.
//   in school time        a warning opens instead, with a button that needs
//                         kOverrideTaps taps. The last tap sends the emergency **and
//                         lifts the school-time lock until the end of that school
//                         time**: if the phone were locked afterwards, the child could
//                         not call back, or be called. Fewer taps do nothing, and
//                         there is no automatic emergency here: in school time the
//                         press must be deliberate.
//
// The search is held for as long as the flow is open (startSearch/stopSearch are
// balanced), and handed on to the ping when one starts, which holds the receiver
// itself.

#include <cstdint>
#include <string>

#include "SosPing.h"

class SosFlow {
public:
  // Seconds until the emergency goes out by itself when nobody chooses.
  static constexpr uint32_t kAutoSeconds = 40;
  // Taps needed in school time.
  static constexpr int kOverrideTaps = 5;

  enum class Phase : uint8_t { Idle, Choosing, QuietConfirm };

  struct Host {
    virtual ~Host() = default;
    virtual uint32_t uptimeSeconds() = 0;  // monotonic
    // True while a ping is searching; a new press then changes nothing.
    virtual bool pingActive() = 0;
    // Power the receiver / release it again. Calls are balanced.
    virtual void startSearch() = 0;
    virtual void stopSearch() = 0;
    // Starts the ping to `target`. False when it could not start.
    virtual bool startPing(const std::string& target, SosPing::Kind kind) = 0;
    // Lifts the school-time lock until the end of the current school time. Called
    // when the last tap is made, whether or not the lock is still on: the host knows.
    virtual void liftQuietLock() = 0;
  };

  explicit SosFlow(Host& host) : host_(host) {}

  // The press of the SOS button. `quietHours` says whether school time is running.
  // Returns false when there is nobody to send to (`target` is empty). A press while
  // a ping is running or the flow is open already changes nothing and returns true:
  // the open choice must keep its countdown and the taps they have made.
  bool begin(const std::string& target, bool quietHours);

  // Call regularly. Sends the emergency when the countdown has run out.
  void tick();

  // The choice: Emergency or Pickup. Only while choosing. Returns whether a ping
  // started; the flow closes either way.
  bool choose(SosPing::Kind kind);

  // One tap on the school-time button. Returns true when this tap was the last one
  // and the emergency ping started.
  bool tap();

  // Leaves the choice or the warning (the back key, the cancel button). Nothing is sent.
  void leave();

  Phase phase() const { return phase_; }
  // Seconds until the automatic emergency; 0 outside the choice.
  uint32_t autoLeft();
  // Taps still needed; 0 outside the warning.
  int tapsLeft() const { return phase_ == Phase::QuietConfirm ? tapsLeft_ : 0; }
  const std::string& target() const { return target_; }

private:
  void close();

  Host& host_;
  Phase phase_ = Phase::Idle;
  uint32_t choiceStart_ = 0;
  int tapsLeft_ = 0;
  std::string target_;
};

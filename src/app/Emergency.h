#pragma once

// Everything that finds the device's position and sends it to a person: the SOS button and the
// reply to a parent's location query.
//
// **One search at a time.** The SOS flow (SosFlow: the choice, the countdown, the school-time
// confirmation) and the ping (SosPing: wait for a good fix, send one text) are the proven
// pieces; this component joins them to the rest of the device. A query reply is the same ping
// with a different text and no alarm for the child. Rules that follow:
//
//   * a second query while a search runs is answered "busy" (startLocationReply returns false);
//   * an emergency replaces a running query reply: the child's call for help must not be lost
//     because a parent asked a moment earlier;
//   * pressing SOS while an emergency or pickup ping runs changes nothing.
//
// **The receiver** is held through Positioning with a reason of its own for each use: SosChoice
// while the flow is open, then SosPing (or RemoteQuery for a reply), handed over without a gap.
// The host must call leave() when the SOS screen closes, because in school time the flow has no
// timeout of its own.
//
// **The outcome** of an emergency or pickup is told once by vibration (sent: three short, failed:
// short-long); a query reply never vibrates, also not at its end, and is only logged. A text the
// network refuses after the modem accepted it was already vibrated by Messaging, so it is not
// vibrated twice.
//
// **School time.** The emergency works in school time: pressing opens a warning that needs
// SosFlow::kOverrideTaps taps, and the last tap lifts the lock to the end of the current window.
//
// **Wording.** The component holds no sentence. The text of the SMS is built from the catalogs in
// the profile's SMS language, whatever language the screen shows: the person who reads it is the
// parent, not the child.

#include <cstdint>
#include <string>

#include "../hal/Clock.h"
#include "../hal/Haptic.h"
#include "../i18n/Translator.h"
#include "Logger.h"
#include "Messaging.h"
#include "Positioning.h"
#include "Profile.h"
#include "SchoolTime.h"
#include "SosFlow.h"
#include "SosPing.h"

class Emergency : private SosFlow::Host, private SosPing::Host {
public:
  Emergency(Clock& clock, Positioning& positioning, Messaging& messaging, SchoolTime& school, const Profile& profile,
            i18n::Translator& translator, Haptic& haptic, Logger& log);

  // --- the SOS button ---
  // Opens the choice (or the school-time warning). `target` is the person to reach; empty means
  // the first call target. False when there is nobody to reach.
  bool begin(const std::string& target = std::string());
  bool choose(SosPing::Kind kind) { return flow_.choose(kind); }
  bool tap() { return flow_.tap(); }
  // The screen closes: nothing is sent, the search is released.
  void leave() { flow_.leave(); }

  // --- the reply to a parent's query ---
  // False when a search runs already, or when the reply cannot start.
  bool startLocationReply(const std::string& phone);

  // The modem accepted the text and the network then refused it. True when that was the reply to
  // a query, whose failure the child must not see.
  bool noteSendFailed(const std::string& phone);

  // Call from the main loop.
  void poll();

  // --- what the interface draws ---
  SosFlow::Phase phase() const { return flow_.phase(); }
  uint32_t autoLeft() { return flow_.autoLeft(); }
  int tapsLeft() const { return flow_.tapsLeft(); }
  const SosPing& ping() const { return sos_; }
  // A search for an emergency or pickup is running (a query reply does not count: the child does
  // not see it).
  bool emergencyRunning() const { return sos_.active() && !sos_.isQuery(); }

private:
  // SosFlow::Host
  uint64_t uptimeMillis() override { return clock_.uptimeMillis(); }
  bool pingActive() override { return emergencyRunning(); }
  void startSearch() override { positioning_.want(Positioning::Reason::SosChoice, true); }
  void stopSearch() override { positioning_.want(Positioning::Reason::SosChoice, false); }
  bool startPing(const std::string& target, SosPing::Kind kind) override;
  void liftQuietLock() override { school_.lift(); }

  // SosPing::Host
  bool readFix(GnssFix& out) override { return positioning_.readFix(out); }
  bool sendText(const char* peer, const std::string& text) override { return messaging_.sendText(peer, text); }

  bool start(const std::string& peer, SosPing::Kind kind);
  SosPing::Wording wording();
  void syncReceiver();

  Clock& clock_;
  Positioning& positioning_;
  Messaging& messaging_;
  SchoolTime& school_;
  const Profile& profile_;
  i18n::Translator& translator_;
  Haptic& haptic_;
  Logger& log_;
  SosPing sos_;
  SosFlow flow_;
  SosPing::Stage lastStage_ = SosPing::Stage::Idle;
};

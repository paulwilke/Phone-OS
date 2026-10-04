#pragma once

// The SOS ping: a button that sends the child's position to the emergency contact.
// Pure logic, no hardware, testable on the host.
//
// All the cleverness is in the waiting. A receiver delivers a position within
// seconds, but it is off by hundreds of metres; it becomes useful only when several
// satellites are in view. So:
//
//   1. Search   the receiver is powered and looks for satellites.
//   2. Refine   there is a fix, but we keep waiting until the accuracy is good (HDOP)
//               **and** a minimum time has passed. Of two positions the more accurate
//               wins, not the newer.
//   3. Done     the best position goes out as an SMS.
//
// These are the three steps the screen shows. After `timeoutSeconds` whatever there
// is gets sent: an inaccurate position is better than none, and the text says how
// accurate it is. With no position at all a message goes out anyway ("no GPS"): the
// child pressed, and the parents must hear of it.
//
// **The wording comes from outside.** The module holds no sentence; it fills the
// templates it is given (Wording) from the catalogs. Placeholders:
//   heads  ({emergency, pickup, location} and their -anonymous forms): {name}
//   position, position-no-map: {head} (the head, already filled), {map} (the address,
//                              only in `position`), {coordinates}, {accuracy} (metres)
//   no-gps: {head}, {seconds}

#include <cstddef>
#include <cstdint>
#include <string>

#include "GnssFix.h"

class SosPing {
public:
  enum class Kind : uint8_t {
    Emergency,  // the child asks for help at once
    Pickup,     // the child asks to be picked up (urgent, not an emergency)
    Query       // the reply to a parent's remote location query: no alarm for the child
  };

  struct Config {
    // Do not send before this time even if the accuracy is good enough already:
    // the first fix of a cold start is usually the worst.
    uint32_t minSeconds = 25;
    // After this, send what there is.
    uint32_t timeoutSeconds = 120;
    // From here on a position counts as good. HDOP 2.5 is about 12 metres.
    float goodHdop = 2.5f;
  };

  enum class Stage : uint8_t {
    Idle,
    Search,
    Refine,
    Done,
    Failed  // the message could not be sent, or was rejected after it left
  };

  // The message templates (see the top of the file). All must be set.
  struct Wording {
    std::string emergency, emergencyAnonymous;
    std::string pickup, pickupAnonymous;
    std::string location, locationAnonymous;
    std::string position, positionNoMap;
    std::string noGps;

    bool complete() const {
      return !emergency.empty() && !emergencyAnonymous.empty() && !pickup.empty() && !pickupAnonymous.empty() &&
             !location.empty() && !locationAnonymous.empty() && !position.empty() && !positionNoMap.empty() &&
             !noGps.empty();
    }
  };

  // What the surroundings must provide. Keeps the module free of modem, receiver and clock.
  struct Host {
    virtual ~Host() = default;
    virtual bool readFix(GnssFix& out) = 0;
    // Cellular, else radio, else the queue. False when not even queueing worked.
    virtual bool sendText(const char* peer, const std::string& text) = 0;
  };

  SosPing(Host& host, const Config& cfg) : host_(host), cfg_(cfg) {}

  // The child's name for the text. A setter of its own and not part of Config,
  // because the name can change with the settings file while the ping object lives.
  void setWho(const std::string& who) { who_ = who; }
  // Template of the map address (MapLink.h). Empty: coordinates only.
  void setMapTemplate(const std::string& tmpl) { mapTemplate_ = tmpl; }
  void setWording(const Wording& wording) { wording_ = wording; }

  // Starts a ping to `peer`. False when the peer is empty, the wording is not
  // complete, or a ping is running already. A running *query* reply gives way to an
  // emergency or pickup ping: the child's call for help must not be lost because a
  // parent happened to ask for the position a moment earlier.
  bool start(const char* peer, uint32_t now, Kind kind = Kind::Emergency);
  void cancel();
  // The modem rejected the message after it had been handed over. The ping was not
  // successful, even though it looked that way for a second.
  void noteSendFailed(const char* peer);
  void poll(uint32_t now);

  bool active() const { return stage_ == Stage::Search || stage_ == Stage::Refine; }
  Stage stage() const { return stage_; }
  Kind kind() const { return kind_; }
  bool isQuery() const { return kind_ == Kind::Query; }
  // 0 … 3: how many of the three steps are done (for the progress bar).
  int progress() const;
  // Seconds since the press, for the display.
  uint32_t elapsed(uint32_t now) const { return active() && now >= start_ ? now - start_ : 0; }
  // The best position seen so far (invalid until there is one).
  const GnssFix& best() const { return best_; }
  // The text that was sent or is being built.
  const std::string& text() const { return text_; }
  const std::string& peer() const { return peer_; }

  // Accuracy in metres from the HDOP; the same rule of thumb everywhere so that all
  // replies mean the same.
  static unsigned accuracyMeters(float hdop);

  // Builds the message. `waited` is the number of seconds spent searching (shown
  // when there is no position).
  static std::string buildText(const Wording& wording, const std::string& who, const std::string& mapTemplate,
                               const GnssFix& fix, bool haveFix, unsigned waited, Kind kind);

private:
  void finish(uint32_t waited);

  Host& host_;
  Config cfg_;
  Wording wording_;
  Stage stage_ = Stage::Idle;
  uint32_t start_ = 0;
  std::string peer_;
  GnssFix best_;
  std::string text_;
  std::string who_;
  Kind kind_ = Kind::Emergency;
  std::string mapTemplate_;
};

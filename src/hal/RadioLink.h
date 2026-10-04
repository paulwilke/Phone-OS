#pragma once

// Capability (optional): messaging over a long-range radio network, as a fallback where
// the mobile network does not reach. Narrow on purpose: send, receive, "does it work
// right now". Everything else (contacts with public-key identity, path routing,
// advertisements) stays behind this interface. The application does not know which
// radio software sits underneath.

#include <cstddef>
#include <cstdint>
#include <string>

// A known radio contact. It arises from the advertisements of other devices; we do not
// create contacts.
struct RadioPeer {
  std::string name;
  bool direct = false;    // the path is known (otherwise it goes by detours / flooding)
  uint32_t lastSeen = 0;  // Unix time of the last report, 0 = unknown
};

// The radio's report on a sent message. The network confirms arrival, or the time
// window runs out. Reports come **in the order of sending**, so the application can
// match each to the oldest open message, and there is exactly one per sent message.
// "Unconfirmed" does not mean "lost", only: we do not know.
struct RadioDelivery {
  std::string peer;
  bool confirmed = false;
};

struct RadioMessage {
  std::string peer;  // the name of the radio contact, not a phone number
  std::string text;
  uint32_t timestamp = 0;
};

class RadioLink {
public:
  virtual ~RadioLink() = default;

  // The radio runs and at least one contact is known.
  virtual bool ready() = 0;
  // The contact messages go to when none is named; empty = none.
  virtual std::string defaultPeer() = 0;

  // An empty `peer` means defaultPeer(). False when sending is not possible now, the
  // contact is unknown, or the text is empty; nothing is queued for a delivery report then.
  virtual bool send(const std::string& peer, const std::string& text) = 0;

  // The next received message, oldest first; false when none is waiting.
  virtual bool poll(RadioMessage& out) = 0;

  // Signal strength of the last message in dBm; 0 = unknown.
  virtual int lastRssi() = 0;

  // The next report on a sent message, each exactly once; false when none is waiting.
  virtual bool pollDelivery(RadioDelivery& out) = 0;

  // The known contacts, for the radio screen.
  virtual size_t peerCount() = 0;
  virtual bool peerAt(size_t index, RadioPeer& out) = 0;
};

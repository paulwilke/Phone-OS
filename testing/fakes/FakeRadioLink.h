#pragma once

// A radio network with scripted contacts.
//
// Scripting:
//   setUp(bool)                          the radio runs or not
//   addPeer(name, direct, lastSeen)      a contact appears (as from an advertisement)
//   setDefaultPeer(name)                 the contact messages go to by default
//   deliver(peer, text, timestamp)       a message arrives
//   confirm(bool)                        the radio answers for the OLDEST open message:
//                                        arrived (true) or no confirmation (false);
//                                        false if nothing is open
//   setRssi(dBm)
// Reading back: sent() (peer and text of every accepted message).
// ready() is "up and at least one contact", as the contract says.

#include <deque>
#include <string>
#include <vector>

#include <hal/RadioLink.h>

class FakeRadioLink : public RadioLink {
public:
  struct Sent {
    std::string peer;
    std::string text;
  };

  bool ready() override { return up_ && !peers_.empty(); }
  std::string defaultPeer() override {
    if (!defaultPeer_.empty()) return defaultPeer_;
    return peers_.empty() ? "" : peers_.front().name;
  }

  bool send(const std::string& peer, const std::string& text) override {
    if (!ready() || text.empty()) return false;
    const std::string target = peer.empty() ? defaultPeer() : peer;
    if (!known(target)) return false;
    sent_.push_back({target, text});
    open_.push_back(target);
    return true;
  }
  bool poll(RadioMessage& out) override {
    if (inbox_.empty()) return false;
    out = inbox_.front();
    inbox_.pop_front();
    return true;
  }
  int lastRssi() override { return up_ ? rssi_ : 0; }
  bool pollDelivery(RadioDelivery& out) override {
    if (deliveries_.empty()) return false;
    out = deliveries_.front();
    deliveries_.pop_front();
    return true;
  }
  size_t peerCount() override { return up_ ? peers_.size() : 0; }
  bool peerAt(size_t index, RadioPeer& out) override {
    if (!up_ || index >= peers_.size()) return false;
    out = peers_[index];
    return true;
  }

  // Scripting.
  void setUp(bool up) { up_ = up; }
  void addPeer(const std::string& name, bool direct, uint32_t lastSeen) {
    for (RadioPeer& p : peers_) {
      if (p.name == name) {
        p.direct = direct;
        p.lastSeen = lastSeen;
        return;
      }
    }
    peers_.push_back({name, direct, lastSeen});
  }
  void setDefaultPeer(const std::string& name) { defaultPeer_ = name; }
  void deliver(const std::string& peer, const std::string& text, uint32_t timestamp) {
    inbox_.push_back({peer, text, timestamp});
  }
  bool confirm(bool arrived) {
    if (open_.empty()) return false;
    deliveries_.push_back({open_.front(), arrived});
    open_.pop_front();
    return true;
  }
  void setRssi(int dbm) { rssi_ = dbm; }
  const std::vector<Sent>& sent() const { return sent_; }

private:
  bool known(const std::string& name) const {
    for (const RadioPeer& p : peers_) {
      if (p.name == name) return true;
    }
    return false;
  }

  bool up_ = false;
  int rssi_ = -95;
  std::string defaultPeer_;
  std::vector<RadioPeer> peers_;
  std::deque<RadioMessage> inbox_;
  std::deque<std::string> open_;
  std::deque<RadioDelivery> deliveries_;
  std::vector<Sent> sent_;
};

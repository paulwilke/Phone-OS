// The one file that includes MeshCore (scripts/check_layers.py allows it nowhere else). See MeshCoreRadio.h.
//
// What of MeshCore is used, and why it is enough for a chat node:
//   CustomSX1262 + CustomSX1262Wrapper  the SX1262 on RadioLib and MeshCore's mesh::Radio on top of it
//   BaseChatMesh                         contacts from advertisements, direct and flooded messages, ACKs
//   SimpleMeshTables, StaticPoolPacketManager, ArduinoMillis, StdRNG   the tables, the packet pool, the clocks
// MeshCore's own cipher, contacts and airtime limit are used as they are (D-59).

#include "MeshCoreRadio.h"

#include <Arduino.h>

#include <deque>

#include <board/PinMap.h>

#include "Esp32Spi.h"

#include <helpers/AdvertDataHelpers.h>
#include <helpers/ArduinoHelpers.h>
#include <helpers/BaseChatMesh.h>
#include <helpers/SimpleMeshTables.h>
#include <helpers/StaticPoolPacketManager.h>
#include <helpers/radiolib/CustomSX1262Wrapper.h>

namespace {

constexpr size_t kMaxInbox = 16;     // received messages waiting for poll(); the oldest is dropped beyond this
constexpr size_t kMaxPending = 16;   // sent messages waiting for their report
constexpr uint8_t kPacketPool = 16;  // as MeshCore's companion firmware

// The board has no clock of its own: the epoch comes from the modem or the receiver and is 0 until then.
// MeshCore wants a time that never goes back and is not 0, so until the real one is known it counts the
// seconds from the first date a clock may be set to. A receiver that has the real time later sees a message
// "from the future" at worst; one with a time of 0 would be ignored.
class DeviceTime : public mesh::RTCClock {
public:
  explicit DeviceTime(Clock& clock) : clock_(clock) {}
  uint32_t getCurrentTime() override {
    const uint32_t epoch = clock_.epochUtc();
    return epoch != 0 ? epoch : Clock::kEarliestEpoch + clock_.uptimeSeconds();
  }
  void setCurrentTime(uint32_t) override {}  // the application's Clock is set by its own sources

private:
  Clock& clock_;
};

// What MeshCore asks of a board; nothing of it applies here (no battery reading, sleep or OTA through it).
class DeviceBoard : public mesh::MainBoard {
public:
  uint16_t getBattMilliVolts() override { return 0; }
  const char* getManufacturerName() const override { return "LilyGo T-Deck Pro MAX"; }
  void reboot() override { ESP.restart(); }
  uint8_t getStartupReason() const override { return BD_STARTUP_NORMAL; }
};

struct Pending {
  std::string peer;
  uint32_t ack = 0;
  unsigned long deadline = 0;  // millis
  bool resolved = false;
  bool confirmed = false;
};

class ChatNode : public BaseChatMesh {
public:
  ChatNode(CustomSX1262Wrapper& radio, mesh::RNG& rng, mesh::RTCClock& rtc, SimpleMeshTables& tables)
      : BaseChatMesh(radio, *new ArduinoMillis(), rng, rtc, *new StaticPoolPacketManager(kPacketPool), tables),
        radio_(&radio) {}

  void startWith(const mesh::LocalIdentity& id) { self_id = id; }
  mesh::LocalIdentity& identityNow() { return self_id; }
  mesh::RTCClock* clock() { return getRTCClock(); }
  unsigned long now() { return _ms->getMillis(); }

  std::deque<RadioMessage> inbox;
  std::deque<Pending> pending;
  int rssi = 0;

  void expire() {
    const unsigned long t = now();
    for (Pending& p : pending) {
      if (!p.resolved && static_cast<long>(t - p.deadline) >= 0) {
        p.resolved = true;  // "unconfirmed" is not "lost": we only did not hear an ACK in time
      }
    }
  }

protected:
  // A chat node never repeats what it hears. MeshCore's default is false already; this says it where a change
  // would be noticed, because a repeating child's phone would take the airtime of strangers.
  bool allowPacketForward(const mesh::Packet*) override { return false; }

  // Only chat nodes become contacts: a repeater or a room is not somebody to write to.
  bool shouldAutoAddContactType(uint8_t type) const override { return type == ADV_TYPE_CHAT; }
  // Evict the oldest contact rather than refusing a new one: in an emergency the new stranger is the one in range.
  bool shouldOverwriteWhenFull() const override { return true; }

  void onDiscoveredContact(ContactInfo&, bool, uint8_t, const uint8_t*) override {}
  void onContactPathUpdated(const ContactInfo&) override {}

  ContactInfo* processAck(const uint8_t* data) override {
    uint32_t crc;
    memcpy(&crc, data, sizeof(crc));
    for (Pending& p : pending) {
      if (!p.resolved && p.ack == crc) {
        p.resolved = true;
        p.confirmed = true;
        break;
      }
    }
    return nullptr;  // no connection state to update
  }

  void onMessageRecv(const ContactInfo& from, mesh::Packet*, uint32_t, const char* text) override {
    if (inbox.size() >= kMaxInbox) {
      inbox.pop_front();
    }
    RadioMessage m;
    m.peer = from.name;
    m.text = text;
    m.timestamp = clock()->getCurrentTime();  // ours: the sender's clock may be anything
    inbox.push_back(std::move(m));
    rssi = static_cast<int>(lroundf(radio_->getLastRSSI()));
  }

  // Commands, signed messages, remote CLI, requests and channel messages are not served or shown: the radio is
  // a way to write to a person, not a way to configure or query this phone.
  void onCommandDataRecv(const ContactInfo&, mesh::Packet*, uint32_t, const char*) override {}
  void onCLICommandRecv(const ContactInfo&, mesh::Packet*, uint32_t, const char*, char*) override {}
  void onSignedMessageRecv(const ContactInfo&, mesh::Packet*, uint32_t, const uint8_t*, const char*) override {}
  void onChannelMessageRecv(const mesh::GroupChannel&, mesh::Packet*, uint32_t, const char*) override {}
  uint8_t onContactRequest(const ContactInfo&, uint32_t, const uint8_t*, uint8_t, uint8_t*) override { return 0; }
  void onContactResponse(const ContactInfo&, const uint8_t*, uint8_t) override {}

  // The waiting times of MeshCore's own chat example; the ACK window is ours (Pending::deadline), this
  // only has to answer.
  uint32_t calcFloodTimeoutMillisFor(uint32_t airtime) const override { return 500 + 16 * airtime; }
  uint32_t calcDirectTimeoutMillisFor(uint32_t airtime, uint8_t pathLen) const override {
    return 500 + (airtime * 6 + 250) * ((pathLen & 63) + 1);
  }
  void onSendTimeout() override {}

  // The duty cycle (D-59): 1 / (1 + factor), so 9 is 10 %, the limit of the EU sub-band that LORA_FREQ is in.
  float getAirtimeBudgetFactor() const override { return 9.0f; }
  // A chat node answers at once; the delays MeshCore uses to space out repeaters do not apply.
  int calcRxDelay(float, uint32_t) const override { return 0; }

private:
  CustomSX1262Wrapper* radio_;
};

}  // namespace

struct MeshCoreRadio::Impl {
  explicit Impl(Clock& c)
      : time(c),
        spi(boardSpi()),
        module(board::pin::kRadioCs, board::pin::kRadioDio1, board::pin::kRadioReset, board::pin::kRadioBusy, spi),
        chip(&module),
        driver(chip, mcBoard),
        node(driver, rng, time, tables) {}

  DeviceTime time;
  DeviceBoard mcBoard;
  SPIClass& spi;
  Module module;
  CustomSX1262 chip;
  CustomSX1262Wrapper driver;
  StdRNG rng;
  SimpleMeshTables tables;
  ChatNode node;
  mesh::LocalIdentity stored;
  bool haveStored = false;
  bool up = false;
  std::string defaultName;
  std::string nodeName;

  // Chat contacts, in MeshCore's order.
  template <typename F>
  void eachChat(F f) {
    ContactsIterator it = node.startContactsIterator();
    ContactInfo c;
    while (it.hasNext(&node, c)) {
      if (c.type == ADV_TYPE_CHAT && !f(c)) {
        return;
      }
    }
  }
  bool find(const std::string& name, ContactInfo& out) {
    bool found = false;
    eachChat([&](const ContactInfo& c) {
      if (name == c.name) {
        out = c;
        found = true;
      }
      return !found;
    });
    return found;
  }
};

MeshCoreRadio::MeshCoreRadio(Clock& clock) : impl_(new Impl(clock)) {}
MeshCoreRadio::~MeshCoreRadio() = default;

std::string MeshCoreRadio::identity() const {
  if (!impl_->up) {
    return std::string();
  }
  uint8_t buf[PRV_KEY_SIZE + PUB_KEY_SIZE];
  const size_t n = impl_->node.identityNow().writeTo(buf, sizeof(buf));
  return std::string(reinterpret_cast<const char*>(buf), n);
}

bool MeshCoreRadio::setIdentity(const std::string& blob) {
  if (blob.size() != PRV_KEY_SIZE + PUB_KEY_SIZE) {
    return false;
  }
  mesh::LocalIdentity id;
  id.readFrom(reinterpret_cast<const uint8_t*>(blob.data()), blob.size());
  impl_->stored = id;
  impl_->haveStored = true;
  return true;
}

bool MeshCoreRadio::begin(const std::string& nodeName) {
  Impl& m = *impl_;
  if (!m.chip.std_init(&m.spi)) {
    return false;
  }
  m.rng.begin(m.driver.getRngSeed());
  if (m.haveStored) {
    m.node.startWith(m.stored);
  } else {
    RadioNoiseListener noise(m.chip);  // slow, and only needed once
    mesh::LocalIdentity fresh(&noise);
    int tries = 0;
    while (tries++ < 10 && (fresh.pub_key[0] == 0x00 || fresh.pub_key[0] == 0xFF)) {  // reserved hashes
      fresh = mesh::LocalIdentity(&noise);
    }
    m.node.startWith(fresh);
  }
  m.node.begin();
  m.nodeName = nodeName;
  m.up = true;
  return true;
}

void MeshCoreRadio::loop() {
  Impl& m = *impl_;
  if (!m.up) {
    return;
  }
  m.node.loop();
  m.node.clock()->tick();
  m.node.expire();
}

bool MeshCoreRadio::advertise() {
  Impl& m = *impl_;
  if (!m.up) {
    return false;
  }
  mesh::Packet* pkt = m.node.createSelfAdvert(m.nodeName.c_str());
  if (pkt == nullptr) {
    return false;
  }
  m.node.sendFlood(pkt);
  return true;
}

void MeshCoreRadio::setDefaultPeer(const std::string& name) {
  ContactInfo c;
  impl_->defaultName = impl_->find(name, c) ? name : std::string();
}

bool MeshCoreRadio::ready() {
  Impl& m = *impl_;
  bool any = false;
  if (m.up) {
    m.eachChat([&](const ContactInfo&) {
      any = true;
      return false;
    });
  }
  return any;
}

std::string MeshCoreRadio::defaultPeer() {
  Impl& m = *impl_;
  if (!m.up) {
    return std::string();
  }
  ContactInfo c;
  if (!m.defaultName.empty() && m.find(m.defaultName, c)) {
    return m.defaultName;
  }
  // None chosen: the contact heard most recently, because that is the one known to be in range now.
  std::string best;
  uint32_t seen = 0;
  m.eachChat([&](const ContactInfo& each) {
    if (best.empty() || each.lastmod >= seen) {
      best = each.name;
      seen = each.lastmod;
    }
    return true;
  });
  return best;
}

bool MeshCoreRadio::send(const std::string& peer, const std::string& text) {
  Impl& m = *impl_;
  if (!m.up || text.empty() || text.size() >= MAX_TEXT_LEN || m.node.pending.size() >= kMaxPending) {
    return false;
  }
  const std::string to = peer.empty() ? defaultPeer() : peer;
  ContactInfo c;
  if (to.empty() || !m.find(to, c)) {
    return false;
  }
  uint32_t ack = 0;
  uint32_t window = 0;
  if (m.node.sendMessage(c, m.node.clock()->getCurrentTimeUnique(), 0, text.c_str(), ack, window) == MSG_SEND_FAILED) {
    return false;
  }
  Pending p;
  p.peer = to;
  p.ack = ack;
  p.deadline = m.node.now() + window;
  m.node.pending.push_back(std::move(p));
  return true;
}

bool MeshCoreRadio::poll(RadioMessage& out) {
  auto& inbox = impl_->node.inbox;
  if (inbox.empty()) {
    return false;
  }
  out = std::move(inbox.front());
  inbox.pop_front();
  return true;
}

int MeshCoreRadio::lastRssi() { return impl_->node.rssi; }

bool MeshCoreRadio::pollDelivery(RadioDelivery& out) {
  // In the order of sending: a later message that is already answered waits for the older one.
  auto& pending = impl_->node.pending;
  if (pending.empty() || !pending.front().resolved) {
    return false;
  }
  out.peer = pending.front().peer;
  out.confirmed = pending.front().confirmed;
  pending.pop_front();
  return true;
}

size_t MeshCoreRadio::peerCount() {
  size_t n = 0;
  if (impl_->up) {
    impl_->eachChat([&](const ContactInfo&) {
      ++n;
      return true;
    });
  }
  return n;
}

bool MeshCoreRadio::peerAt(size_t index, RadioPeer& out) {
  size_t i = 0;
  bool found = false;
  if (impl_->up) {
    impl_->eachChat([&](const ContactInfo& c) {
      if (i++ == index) {
        out.name = c.name;
        out.direct = c.out_path_len != OUT_PATH_UNKNOWN;
        out.lastSeen = c.lastmod;
        found = true;
      }
      return !found;
    });
  }
  return found;
}

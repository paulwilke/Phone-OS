#pragma once

// RadioLink on MeshCore (D-59): a chat node (companion), never a repeater. MeshCore is fetched by the
// firmware build and included in MeshCoreRadio.cpp only; this header names nothing of it, so the rest of the
// firmware, and the layer check, never see it.
//
// Driven by device/main.cpp (D-62): created when the application asks for the radio, destroyed when the
// emergency ends. The caller must have switched the radio's rail on (board::leg::kRadioPower) and chosen the
// antenna before begin(), and must call loop() on every pass of its main loop.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include <hal/Clock.h>
#include <hal/RadioLink.h>

class MeshCoreRadio : public RadioLink {
public:
  explicit MeshCoreRadio(Clock& clock);
  ~MeshCoreRadio() override;
  MeshCoreRadio(const MeshCoreRadio&) = delete;
  MeshCoreRadio& operator=(const MeshCoreRadio&) = delete;

  // The key pair of this node as bytes, to be kept by the caller; empty before begin(). Without a stored one,
  // begin() makes a new identity and the node is a stranger to everybody again after a restart.
  std::string identity() const;
  // A stored key pair, before begin(). False for a blob of the wrong size.
  bool setIdentity(const std::string& blob);

  // Starts the SX1262 on the shared SPI bus (EU settings from platformio.ini) and the mesh. False when the
  // chip does not answer; the object then stays not ready and sends nothing.
  bool begin(const std::string& nodeName);

  // Receives, sends from the queue, counts the ACK windows. Cheap when nothing is happening.
  void loop();

  // Announces this node once (an advertisement, flooded, so that nodes in range learn it as a contact).
  bool advertise();

  // The contact messages go to when none is named. An unknown name clears the choice.
  void setDefaultPeer(const std::string& name);

  // RadioLink. Contacts are chat nodes only: repeaters and rooms are not listed.
  bool ready() override;
  std::string defaultPeer() override;
  bool send(const std::string& peer, const std::string& text) override;
  bool poll(RadioMessage& out) override;
  int lastRssi() override;
  bool pollDelivery(RadioDelivery& out) override;
  size_t peerCount() override;
  bool peerAt(size_t index, RadioPeer& out) override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

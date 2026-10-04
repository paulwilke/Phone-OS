#pragma once

// The rigs that run the contract suites against the test doubles. A rig owns one
// implementation and gives the suite the hooks it needs; the suites never name the
// fake. The rig for a new implementation (the device, the simulator) goes in a file
// like this one.

#include <fakes/FakeClock.h>
#include <fakes/FakeConfigStore.h>
#include <fakes/FakeEventLog.h>
#include <fakes/FakeGnss.h>
#include <fakes/FakeHaptic.h>
#include <fakes/FakeModem.h>
#include <fakes/FakeMotion.h>
#include <fakes/FakePower.h>
#include <fakes/FakeRadioLink.h>
#include <fakes/FakeTcpServer.h>
#include <fakes/FakeWifi.h>

struct FakeClockRig {
  FakeClock impl;
  Clock& clock() { return impl; }
  void advance(uint32_t ms) { impl.advance(ms); }
};

struct FakePowerRig {
  FakePower impl;
  Power& power() { return impl; }
  bool breakDomain(PowerDomain d) {
    impl.failDomain(d, true);
    return true;
  }
  bool plugIn(ChargeState s) {
    impl.plugIn(s);
    return true;
  }
  bool unplug() {
    impl.unplug();
    return true;
  }
  bool forbidPowerOff() {
    impl.allowPowerOff(false);
    return true;
  }
};

struct FakeGnssRig {
  FakeClock clockImpl;
  FakePower powerImpl{&clockImpl};
  FakeGnss impl{powerImpl, clockImpl};
  GnssReceiver& gnss() { return impl; }
  Power& power() { return powerImpl; }
  void advanceSeconds(uint32_t s) { clockImpl.advanceSeconds(s); }
  bool scriptFix(double lat, double lon, float hdop, uint32_t afterSeconds) {
    impl.fixAfter(afterSeconds, lat, lon, hdop);
    return true;
  }
  bool scriptNoFix() {
    impl.never();
    return true;
  }
};

struct FakeModemRig {
  FakeClock clockImpl;
  FakeModem impl{clockImpl};
  Modem& modem() { return impl; }
  void advanceSeconds(uint32_t s) { clockImpl.advanceSeconds(s); }
  bool setNetwork(const std::string& op, int bars, bool voice) {
    impl.setNetwork(op, bars, voice);
    return true;
  }
  bool setSim(SimStatus s) {
    impl.setSimStatus(s);
    return true;
  }
  bool injectSms(const std::string& phone, const std::string& body, uint32_t ts) {
    impl.injectSms(phone, body, ts);
    return true;
  }
  bool injectIncomingCall(const std::string& phone) { return impl.injectIncomingCall(phone); }
  bool remoteAnswer() {
    impl.remoteAnswer();
    return true;
  }
  bool remoteBusy() {
    impl.remoteBusy();
    return true;
  }
  bool remoteNoAnswer() {
    impl.remoteNoAnswer();
    return true;
  }
  bool failDial(const std::string& reason) {
    impl.failDial(reason);
    return true;
  }
  bool remoteHangup() {
    impl.remoteHangup();
    return true;
  }
  bool failNextSms(const std::string& reason) {
    impl.failNextSms(reason);
    return true;
  }
  bool setVolumeSupported(bool supported) {
    impl.setVolumeSupported(supported);
    return true;
  }
};

struct FakeEventLogRig {
  FakeEventLog impl;
  EventLog& log() { return impl; }
  std::vector<std::string> records() { return impl.lines(); }
};

struct FakeHapticRig {
  FakeClock clockImpl;
  FakeHaptic impl{clockImpl};
  Haptic& haptic() { return impl; }
  void advance(uint32_t ms) { clockImpl.advance(ms); }
  std::vector<uint8_t> fired() {
    // The fake records times only; the effect numbers come from the real pattern
    // table through the rhythm it played, so a rig for a driver reads them from the driver.
    std::vector<uint8_t> out;
    for (const auto& p : impl.fired()) out.push_back(p.effect);
    return out;
  }
  std::vector<uint64_t> firedAt() {
    std::vector<uint64_t> out;
    for (const auto& p : impl.fired()) out.push_back(p.atMs);
    return out;
  }
};

struct FakeConfigStoreRig {
  FakeConfigStore impl;
  ConfigStore& store() { return impl; }
  bool failNextWrite() {
    impl.failNextWrite();
    return true;
  }
  bool failAllWrites(bool fail) {
    impl.failAllWrites(fail);
    return true;
  }
};

struct FakeWifiRig {
  FakeWifi impl;
  Wifi& wifi() { return impl; }
  bool finishConnecting(bool success) {
    impl.finishConnecting(success);
    return true;
  }
  bool failAccessPoint(bool fails) {
    impl.failAccessPoint(fails);
    return true;
  }
};

struct FakeTcpPeer : TcpPeer {
  explicit FakeTcpPeer(FakeTcpConnection* c) : c_(c) {}
  void send(const std::string& bytes) override { c_->fromPeer(bytes); }
  void close() override { c_->peerClose(); }
  std::string received() override { return c_->sentToPeer(); }
  FakeTcpConnection* c_;
};

struct FakeTcpServerRig {
  FakeTcpServer impl;
  TcpServer& server() { return impl; }
  std::unique_ptr<TcpPeer> connect() {
    FakeTcpConnection* c = impl.connect();
    return c ? std::make_unique<FakeTcpPeer>(c) : nullptr;
  }
};

struct FakeMotionRig {
  FakeMotion impl;
  MotionSensor& sensor() { return impl; }
  bool setAvailable(bool available) {
    impl.setAvailable(available);
    return true;
  }
  bool failNextStart() {
    impl.failNextStart();
    return true;
  }
  bool tilt(float x, float y, float z) {
    impl.tilt(x, y, z);
    return true;
  }
};

struct FakeRadioLinkRig {
  FakeRadioLink impl;
  RadioLink& link() { return impl; }
  bool setUp(bool up) {
    impl.setUp(up);
    return true;
  }
  bool addPeer(const std::string& name, bool direct, uint32_t lastSeen) {
    impl.addPeer(name, direct, lastSeen);
    return true;
  }
  bool deliver(const std::string& peer, const std::string& text, uint32_t ts) {
    impl.deliver(peer, text, ts);
    return true;
  }
  bool confirm(bool arrived) { return impl.confirm(arrived); }
};

#pragma once

// The rigs that run the Wifi and TcpServer contracts against the board classes on a scripted radio and socket.

#include <memory>

#include <board/BoardTcpServer.h>
#include <board/BoardWifi.h>
#include <contracts/TcpServerContract.h>
#include <fakes/FakeTcpListener.h>
#include <fakes/FakeWifiRadio.h>

struct BoardWifiRig {
  FakeWifiRadio radio;
  board::BoardWifi impl{radio};
  Wifi& wifi() { return impl; }
  bool finishConnecting(bool success) {
    radio.finishConnecting(success);
    return true;
  }
  bool failAccessPoint(bool fails) {
    radio.failAccessPoint(fails);
    return true;
  }
};

struct BoardTcpPeer : TcpPeer {
  explicit BoardTcpPeer(FakeTcpClient* c) : c_(c) {}
  void send(const std::string& bytes) override { c_->send(bytes); }
  void close() override { c_->leave(); }
  std::string received() override { return c_->received(); }
  FakeTcpClient* c_;
};

struct BoardTcpServerRig {
  FakeTcpListener listener;
  board::BoardTcpServer impl{listener};
  TcpServer& server() { return impl; }
  std::unique_ptr<TcpPeer> connect() {
    FakeTcpClient* c = listener.connect();
    return c ? std::make_unique<BoardTcpPeer>(c) : nullptr;
  }
};

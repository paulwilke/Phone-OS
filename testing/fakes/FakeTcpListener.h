#pragma once

// A listening socket and its clients, scripted. connect() is a client arriving (null when nobody listens);
// the returned handle sends bytes, goes away, and sees what the software wrote.
// Reading back: listening(), port(), stopped() of a handle (the software closed it).

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include <board/TcpListener.h>

class FakeTcpClient : public board::TcpSocket {
public:
  int available() override { return (int)inbox_.size(); }
  int read(uint8_t* buf, size_t size) override {
    const size_t n = size < inbox_.size() ? size : inbox_.size();
    for (size_t i = 0; i < n; ++i) buf[i] = (uint8_t)inbox_[i];
    inbox_.erase(0, n);
    return (int)n;
  }
  size_t write(const uint8_t* data, size_t len) override {
    if (peerGone_ || stopped_) return 0;
    outbox_.append(reinterpret_cast<const char*>(data), len);
    return len;
  }
  bool connected() override { return !peerGone_ && !stopped_; }
  void stop() override { stopped_ = true; }

  // The client's side.
  void send(const std::string& bytes) { inbox_ += bytes; }
  void leave() { peerGone_ = true; }
  const std::string& received() const { return outbox_; }
  bool stopped() const { return stopped_; }

private:
  std::string inbox_, outbox_;
  bool peerGone_ = false;
  bool stopped_ = false;
};

class FakeTcpListener : public board::TcpListener {
public:
  bool listen(uint16_t port) override {
    listening_ = true;
    port_ = port;
    return true;
  }
  void stopListening() override { listening_ = false; }
  board::TcpSocket* nextClient() override {
    if (!listening_ || waiting_.empty()) return nullptr;
    FakeTcpClient* c = waiting_.front();
    waiting_.pop_front();
    return c;
  }

  FakeTcpClient* connect() {
    if (!listening_) return nullptr;
    all_.push_back(std::make_unique<FakeTcpClient>());
    waiting_.push_back(all_.back().get());
    return all_.back().get();
  }
  bool listening() const { return listening_; }
  uint16_t port() const { return port_; }

private:
  bool listening_ = false;
  uint16_t port_ = 0;
  std::vector<std::unique_ptr<FakeTcpClient>> all_;
  std::deque<FakeTcpClient*> waiting_;
};

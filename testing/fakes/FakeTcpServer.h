#pragma once

// A TCP server whose clients are scripted by the test.
//
// Scripting: connect() makes a client wait to be accepted and returns it (null when the
// server is not listening, as a real connection would be refused). On the returned
// connection the test calls
//   fromPeer(bytes)   the client sends bytes
//   peerClose()       the client goes away
// and reads back sentToPeer() (what the software wrote) and closedBySoftware().
// One connection at a time: accept() replaces the previous one, which is destroyed, as
// the contract says (the pointer is valid until close() or the next accept()).

#include <algorithm>
#include <deque>
#include <memory>
#include <string>

#include <hal/TcpServer.h>

class FakeTcpConnection : public TcpConnection {
public:
  int read(uint8_t* buf, size_t size) override {
    if (closed_) return -1;
    if (inbox_.empty()) return peerGone_ ? -1 : 0;
    const size_t n = std::min(size, inbox_.size());
    for (size_t i = 0; i < n; ++i) buf[i] = (uint8_t)inbox_[i];
    inbox_.erase(0, n);
    return (int)n;
  }
  size_t write(const uint8_t* data, size_t len) override {
    if (closed_ || peerGone_) return 0;
    const size_t n = writeLimit_ ? std::min(len, writeLimit_) : len;
    sent_.append((const char*)data, n);
    return n;
  }
  void close() override { closed_ = true; }

  // Scripting.
  void fromPeer(const std::string& bytes) { inbox_ += bytes; }
  void peerClose() { peerGone_ = true; }
  void limitWritesTo(size_t bytesPerCall) { writeLimit_ = bytesPerCall; }
  const std::string& sentToPeer() const { return sent_; }
  bool closedBySoftware() const { return closed_; }

private:
  std::string inbox_, sent_;
  bool peerGone_ = false;
  bool closed_ = false;
  size_t writeLimit_ = 0;
};

class FakeTcpServer : public TcpServer {
public:
  bool begin(uint16_t port) override {
    if (port == 0) return false;
    stop();
    listening_ = true;
    port_ = port;
    return true;
  }
  void stop() override {
    if (current_) current_->close();
    current_.reset();
    pending_.clear();
    listening_ = false;
  }
  TcpConnection* accept() override {
    if (!listening_ || pending_.empty()) return nullptr;
    current_ = std::move(pending_.front());
    pending_.pop_front();
    return current_.get();
  }

  // Scripting.
  FakeTcpConnection* connect() {
    if (!listening_) return nullptr;
    pending_.push_back(std::make_unique<FakeTcpConnection>());
    return pending_.back().get();
  }
  bool listening() const { return listening_; }
  uint16_t port() const { return port_; }

private:
  bool listening_ = false;
  uint16_t port_ = 0;
  std::deque<std::unique_ptr<FakeTcpConnection>> pending_;
  std::unique_ptr<FakeTcpConnection> current_;
};

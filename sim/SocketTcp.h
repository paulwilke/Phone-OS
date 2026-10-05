#pragma once

// A TcpServer on a real socket of the computer, bound to the loopback address only: the parents' web
// page of the simulator is reachable from this machine and from nowhere else. Not blocking, one
// connection at a time, as the interface says (hal/TcpServer.h).
//
// The web server asks for port 80, which a user may not open; the simulator therefore says which port
// to listen on whatever is asked (Fixed), or asks the system for any free one (Any, for tests that
// must not collide with a simulator that is running). AsAsked opens the port that is asked for.
// POSIX only: the Mac and the Linux runner.

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <memory>
#include <string>

#include <hal/TcpServer.h>

namespace sim {

class SocketConnection : public TcpConnection {
public:
  explicit SocketConnection(int fd) : fd_(fd) {}
  ~SocketConnection() override { close(); }

  int read(uint8_t* buf, size_t size) override {
    if (fd_ < 0) return -1;
    const ssize_t n = ::recv(fd_, buf, size, 0);
    if (n > 0) return static_cast<int>(n);
    if (n == 0) return -1;  // the other side has gone and everything it sent has been read
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR ? 0 : -1;
  }

  size_t write(const uint8_t* data, size_t len) override {
    if (fd_ < 0 || len == 0) return 0;
    // A client that has gone is not told so by the first send (the system buffers it): ask first.
    // A peek that returns 0 is the end of the stream. Clients must therefore not half-close their
    // side before they have read the answer, which no browser does.
    char probe;
    if (::recv(fd_, &probe, 1, MSG_PEEK | MSG_DONTWAIT) == 0) return 0;
    const ssize_t n = ::send(fd_, data, len, MSG_NOSIGNAL_FLAG);
    return n > 0 ? static_cast<size_t>(n) : 0;
  }

  void close() override {
    if (fd_ >= 0) ::close(fd_);
    fd_ = -1;
  }

private:
#ifdef MSG_NOSIGNAL
  static constexpr int MSG_NOSIGNAL_FLAG = MSG_NOSIGNAL;
#else
  static constexpr int MSG_NOSIGNAL_FLAG = 0;  // macOS: the socket option below does it
#endif
  int fd_;
};

class SocketTcpServer : public TcpServer {
public:
  enum class Listen { AsAsked, Fixed, Any };

  explicit SocketTcpServer(Listen mode = Listen::AsAsked, uint16_t port = 0) : mode_(mode), fixed_(port) {}
  ~SocketTcpServer() override { stop(); }

  bool begin(uint16_t port) override {
    stop();
    if (port == 0) return fail("port 0 cannot be opened");
    const uint16_t want = mode_ == Listen::Fixed ? fixed_ : mode_ == Listen::Any ? 0 : port;
    fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) return fail(std::strerror(errno));
    const int yes = 1;
    ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(want);
    if (::bind(fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 || ::listen(fd_, 8) != 0) {
      const std::string why = std::strerror(errno);
      stop();
      return fail(why);
    }
    socklen_t length = sizeof(address);
    ::getsockname(fd_, reinterpret_cast<sockaddr*>(&address), &length);
    bound_ = ntohs(address.sin_port);
    ::fcntl(fd_, F_SETFL, ::fcntl(fd_, F_GETFL, 0) | O_NONBLOCK);
    error_.clear();
    return true;
  }

  void stop() override {
    current_.reset();
    if (fd_ >= 0) ::close(fd_);
    fd_ = -1;
    bound_ = 0;
  }

  TcpConnection* accept() override {
    if (fd_ < 0) return nullptr;
    const int client = ::accept(fd_, nullptr, nullptr);
    if (client < 0) return nullptr;
    ::fcntl(client, F_SETFL, ::fcntl(client, F_GETFL, 0) | O_NONBLOCK);
#ifdef SO_NOSIGPIPE
    ::setsockopt(client, SOL_SOCKET, SO_NOSIGPIPE, &one_, sizeof(one_));
#endif
    current_ = std::make_unique<SocketConnection>(client);  // the one before is closed by its destructor
    return current_.get();
  }

  // The port that is open now, 0 when none; and why the last attempt to open one failed.
  uint16_t boundPort() const { return bound_; }
  const std::string& lastError() const { return error_; }

private:
  bool fail(const std::string& why) {
    error_ = why;
    return false;
  }

  Listen mode_;
  uint16_t fixed_;
  int fd_ = -1;
  uint16_t bound_ = 0;
  std::string error_;
  std::unique_ptr<SocketConnection> current_;
  int one_ = 1;
};

}  // namespace sim

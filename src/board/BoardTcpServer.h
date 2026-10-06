#pragma once

// The TcpServer capability on the board. Not blocking, one connection at a time: the connection object belongs
// to the server and is valid until close() is called or the next client is accepted, which closes the one
// before. read() gives 0 for "nothing now" while the other side is there, and -1 once it has gone and everything
// it sent has been read (also after close()); write() gives the number of bytes taken, 0 after close() and
// when the other side is gone.

#include "../hal/TcpServer.h"
#include "TcpListener.h"

namespace board {

class BoardTcpConnection : public TcpConnection {
public:
  void attach(TcpSocket* socket) {
    socket_ = socket;
    closed_ = false;
  }
  int read(uint8_t* buf, size_t size) override;
  size_t write(const uint8_t* data, size_t len) override;
  void close() override;

private:
  TcpSocket* socket_ = nullptr;
  bool closed_ = true;
};

class BoardTcpServer : public TcpServer {
public:
  explicit BoardTcpServer(TcpListener& listener) : listener_(listener) {}
  ~BoardTcpServer() override { stop(); }

  bool begin(uint16_t port) override;
  void stop() override;
  TcpConnection* accept() override;

private:
  TcpListener& listener_;
  BoardTcpConnection connection_;
  bool listening_ = false;
};

}  // namespace board

#pragma once

// The seam below the TcpServer capability: a listening socket and the clients it hands out, in the terms of
// the Wi-Fi stack of the processor (device/Esp32TcpListener.h) or of a scripted one in the tests
// (testing/fakes/FakeTcpListener.h). What makes it the capability (one connection at a time, 0 for "nothing
// now" and -1 for "gone") is BoardTcpServer's and is tested on the host.

#include <cstddef>
#include <cstdint>

namespace board {

class TcpSocket {
public:
  virtual ~TcpSocket() = default;
  // Bytes that have arrived and can be read now.
  virtual int available() = 0;
  virtual int read(uint8_t* buf, size_t size) = 0;
  virtual size_t write(const uint8_t* data, size_t len) = 0;
  // Is the other side still there?
  virtual bool connected() = 0;
  virtual void stop() = 0;
};

class TcpListener {
public:
  virtual ~TcpListener() = default;
  virtual bool listen(uint16_t port) = 0;
  virtual void stopListening() = 0;
  // The next client that is waiting, or nullptr. The socket is valid until stop() is called on it or the
  // next client is taken.
  virtual TcpSocket* nextClient() = 0;
};

}  // namespace board

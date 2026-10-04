#pragma once

// Capability: accept TCP connections. The web server speaks HTTP through these two
// interfaces; the WLAN stack on the device and a socket on the computer in the
// simulator implement them. The whole web server can thus be tested without a WLAN chip.
//
// Not blocking: read() returns 0 when nothing is there right now, and -1 when the other
// side has gone and everything it sent has been read.

#include <cstddef>
#include <cstdint>

class TcpConnection {
public:
  virtual ~TcpConnection() = default;
  // Reads what has arrived, at most `size` bytes. 0 = nothing now; -1 = the other side
  // is gone and the data is used up (also after close()).
  virtual int read(uint8_t* buf, size_t size) = 0;
  // Writes; returns the number of bytes taken, which may be fewer than `len`. 0 after
  // close() or when the other side is gone.
  virtual size_t write(const uint8_t* data, size_t len) = 0;
  virtual void close() = 0;
};

class TcpServer {
public:
  virtual ~TcpServer() = default;
  // Starts listening on `port` (not 0). A server that is listening already is
  // restarted. False when the port cannot be opened.
  virtual bool begin(uint16_t port) = 0;
  // Stops listening and closes the connection.
  virtual void stop() = 0;
  // A new connection, or nullptr. One at a time: the object belongs to the server and is
  // valid until close() is called or the next connection is accepted. nullptr when the
  // server is not listening.
  virtual TcpConnection* accept() = 0;
};

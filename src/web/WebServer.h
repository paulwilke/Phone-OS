#pragma once

// The web server (L1): accepts connections from TcpServer, reads the request
// with the small parser, lets a RequestHandler respond, and sends the response.
// One connection at a time — a device with 300 KB RAM needs no concurrency, and
// the buffers are large (request up to 9 KB, response up to 12 KB). Further
// connections wait in the network stack's backlog. Every response closes the
// connection; a connection that goes silent is dropped after kIdleSeconds so
// that a stalled browser does not lock everyone out.

#include <cstddef>
#include <cstdint>

#include "../hal/TcpServer.h"
#include "Http.h"
#include "RequestHandler.h"

namespace web {

class WebServer {
public:
  static constexpr uint16_t kPort = 80;
  static constexpr uint32_t kIdleSeconds = 10;

  WebServer(TcpServer& tcp, RequestHandler& handler) : tcp_(tcp), handler_(handler) {}

  // Start listening. Returns false if the port cannot be opened.
  bool begin(uint16_t port = kPort);
  void stop();
  bool running() const { return running_; }

  void poll(uint32_t nowSeconds);

  // How many requests have been answered (for status display and tests).
  uint32_t served() const { return served_; }

private:
  enum class Phase : uint8_t { Idle, Reading, Writing };
  void finish();
  void respond(uint32_t now);

  TcpServer& tcp_;
  RequestHandler& handler_;
  bool running_ = false;
  TcpConnection* conn_ = nullptr;
  Phase phase_ = Phase::Idle;
  uint32_t lastProgress_ = 0;
  uint32_t served_ = 0;

  http::Parser parser_;
  // Response in two parts: the head in its own buffer, the body where it lives
  // (API buffer or flash). Only the remaining bytes are sent each call.
  char head_[512];
  size_t headLen_ = 0;
  const char* body_ = nullptr;
  size_t bodyLen_ = 0;
  size_t sent_ = 0;
};

}  // namespace web

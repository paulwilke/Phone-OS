#pragma once

// The web server (L1): accepts connections from TcpServer, reads the request
// with the small parser, lets a RequestHandler respond, and sends the response.
// One connection at a time — a device with 300 KB RAM needs no concurrency, and
// the buffers are large (request up to 9 KB, response up to 12 KB). Further
// connections wait in the network stack's backlog. Every response closes the
// connection; a connection that goes silent is dropped after kIdleSeconds so
// that a stalled browser does not lock everyone out.
//
// What a browser does to a server like this (docs/web/web-interface.md, "seen on the device 2026-10-07"):
// it opens several connections at once and keeps spare ones that never send a byte, so a connection that
// has said nothing is let go after kFirstByteSeconds, not after the full idle time; a connection is never
// closed with input unread (the stack would answer that with a reset that can throw the response away),
// so an early error answer is followed by a short lingering close; and every request that ends badly
// leaves one line in the log.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include "../hal/TcpServer.h"
#include "Http.h"
#include "RequestHandler.h"

namespace web {

class WebServer {
public:
  static constexpr uint16_t kPort = 80;
  // Silence once a request has started, and the time a response may take without any progress.
  static constexpr uint32_t kIdleSeconds = 10;
  // A connection that has not sent one byte yet: browsers keep spare connections open, and with one
  // connection at a time each of them would block everyone behind it for the full idle time.
  static constexpr uint32_t kFirstByteSeconds = 3;
  // After an error answer that was sent before the request was read to the end, the input is read and
  // thrown away for this long, so that the close does not turn into a reset.
  static constexpr uint32_t kLingerSeconds = 2;
  // At most this many lines per minute in the log; the rest is counted and reported once.
  static constexpr uint32_t kLogLinesPerMinute = 20;

  WebServer(TcpServer& tcp, RequestHandler& handler) : tcp_(tcp), handler_(handler) {}

  // Start listening. Returns false if the port cannot be opened.
  bool begin(uint16_t port = kPort);
  void stop();
  bool running() const { return running_; }

  void poll(uint32_t nowSeconds);

  // Where the lines about requests that ended badly go (the application log). Without one nothing is logged.
  void setLog(std::function<void(const std::string&)> log) { log_ = std::move(log); }

  // How many requests have been answered (for status display and tests).
  uint32_t served() const { return served_; }

private:
  enum class Phase : uint8_t { Idle, Reading, Writing, Lingering };
  enum class End : uint8_t { Done, PeerGone, Silent, Stalled, Stopped };
  void finish(End how, uint32_t now);
  void respond(uint32_t now);
  void report(End how, uint32_t now);
  void emit(const std::string& line, uint32_t now);

  TcpServer& tcp_;
  RequestHandler& handler_;
  bool running_ = false;
  TcpConnection* conn_ = nullptr;
  Phase phase_ = Phase::Idle;
  uint32_t lastProgress_ = 0;
  uint32_t accepted_ = 0;      // when the connection was taken, for the wait in the log
  uint32_t served_ = 0;
  size_t received_ = 0;        // bytes of the request that arrived
  int status_ = 0;             // 0 until a response has been made
  char method_[8] = {};
  char path_[121] = {};
  std::string queryKeys_;      // the names of the query parameters, never their values
  std::function<void(const std::string&)> log_;
  uint32_t logWindow_ = 0;
  uint32_t logged_ = 0;
  uint32_t suppressed_ = 0;

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

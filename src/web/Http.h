#pragma once

// A small HTTP/1.1 layer (no networking): parse a request, format a response.
// That is all the web interface needs — an own parser with fixed limits is safer
// than a library for a port that exposes the family's configuration.
//
// Limits, intentionally tight: head at most 1 KB, body at most 8 KB, path at
// most 120 characters. Everything beyond is refused with 4xx, never buffered.
// No chunked requests, no keep-alive (every response closes), no upgrade.

#include <cstddef>
#include <cstdint>

namespace http {

constexpr size_t kMaxHead = 1024;
constexpr size_t kMaxBody = 8192;

struct Request {
  char method[8] = {};
  char path[120] = {};       // without the query
  char query[200] = {};
  char authorization[160] = {};
  char custom[16] = {};      // value of X-Phoneplus, "" when not sent
  char contentType[40] = {};
  size_t contentLength = 0;
  const char* body = nullptr;  // points into the parser's buffer
};

struct Response {
  int status = 200;
  const char* contentType = "text/plain; charset=utf-8";
  const char* body = "";
  size_t length = 0;
  const char* extraHeader = nullptr;  // e.g. "WWW-Authenticate: Basic realm=\"phoneplus\""
};

// Fed byte by byte or in chunks; evaluates as soon as the request is complete.
class Parser {
public:
  enum class State : uint8_t { Head, Body, Done, Error };

  void reset();
  // Returns the new state.
  State feed(const uint8_t* data, size_t len);
  State state() const { return state_; }
  const Request& request() const { return req_; }
  // On Error: the status code to respond with (400, 411, 413, 414, 431, 501).
  int errorStatus() const { return errorStatus_; }

private:
  void parseHead();
  State fail(int status);

  State state_ = State::Head;
  int errorStatus_ = 0;
  char buf_[kMaxHead + kMaxBody + 1];
  size_t used_ = 0;
  size_t bodyStart_ = 0;
  Request req_;
};

// Only the head (including the blank line); 0 if it does not fit. The body is
// not copied: the web page is larger than any response buffer, and on the device
// it lives in flash anyway.
size_t formatHead(const Response& r, char* out, size_t size);
// Head and body into `out`; 0 if it does not fit. For tests and small responses.
size_t format(const Response& r, char* out, size_t size);

const char* reasonPhrase(int status);

// Basic auth: the password from "Basic <base64(user:password)>", false on bad
// format. `out` receives the password only.
bool basicPassword(const char* authorization, char* out, size_t size);

// Comparison in constant time so that the response time reveals nothing about
// the length of the common prefix.
bool constantTimeEquals(const char* a, const char* b);

}  // namespace http

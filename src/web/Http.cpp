#include "Http.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace http {

namespace {

void copyTo(char* dst, size_t size, const char* src, size_t n) {
  if (n >= size) n = size - 1;
  memcpy(dst, src, n);
  dst[n] = '\0';
}

// Header value when the name matches (case-insensitively).
const char* headerValue(const char* line, size_t len, const char* name) {
  const size_t n = strlen(name);
  if (len <= n || line[n] != ':') return nullptr;
  for (size_t i = 0; i < n; ++i) {
    if (tolower((unsigned char)line[i]) != tolower((unsigned char)name[i])) return nullptr;
  }
  const char* v = line + n + 1;
  while (v < line + len && (*v == ' ' || *v == '\t')) ++v;
  return v;
}

int b64(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

}  // namespace

void Parser::reset() {
  state_ = State::Head;
  errorStatus_ = 0;
  used_ = 0;
  bodyStart_ = 0;
  req_ = Request();
}

Parser::State Parser::fail(int status) {
  state_ = State::Error;
  errorStatus_ = status;
  return state_;
}

Parser::State Parser::feed(const uint8_t* data, size_t len) {
  if (state_ == State::Done || state_ == State::Error) return state_;
  if (used_ + len > sizeof(buf_) - 1) {
    // More than head plus the largest allowed body can ever be.
    return fail(used_ < kMaxHead ? 431 : 413);
  }
  memcpy(buf_ + used_, data, len);
  used_ += len;
  buf_[used_] = '\0';

  if (state_ == State::Head) {
    const char* end = strstr(buf_, "\r\n\r\n");
    if (!end) {
      return used_ > kMaxHead ? fail(431) : state_;
    }
    if ((size_t)(end - buf_) > kMaxHead) return fail(431);
    bodyStart_ = (size_t)(end - buf_) + 4;
    parseHead();
    if (state_ == State::Error) return state_;
    state_ = State::Body;
  }

  if (state_ == State::Body) {
    if (used_ - bodyStart_ >= req_.contentLength) {
      req_.body = buf_ + bodyStart_;
      buf_[bodyStart_ + req_.contentLength] = '\0';
      state_ = State::Done;
    }
  }
  return state_;
}

void Parser::parseHead() {
  // Request line: METHOD PATH HTTP/1.x
  const char* p = buf_;
  const char* sp1 = strchr(p, ' ');
  if (!sp1 || sp1 == p) { fail(400); return; }
  copyTo(req_.method, sizeof(req_.method), p, (size_t)(sp1 - p));
  const char* target = sp1 + 1;
  const char* sp2 = strchr(target, ' ');
  const char* eol = strstr(target, "\r\n");
  if (!sp2 || !eol || sp2 > eol) { fail(400); return; }
  if (strncmp(sp2 + 1, "HTTP/1.", 7) != 0) { fail(400); return; }
  if (sp2 - target > (long)sizeof(req_.path) - 1 + (long)sizeof(req_.query)) { fail(414); return; }

  const char* q = (const char*)memchr(target, '?', (size_t)(sp2 - target));
  const char* pathEnd = q ? q : sp2;
  if (pathEnd - target >= (long)sizeof(req_.path)) { fail(414); return; }
  copyTo(req_.path, sizeof(req_.path), target, (size_t)(pathEnd - target));
  if (q) copyTo(req_.query, sizeof(req_.query), q + 1, (size_t)(sp2 - q - 1));
  if (req_.path[0] != '/') { fail(400); return; }

  // Header fields
  const char* line = eol + 2;
  bool haveLength = false;
  while (line < buf_ + bodyStart_ - 2) {
    const char* e = strstr(line, "\r\n");
    if (!e) break;
    const size_t n = (size_t)(e - line);
    if (const char* v = headerValue(line, n, "Content-Length")) {
      char tmp[12];
      copyTo(tmp, sizeof(tmp), v, (size_t)(e - v));
      char* endp = nullptr;
      const long l = strtol(tmp, &endp, 10);
      if (endp == tmp || *endp != '\0' || l < 0) { fail(400); return; }
      if ((size_t)l > kMaxBody) { fail(413); return; }
      req_.contentLength = (size_t)l;
      haveLength = true;
    } else if (const char* v = headerValue(line, n, "Authorization")) {
      copyTo(req_.authorization, sizeof(req_.authorization), v, (size_t)(e - v));
    } else if (const char* v = headerValue(line, n, "X-Phone-OS")) {
      copyTo(req_.custom, sizeof(req_.custom), v, (size_t)(e - v));
    } else if (const char* v = headerValue(line, n, "Content-Type")) {
      copyTo(req_.contentType, sizeof(req_.contentType), v, (size_t)(e - v));
    } else if (headerValue(line, n, "Transfer-Encoding")) {
      fail(501);  // chunked not accepted
      return;
    }
    line = e + 2;
  }
  // A body without a length declared is refused — better to reject than to guess.
  if (!haveLength && (strcmp(req_.method, "POST") == 0 || strcmp(req_.method, "PUT") == 0)) {
    fail(411);
  }
}

const char* reasonPhrase(int status) {
  switch (status) {
    case 200: return "OK";
    case 204: return "No Content";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 411: return "Length Required";
    case 413: return "Payload Too Large";
    case 414: return "URI Too Long";
    case 429: return "Too Many Requests";
    case 431: return "Request Header Fields Too Large";
    case 501: return "Not Implemented";
    case 503: return "Service Unavailable";
    default: return "Error";
  }
}

size_t formatHead(const Response& r, char* out, size_t size) {
  // No CORS, no cache: the responses contain the family's configuration, and no
  // browser should hold onto them or pass them to third-party pages.
  const int n = snprintf(out, size,
                         "HTTP/1.1 %d %s\r\n"
                         "Content-Type: %s\r\n"
                         "Content-Length: %u\r\n"
                         "Cache-Control: no-store\r\n"
                         "X-Content-Type-Options: nosniff\r\n"
                         "Connection: close\r\n"
                         "%s%s"
                         "\r\n",
                         r.status, reasonPhrase(r.status), r.contentType, (unsigned)r.length,
                         r.extraHeader ? r.extraHeader : "", r.extraHeader ? "\r\n" : "");
  if (n < 0 || (size_t)n >= size) return 0;
  return (size_t)n;
}

size_t format(const Response& r, char* out, size_t size) {
  const size_t n = formatHead(r, out, size);
  if (n == 0 || n + r.length >= size) return 0;
  memcpy(out + n, r.body, r.length);
  return n + r.length;
}

bool basicPassword(const char* authorization, char* out, size_t size) {
  if (!authorization || !out || size == 0) return false;
  out[0] = '\0';
  if (strncasecmp(authorization, "Basic ", 6) != 0) return false;
  const char* in = authorization + 6;
  char decoded[128];
  size_t n = 0;
  int acc = 0, bits = 0;
  for (; *in && *in != '='; ++in) {
    const int v = b64(*in);
    if (v < 0) return false;
    acc = (acc << 6) | v;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      if (n + 1 >= sizeof(decoded)) return false;
      decoded[n++] = (char)((acc >> bits) & 0xFF);
    }
  }
  decoded[n] = '\0';
  const char* colon = strchr(decoded, ':');
  if (!colon) return false;
  copyTo(out, size, colon + 1, strlen(colon + 1));
  return true;
}

bool constantTimeEquals(const char* a, const char* b) {
  const size_t la = strlen(a), lb = strlen(b);
  unsigned char diff = (unsigned char)(la ^ lb);
  const size_t n = la > lb ? la : lb;
  for (size_t i = 0; i < n; ++i) {
    const unsigned char ca = i < la ? (unsigned char)a[i] : 0;
    const unsigned char cb = i < lb ? (unsigned char)b[i] : 0;
    diff |= (unsigned char)(ca ^ cb);
  }
  return diff == 0;
}

}  // namespace http

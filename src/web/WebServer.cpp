#include "WebServer.h"

#include <cstdio>
#include <cstring>

namespace web {

bool WebServer::begin(uint16_t port) {
  if (running_) return true;
  running_ = tcp_.begin(port);
  phase_ = Phase::Idle;
  conn_ = nullptr;
  return running_;
}

void WebServer::stop() {
  finish(End::Stopped, lastProgress_);
  conn_ = nullptr;
  phase_ = Phase::Idle;
  if (running_) tcp_.stop();
  running_ = false;
}

namespace {

// Text from the network that goes into the log: one line, printable ASCII only, so that a request can
// never forge a second log line or smuggle a control character into it.
std::string printable(const char* text, size_t limit) {
  std::string out;
  for (const char* p = text; *p && out.size() < limit; ++p) out += (*p >= 0x20 && *p < 0x7f) ? *p : '?';
  return out;
}

}  // namespace

// Ends the connection. A connection is never closed with input unread: the stack answers that with a
// reset, and a reset can make the browser throw away the response it has received but not yet shown.
void WebServer::finish(End how, uint32_t now) {
  if (!conn_) return;
  report(how, now);
  if (how == End::Done) {
    uint8_t scrap[256];
    for (int i = 0; i < 8 && conn_->read(scrap, sizeof(scrap)) > 0; ++i) {}
  }
  conn_->close();
  conn_ = nullptr;
  phase_ = Phase::Idle;
}

// One line in the log for a request that ended badly: an error status or a response that did not go out
// completely, and a connection that closed without a request (the browser's spare connections, which is
// what holds a one-connection server up). Method, path and the names of the query parameters, status and
// bytes sent: nothing from a body, an Authorization header or a query value.
void WebServer::report(End how, uint32_t now) {
  if (!log_) return;
  const size_t total = headLen_ + bodyLen_;
  const char* why = how == End::PeerGone ? "peer gone" : how == End::Silent ? "silent" : how == End::Stalled ? "stalled" : "stopped";
  char line[320];
  if (status_ == 0) {
    if (how == End::Done) return;
    snprintf(line, sizeof(line), "WEB: connection closed without a complete request (%s, %u s, %u bytes received)", why,
             (unsigned)(now - accepted_), (unsigned)received_);
    return emit(line, now);
  }
  const bool aborted = how != End::Done || sent_ < total;
  if (!aborted && status_ < 400) {
    // The page asks for its words, labels and hints in three requests after the login (/api/lang/<code>,
    // .../labels, .../help). When a field shows its key instead of its label, these lines say whether the device
    // was asked and answered. Three lines for a page load; nothing else that went well is logged.
    if (strcmp(method_, "GET") == 0 && strncmp(path_, "/api/lang/", 10) == 0) {
      snprintf(line, sizeof(line), "WEB: GET %s -> %d, sent %u bytes", printable(path_, sizeof(path_)).c_str(), status_, (unsigned)sent_);
      emit(line, now);
    }
    return;
  }
  std::string target = printable(path_, sizeof(path_));
  if (!queryKeys_.empty()) target += "?" + queryKeys_;
  snprintf(line, sizeof(line), "WEB: %s %s -> %d%s%s%s, sent %u of %u bytes", printable(method_, 7).c_str(), target.c_str(), status_,
           aborted ? ", aborted (" : "", aborted ? why : "", aborted ? ")" : "", (unsigned)sent_, (unsigned)total);
  emit(line, now);
}

void WebServer::emit(const std::string& line, uint32_t now) {
  if (logged_ == 0) logWindow_ = now;
  if (now - logWindow_ >= 60) {
    if (suppressed_ > 0) log_("WEB: " + std::to_string(suppressed_) + " more requests ended badly in the last minute");
    logWindow_ = now;
    logged_ = 0;
    suppressed_ = 0;
  }
  if (logged_ >= kLogLinesPerMinute) {
    ++suppressed_;
    return;
  }
  ++logged_;
  log_(line);
}

// Build the response from a complete (or rejected) request.
void WebServer::respond(uint32_t now) {
  http::Response res;
  const http::Request& req = parser_.request();
  snprintf(method_, sizeof(method_), "%s", req.method);
  snprintf(path_, sizeof(path_), "%s", req.path);
  queryKeys_.clear();
  for (const char* p = req.query; *p;) {
    const char* end = p;
    while (*end && *end != '&') ++end;
    const char* eq = (const char*)memchr(p, '=', (size_t)(end - p));
    std::string key(p, (size_t)((eq ? eq : end) - p));
    if (!key.empty() && queryKeys_.size() < 60) queryKeys_ += (queryKeys_.empty() ? "" : "&") + printable(key.c_str(), 30);
    p = *end ? end + 1 : end;
  }
  if (parser_.state() == http::Parser::State::Done) {
    handler_.handle(req, res, now);
  } else {
    handler_.error(parser_.errorStatus(), res);
  }
  status_ = res.status;
  headLen_ = http::formatHead(res, head_, sizeof(head_));
  body_ = res.body;
  bodyLen_ = res.length;
  if (headLen_ == 0) {
    // The head did not fit (an overlong extra header): send a small error
    // rather than nothing at all.
    http::Response small;
    small.status = 500;
    small.contentType = "application/json; charset=utf-8";
    small.body = "{\"error\":\"response too large\"}";
    small.length = strlen(small.body);
    headLen_ = http::formatHead(small, head_, sizeof(head_));
    body_ = small.body;
    bodyLen_ = small.length;
    status_ = 500;
  }
  sent_ = 0;
  phase_ = Phase::Writing;
  ++served_;
}

void WebServer::poll(uint32_t now) {
  if (!running_) return;

  if (!conn_) {
    conn_ = tcp_.accept();
    if (!conn_) return;
    parser_.reset();
    phase_ = Phase::Reading;
    lastProgress_ = now;
    accepted_ = now;
    received_ = 0;
    status_ = 0;
    headLen_ = bodyLen_ = sent_ = 0;
    method_[0] = path_[0] = '\0';
    queryKeys_.clear();
  }

  if (phase_ == Phase::Reading) {
    uint8_t buf[512];
    // At most a few chunks per call so the main loop does not stall on a slow
    // browser.
    for (int i = 0; i < 8 && phase_ == Phase::Reading; ++i) {
      const int n = conn_->read(buf, sizeof(buf));
      if (n < 0) { finish(End::PeerGone, now); return; }  // remote end gone
      if (n == 0) break;
      lastProgress_ = now;
      received_ += (size_t)n;
      const http::Parser::State st = parser_.feed(buf, (size_t)n);
      if (st == http::Parser::State::Done || st == http::Parser::State::Error) {
        respond(now);
        break;
      }
    }
  }

  if (phase_ == Phase::Writing) {
    // As much as the network takes now, in a few chunks, and the rest in the next pass. The socket never waits
    // (on the device: decision D-41), so the main loop goes on however slowly the browser reads.
    for (int i = 0; i < 8 && sent_ < headLen_ + bodyLen_; ++i) {
      size_t w = 0;
      if (sent_ < headLen_)
        w = conn_->write((const uint8_t*)head_ + sent_, headLen_ - sent_);
      else
        w = conn_->write((const uint8_t*)body_ + (sent_ - headLen_), headLen_ + bodyLen_ - sent_);
      if (w == 0) {
        // Nothing was taken: no room yet, or a dead connection, which must not hold the one slot for the idle
        // time. Reading says which: -1 is "gone".
        uint8_t probe;
        if (conn_->read(&probe, 1) < 0) { finish(End::PeerGone, now); return; }
        break;
      }
      sent_ += w;
      lastProgress_ = now;
    }
    if (sent_ >= headLen_ + bodyLen_) {
      if (parser_.state() == http::Parser::State::Error) {
        // The answer went out before the request was read to the end: read what is still coming.
        phase_ = Phase::Lingering;
        lastProgress_ = now;
      } else {
        finish(End::Done, now);
        return;
      }
    }
  }

  if (phase_ == Phase::Lingering) {
    uint8_t scrap[512];
    for (int i = 0; i < 8; ++i) {
      const int n = conn_->read(scrap, sizeof(scrap));
      if (n < 0) { finish(End::Done, now); return; }  // the browser has closed its side: the end of the exchange
      if (n == 0) break;
    }
    if (now - lastProgress_ >= kLingerSeconds) { finish(End::Done, now); return; }
    return;
  }

  if (conn_) {
    const uint32_t limit = (phase_ == Phase::Reading && received_ == 0) ? kFirstByteSeconds : kIdleSeconds;
    if (now - lastProgress_ >= limit) finish(phase_ == Phase::Reading ? End::Silent : End::Stalled, now);
  }
}

}  // namespace web

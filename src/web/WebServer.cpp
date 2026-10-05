#include "WebServer.h"

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
  if (conn_) conn_->close();
  conn_ = nullptr;
  phase_ = Phase::Idle;
  if (running_) tcp_.stop();
  running_ = false;
}

void WebServer::finish() {
  if (conn_) conn_->close();
  conn_ = nullptr;
  phase_ = Phase::Idle;
}

// Build the response from a complete (or rejected) request.
void WebServer::respond(uint32_t now) {
  http::Response res;
  if (parser_.state() == http::Parser::State::Done) {
    handler_.handle(parser_.request(), res, now);
  } else {
    handler_.error(parser_.errorStatus(), res);
  }
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
  }

  if (phase_ == Phase::Reading) {
    uint8_t buf[512];
    // At most a few chunks per call so the main loop does not stall on a slow
    // browser.
    for (int i = 0; i < 8 && phase_ == Phase::Reading; ++i) {
      const int n = conn_->read(buf, sizeof(buf));
      if (n < 0) { finish(); return; }  // remote end gone
      if (n == 0) break;
      lastProgress_ = now;
      const http::Parser::State st = parser_.feed(buf, (size_t)n);
      if (st == http::Parser::State::Done || st == http::Parser::State::Error) {
        respond(now);
        break;
      }
    }
  }

  if (phase_ == Phase::Writing) {
    size_t w = 0;
    if (sent_ < headLen_)
      w = conn_->write((const uint8_t*)head_ + sent_, headLen_ - sent_);
    else
      w = conn_->write((const uint8_t*)body_ + (sent_ - headLen_), headLen_ + bodyLen_ - sent_);
    if (w > 0) {
      sent_ += w;
      lastProgress_ = now;
    }
    if (sent_ >= headLen_ + bodyLen_) { finish(); return; }
  }

  if (conn_ && now - lastProgress_ >= kIdleSeconds) finish();
}

}  // namespace web

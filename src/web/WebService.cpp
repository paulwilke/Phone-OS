#include "WebService.h"

#include <cstring>


WebService::WebService(TcpServer& tcp, ConfigStore& store, WebAssets& assets, AppCore& app, i18n::Translator& translator)
    : app_(app), api_(store, assets, *this, app, translator), server_(tcp, api_) {}

size_t WebService::handleRaw(const uint8_t* request, size_t len, uint32_t now,
                              char* head, size_t headSize,
                              const char** body, size_t* bodyLen) {
  rawParser_.reset();
  rawParser_.feed(request, len);
  http::Response res;
  if (rawParser_.state() == http::Parser::State::Done) {
    api_.handle(rawParser_.request(), res, now);
  } else if (rawParser_.state() == http::Parser::State::Error) {
    api_.error(rawParser_.errorStatus(), res);
  } else {
    api_.error(400, res);
  }
  const size_t n = http::formatHead(res, head, headSize);
  *body    = res.body;
  *bodyLen = res.length;
  return n;
}

void WebService::poll(uint32_t now) {
  // The button of the page asks for Wi-Fi off; it is done after the answer has gone out.
  if (wifiOffRequested_) {
    wifiOffRequested_ = false;
    app_.setWifiMode(WifiMode::Off);
  }
  // The core runs the controller (its idle time, its attempts); the server follows what it says.
  const WifiController* wifi = app_.wifi();
  const WifiPhase phase = wifi ? wifi->phase() : WifiPhase::Off;
  const bool reachable = phase == WifiPhase::Connected || phase == WifiPhase::AccessPoint;
  if (reachable && !server_.running()) server_.begin();
  if (!reachable && server_.running()) server_.stop();
  server_.poll(now);
}

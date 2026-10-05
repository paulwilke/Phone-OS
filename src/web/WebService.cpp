#include "WebService.h"

#include <cstring>

#include "../config/ConfigLoader.h"

WebService::WebService(Wifi& wifi, TcpServer& tcp, ConfigStore& store, WebAssets& assets,
                       AppCore& app, i18n::Translator& translator, WifiController::Random random)
    : store_(store),
      wifi_(wifi, random),
      api_(store, assets, *this, app, wifi_, translator),
      server_(tcp, api_) {}

void WebService::begin() {
  config::ConfigLoader loader(store_);
  const auto result = loader.load();
  wifi_.configure(result.profile.wifi);
}

bool WebService::setWifiMode(WifiMode mode, uint32_t now) {
  return wifi_.request(mode, now);
}

WifiStatus WebService::wifiStatus(uint32_t now) {
  WifiStatus s;
  s.mode         = wifi_.mode();
  s.phase        = wifi_.phase();
  s.ssid         = wifi_.ssid();
  s.apPassword   = wifi_.apPassword();
  s.address      = wifi_.address();
  s.reason       = wifi_.reason();
  s.secondsLeft  = wifi_.secondsLeft(now);
  s.serverRunning = server_.running();
  return s;
}

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
  if (wifiOffRequested_) {
    wifiOffRequested_ = false;
    wifi_.stop();
  }
  wifi_.poll(now);

  const WifiPhase p = wifi_.phase();
  const bool reachable = p == WifiPhase::Connected || p == WifiPhase::AccessPoint;
  if (reachable  && !server_.running()) server_.begin();
  if (!reachable && server_.running())  server_.stop();
  server_.poll(now);
}

#pragma once

// The web server of the parents' page, tied to the device's Wi-Fi. There is **one Wi-Fi controller**,
// the application core's: the device's own screens switch it, this service tells it about every
// authenticated request (so the idle time restarts, see WebApi.h), and the server runs exactly while
// that controller says Wi-Fi is up — it starts when a connection or the access point is established
// and stops when Wi-Fi goes off, whoever switched it off or whatever the reason (the idle time, the
// "switch off" button, a lost access point).
//
// Wiring in main.cpp / sim/main.cpp: one TcpServer, one ConfigStore and one WebAssets
// implementation; AppCore and Translator are shared, and the core has its Wi-Fi set (setWifi):
//
//   WebService web(tcp, store, assets, app, translator);
//   // in the main loop:
//   web.poll(clock.uptimeSeconds());
#include <cstddef>
#include <cstdint>
#include "../app/AppCore.h"
#include "../hal/ConfigStore.h"
#include "../hal/TcpServer.h"
#include "../i18n/Translator.h"
#include "WebApi.h"
#include "WebAssets.h"
#include "WebServer.h"
class WebService : public WebHost {
public:
  WebService(TcpServer& tcp, ConfigStore& store, WebAssets& assets, AppCore& app, i18n::Translator& translator);

  void poll(uint32_t nowSeconds);

  // WebHost
  void requestRestart() override { restartRequested_ = true; }
  void requestWifiOff()  override { wifiOffRequested_ = true; }

  // USB bridge: parse a raw HTTP request and fill `head`/`body` with the
  // response. Used by the USB console (scripts/webbridge.py). Returns the
  // head length (0 on internal error); `*body` and `*bodyLen` point into a
  // buffer that is valid until the next call.
  size_t handleRaw(const uint8_t* request, size_t len, uint32_t nowSeconds,
                   char* head, size_t headSize,
                   const char** body, size_t* bodyLen);

  bool restartRequested() const { return restartRequested_; }
  void clearRestartRequest() { restartRequested_ = false; }

  // Exposed for tests.
  web::WebServer& server() { return server_; }

private:
  AppCore& app_;
  WebApi api_;
  web::WebServer server_;
  http::Parser rawParser_;
  bool restartRequested_ = false;
  bool wifiOffRequested_  = false;
};

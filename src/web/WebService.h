#pragma once

// Wi-Fi and the web server as one unit. The server runs only while Wi-Fi is
// up — it starts when a connection or access point is established and stops
// when Wi-Fi goes off. Both start only on request from the device screen and
// switch themselves off after an idle period.
//
// Wiring in main.cpp / sim/main.cpp: one Wifi, one TcpServer, one ConfigStore
// and one WebAssets implementation; AppCore and Translator are shared:
//
//   WebService web(wifi, tcp, store, assets, app, translator, random);
//   web.begin();
//   // in the main loop:
//   web.poll(clock.uptimeSeconds());

#include <cstddef>
#include <cstdint>

#include "../app/AppCore.h"
#include "../app/WifiController.h"
#include "../hal/ConfigStore.h"
#include "../hal/TcpServer.h"
#include "../hal/Wifi.h"
#include "../i18n/Translator.h"
#include "WebApi.h"
#include "WebAssets.h"
#include "WebControl.h"
#include "WebServer.h"

class WebService : public WebControl, public WebHost {
public:
  WebService(Wifi& wifi, TcpServer& tcp, ConfigStore& store, WebAssets& assets,
             AppCore& app, i18n::Translator& translator, WifiController::Random random);

  // Reads the Wi-Fi settings from the config store and configures the controller.
  void begin();
  void poll(uint32_t nowSeconds);

  // WebControl
  bool setWifiMode(WifiMode mode, uint32_t nowSeconds) override;
  WifiStatus wifiStatus(uint32_t nowSeconds) override;

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
  bool active() const { return wifi_.active(); }

  // Exposed for tests and the display layer.
  WifiController& wifi()   { return wifi_; }
  web::WebServer& server() { return server_; }

private:
  ConfigStore& store_;
  WifiController wifi_;
  WebApi api_;
  web::WebServer server_;
  http::Parser rawParser_;
  bool restartRequested_ = false;
  bool wifiOffRequested_  = false;
};

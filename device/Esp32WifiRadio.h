#pragma once

// The Wi-Fi chip of the ESP32 behind the WifiRadio seam. Nothing is written to the flash
// (`persistent(false)`): the credentials are on the card and nowhere else. The access point uses channel 6,
// is not hidden and takes at most two stations: whoever sets the device up is alone, and every further station
// costs current and gives an attacker more to try.
//
// Built, not measured: the first run on the device is on the owner's list (docs/bring-up.md).

#include <WiFi.h>

#include <cstdio>
#include <string>

#include <board/WifiRadio.h>

#include "Esp32Heap.h"

class Esp32WifiRadio : public board::WifiRadio {
public:
  void beginStation(const std::string& ssid, const std::string& password) override {
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);  // trying again is the controller's business
    WiFi.mode(WIFI_STA);
    WiFi.setHostname("phoneplus");
    WiFi.begin(ssid.c_str(), password.empty() ? nullptr : password.c_str());
  }
  bool beginAccessPoint(const std::string& ssid, const std::string& password) override {
    WiFi.persistent(false);
    WiFi.mode(WIFI_AP);
    return WiFi.softAP(ssid.c_str(), password.c_str(), 6, 0, 2);
  }
  void shutdown() override {
    WiFi.softAPdisconnect(true);
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_OFF);  // the chip is without power afterwards
  }
  board::StationLink stationLink() override {
    const wl_status_t s = WiFi.status();
    if (s == WL_CONNECTED) return board::StationLink::Connected;
    if (s == WL_CONNECT_FAILED || s == WL_NO_SSID_AVAIL) return board::StationLink::Failed;
    return board::StationLink::Connecting;
  }
  // The raw wl_status_t: stationLink() folds all but three values into "connecting", and a link that
  // reads "not connected" while a page is being served has to be explained afterwards.
  std::string linkDetail() override { return "status " + std::to_string((int)WiFi.status()) + ", " + heapSummary(); }
  std::string stationAddress() override { return format(WiFi.localIP()); }
  std::string accessPointAddress() override { return format(WiFi.softAPIP()); }

private:
  static std::string format(const IPAddress& ip) {
    if (ip[0] == 0 && ip[1] == 0) return std::string();
    char text[16];
    std::snprintf(text, sizeof(text), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    return text;
  }
};

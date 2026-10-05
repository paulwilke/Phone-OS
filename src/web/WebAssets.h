#pragma once

// Interfaces the API uses from the outside world: the web page in flash (or on
// disk in the simulator) and the host's restart / Wi-Fi-off actions.

#include <cstddef>

class WebAssets {
public:
  virtual ~WebAssets() = default;
  // The embedded index page and its length in bytes.
  virtual const char* indexHtml(size_t& length) = 0;
};

class WebHost {
public:
  virtual ~WebHost() = default;
  virtual void requestRestart() = 0;
  virtual void requestWifiOff() = 0;
};

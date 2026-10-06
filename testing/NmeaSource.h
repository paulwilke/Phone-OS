#pragma once

// A receiver's NMEA stream for tests: once a second, while its supply is on, a pair of sentences with correct
// checksums goes into the scripted UART. Before the position is found the sentences say "no fix"; after, they
// carry the scripted position. A switch-on starts the search again, as a receiver does.
//   script(lat, lon, hdop, afterSeconds)   find this position that many seconds after switch-on
//   never()                                 search for ever
//   silence(bool)                           the receiver says nothing at all (a dead wire)
// Call step() once for each second that passes.

#include <cstdio>
#include <string>

#include <board/BoardPower.h>
#include <fakes/FakeUartPort.h>

class NmeaSource {
public:
  NmeaSource(FakeUartPort& uart, board::BoardPower& power) : uart_(uart), power_(power) {}

  void script(double lat, double lon, float hdop, uint32_t afterSeconds) {
    lat_ = lat;
    lon_ = lon;
    hdop_ = hdop;
    after_ = afterSeconds;
    never_ = false;
  }
  void never() { never_ = true; }
  void silence(bool silent) { silent_ = silent; }

  void step() {
    ++clockSeconds_;
    if (!power_.isEnabled(PowerDomain::Positioning)) {
      lastOns_ = power_.switchOns(PowerDomain::Positioning);
      onFor_ = 0;
      return;
    }
    if (power_.switchOns(PowerDomain::Positioning) != lastOns_) {
      lastOns_ = power_.switchOns(PowerDomain::Positioning);
      onFor_ = 0;
    }
    ++onFor_;
    if (silent_) return;
    const bool found = !never_ && onFor_ >= after_;
    const std::string time = clockText();
    if (found) {
      uart_.feed(wrap("GNGGA," + time + "," + position() + ",1,09," + hdopText() + ",140.0,M,47.0,M,,"));
      uart_.feed(wrap("GNRMC," + time + ",A," + position() + ",0.03,0.00,160926,,,A"));
    } else {
      uart_.feed(wrap("GNGGA," + time + ",,,,,0,00,99.99,,,,,,"));
      uart_.feed(wrap("GNRMC," + time + ",V,,,,,,,160926,,,N"));
    }
  }

private:
  static std::string wrap(const std::string& body) {
    unsigned char sum = 0;
    for (char c : body) sum ^= (unsigned char)c;
    char tail[8];
    std::snprintf(tail, sizeof(tail), "*%02X\r\n", sum);
    return "$" + body + tail;
  }
  std::string clockText() const {
    char b[16];
    std::snprintf(b, sizeof(b), "%02u%02u%02u.00", (12 + clockSeconds_ / 3600) % 24, (clockSeconds_ / 60) % 60, clockSeconds_ % 60);
    return b;
  }
  std::string hdopText() const {
    char b[16];
    std::snprintf(b, sizeof(b), "%.2f", hdop_);
    return b;
  }
  std::string position() const {
    auto part = [](double v, bool lat) {
      const double a = v < 0 ? -v : v;
      const int degrees = (int)a;
      const double minutes = (a - degrees) * 60.0;
      char b[24];
      if (lat) std::snprintf(b, sizeof(b), "%02d%07.4f,%c", degrees, minutes, v < 0 ? 'S' : 'N');
      else std::snprintf(b, sizeof(b), "%03d%07.4f,%c", degrees, minutes, v < 0 ? 'W' : 'E');
      return std::string(b);
    };
    return part(lat_, true) + "," + part(lon_, false);
  }

  FakeUartPort& uart_;
  board::BoardPower& power_;
  double lat_ = 0, lon_ = 0;
  float hdop_ = 1.5f;
  uint32_t after_ = 0;
  bool never_ = true;
  bool silent_ = false;
  uint32_t lastOns_ = 0;
  uint32_t onFor_ = 0;
  uint32_t clockSeconds_ = 0;
};

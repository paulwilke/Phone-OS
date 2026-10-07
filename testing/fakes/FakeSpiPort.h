#pragma once

// A recording SPI port: every write with the levels of the data/command line and the chip select at that moment,
// so a test can say "this byte was a command and that one data, and the chip was selected".
// flatten() gives the traffic as the panel saw it: a command byte as "C:xx", the data after it as "D:" plus bytes.

#include <cstdio>
#include <string>
#include <vector>

#include <board/PinMap.h>
#include <board/SpiPort.h>
#include <fakes/FakeGpio.h>

class FakeSpiPort : public SpiPort {
public:
  struct Write {
    std::vector<uint8_t> bytes;
    bool dcHigh;
    bool csLow;
  };

  explicit FakeSpiPort(FakeGpio& gpio) : gpio_(gpio) {}

  void write(const uint8_t* data, size_t len) override {
    writes_.push_back({std::vector<uint8_t>(data, data + len), gpio_.level(board::pin::kDisplayDc),
                       !gpio_.level(board::pin::kDisplayCs)});
  }

  const std::vector<Write>& writes() const { return writes_; }
  void clear() { writes_.clear(); }

  // "C:12" for a command, "D:01 02 03" for data.
  std::vector<std::string> flatten() const {
    std::vector<std::string> out;
    for (const Write& w : writes_) {
      char head[8];
      if (!w.dcHigh) {
        std::snprintf(head, sizeof(head), "C:%02X", w.bytes[0]);
        out.push_back(head);
      } else {
        std::string s = "D:";
        for (size_t i = 0; i < w.bytes.size(); ++i) {
          char b[4];
          std::snprintf(b, sizeof(b), i ? " %02X" : "%02X", w.bytes[i]);
          s += b;
        }
        out.push_back(s);
      }
    }
    return out;
  }

private:
  FakeGpio& gpio_;
  std::vector<Write> writes_;
};

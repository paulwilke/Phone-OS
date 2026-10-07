#pragma once

// The USB console: the configuration, the logs, the parents' page and a few diagnostics over the cable, without
// taking the SD card out. The parsing is plain C++ and tested on the host; the port behind it is a UartPort.
//
//   CONFIG LIST                       the files in /config with their sizes
//   CONFIG READ <file>                the file between markers (of a long log the end)
//   CONFIG WRITE <file> <n> <sum>     then n raw bytes; <sum> is their 16-bit sum
//   CONFIG RESTART                    restart the device so that a new file counts
//   CONFIG WEB <n>                    then n bytes of an HTTP request; the answer is "CONFIG WEB <m>" and m bytes
//                                     (the bridge of scripts/webbridge.py: the same checks and the same password
//                                     as over Wi-Fi, only the transport is the cable)
//   BATTERY                           the diagnosis of the fuel gauge and the charger (reads, writes nothing)
//   SMS <number> <text>               send a text by hand (`\n` in it is a line break)
// Each of them may also be written with the word CONFIG in front. A line that is none of these is not for the
// console and is ignored.
//
// **Over the cable only the known files are reachable**: settings.toml, contacts.toml and secrets.toml to read
// and write, and the logs system.log, system.old.log and messages.log to read. No path, nothing outside /config
// and the logs: whoever is at the cable sets the device up; any other file is the parents' page's business, with
// its password.
//
// The console never blocks: poll() takes what has arrived and returns. Raw bytes of a write or a request
// are collected over several polls and given up after ten seconds. Output never needs anybody to read it:
// with no computer attached the port drops it at once; with one attached, say() waits for it to take the bytes
// (a long answer is longer than the transmit buffer) but gives up after kConsoleWriteStallMs without progress. **Opening the serial port from a computer resets the ESP32**, which is why
// a tool that reads the logs restarts the device; the console needs no port to be opened to work.

#include <cstdint>
#include <string>
#include <vector>

#include "../hal/ConfigStore.h"
#include "Delay.h"
#include "MillisSource.h"
#include "UartPort.h"

namespace board {

constexpr size_t kConsoleLineMax = 320;
constexpr uint32_t kConsoleMaxFile = 8192;
constexpr uint32_t kConsoleMaxRequest = 9300;  // the head and the largest body the web parser accepts
constexpr uint32_t kConsoleRawTimeoutMs = 10000;
// How long say() waits for a connected computer to take output before it gives up: the time without any progress,
// so every accepted byte starts it anew. The USB transmit buffer is small and drains in milliseconds, so a reading
// computer is never near this; 300 ms is far above that and still short enough that a computer which stopped
// reading (a terminal paused, a tool that crashed) delays one loop pass by a third of a second, not for good.
constexpr uint32_t kConsoleWriteStallMs = 300;
constexpr uint32_t kConsoleWritePauseMs = 2;  // between two tries, so the wait does not spin

struct ConsoleCommand {
  enum class Kind : uint8_t { None, List, Read, Write, Restart, Web, Battery, Sms, Unknown };
  Kind kind = Kind::None;
  std::string file;   // the name without a path
  std::string path;   // where it is on the card
  std::string phone;
  std::string text;
  uint32_t length = 0;  // of the raw bytes that follow (Write, Web)
  uint16_t sum = 0;     // Write
  const char* error = nullptr;
};

ConsoleCommand parseConsoleLine(const std::string& line);
uint16_t consoleChecksum(const uint8_t* data, size_t len);

// What the console asks of the device.
class ConsoleHost {
public:
  virtual ~ConsoleHost() = default;
  virtual void restart() = 0;
  virtual bool sendText(const std::string& phone, const std::string& text) = 0;
  // Answers a raw HTTP request; false when there is no web service.
  virtual bool handleWeb(const std::vector<uint8_t>& request, std::string& response) = 0;
  virtual std::vector<std::string> batteryReport() = 0;
};

class Console {
public:
  Console(UartPort& port, ConfigStore& store, ConsoleHost& host, MillisSource& millis, Delay& delay)
      : port_(port), store_(store), host_(host), millis_(millis), delay_(delay) {}

  void poll();

private:
  void line(const std::string& text);
  void execute(const ConsoleCommand& c);
  void finishRaw();
  void say(const std::string& text);

  UartPort& port_;
  ConfigStore& store_;
  ConsoleHost& host_;
  MillisSource& millis_;
  Delay& delay_;

  std::string line_;
  bool discarding_ = false;  // a line that was too long: its rest is not a command
  ConsoleCommand raw_;       // the Write or Web that is waiting for its bytes
  bool collecting_ = false;
  std::vector<uint8_t> bytes_;
  uint32_t deadline_ = 0;
};

}  // namespace board

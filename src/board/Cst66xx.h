#pragma once

// The touch chip at 0x1A and the three keys under the screen, which are keys of the same chip.
//
// **Which chip sits there is checked, not assumed.** The wiki and the pin list name a CST328 (family
// cst3xx); what is fitted is a CST66xx. It answers on 0x1A, acknowledges every command and returns
// obligingly seven bytes, but they are its program memory, recognisable as machine code. The families
// differ in the length of their register addresses (cst3xx two bytes, cst66xx four) and of their reports
// (seven and nine bytes), so a driver for the wrong one reads nonsense. This driver asks the chip for its
// identity first and reports a family it does not speak as unknown instead of interpreting it.
//
// A report is read from 0xD0070000 (nine bytes):
//
//   buf[0..1]  checksum: 0x55 plus the sum of the bytes from buf[4]
//   buf[2]     kind: 0xFF position or key, 0xF0 gesture
//   buf[3]     low nibble: number of fingers, high nibble: number of sensor keys
//   buf[4..8]  one message of five bytes. **Keys come before fingers.**
//
// A finger message: x = buf[4] + ((buf[7] & 0x0F) << 8), y = buf[5] + ((buf[7] & 0xF0) << 4), pressure
// buf[6], buf[8] low nibble the finger number and high nibble the event (0 = lifted). The resolution is
// 240 by 320, which is the display's own coordinate system: nothing is converted or mirrored.
// A key message: buf[8] low nibble the key number (0 left, 1 middle, 2 right), high nibble non-zero when
// pressed. The keys are only reported when the chip is in its normal mode (see begin()).
//
// Only one finger and no gestures are evaluated; with nine bytes read there is exactly one message.

#include <cstddef>
#include <cstdint>

#include "../ui/Input.h"
#include "Delay.h"
#include "I2cBus.h"

namespace board {

namespace cst66xx {

constexpr uint8_t kKeyCount = 3;

struct Report {
  uint8_t kind = 0;
  uint8_t fingers = 0;
  uint8_t keys = 0;
  bool checksumOk = false;  // checkable only when exactly one message is present

  bool hasPosition = false;  // the first message is a finger
  int x = 0;
  int y = 0;
  bool down = false;  // the event nibble is not zero: the finger is on the screen

  bool hasKey = false;  // the first message is a sensor key
  uint8_t keyId = 0;
  bool keyPressed = false;
};

uint16_t checksum(const uint8_t* data, size_t len);
Report parseReport(const uint8_t* buf, size_t len);

}  // namespace cst66xx

class Cst66xxTouch {
public:
  Cst66xxTouch(I2cBus& bus, Delay& delay) : bus_(bus), delay_(delay) {}

  // Asks the chip who it is (up to five tries, taking it out of its boot loader between them) and puts it
  // into its normal mode. False when nothing answers or the family is not CST66xx; the chip is then not
  // read at all.
  bool begin();
  bool ready() const { return ready_; }
  // What the chip said about its coordinate space; 0 until begin() succeeded.
  uint16_t resolutionX() const { return resX_; }
  uint16_t resolutionY() const { return resY_; }

  // Reads one report and acknowledges it (otherwise the chip repeats it). False when the bus failed.
  bool poll(cst66xx::Report& out);

private:
  bool command(uint32_t reg);
  bool readAt(uint32_t reg, uint8_t* out, size_t len);

  I2cBus& bus_;
  Delay& delay_;
  bool ready_ = false;
  uint16_t resX_ = 0;
  uint16_t resY_ = 0;
};

// Turns reports into what the interface understands: one touch per contact, at the moment the finger
// lands, and one hardware key per press.
//
// A key is reported only with a correct checksum and only on the way down. An invented key press jumps
// to another screen, a swallowed one costs a second press, so keys are strict; the finger is not (a
// finger report is used as the device has proven it works, without a checksum).
class TouchDecoder {
public:
  bool decode(const cst66xx::Report& report, ui::Input& out);
  void reset();

private:
  bool fingerDown_ = false;
  bool keyDown_[cst66xx::kKeyCount] = {};
};

}  // namespace board

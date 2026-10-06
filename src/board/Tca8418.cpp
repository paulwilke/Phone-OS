#include "Tca8418.h"

#include "PinMap.h"

namespace board {

namespace {
constexpr uint8_t kRegConfig = 0x01;
constexpr uint8_t kRegInterruptStatus = 0x02;
constexpr uint8_t kRegEventCount = 0x03;
constexpr uint8_t kRegEvent = 0x04;
constexpr uint8_t kRegKeypadRows = 0x1D;     // KP_GPIO1: which rows are part of the matrix
constexpr uint8_t kRegKeypadColumns0 = 0x1E;  // KP_GPIO2: columns 0..7
constexpr uint8_t kRegKeypadColumns1 = 0x1F;  // KP_GPIO3: columns 8..9
constexpr uint8_t kRegGpiEventMode1 = 0x20;
constexpr uint8_t kRegGpiEventMode2 = 0x21;
constexpr uint8_t kRegGpiEventMode3 = 0x22;
// DEBOUNCE_DIS_1: a set bit switches the debounce of that row OFF. Meck wrote 0x03 under the comment "set
// debounce", which switched it off for the first two rows, twenty of the 35 keys; a bouncing key then
// types its letter twice. 0x00 is the chip's default.
constexpr uint8_t kRegDebounceDisable1 = 0x29;
constexpr uint8_t kClearAllInterrupts = 0x1F;
constexpr uint8_t kScannerOff = 0x00;
constexpr uint8_t kScannerOn = 0x11;  // key events interrupt enable + the interrupt line configuration
constexpr int kMaxQueued = 16;        // the chip's FIFO holds ten events
}  // namespace

bool Tca8418::drain() {
  for (int i = 0; i < kMaxQueued; ++i) {
    uint8_t count = 0;
    if (!bus_.readRegister(i2c::kKeyboard, kRegEventCount, count)) return false;
    if ((count & 0x0F) == 0) break;
    uint8_t discarded = 0;
    if (!bus_.readRegister(i2c::kKeyboard, kRegEvent, discarded)) return false;
  }
  return bus_.writeRegister(i2c::kKeyboard, kRegInterruptStatus, kClearAllInterrupts);
}

bool Tca8418::begin() {
  ready_ = false;
  if (!bus_.probe(i2c::kKeyboard)) return false;
  const uint8_t a = i2c::kKeyboard;
  const bool ok = bus_.writeRegister(a, kRegConfig, kScannerOff) && drain() &&
                  bus_.writeRegister(a, kRegGpiEventMode1, 0x00) && bus_.writeRegister(a, kRegGpiEventMode2, 0x00) &&
                  bus_.writeRegister(a, kRegGpiEventMode3, 0x00) && bus_.writeRegister(a, kRegKeypadRows, 0xFF) &&
                  bus_.writeRegister(a, kRegKeypadColumns0, 0xFF) && bus_.writeRegister(a, kRegKeypadColumns1, 0x03) &&
                  bus_.writeRegister(a, kRegDebounceDisable1, 0x00) &&
                  bus_.writeRegister(a, kRegInterruptStatus, kClearAllInterrupts) &&
                  bus_.writeRegister(a, kRegConfig, kScannerOn);
  if (!ok) return false;
  // The scanner settles, and what its first pass queued by accident is thrown away.
  delay_.delayMs(5);
  if (!drain()) return false;
  ready_ = true;
  return true;
}

bool Tca8418::next(KeyEvent& out) {
  if (!ready_) return false;
  uint8_t count = 0;
  if (!bus_.readRegister(i2c::kKeyboard, kRegEventCount, count) || (count & 0x0F) == 0) return false;
  uint8_t raw = 0;
  if (!bus_.readRegister(i2c::kKeyboard, kRegEvent, raw)) return false;
  bus_.writeRegister(i2c::kKeyboard, kRegInterruptStatus, kClearAllInterrupts);
  out.pressed = (raw & 0x80) != 0;
  out.code = raw & 0x7F;
  return out.code != 0;
}

}  // namespace board

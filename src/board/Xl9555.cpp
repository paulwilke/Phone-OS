#include "Xl9555.h"

#include "PinMap.h"

namespace board {

bool Xl9555::begin() {
  ready_ = false;
  if (!bus_.probe(i2c::kExpander)) return false;
  // All legs are outputs (0 = output). The levels are written after the direction, in the order the device
  // was brought up in; the boot state keeps the resets released and the modem off.
  if (!bus_.writeRegister(i2c::kExpander, xl9555::kConfig0, 0x00)) return false;
  if (!bus_.writeRegister(i2c::kExpander, xl9555::kConfig1, 0x00)) return false;
  if (!bus_.writeRegister(i2c::kExpander, xl9555::kOutput0, xl9555::kBootPort0)) return false;
  if (!bus_.writeRegister(i2c::kExpander, xl9555::kOutput1, xl9555::kBootPort1)) return false;
  port_[0] = xl9555::kBootPort0;
  port_[1] = xl9555::kBootPort1;
  ready_ = true;
  return true;
}

bool Xl9555::set(uint8_t leg, bool high) {
  if (!ready_ || leg >= leg::kCount) return false;
  const uint8_t port = leg / 8;
  const uint8_t bit = (uint8_t)(1u << (leg % 8));
  const uint8_t next = high ? (uint8_t)(port_[port] | bit) : (uint8_t)(port_[port] & ~bit);
  if (!bus_.writeRegister(i2c::kExpander, port == 0 ? xl9555::kOutput0 : xl9555::kOutput1, next)) return false;
  port_[port] = next;
  return true;
}

bool Xl9555::level(uint8_t leg) const {
  if (leg >= leg::kCount) return false;
  return (port_[leg / 8] & (1u << (leg % 8))) != 0;
}

}  // namespace board

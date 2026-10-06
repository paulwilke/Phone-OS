#include "Cst66xx.h"

#include "../ui/Display.h"
#include "PinMap.h"

namespace board {

namespace cst66xx {

uint16_t checksum(const uint8_t* data, size_t len) {
  uint16_t sum = 0x55;
  for (size_t i = 0; i < len; ++i) sum = (uint16_t)(sum + data[i]);
  return sum;
}

Report parseReport(const uint8_t* buf, size_t len) {
  Report r;
  if (!buf || len < 9) return r;
  r.kind = buf[2];
  r.fingers = (uint8_t)(buf[3] & 0x0F);
  r.keys = (uint8_t)((buf[3] & 0xF0) >> 4);
  const unsigned messages = (unsigned)r.fingers + (unsigned)r.keys;
  // The sum covers all messages, and nine bytes hold exactly one. With more the rest is missing, so it
  // stays unconfirmed and the caller decides.
  if (messages == 1) r.checksumOk = checksum(&buf[4], 5) == (uint16_t)(buf[0] | ((uint16_t)buf[1] << 8));
  if (r.kind != 0xFF || messages == 0) return r;

  if (r.keys != 0) {
    r.hasKey = true;
    r.keyId = (uint8_t)(buf[8] & 0x0F);
    r.keyPressed = (buf[8] >> 4) != 0;
    return r;  // a finger would come behind it, outside the nine bytes
  }
  r.hasPosition = true;
  r.x = buf[4] + (((int)(buf[7] & 0x0F)) << 8);
  r.y = buf[5] + (((int)(buf[7] & 0xF0)) << 4);
  r.down = (buf[8] >> 4) != 0;
  return r;
}

}  // namespace cst66xx

namespace {
// Registers are four bytes long; a command is an address without data.
constexpr uint32_t kInfo = 0xD0030000;
constexpr uint32_t kReport = 0xD0070000;
constexpr uint32_t kLeaveBootloader = 0xD0000400;
constexpr uint32_t kAcknowledge = 0xD00002AB;
constexpr size_t kInfoLength = 50;
constexpr int kIdentifyTries = 5;
// The wire reads at most this much at once; the chip keeps handing out the following bytes.
constexpr size_t kChunk = 32;
}  // namespace

bool Cst66xxTouch::command(uint32_t reg) {
  const uint8_t frame[4] = {(uint8_t)(reg >> 24), (uint8_t)(reg >> 16), (uint8_t)(reg >> 8), (uint8_t)reg};
  return bus_.write(i2c::kTouch, frame, 4);
}

bool Cst66xxTouch::readAt(uint32_t reg, uint8_t* out, size_t len) {
  // The address is written and finished with a stop, and the read is a transaction of its own, a moment
  // later: with a repeated start this chip returns garbage (different bytes at every try, while its
  // interrupt line says it has data).
  if (!command(reg)) return false;
  delay_.delayUs(200);
  for (size_t done = 0; done < len;) {
    const size_t piece = len - done > kChunk ? kChunk : len - done;
    if (!bus_.read(i2c::kTouch, out + done, piece)) return false;
    done += piece;
  }
  return true;
}

bool Cst66xxTouch::begin() {
  ready_ = false;
  resX_ = resY_ = 0;
  if (!bus_.probe(i2c::kTouch)) return false;
  for (int attempt = 0; attempt < kIdentifyTries; ++attempt) {
    uint8_t info[kInfoLength] = {};
    if (readAt(kInfo, info, sizeof(info)) && info[2] == 0xCA && info[3] == 0xCA) {
      resX_ = (uint16_t)((info[29] << 8) | info[28]);
      resY_ = (uint16_t)((info[31] << 8) | info[30]);
      // The normal mode, in the order the manufacturer's driver sends it. Without these commands the chip
      // delivers fingers but not the sensor keys (confirmed on the device on 2026-10-01); nobody documents
      // what each one means, so the sequence is kept as it is.
      command(kLeaveBootloader);
      delay_.delayMs(1);
      command(kLeaveBootloader);
      command(0xD0000000);
      command(0xD0000C00);
      command(0xD0000100);
      ready_ = true;
      return true;
    }
    delay_.delayMs(10);
    command(kLeaveBootloader);
  }
  return false;
}

bool Cst66xxTouch::poll(cst66xx::Report& out) {
  if (!ready_) return false;
  uint8_t buf[9] = {};
  if (!readAt(kReport, buf, sizeof(buf))) return false;
  out = cst66xx::parseReport(buf, sizeof(buf));
  command(kAcknowledge);
  return true;
}

void TouchDecoder::reset() {
  fingerDown_ = false;
  for (bool& k : keyDown_) k = false;
}

bool TouchDecoder::decode(const cst66xx::Report& r, ui::Input& out) {
  if (r.hasKey) {
    if (!r.checksumOk || r.keyId >= cst66xx::kKeyCount) return false;
    const bool edge = r.keyPressed && !keyDown_[r.keyId];
    keyDown_[r.keyId] = r.keyPressed;
    if (!edge) return false;
    static const ui::Key keys[cst66xx::kKeyCount] = {ui::Key::Home, ui::Key::Inbox, ui::Key::Compose};
    out = ui::Input::pressed(keys[r.keyId]);
    return true;  // a key is no touch of the screen
  }
  const bool down = r.hasPosition && r.down;
  const bool landed = down && !fingerDown_;
  fingerDown_ = down;
  if (!landed) return false;
  int x = r.x, y = r.y;
  if (x < 0) x = 0;
  if (x >= ui::kScreenWidth) x = ui::kScreenWidth - 1;
  if (y < 0) y = 0;
  if (y >= ui::kScreenHeight) y = ui::kScreenHeight - 1;
  out = ui::Input::touch(x, y);
  return true;
}

}  // namespace board

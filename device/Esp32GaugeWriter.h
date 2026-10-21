#pragma once

// Writes the battery size into the fuel gauge with SensorLib's procedure (unseal, enter the configuration
// update, write the data memory with its checksum, leave, seal again). That procedure is the library's
// (MIT); the decision whether to write at all is board::ensureCapacity()'s and is tested on the host.
//
// The library writes nothing but the two capacities and keeps its procedure private, so the other data-memory
// items (the end-of-discharge thresholds, D-42) go through its public raw register calls and repeat the
// manual's sequence (BQ27220 reference manual SLUUBD4A p.65-66, docs/hardware/fuel-gauge-data-memory.md). The bytes
// of a block come from board::dataMemoryBlock(), which is host-tested; which items to write is
// board::ensureEndOfDischarge()'s.

#include <Arduino.h>
#include <GaugeDrv.hpp>
#include <Wire.h>

#include <board/FuelGauge.h>

class Esp32GaugeWriter : public board::GaugeCapacityWriter {
public:
  bool setCapacities(uint16_t designMah, uint16_t fullChargeMah) override {
    // The bus is already started by the caller; no pins are passed so the library does not start it again.
    if (!gauge_.begin(Wire)) return false;
    return gauge_.setNewCapacity(designMah, fullChargeMah);
  }

  bool readDataMemory(board::DataMemoryWord* words, size_t count) override {
    bool sealed = false;
    if (!enterUpdate(sealed)) return false;
    bool ok = true;
    for (size_t i = 0; ok && i < count; ++i) ok = readWord(words[i]);
    // Nothing changed, so the gauge is not restarted: the restart (0x0091) is for a write.
    return leaveUpdate(kExitKeep, sealed) && ok;
  }

  bool writeDataMemory(const board::DataMemoryWord* words, size_t count) override {
    bool sealed = false;
    if (!enterUpdate(sealed)) return false;
    bool ok = true;
    for (size_t i = 0; ok && i < count; ++i) ok = writeWord(words[i]);
    // The library leaves every write with the restart, and that is the way that has taken on the device;
    // which exit makes new thresholds count, the manual does not say.
    return leaveUpdate(kExitReinit, sealed) && ok;
  }

private:
  static constexpr uint8_t kRegControl = 0x00;
  static constexpr uint8_t kRegOperationStatus = 0x3A;
  static constexpr uint8_t kRegAccessControl = 0x3E;
  static constexpr uint8_t kRegMacData = 0x40;
  static constexpr uint8_t kRegMacDataSum = 0x60;
  static constexpr uint8_t kRegMacDataLen = 0x61;
  static constexpr uint16_t kStatusConfigUpdate = 0x0400;
  static constexpr uint16_t kEnterUpdate = 0x0090;
  static constexpr uint16_t kExitReinit = 0x0091;
  static constexpr uint16_t kExitKeep = 0x0092;
  static constexpr uint16_t kSeal = 0x0030;
  static constexpr uint8_t kAccessSealed = 3;
  static constexpr uint8_t kAccessFull = 1;
  // The manual allows up to a second each way (p.65-66) and asks for at least 1.1 s before the flag (p.75).
  static constexpr uint32_t kEnterTimeoutMs = 2000;
  static constexpr uint32_t kExitTimeoutMs = 3000;

  bool readStatus(uint16_t& status) {
    uint8_t b[2];
    if (gauge_.readRegBuff(kRegOperationStatus, b, 2) < 0) return false;
    status = (uint16_t)(b[0] | (b[1] << 8));
    return true;
  }

  bool command(uint16_t subcommand) {
    uint8_t b[2] = {(uint8_t)(subcommand & 0xFF), (uint8_t)(subcommand >> 8)};
    const bool ok = gauge_.writeRegBuff(kRegControl, b, 2) >= 0;
    delay(10);
    return ok;
  }

  bool waitForUpdateMode(bool on, uint32_t timeoutMs) {
    const uint32_t start = millis();
    uint16_t status = 0;
    while (millis() - start < timeoutMs) {
      if (readStatus(status) && ((status & kStatusConfigUpdate) != 0) == on) return true;
      delay(100);
    }
    return false;
  }

  // The keys are the manual's example's (p.65) and the library's: unseal 0x0414, 0x3672; full access
  // 0xFFFF twice. A chip that was sealed is sealed again on the way out.
  bool enterUpdate(bool& sealed) {
    if (!gauge_.begin(Wire)) return false;
    uint16_t status = 0;
    if (!readStatus(status)) return false;
    const uint8_t access = (uint8_t)((status >> 1) & 0x03);
    sealed = access == kAccessSealed;
    if (sealed && !(command(0x0414) && command(0x3672))) return false;
    if (access != kAccessFull && !(command(0xFFFF) && command(0xFFFF))) return false;
    if (command(kEnterUpdate) && waitForUpdateMode(true, kEnterTimeoutMs)) return true;
    // The library returns here and leaves the chip unsealed in the update mode until its four-minute timeout.
    leaveUpdate(kExitKeep, sealed);
    return false;
  }

  bool leaveUpdate(uint16_t exit, bool sealed) {
    const bool left = command(exit) && waitForUpdateMode(false, kExitTimeoutMs);
    if (sealed) command(kSeal);
    return left;
  }

  // The address one byte at a time, as the library does: above 100 kHz the manual asks for single-byte writes.
  bool selectAddress(const board::DataMemoryBlock& block) {
    const bool ok = gauge_.writeReg(kRegAccessControl, block.bytes[0]) >= 0;
    delay(10);
    const bool ok2 = gauge_.writeReg(kRegAccessControl + 1, block.bytes[1]) >= 0;
    delay(10);
    return ok && ok2;
  }

  bool readWord(board::DataMemoryWord& word) {
    if (!selectAddress(board::dataMemoryBlock(word.address, 0))) return false;
    uint8_t b[2];
    if (gauge_.readRegBuff(kRegMacData, b, 2) < 0) return false;
    word.value = (uint16_t)((b[0] << 8) | b[1]);
    return true;
  }

  bool writeWord(const board::DataMemoryWord& word) {
    board::DataMemoryBlock block = board::dataMemoryBlock(word.address, word.value);
    if (!selectAddress(block)) return false;
    // The chip takes the block into its RAM when the checksum and the length arrive.
    const bool ok = gauge_.writeRegBuff(kRegMacData, &block.bytes[2], 2) >= 0 &&
                    gauge_.writeReg(kRegMacDataSum, block.checksum) >= 0 &&
                    gauge_.writeReg(kRegMacDataLen, block.length) >= 0;
    delay(10);
    return ok;
  }

  GaugeBQ27220 gauge_;
};

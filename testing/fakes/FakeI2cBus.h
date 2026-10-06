#pragma once

// A scripted I2C bus: every chip is a file of byte registers, so a driver is exercised through the real
// transactions and a test reads back what it wrote.
//
// Chips: addChip(address) makes one answer (an address nobody added is not acknowledged, which is what a
// missing chip does). A write puts its data into the registers from the first byte on, a read returns the
// registers from the last register byte written, both with auto-increment, as the chips on this board do.
//   setRegister(address, reg, value), setRegisters(address, reg, {bytes…})   scripting
//   setRegister16(address, reg, value)                                       little endian
//   removeChip(address)                                                      the chip stops answering
//   refuseWrites(address, bool)                                              the chip acknowledges but a write fails
//   refuseRegisterWrite(address, reg)                                        a write that touches this register fails
// Reading back: reg(address, reg), reg16(), transactions() (every call in order, stamped with the
// delay's elapsed time when one was given), writesTo(address) (the data of each write to a chip),
// writeCount(address, reg) (how many writes touched a register, so "write only on a difference" is a test).

#include <cstdint>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include <board/I2cBus.h>
#include "FakeDelay.h"

class FakeI2cBus : public I2cBus {
public:
  struct Transaction {
    uint32_t atMs;
    uint8_t address;
    bool isWrite;  // false: a read, or the read half of a write-read
    std::vector<uint8_t> data;
  };

  explicit FakeI2cBus(const FakeDelay* time = nullptr) : time_(time) {}

  bool write(uint8_t address, const uint8_t* data, size_t len) override {
    if (!chips_.count(address)) return false;
    log(address, true, std::vector<uint8_t>(data, data + len));
    if (refuseAll_.count(address)) return false;
    if (len == 0) return true;
    for (size_t i = 1; i < len; ++i)
      if (refuseReg_.count({address, (uint8_t)(data[0] + i - 1)})) return false;
    for (size_t i = 1; i < len; ++i) {
      const uint8_t reg = (uint8_t)(data[0] + i - 1);
      regs_[{address, reg}] = data[i];
      ++writes_[{address, reg}];
    }
    pointer_[address] = (uint8_t)(data[0] + (len > 1 ? len - 1 : 0));
    return true;
  }

  bool read(uint8_t address, uint8_t* out, size_t len) override {
    if (!chips_.count(address)) return false;
    uint8_t reg = pointer_[address];
    for (size_t i = 0; i < len; ++i) out[i] = regs_[{address, reg++}];
    pointer_[address] = reg;
    log(address, false, std::vector<uint8_t>(out, out + len));
    return true;
  }

  bool writeRead(uint8_t address, const uint8_t* data, size_t len, uint8_t* out, size_t outLen) override {
    if (!chips_.count(address)) return false;
    log(address, true, std::vector<uint8_t>(data, data + len));
    if (len > 0) pointer_[address] = data[len - 1];
    return read(address, out, outLen);
  }

  // Scripting.
  void addChip(uint8_t address) { chips_.insert(address); }
  void removeChip(uint8_t address) { chips_.erase(address); }
  void setRegister(uint8_t address, uint8_t reg, uint8_t value) { regs_[{address, reg}] = value; }
  void setRegisters(uint8_t address, uint8_t reg, const std::vector<uint8_t>& bytes) {
    for (uint8_t b : bytes) regs_[{address, reg++}] = b;
  }
  void setRegister16(uint8_t address, uint8_t reg, uint16_t value) {
    setRegister(address, reg, (uint8_t)(value & 0xFF));
    setRegister(address, (uint8_t)(reg + 1), (uint8_t)(value >> 8));
  }
  void refuseWrites(uint8_t address, bool refuse) {
    if (refuse) refuseAll_.insert(address);
    else refuseAll_.erase(address);
  }
  void refuseRegisterWrite(uint8_t address, uint8_t reg) { refuseReg_.insert({address, reg}); }

  // Reading back.
  uint8_t reg(uint8_t address, uint8_t reg) const {
    const auto it = regs_.find({address, reg});
    return it == regs_.end() ? 0 : it->second;
  }
  uint16_t reg16(uint8_t address, uint8_t reg) const {
    return (uint16_t)(this->reg(address, reg) | (this->reg(address, (uint8_t)(reg + 1)) << 8));
  }
  const std::vector<Transaction>& transactions() const { return log_; }
  std::vector<std::vector<uint8_t>> writesTo(uint8_t address) const {
    std::vector<std::vector<uint8_t>> out;
    for (const Transaction& t : log_)
      if (t.address == address && t.isWrite) out.push_back(t.data);
    return out;
  }
  int writeCount(uint8_t address, uint8_t reg) const {
    const auto it = writes_.find({address, reg});
    return it == writes_.end() ? 0 : it->second;
  }

private:
  void log(uint8_t address, bool isWrite, std::vector<uint8_t> data) {
    log_.push_back({time_ ? time_->elapsedMs() : 0, address, isWrite, std::move(data)});
  }

  const FakeDelay* time_;
  std::set<uint8_t> chips_;
  std::set<uint8_t> refuseAll_;
  std::set<std::pair<uint8_t, uint8_t>> refuseReg_;
  std::map<std::pair<uint8_t, uint8_t>, uint8_t> regs_;
  std::map<std::pair<uint8_t, uint8_t>, int> writes_;
  std::map<uint8_t, uint8_t> pointer_;
  std::vector<Transaction> log_;
};

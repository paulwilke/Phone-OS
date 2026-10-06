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
//   addFifo(address, dataReg, countReg)  reading dataReg takes the next queued byte (0 when empty); reading
//                                        countReg gives how many are queued (low nibble, as the keyboard chip does)
//   pushFifo(address, dataReg, bytes)    queue bytes
//   setReply(address, command, reply)    a chip without registers: after a write of exactly `command` the next
//                                        read returns `reply` (padded with zeros), and further reads continue
//                                        behind it; the write is logged as usual
// Reading back: reg(address, reg), reg16(), transactions() (every call in order, stamped with the
// delay's elapsed time when one was given), writesTo(address) (the data of each write to a chip),
// writeCount(address, reg) (how many writes touched a register, so "write only on a difference" is a test).

#include <cstdint>
#include <deque>
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
    pendingReply_.erase(address);
    replyAt_[address] = 0;
    const auto reply = replies_.find({address, std::vector<uint8_t>(data, data + len)});
    if (reply != replies_.end()) {
      pendingReply_[address] = reply->second;
      return true;
    }
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
    const auto pending = pendingReply_.find(address);
    if (pending != pendingReply_.end()) {
      // Reads continue where the last one stopped, so a long reply can be taken in pieces.
      size_t& at = replyAt_[address];
      for (size_t i = 0; i < len; ++i, ++at) out[i] = at < pending->second.size() ? pending->second[at] : 0;
      log(address, false, std::vector<uint8_t>(out, out + len));
      return true;
    }
    uint8_t reg = pointer_[address];
    for (size_t i = 0; i < len; ++i) {
      const auto fifo = fifos_.find({address, reg});
      if (fifo != fifos_.end()) {
        out[i] = fifo->second.empty() ? 0 : fifo->second.front();
        if (!fifo->second.empty()) fifo->second.pop_front();
      } else {
        out[i] = counts_.count({address, reg}) ? (uint8_t)(fifoSize(address, counts_[{address, reg}]) & 0x0F) : regs_[{address, reg}];
      }
      ++reg;
    }
    pointer_[address] = reg;
    log(address, false, std::vector<uint8_t>(out, out + len));
    return true;
  }

  bool writeRead(uint8_t address, const uint8_t* data, size_t len, uint8_t* out, size_t outLen) override {
    if (!chips_.count(address)) return false;
    ++repeatedStarts_[address];
    log(address, true, std::vector<uint8_t>(data, data + len));
    pendingReply_.erase(address);
    replyAt_[address] = 0;
    const auto reply = replies_.find({address, std::vector<uint8_t>(data, data + len)});
    if (reply != replies_.end()) pendingReply_[address] = reply->second;
    else if (len > 0) pointer_[address] = data[len - 1];
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
  void addFifo(uint8_t address, uint8_t dataReg, uint8_t countReg) {
    fifos_[{address, dataReg}];
    counts_[{address, countReg}] = dataReg;
  }
  void pushFifo(uint8_t address, uint8_t dataReg, const std::vector<uint8_t>& bytes) {
    for (uint8_t b : bytes) fifos_[{address, dataReg}].push_back(b);
  }
  void setReply(uint8_t address, const std::vector<uint8_t>& command, const std::vector<uint8_t>& reply) {
    replies_[{address, command}] = reply;
  }

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
  // How many write-then-read transactions with a repeated start the chip saw (some chips return garbage for those).
  int repeatedStarts(uint8_t address) const {
    const auto it = repeatedStarts_.find(address);
    return it == repeatedStarts_.end() ? 0 : it->second;
  }
  int writeCount(uint8_t address, uint8_t reg) const {
    const auto it = writes_.find({address, reg});
    return it == writes_.end() ? 0 : it->second;
  }

private:
  size_t fifoSize(uint8_t address, uint8_t dataReg) { return fifos_[{address, dataReg}].size(); }
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
  std::map<std::pair<uint8_t, uint8_t>, std::deque<uint8_t>> fifos_;
  std::map<std::pair<uint8_t, uint8_t>, uint8_t> counts_;  // (address, countReg) -> dataReg
  std::map<std::pair<uint8_t, std::vector<uint8_t>>, std::vector<uint8_t>> replies_;
  std::map<uint8_t, std::vector<uint8_t>> pendingReply_;
  std::map<uint8_t, size_t> replyAt_;
  std::map<uint8_t, int> repeatedStarts_;
  std::vector<Transaction> log_;
};

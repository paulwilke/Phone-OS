#pragma once

// The fuel gauge's capacity writer on the scripted bus: it does what the chip does, in its registers
// (design capacity 0x3C, full-charge capacity 0x12), so that the check that reads back sees the result.
//   refuse()          the chip refuses the write (the call returns false, the registers stay)
//   acceptButKeep()   the call succeeds but the registers do not change (a write that did not take)
// Reading back: calls() and the last values given.
//
// The data memory: a map of 16-bit items by address, holding the chip's defaults of the end-of-discharge items
// (manual SLUUBD4A p.30: Gauging Configuration 0x102A, Fixed EDV 0/1/2 3031/3385/3501 mV).
//   setDataMemory(address, value)   scripting
//   refuseDataMemory()              a write returns false and nothing changes
//   failDataMemoryRead()            reading returns false
//   keepDataMemory()                a write succeeds but the items do not change
// Reading back: dataMemory(address), dataMemoryWrites() (every address written, in order), dataMemoryReads().

#include <map>
#include <vector>

#include <board/FuelGauge.h>
#include "FakeI2cBus.h"

class FakeGaugeWriter : public board::GaugeCapacityWriter {
public:
  explicit FakeGaugeWriter(FakeI2cBus& bus) : bus_(bus) {}

  bool setCapacities(uint16_t designMah, uint16_t fullChargeMah) override {
    ++calls_;
    lastDesign_ = designMah;
    lastFull_ = fullChargeMah;
    if (refuse_) return false;
    if (!keep_) {
      bus_.setRegister16(0x55, 0x3C, designMah);
      bus_.setRegister16(0x55, 0x12, fullChargeMah);
    }
    return true;
  }

  bool readDataMemory(board::DataMemoryWord* words, size_t count) override {
    ++dataMemoryReads_;
    if (failDataMemoryRead_) return false;
    for (size_t i = 0; i < count; ++i) words[i].value = dataMemory(words[i].address);
    return true;
  }

  bool writeDataMemory(const board::DataMemoryWord* words, size_t count) override {
    if (refuseDataMemory_) return false;
    for (size_t i = 0; i < count; ++i) {
      dataMemoryWrites_.push_back(words[i].address);
      if (!keepDataMemory_) dataMemory_[words[i].address] = words[i].value;
    }
    return true;
  }

  void setDataMemory(uint16_t address, uint16_t value) { dataMemory_[address] = value; }
  uint16_t dataMemory(uint16_t address) const {
    const auto it = dataMemory_.find(address);
    return it == dataMemory_.end() ? 0 : it->second;
  }
  void refuseDataMemory(bool on = true) { refuseDataMemory_ = on; }
  void failDataMemoryRead(bool on = true) { failDataMemoryRead_ = on; }
  void keepDataMemory(bool on = true) { keepDataMemory_ = on; }
  const std::vector<uint16_t>& dataMemoryWrites() const { return dataMemoryWrites_; }
  int dataMemoryReads() const { return dataMemoryReads_; }

  void refuse(bool on = true) { refuse_ = on; }
  void acceptButKeep(bool on = true) { keep_ = on; }
  int calls() const { return calls_; }
  uint16_t lastDesign() const { return lastDesign_; }
  uint16_t lastFullCharge() const { return lastFull_; }

private:
  FakeI2cBus& bus_;
  bool refuse_ = false;
  bool keep_ = false;
  int calls_ = 0;
  uint16_t lastDesign_ = 0;
  uint16_t lastFull_ = 0;
  std::map<uint16_t, uint16_t> dataMemory_{{0x929B, 0x102A}, {0x92B4, 3031}, {0x92B7, 3385}, {0x92BA, 3501}};
  bool refuseDataMemory_ = false;
  bool failDataMemoryRead_ = false;
  bool keepDataMemory_ = false;
  std::vector<uint16_t> dataMemoryWrites_;
  int dataMemoryReads_ = 0;
};

#pragma once

// The fuel gauge's capacity writer on the scripted bus: it does what the chip does, in its registers
// (design capacity 0x3C, full-charge capacity 0x12), so that the check that reads back sees the result.
//   refuse()          the chip refuses the write (the call returns false, the registers stay)
//   acceptButKeep()   the call succeeds but the registers do not change (a write that did not take)
// Reading back: calls() and the last values given.

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
};

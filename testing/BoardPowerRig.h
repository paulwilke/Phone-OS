#pragma once

// The device's power side on scripted chips: the expander, the fuel gauge and the charger on a FakeI2cBus, a
// modem fake for the cellular domain and a log. Start-up has run (the rails nobody asked for are off, the
// gauge holds the configured capacity); the device is on its battery at 80 %.
//
// scriptMeasured() puts in what was read from the real device on 2026-10-06: a full battery on a cable,
// the charger saying "charge done", and a gauge that still held the factory capacity and said 56 %.
// The hooks at the bottom are those of the Power contract (testing/contracts/PowerContract.h).

#include <board/BoardGnss.h>
#include <board/BoardPower.h>
#include <board/PinMap.h>
#include <fakes/FakeClock.h>
#include <fakes/FakeDelay.h>
#include <fakes/FakeEventLog.h>
#include <fakes/FakeGaugeWriter.h>
#include <fakes/FakeGpio.h>
#include <fakes/FakeI2cBus.h>
#include <fakes/FakeModem.h>
#include <fakes/FakeUartPort.h>
#include <NmeaSource.h>

struct BoardPowerRig {
  static constexpr uint16_t kConfiguredMah = 1500;

  FakeDelay delay;
  FakeI2cBus bus{&delay};
  FakeGpio gpio{&delay};
  FakeClock clock;
  FakeModem modem{clock};
  FakeEventLog log;
  FakeGaugeWriter writer{bus};
  board::Xl9555 expander{bus};
  board::Charger charger{bus};
  board::FuelGauge gauge{bus};
  board::BoardPower impl{expander, gpio, charger, gauge, modem};

  // `start` false leaves the chips attached and scripted but does not run the start-up.
  explicit BoardPowerRig(bool start = true) {
    bus.addChip(board::i2c::kExpander);
    bus.addChip(board::i2c::kFuelGauge);
    bus.addChip(board::i2c::kChargerSy6970);
    expander.begin();
    scriptGauge(3900, 80, kConfiguredMah, kConfiguredMah);
    scriptCharger(board::i2c::kChargerSy6970, 0x00);
    if (start) impl.begin(kConfiguredMah, writer, log);
  }

  void scriptGauge(int millivolts, int percent, int designMah, int fullChargeMah) {
    const uint8_t a = board::i2c::kFuelGauge;
    bus.setRegister16(a, 0x08, (uint16_t)millivolts);
    bus.setRegister16(a, 0x0C, (uint16_t)(int16_t)-35);
    bus.setRegister16(a, 0x10, (uint16_t)(fullChargeMah * percent / 100));
    bus.setRegister16(a, 0x12, (uint16_t)fullChargeMah);
    bus.setRegister16(a, 0x14, (uint16_t)(int16_t)-35);
    bus.setRegister16(a, 0x16, 600);
    bus.setRegister16(a, 0x28, 2981);
    bus.setRegister16(a, 0x2A, 0);
    bus.setRegister16(a, 0x2C, (uint16_t)percent);
    bus.setRegister16(a, 0x3C, (uint16_t)designMah);
  }

  // The charger's registers as the real one had them; `status` is register 0x0B.
  void scriptCharger(uint8_t address, uint8_t status) {
    bus.setRegister(address, 0x00, 0x48);
    bus.setRegister(address, 0x04, 0x20);
    bus.setRegister(address, 0x05, 0x13);
    bus.setRegister(address, 0x06, 0x5E);
    bus.setRegister(address, 0x09, 0x44);
    bus.setRegister(address, 0x0B, status);
    bus.setRegister(address, 0x0E, 0x5D);
    bus.setRegister(address, 0x0F, 0x5E);
    bus.setRegister(address, 0x11, (uint8_t)((status & 0x04) ? 0x98 : 0x00));
  }

  void scriptMeasured() {
    scriptGauge(4118, 56, 3000, 3000);
    bus.setRegister16(board::i2c::kFuelGauge, 0x10, 1655);
    bus.setRegister16(board::i2c::kFuelGauge, 0x16, 2858);
    scriptCharger(board::i2c::kChargerSy6970, 0xBE);
  }

  // The charger's status register, with the chip on its address.
  void setStatus(uint8_t status) { bus.setRegister(chargerAddress(), 0x0B, status); }
  uint8_t chargerAddress() const {
    return charger.found() ? charger.address() : board::i2c::kChargerSy6970;
  }

  // The Power contract's rig.
  Power& power() { return impl; }
  bool breakDomain(PowerDomain d) {
    if (d != PowerDomain::Positioning && d != PowerDomain::Radio) return false;
    bus.refuseRegisterWrite(board::i2c::kExpander, board::xl9555::kOutput0);
    return true;
  }
  bool plugIn(ChargeState s) {
    setStatus(s == ChargeState::Full ? 0x1C : 0x14);
    return true;
  }
  bool unplug() {
    setStatus(0x00);
    return true;
  }
  bool forbidPowerOff() { return plugIn(ChargeState::Charging); }
};

// The receiver: BoardGnss on a scripted UART behind the power side of the board. The contract's `advanceSeconds`
// moves the clock a second at a time and lets the scripted receiver say its sentences after each.
struct BoardGnssRig {
  BoardPowerRig board;
  FakeUartPort uart;
  NmeaSource receiver{uart, board.impl};
  board::BoardGnss impl{board.impl, uart, board.clock};

  GnssReceiver& gnss() { return impl; }
  Power& power() { return board.impl; }
  void advanceSeconds(uint32_t seconds) {
    for (uint32_t i = 0; i < seconds; ++i) {
      board.clock.advanceSeconds(1);
      receiver.step();
    }
  }
  bool scriptFix(double lat, double lon, float hdop, uint32_t afterSeconds) {
    receiver.script(lat, lon, hdop, afterSeconds);
    return true;
  }
  bool scriptNoFix() {
    receiver.never();
    return true;
  }
};

#pragma once

// The battery charger. Newer boards carry an SY6970 at 0x6A, older ones a BQ25896 at 0x6B; the registers
// are the same, so one class serves both and finds the chip by asking both addresses. A driver that knew
// one address took the bus of an older board for broken.
//
// Decoding is a pure function of the 21 registers 0x00..0x14, tested with values measured on the device.
// Reading the registers writes nothing. (Reading 0x0C clears the chip's fault latch, as the chip does for
// any reader; the diagnostic reads it once.)

#include <cstdint>

#include "I2cBus.h"

namespace board {

enum class ChargerChip : uint8_t { None, Sy6970, Bq25896 };

enum class ChargePhase : uint8_t { NotCharging, PreCharge, FastCharge, Done };

// What the charger says is on the input (register 0x0B bits 7:5).
enum class InputKind : uint8_t { None, UsbHost, UsbCharging, UsbDedicated, Adjustable, UnknownAdapter, NonStandard, OnTheGo };

constexpr uint8_t kChargerRegisterCount = 0x15;

struct ChargerStatus {
  // 0x0B: the power-good bit says a supply is attached, whatever the charge state is. A full battery on a
  // cable reads "not charging" or "done", and decisions about a cable (nothing sleeps while one is attached;
  // the USB console disappears in light sleep) follow this bit and nothing else.
  bool powerGood = false;
  ChargePhase phase = ChargePhase::NotCharging;
  InputKind input = InputKind::None;
  // 0x00: the input current limit the chip applies, in mA, and whether the limit register is in force.
  int inputCurrentLimitMa = 0;
  bool inputLimitEnabled = false;
  // 0x04, 0x05, 0x06: what the chip is set to charge with.
  int chargeCurrentMa = 0;
  int prechargeCurrentMa = 0;
  int terminationCurrentMa = 0;
  int chargeVoltageMv = 0;
  // 0x0E, 0x0F, 0x11, 0x12: what it measures.
  int batteryMv = 0;
  int systemMv = 0;
  int vbusMv = 0;
  int chargingCurrentMa = 0;
  bool vbusGood = false;
  uint8_t fault = 0;  // 0x0C, as the chip has it
};

ChargerStatus decodeCharger(const uint8_t (&registers)[kChargerRegisterCount]);

const char* chargerChipName(ChargerChip chip);
const char* chargePhaseName(ChargePhase phase);
const char* inputKindName(InputKind kind);

class Charger {
public:
  explicit Charger(I2cBus& bus) : bus_(bus) {}

  // Asks 0x6A, then 0x6B. False when neither answers: the charger is then absent, which is not the same as
  // a broken bus.
  bool detect();
  bool found() const { return chip_ != ChargerChip::None; }
  ChargerChip chip() const { return chip_; }
  uint8_t address() const { return address_; }

  // The status register alone (0x0B), for the questions that are asked often.
  bool readStatus(ChargerStatus& out);
  // All registers 0x00..0x14, raw and decoded.
  bool readAll(uint8_t (&registers)[kChargerRegisterCount], ChargerStatus& out);

  // Disconnects the battery from the board (BATFET_DIS in register 0x09), the way the device is switched
  // off. Reads the register first and writes only the one bit. The caller refuses it while a cable feeds
  // the board, because VBUS keeps the board powered then.
  bool disconnectBattery();

private:
  I2cBus& bus_;
  ChargerChip chip_ = ChargerChip::None;
  uint8_t address_ = 0;
};

}  // namespace board

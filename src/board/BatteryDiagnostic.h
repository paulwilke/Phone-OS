#pragma once

// The diagnosis of both power chips, so that nobody has to flash a special program to find out why the
// battery reading looks wrong (it took one on 2026-10-06). It only reads: the fuel gauge (voltage,
// current, remaining, full-charge and design capacity, cycle count, state of charge, temperature) and the
// charger (all registers 0x00..0x14, decoded). The text is for a console line by line, and the same data
// is there for a web state.

#include <string>
#include <vector>

#include "Charger.h"
#include "FuelGauge.h"

namespace board {

struct BatteryDiagnostic {
  bool gaugeAnswers = false;
  GaugeReading gauge;
  bool chargerFound = false;
  bool chargerAnswers = false;
  ChargerChip chargerChip = ChargerChip::None;
  uint8_t chargerAddress = 0;
  uint8_t chargerRegisters[kChargerRegisterCount] = {};
  ChargerStatus charger;
};

BatteryDiagnostic readBatteryDiagnostic(FuelGauge& gauge, Charger& charger);

// One line per statement, English and technical. `configuredMah` is the battery size of the
// configuration; a gauge that holds another one is called out.
std::vector<std::string> formatBatteryDiagnostic(const BatteryDiagnostic& d, int configuredMah);

}  // namespace board

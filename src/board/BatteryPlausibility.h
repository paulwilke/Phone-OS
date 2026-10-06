#pragma once

// Never show a percentage the voltage contradicts. Every value comes from one chip and looks plausible,
// so a gauge that holds the wrong battery size shows no error: on 2026-10-06 it said 56 % at 4118 mV
// with the charger reporting "charge done" (docs/hardware-lessons.md). The rule compares the three
// witnesses and, when the number is contradicted, withholds it and lets the charger's word stand.
//
// The rule: the percentage is contradicted when the gauge says less than 70 % while the cell is above
// 4.1 V or the charger says the charge is done. A contradicted percentage is reported as unknown (-1).
// "Full" is claimed only on a cable and only on the charger's word: the charge is done, or the charger
// is not charging a cell that is above 4.1 V (a full battery on a cable reads "not charging").

namespace board {

constexpr int kCellHighMv = 4100;
constexpr int kImplausiblyLowPercent = 70;

struct BatteryInputs {
  int gaugePercent = -1;     // -1: the gauge does not answer
  int gaugeMillivolts = 0;   // 0: unknown
  int chargerMillivolts = 0; // 0: unknown; used when the gauge does not give a voltage
  bool chargerKnown = false; // the charger answered
  bool cable = false;        // the charger's power-good bit
  bool chargeDone = false;   // the charger's phase is "charge done"
  bool notCharging = false;  // the charger's phase is "not charging"
};

struct BatteryAssessment {
  int percent = -1;           // 0..100, or -1 when unknown or contradicted
  bool contradicted = false;  // the gauge's number disagrees with the voltage
  bool full = false;          // the trustworthy state: on a cable and full
};

BatteryAssessment assessBattery(const BatteryInputs& in);

}  // namespace board

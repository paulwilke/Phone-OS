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

// The second rule, on the battery: a gauge that says far less than the voltage gives is wrong too. On
// 2026-10-09 the gauge fell from 55 % to 6 % in one step at 3627 mV and then said 0 % for three hours while the
// device ran on down to 3426 mV with the same load (140 mA) all the time: it had not learned this battery
// (cycle count 0) and took the cell for empty. The percentage is then taken from the voltage, with the table
// that the same discharge measured (percentFromVoltage), and the time remaining is withheld. Only this direction
// is checked: a gauge that says more than the voltage gives could be fooled by a dip while the modem sends.

#include "../hal/Power.h"

namespace board {

constexpr int kCellHighMv = 4100;
constexpr int kImplausiblyLowPercent = 70;
constexpr int kGaugeTooLowByPoints = 30;
// The way back is shorter than the way in, so that a reading on the border does not switch the rule on and off
// with every sample (seen on 2026-10-13: about once a second for minutes, in the log and on the screen).
constexpr int kGaugeBackWithinPoints = 15;
// A gauge that says (nearly) nothing while the cell's voltage is clearly above empty has not learned this cell:
// on 2026-10-13 it said 0 % at 3579 mV and the device ran four hours more, down to the switch-off at 3300 mV.
constexpr int kGaugeEmptyPercent = 3;
constexpr int kVoltageClearlyAbovePercent = 6;   // to start trusting the voltage over an empty gauge
constexpr int kVoltageStillAbovePercent = 4;     // to go on doing so

// The percentage the cell voltage alone gives under a load of about 140 mA, 0..100. A lighter load gives a
// higher voltage and so a slightly higher number; a heavier one a lower number.
int percentFromVoltage(int millivolts);

struct BatteryInputs {
  int gaugePercent = -1;     // -1: the gauge does not answer
  int gaugeMillivolts = 0;   // 0: unknown
  int chargerMillivolts = 0; // 0: unknown; used when the gauge does not give a voltage
  bool chargerKnown = false; // the charger answered
  bool cable = false;        // the charger's power-good bit
  bool chargeDone = false;   // the charger's phase is "charge done"
  bool notCharging = false;  // the charger's phase is "not charging"
  bool wasEstimated = false; // the last answer was the voltage's number: the way back needs a margin
};

struct BatteryAssessment {
  int percent = -1;           // 0..100, or -1 when unknown or withheld; the voltage's own number when `estimated`
  bool contradicted = false;  // the gauge's number disagrees with the voltage
  bool estimated = false;     // the number is the voltage's, because the gauge says far too little
  bool full = false;          // the trustworthy state: on a cable and full
};

BatteryAssessment assessBattery(const BatteryInputs& in);

}  // namespace board

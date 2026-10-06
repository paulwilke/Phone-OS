#include "BatteryPlausibility.h"

namespace board {

BatteryAssessment assessBattery(const BatteryInputs& in) {
  BatteryAssessment out;
  const int millivolts = in.gaugeMillivolts > 0 ? in.gaugeMillivolts : in.chargerMillivolts;
  const bool cellHigh = millivolts > kCellHighMv;
  const bool chargerSaysFull = in.chargerKnown && in.cable && (in.chargeDone || (in.notCharging && cellHigh));

  out.full = chargerSaysFull;
  out.contradicted = in.gaugePercent >= 0 && in.gaugePercent < kImplausiblyLowPercent &&
                     ((in.chargerKnown && in.chargeDone) || cellHigh);
  out.percent = out.contradicted ? -1 : in.gaugePercent;
  return out;
}

}  // namespace board

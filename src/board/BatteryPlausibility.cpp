#include "BatteryPlausibility.h"

namespace board {

namespace {
struct Point {
  int millivolts;
  int percent;
};
// Measured 2026-10-09 on the device: a discharge from full to flat at 135 to 147 mA, the charge counted in
// time times current (1280 mAh used up to 3426 mV), so 0 % is where this cell was flat under that load.
constexpr Point kCurve[] = {{4100, 100}, {4014, 93}, {3941, 86}, {3874, 79}, {3810, 72}, {3749, 63}, {3692, 56}, {3644, 49},
                            {3614, 41},  {3590, 34}, {3568, 26}, {3543, 19}, {3508, 11}, {3457, 4},  {3430, 0}};
constexpr int kCurveSize = sizeof(kCurve) / sizeof(kCurve[0]);
}  // namespace

int percentFromVoltage(int millivolts) {
  if (millivolts >= kCurve[0].millivolts) return 100;
  if (millivolts <= kCurve[kCurveSize - 1].millivolts) return 0;
  for (int i = 1; i < kCurveSize; ++i) {
    if (millivolts >= kCurve[i].millivolts) {
      const Point& hi = kCurve[i - 1];
      const Point& lo = kCurve[i];
      return lo.percent + (hi.percent - lo.percent) * (millivolts - lo.millivolts) / (hi.millivolts - lo.millivolts);
    }
  }
  return 0;
}

BatteryAssessment assessBattery(const BatteryInputs& in) {
  BatteryAssessment out;
  const int millivolts = in.gaugeMillivolts > 0 ? in.gaugeMillivolts : in.chargerMillivolts;
  const bool cellHigh = millivolts > kCellHighMv;
  const bool chargerSaysFull = in.chargerKnown && in.cable && (in.chargeDone || (in.notCharging && cellHigh));

  out.full = chargerSaysFull;
  out.contradicted = in.gaugePercent >= 0 && in.gaugePercent < kImplausiblyLowPercent &&
                     ((in.chargerKnown && in.chargeDone) || cellHigh);
  out.percent = out.contradicted ? -1 : in.gaugePercent;

  // Only on the battery, and only when the charger has said so: a cell that is being charged has a raised voltage.
  if (!out.contradicted && in.gaugePercent >= 0 && millivolts > 0 && in.chargerKnown && !in.cable) {
    const int fromVoltage = percentFromVoltage(millivolts);
    if (fromVoltage - in.gaugePercent >= kGaugeTooLowByPoints) {
      out.contradicted = true;
      out.estimated = true;
      out.percent = fromVoltage;
    }
  }
  return out;
}

}  // namespace board

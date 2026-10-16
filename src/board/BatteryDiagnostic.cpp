#include "BatteryDiagnostic.h"

#include <cstdio>
#include <cstdlib>

#include "BatteryPlausibility.h"

namespace board {

namespace {
using std::to_string;

std::string hex2(unsigned value) {
  char buf[8];
  std::snprintf(buf, sizeof(buf), "%02X", value & 0xFF);
  return buf;
}

// 245 -> "24.5", -5 -> "-0.5"
std::string tenths(int value) {
  return std::string(value < 0 ? "-" : "") + to_string(std::abs(value) / 10) + "." + to_string(std::abs(value) % 10);
}

BatteryInputs inputsOf(const BatteryDiagnostic& d) {
  BatteryInputs in;
  in.gaugePercent = d.gaugeAnswers ? d.gauge.percent : -1;
  in.gaugeMillivolts = d.gaugeAnswers ? d.gauge.millivolts : 0;
  in.chargerKnown = d.chargerAnswers;
  in.chargerMillivolts = d.chargerAnswers ? d.charger.batteryMv : 0;
  in.cable = d.chargerAnswers && d.charger.powerGood;
  in.chargeDone = d.chargerAnswers && d.charger.phase == ChargePhase::Done;
  in.notCharging = d.chargerAnswers && d.charger.phase == ChargePhase::NotCharging;
  return in;
}

// The ADC registers are shown as the chip holds them, with a note when they are not a measurement: the
// diagnosis writes nothing, so it cannot start a conversion to get a real value.
constexpr const char* kNotConverted = " (not measured: no ADC conversion started)";

void addGaugeLines(const BatteryDiagnostic& d, int configuredMah, std::vector<std::string>& lines) {
  if (!d.gaugeAnswers) {
    lines.push_back("gauge: BQ27220 does not answer");
    return;
  }
  const GaugeReading& g = d.gauge;
  lines.push_back("gauge: voltage " + to_string(g.millivolts) + " mV, current " + to_string(g.currentMa) +
                  " mA (average " + to_string(g.averageCurrentMa) + " mA), temperature " + tenths(g.temperatureTenthC) + " C");
  std::string capacity = "gauge: remaining " + to_string(g.remainingMah) + " mAh, full charge " + to_string(g.fullChargeMah) +
                         " mAh, design " + to_string(g.designMah) + " mAh";
  if (g.designMah != configuredMah || g.fullChargeMah != configuredMah)
    capacity += " (configured " + to_string(configuredMah) + " mAh: the gauge holds another battery size)";
  lines.push_back(capacity);
  lines.push_back("gauge: state of charge " + to_string(g.percent) + " %, cycle count " + to_string(g.cycleCount) +
                  ", time to empty " + (g.minutesToEmpty < 0 ? std::string("not discharging") : to_string(g.minutesToEmpty) + " min"));
}

void addChargerLines(const BatteryDiagnostic& d, std::vector<std::string>& lines) {
  if (!d.chargerFound) {
    lines.push_back("charger: no answer at 0x6A (SY6970) or 0x6B (BQ25896)");
    return;
  }
  const std::string who = std::string("charger: ") + chargerChipName(d.chargerChip) + " at 0x" + hex2(d.chargerAddress);
  if (!d.chargerAnswers) {
    lines.push_back(who + " does not answer a register read");
    return;
  }
  const ChargerStatus& c = d.charger;
  lines.push_back(who);
  std::string registers = "charger: registers";
  for (int i = 0; i < kChargerRegisterCount; ++i) registers += " " + hex2((unsigned)i) + "=" + hex2(d.chargerRegisters[i]);
  lines.push_back(registers);
  // With the ADC running, a good VBUS that reads 0 is the bare offset of 2600 mV: no measurement either.
  std::string vbus = "VBUS " + to_string(c.vbusMv) + " mV";
  if (!c.adcStarted) vbus += kNotConverted;
  else if (c.vbusGood && c.vbusMv == 2600) vbus = "VBUS not measured";
  lines.push_back("charger: input current limit " + to_string(c.inputCurrentLimitMa) + " mA (" +
                  (c.inputLimitEnabled ? "limit register in force" : "limit register not in force") +
                  "), input: " + inputKindName(c.input) + ", " + vbus);
  lines.push_back(std::string("charger: ") + chargePhaseName(c.phase) + ", power good " + (c.powerGood ? "yes" : "no") +
                  ", fault register 0x" + hex2(c.fault));
  lines.push_back("charger: charge voltage " + to_string(c.chargeVoltageMv) + " mV, charge current " + to_string(c.chargeCurrentMa) +
                  " mA, pre-charge " + to_string(c.prechargeCurrentMa) + " mA, termination " + to_string(c.terminationCurrentMa) + " mA, watchdog " +
                  (c.watchdogSeconds ? to_string(c.watchdogSeconds) + " s" : std::string("off")));
  lines.push_back("charger: battery " + to_string(c.batteryMv) + " mV, system " + to_string(c.systemMv) +
                  " mV, charging current " + to_string(c.chargingCurrentMa) + " mA" + (c.adcStarted ? "" : kNotConverted));
}
}  // namespace

BatteryDiagnostic readBatteryDiagnostic(FuelGauge& gauge, Charger& charger) {
  BatteryDiagnostic d;
  d.gaugeAnswers = gauge.read(d.gauge);
  d.chargerFound = charger.found() || charger.detect();
  if (d.chargerFound) {
    d.chargerChip = charger.chip();
    d.chargerAddress = charger.address();
    d.chargerAnswers = charger.readAll(d.chargerRegisters, d.charger);
  }
  return d;
}

std::vector<std::string> formatBatteryDiagnostic(const BatteryDiagnostic& d, int configuredMah) {
  std::vector<std::string> lines;
  addGaugeLines(d, configuredMah, lines);
  addChargerLines(d, lines);
  if (d.gaugeAnswers) {
    const BatteryAssessment a = assessBattery(inputsOf(d));
    if (a.contradicted)
      lines.push_back("verdict: the gauge's " + to_string(d.gauge.percent) +
                      " % is contradicted by the cell voltage and the charger; the interface shows " +
                      (a.full ? "full" : "no percentage"));
    else
      lines.push_back(std::string("verdict: the gauge's percentage agrees with the voltage") +
                      (a.full ? "; the charger says full" : ""));
  }
  return lines;
}

}  // namespace board

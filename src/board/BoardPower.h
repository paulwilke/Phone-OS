#pragma once

// The Power capability on the T-Deck Pro MAX: the rails through the expander, the battery through the fuel
// gauge and the charger, shutdown through the charger's battery switch.
//
//   Cellular     the modem: begin() and shutdown() of the Modem, which owns its supply and its power key
//   Positioning  the satellite receiver's rail (expander)
//   Radio        the radio chip's rail (expander); the feature is off, nothing uses it yet
//   Frontlight   the display's light (a pin of the processor)
//
// "Cable present" is the charger's power-good bit and nothing else, and externalPower() follows it. The
// charge state is derived from it and the charge phase; the battery percentage passes the plausibility
// rule (BatteryPlausibility.h) before anybody sees it.

#include "../hal/EventLog.h"
#include "../hal/Modem.h"
#include "../hal/Power.h"

#include "BatteryPlausibility.h"
#include "Charger.h"
#include "FuelGauge.h"
#include "Gpio.h"
#include "MillisSource.h"
#include "Xl9555.h"

namespace board {

class BoardPower : public Power {
public:
  BoardPower(Xl9555& expander, Gpio& gpio, Charger& charger, FuelGauge& gauge, Modem& modem)
      : expander_(expander), gpio_(gpio), charger_(charger), gauge_(gauge), modem_(modem) {}

  // The start of the power side, once, after board::powerUp(): finds the charger (both addresses), checks
  // the gauge's capacity against the configured battery size and its end-of-discharge thresholds against
  // this cell's (D-42), writes each only on a difference, and puts
  // the rails nobody has asked for yet into their idle state (receiver and radio off). Says what it found
  // in the log, one line each.
  void begin(uint16_t batteryCapacityMah, GaugeCapacityWriter& writer, EventLog& log);

  // With a clock, one reading of the gauge and the charger serves every question for kReadingTtlMs: the
  // core's state asks five questions in one pass, each a reading over I2C at 100 kHz, which cost the loop
  // 52 ms of every pass in the night of 2026-10-13 (decision D-31). Without a clock every question reads.
  void setClock(MillisSource& millis) { millis_ = &millis; }
  static constexpr uint32_t kReadingTtlMs = 1000;

  // How often a domain was switched from off to on since the start. A receiver that was switched off and on
  // again between two readings still has to start afresh, and only a counter shows that it happened.
  uint32_t switchOns(PowerDomain domain) const { return switchOns_[(int)domain]; }

  bool setEnabled(PowerDomain domain, bool on) override;
  bool isEnabled(PowerDomain domain) override;
  // Switches the rails off and disconnects the battery. False, with nothing changed, while a cable feeds
  // the board (VBUS keeps it powered, so the battery switch would do nothing) or when the charger does
  // not answer. A modem that should say goodbye to the network is shut down by the caller first.
  bool powerOff() override;
  bool externalPower() override;
  int batteryPercent() override;
  int batteryMillivolts() override;
  int batteryMinutesRemaining() override;
  ChargeState chargeState() override;

private:
  struct Reading {
    bool chargerOk = false;
    ChargerStatus charger;
    bool gaugeOk = false;
    GaugeReading gauge;
  };
  Reading read();
  BatteryInputs inputsOf(const Reading& r) const;
  // While a cable feeds the board, every reading checks the input current limit and writes it again when
  // the chip set its own: the SY6970 runs its input detection at every new supply and puts 500 mA into the
  // register, over what begin() wrote (seen on 2026-10-21, decision D-43).
  void keepInputLimit();

  Xl9555& expander_;
  Gpio& gpio_;
  Charger& charger_;
  FuelGauge& gauge_;
  Modem& modem_;
  EventLog* log_ = nullptr;
  MillisSource* millis_ = nullptr;
  Reading last_;
  bool haveReading_ = false;
  uint32_t readAt_ = 0;
  bool estimating_ = false;  // the percentage is the voltage's: said once when it starts and when it ends
  bool inputLimitRefused_ = false;  // a refused write while on the cable is said once, not every second
  bool frontlight_ = false;
  uint32_t switchOns_[4] = {};
};

}  // namespace board

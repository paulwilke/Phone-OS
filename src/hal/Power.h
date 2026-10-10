#pragma once

// Capability: power supply. The application decides which component gets power at
// all, not only whether it sleeps.
//
// The difference to a sleeping modem: sleep keeps the modem registered in the network
// (reachable, little power); switching the cellular domain off turns it off entirely
// (not reachable, no power) — the "weeks instead of days" mode. How that is done (an
// AT command, a regulator, a power key) only the implementation knows.
//
// The radio is the one domain that needs care. While radio software runs, the chip
// belongs to it, and cutting its power leaves the radio library in an unknown state.
// The caller (see PowerPolicy) cuts radio power only while nobody uses the radio.

#include <cstdint>

enum class PowerDomain : uint8_t {
  Cellular,     // the mobile modem: calls, SMS, network time
  Positioning,  // the satellite receiver: only while a position is wanted
  Frontlight,   // the reading light of the e-ink display; without it the display is black in the dark
  Radio,        // the long-range radio chip
};

// What the battery is: the cell's usable voltage range, measured on the device (docs/battery.md). Below the empty
// mark the cell is flat; this is not the cell's safe limit, which nothing in this repository knows (the data
// sheet of the cell is not here), and nothing below it is protected by the firmware.
constexpr int kBatteryEmptyMv = 3430;
constexpr int kBatteryFullMv = 4200;
// At or below this percentage on the battery the interface says the battery is almost empty.
constexpr int kBatteryLowPercent = 10;

// Where the power comes from. Full means: on external power and fully charged.
enum class ChargeState : uint8_t { Unknown, Discharging, Charging, Full };

class Power {
public:
  virtual ~Power() = default;

  // Switches a domain on or off. False when that component cannot be switched; its
  // state is unchanged then. Switching to the state it has is a success.
  virtual bool setEnabled(PowerDomain domain, bool on) = 0;
  virtual bool isEnabled(PowerDomain domain) = 0;

  // Switches the device off. False when that is not possible, for example because a
  // cable feeds the board. Whoever offers it must be able to show the difference, or
  // somebody presses into the void.
  virtual bool powerOff() = 0;

  // Is a supply connected? Not the same as "charging": a full battery is not charged
  // any more, but the cable stays plugged in.
  virtual bool externalPower() = 0;

  // 0 … 100, or -1 when unknown.
  virtual int batteryPercent() = 0;
  // The cell voltage in millivolts; -1 when no chip answers.
  virtual int batteryMillivolts() = 0;
  // Estimate of the remaining time in minutes; -1 when unknown or while charging.
  virtual int batteryMinutesRemaining() = 0;
  // Unknown when the charger does not answer.
  virtual ChargeState chargeState() = 0;
};

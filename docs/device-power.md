# The device's power side: rails, battery, charger

Code: `src/board/` (`BoardPower`, `FuelGauge`, `Charger`, `BatteryPlausibility`, `BatteryDiagnostic`, `Xl9555`), the device's part in `device/Esp32GaugeWriter.h`; tests in `test/test_board_power/`, `test_fuel_gauge/`, `test_charger/`, `test_battery_plausibility/`, `test_battery_diagnostic/` and, for the `Power` contract, `test/test_hardware_contracts/`. The rules come from [hardware-lessons.md](hardware-lessons.md); the start-up order of the board is in [device.md](device.md).

`BoardPower` is the `Power` capability ([hardware-capabilities.md](hardware-capabilities.md)) on the T-Deck Pro MAX. It runs the same `PowerContract` suite as the fake (`BoardPowerRig` in `testing/BoardPowerRig.h`, on a scripted I2C bus with the three chips).

## Domains

| Domain | What it switches |
|---|---|
| `Cellular` | the modem: `Modem::begin()` and `shutdown()`; the modem owns its supply and its power key |
| `Positioning` | the receiver's rail on the expander |
| `Radio` | the radio chip's rail on the expander; the feature is off in this firmware |
| `Frontlight` | the display's light, a pin of the processor |

A rail whose expander write is refused reports `false` and keeps its state. At start the receiver and the radio are switched off: the expander's boot state powers them, nobody has asked for them, and a receiver that stays on drains the battery.

## The fuel gauge ships with the wrong battery size

At start `BoardPower::begin()` reads the gauge's design capacity (register 0x3C) and full-charge capacity (0x12) and compares both with `[battery] capacity_mah` (default 1500, range 100 to 10000, [config-loader.md](config-loader.md)).

- **Only a difference is written**, to both registers, and then read back; the chip's memory wears with every write. A second start writes nothing.
- **One log line** says what happened: nothing to write, written (with what the chip held), refused, written but read back differently, or no gauge. A configured value outside 100 to 10000 is never written.
- The write itself is the library's procedure (SensorLib's `setNewCapacity`: unseal, enter the configuration update, write the data memory with its checksum, leave, seal again), reached through the seam `board::GaugeCapacityWriter`. The host tests stand a fake in its place that changes the registers the way the chip would; **that the real procedure takes on the chip is not verified** (checklist item in `bring-up.md`).

## Never show a percentage the voltage contradicts

`board::assessBattery()` compares three witnesses: the gauge's percentage, the cell voltage (the gauge's, else the charger's) and the charger's phase.

| Situation | Percentage | State |
|---|---|---|
| gauge below 70 % while the cell is above 4.1 V, or while the charger says "charge done" | withheld (`-1`) | `Full` only when the charger says so and a cable is attached |
| a cable, the charge is done | as the gauge says if it agrees | `Full` |
| a cable, "not charging", the cell above 4.1 V | as above | `Full` (a full battery on a cable reads "not charging") |
| a cable, charging, the cell above 4.1 V, gauge low | withheld | `Charging` (never `Full` while the charger still charges) |
| no gauge | `-1` | the charger's |

The time remaining is withheld together with the percentage (it is computed from the same wrong capacity) and is unknown while a cable is attached. The unit tests use the measurement of 2026-10-06: gauge 56 %, 4118 mV, charger "done", design 3000 mAh, which gives `Full` and no number.

A cable that feeds the board but where the charger neither charges nor calls the battery full reports `Unknown`: nothing can be claimed. Note that `StatusScreen` derives "on a cable" from the charge state; the cable bit itself is `externalPower()`.

## The charger

Both revisions are probed: an SY6970 at 0x6A, else a BQ25896 at 0x6B (the registers are the same). A board where neither answers is reported as having no charger, which is not the same as a broken bus; the charge state is `Unknown` and the question is asked again later.

**"Cable present" is the power-good bit** (register 0x0B bit 2), not the charge phase: `externalPower()` follows it, and the power policy lets nothing sleep while it is set (the USB console disappears in light sleep). `powerOff()` is refused, with nothing changed, while the bit is set (VBUS keeps the board powered, so the battery switch would do nothing) and when the charger does not answer; otherwise it switches the rails off and sets BATFET_DIS (register 0x09 bit 5). A modem that should say goodbye to the network is shut down by the caller first. **The caller's order** (`device/main.cpp`, `powerOff()`; seen on the device 2026-10-08 with 4.1.0, changed in 4.1.1): with a cable it refuses at once (`notePowerOffFailed`: the notice "Switch off only without cable" and a log line) and touches nothing; before, the request stayed and the modem was shut down at every pass of the loop. Otherwise it draws the picture "Phone OS ... Switched off" (`ui::drawSplash`, e-ink keeps it without power, so the device does not look on), shuts the modem down, and disconnects the battery; if the charger does not take the command, the device restarts rather than stay without a network behind a picture that says it is off. Every request is logged ("Power: switch off asked for"). Not verified on the device.

## The diagnosis

`board::readBatteryDiagnostic()` reads both chips **without writing** and `formatBatteryDiagnostic()` makes one line per statement:

- gauge: voltage, current, average current, temperature, remaining, full-charge and design capacity (a difference from the configured size is called out), state of charge, cycle count, time to empty;
- charger: chip and address, all registers 0x00 to 0x14, the input current limit, the kind of input, VBUS, the charge phase, power good, the fault register, the charge voltage, charge, pre-charge and termination currents, the battery and system voltage, the charging current;
- a verdict: whether the gauge's percentage agrees with the voltage.

Decoding is tested with the values read from the device on 2026-10-06 (charger registers 0x00 = 0x48, 0x04 = 0x20, 0x05 = 0x13, 0x06 = 0x5E, 0x0B = 0xBE, 0x0E = 0x5D give a 500 mA input limit, 2048 mA charge current, 4208 mV target, 4164 mV cell, "charge done"). Until the USB console exists the firmware prints the diagnosis once at start; the console command comes with the console.

Reading register 0x0C clears the charger's latched fault flags, as it does for any reader; nothing else is changed.

## Where each rule of the lessons is tested

| Rule | Test |
|---|---|
| gauge capacity checked at start, written only on a difference, read back, one line | `test_fuel_gauge`, `test_board_power` |
| `[battery] capacity_mah` through the schema | `test_config_loader` |
| no percentage the voltage contradicts | `test_battery_plausibility`, `test_board_power` |
| the diagnosis reads both chips and writes nothing | `test_battery_diagnostic` |
| both charger revisions, a missing charger | `test_charger`, `test_board_power` |
| the cable is the power-good bit | `test_charger`, `test_board_power` |

## Not verified on the device

Compiled and tested on the host are not verified on the device: that the capacity write takes (the gauge's data memory, the read back after the chip re-initialises), the register layout of the SY6970 against the values decoded here beyond the six measured registers, and the shutdown through the battery switch.

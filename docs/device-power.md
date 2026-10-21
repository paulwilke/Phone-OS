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

## One reading a second

`AppCore::state()` asks the power capability five questions (percentage, voltage, minutes, charge state, cable), and each was a reading of the fuel gauge and the charger over I2C at 100 kHz, about six transactions for the gauge and three for the charger; `Runtime::followCalls` asked for the whole state every pass. In the night of 2026-10-13 that was 52 ms of every 56 ms pass with nothing to do ([hardware-lessons.md](hardware-lessons.md), decision D-31). Since 4.2.4 `followCalls` asks `AppCore::callState()` alone, and `BoardPower` with a clock (`setClock`, the firmware gives it the millisecond counter) keeps one reading for `kReadingTtlMs` (1000 ms): every question within that second is answered from it, the next second reads again, and the wrap of the counter is no gap. Without a clock (the contract suite) every question reads. The energy log reads the gauge on its own and is not affected; `powerOff()` reads the charger afresh. `test_board_power`: five questions cost one reading, the second reads again, the wrap, and that without a clock every question reads.

## The fuel gauge ships with the wrong battery size

At start `BoardPower::begin()` reads the gauge's design capacity (register 0x3C) and full-charge capacity (0x12) and compares both with `[battery] capacity_mah` (default 1500, range 100 to 10000, [config-loader.md](config-loader.md)).

- **Only a difference of the design capacity is written**, then to both registers (the full charge is seeded with the same size), and read back; the chip's memory wears with every write. A second start writes nothing. The full-charge capacity is what the gauge learns from a discharge, and once the design capacity is right it is the gauge's alone: after the night of 2026-10-13 it reported 1372 mAh next to the design 1500, and the earlier rule ("both equal the setting") wrote 1500 over it at every start (decision D-32). The start line then says "nothing to write; it has learned a full charge of 1372 mAh".
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

**The charge current is limited at every start** (decision D-14). The chip charges with 2048 mA unless told otherwise, which is more than 1 C for the 1500 mAh cell, and its I2C watchdog (40 seconds) sets every register back to that default when nobody feeds it; the diagnosis of 2026-10-12 showed the chip on its defaults with the watchdog running (the fault register read 0x90 then and still does with the watchdog off, so it is not a watchdog fault; its meaning is not known). `BoardPower::begin` calls `Charger::limitCharging(chargeCurrentLimitMa(capacity))`: 0.65 C of the configured capacity, rounded down to the chip's step of 64 mA (960 mA for 1500 mAh), and the watchdog switched off so that the value stays. Each register is read, written only on a difference with only its own bits, and read back; the log says one of "charge current limited to 960 mA (it was 2048 mA), the charger's watchdog is off", "... is 960 mA already, nothing to write" or "could not be limited". The diagnosis shows the watchdog next to the charge current. The register map is the BQ25896's, which the SY6970 is documented to follow; the SY6970's own data sheet is not in this repository, which is why every write is read back.

**The input current limit is set to 800 mA at every start** (decision D-38). The chip takes every supply for an "unknown adapter" and limits its input to 500 mA (register 0x00 = 0x48: EN_HIZ 0, EN_ILIM 1, IINLIM 0b001000 = 100 mA + 8 x 50 mA). On 2026-10-15 a mains adapter charged the 1500 mAh cell at 516 falling to 420 mA for 3 h 10 min, and the charge current limit of 960 mA above was never reached. `BoardPower::begin` calls `Charger::limitInput(kInputLimitMa)` after `limitCharging`: IINLIM = (800 - 100) / 50 = 14, the other bits of the register (EN_HIZ, EN_ILIM) kept, written only on a difference and read back. The log says one of "input current limit set to 800 mA (it was 500 mA)", "... is 800 mA already, nothing to write" or "could not be set, the charger keeps its own 500 mA". The constant `kInputLimitMa` is in `Charger.h`: a mains adapter gives at least 1 A, so 800 mA leaves a margin and the cell's 960 mA charge limit stays the one that holds. A computer's USB port that gives less limits itself, or the charger's input voltage regulation lowers the current; the owner accepts that. The chip sets the limit back to what its input detection finds, 500 mA for an "unknown adapter", at every new supply, after whatever the firmware wrote (seen on 2026-10-21: a cable attached while the device ran left register 0x00 at 0x48, while a start with the adapter attached had charged at 812 mA). So while a cable feeds the board, every reading of the chips (`BoardPower::read`, at most once a second with the clock) calls `limitInput` again, which reads the register and writes only on a difference: the log says "Battery: input current limit set to 800 mA again (it was 500 mA: the charger set its own when it detected the supply)" once per plug-in, or "the input current limit could not be set again at the cable, the charger keeps its own 500 mA" once per cable when the write is refused (decision D-43).

## The diagnosis

`board::readBatteryDiagnostic()` reads both chips **without writing** and `formatBatteryDiagnostic()` makes one line per statement:

- gauge: voltage, current, average current, temperature, remaining, full-charge and design capacity (a difference from the configured size is called out), state of charge, cycle count, time to empty;
- charger: chip and address, all registers 0x00 to 0x14, the input current limit, the kind of input, VBUS, the charge phase, power good, the fault register, the charge voltage, charge, pre-charge and termination currents, the battery and system voltage, the charging current (these four marked as not measured while register 0x02 starts no conversion);
- a verdict: whether the gauge's percentage agrees with the voltage.

**The ADC values are marked when nothing measured them.** The charger's ADC (register 0x0E battery, 0x0F system, 0x11 VBUS, 0x12 charging current) converts only after CONV_START (register 0x02 bit 7) or with CONV_RATE (bit 6) set, and this firmware sets neither, so the four registers hold whatever the chip left in them. The device showed it on 2026-10-16 (registers 0x02 = 0x1D, 0x11 = 0x80, 0x0E = 0x47, 0x12 = 0x02, power good yes): VBUS read the bare offset of 2600 mV, the cell was 80 mV away from the gauge, the charging current 370 mA away from the gauge's. When both bits are clear, `ChargerStatus::adcStarted` is false and the diagnosis prints the VBUS value and the battery, system and charging-current values each followed by "(not measured: no ADC conversion started)". It does not start a conversion, because the diagnosis writes nothing; a one-shot conversion that finished earlier looks the same, and nothing is claimed for it. When a conversion has run and a good VBUS still reads 0, the line says "VBUS not measured" instead of 2600 mV. Only the diagnosis changed: the cell voltage that the verdict and the charger fallback of `BoardPower::batteryMillivolts` use is still decoded from register 0x0E.

Decoding is tested with the values read from the device on 2026-10-06 (charger registers 0x00 = 0x48, 0x04 = 0x20, 0x05 = 0x13, 0x06 = 0x5E, 0x0B = 0xBE, 0x0E = 0x5D give a 500 mA input limit, 2048 mA charge current, 4208 mV target, 4164 mV cell, "charge done"). Until the USB console exists the firmware prints the diagnosis once at start; the console command comes with the console.

Reading register 0x0C clears the charger's latched fault flags, as it does for any reader; nothing else is changed.

## Where each rule of the lessons is tested

| Rule | Test |
|---|---|
| gauge capacity checked at start, written only on a difference, read back, one line | `test_fuel_gauge`, `test_board_power` |
| `[battery] capacity_mah` through the schema | `test_config_loader` |
| no percentage the voltage contradicts | `test_battery_plausibility`, `test_board_power` |
| the diagnosis reads both chips and writes nothing | `test_battery_diagnostic` |
| the ADC values are marked as not measured while no conversion is started | `test_battery_diagnostic`, `test_charger` |
| both charger revisions, a missing charger | `test_charger`, `test_board_power` |
| the input limit: the register value, written only on a difference, the other bits kept, read back, the start line | `test_charger`, `test_board_power` |
| the cable is the power-good bit | `test_charger`, `test_board_power` |

## Not verified on the device

The input limit of 800 mA: that the start line says "input current limit set to 800 mA", that `battery` reads it, and that the energy log shows a charge above 500 mA at the adapter (D-38). The charge current: that `battery` reads 960 mA and "watchdog off" after a start, what the fault register 0x90 means (it did not change), and what the energy log shows with a mains adapter on the cable (open point 26).


Compiled and tested on the host are not verified on the device: that the capacity write takes (the gauge's data memory, the read back after the chip re-initialises), the register layout of the SY6970 against the values decoded here beyond the six measured registers, and the shutdown through the battery switch.

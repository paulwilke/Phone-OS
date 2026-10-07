# The hardware drivers: what each does, from what, and what is verified

Three words are kept apart everywhere in this work: **compiled** (the firmware builds), **tested on the host** (a test runs the code against a scripted chip, with values from the notes where there are any) and **verified on the device** (the owner's checklist, [bring-up.md](bring-up.md), says so with a date). Nothing below is "verified on the device" yet: the columns of the checklist are empty.

Where the knowledge comes from: the earlier code base's hardware notes (device-proven, 2026-09-30 and 2026-10-01), the manufacturer's pin list and schematic, the chips' documentation as the earlier code cited it, and the measurement of 2026-10-06 ([hardware-lessons.md](hardware-lessons.md)). Never a library's source that is not MIT or the project's own.

| Driver (code) | What it does | Sources | Tested on the host with | Not verified on the device |
|---|---|---|---|---|
| Pin map (`src/board/PinMap.h`) | every GPIO, I2C address and expander leg in one place | the manufacturer's pin list, checked against their code on 2026-09-22; the UART crossing, the leg numbering and the boot state read back on the device | duplicates, the crossing, the numbering, the boot state bit by bit | — (these were confirmed on the device earlier) |
| Expander and start-up (`Xl9555`, `PowerUp`) | direction, boot state, the two reset pulses, the order | the notes; XL9555 register map checked against SensorLib | the order of the writes, the 20 ms/50 ms timing, a silent expander | the start on the board as written here |
| Fuel gauge (`FuelGauge`) | reads voltage, capacity, charge; checks the capacity at start | BQ27220 standard commands as SensorLib lists them; the write by SensorLib's `setNewCapacity` | the 2026-10-06 values; write only on a difference; refused and unreliable writes | that the capacity write takes on the chip |
| Charger (`Charger`) | SY6970 at 0x6A or BQ25896 at 0x6B; decodes all registers; the battery switch | the notes, XPowersLib's register use | the six registers measured on 2026-10-06 decode to the measured state; both addresses; no write on a read | the other registers' decoding; the shutdown |
| Power (`BoardPower`) | the `Power` capability, the plausibility rule, the cable | hardware-lessons.md | the Power contract; the rule on the measurement | the rails on the board |
| Keyboard (`Tca8418`, `KeyboardDecoder`) | the matrix, the layers, Shift | Meck's matrix, checked on the device 2026-09-30; register 0x29 changed to the chip's default | the table, the layers, Shift in every combination | every key against its print; bounce under longer typing |
| Touch and the three keys (`Cst66xx`) | family check, normal mode, the nine-byte report | the notes (found on the device 2026-09-30/10-01) | the report, the checksum rule, one tap per contact, never a repeated start | position against the picture; the keys |
| Vibration (`Drv2605`, `BoardHaptic`) | an ERM motor in open loop; supply only while used | the chip's library sequence, checked against the data sheet's register map | the sequence; the Haptic contract; the supply cut after 2 s | the rhythms; the restart after the cut |
| Motion (`BoardMotion`) | rail, firmware start, samples unchanged | SensorLib; the notes (2026-10-03) | the Motion contract; z = −0.95 through the page's source | the whole start (about 11 s) |
| Receiver (`NmeaParser`, `BoardGnss`) | NMEA to a fix, with the capability's guarantees | NMEA 0183; the notes | the earlier tests ported; NaN HDOP; impossible positions; stale fix; restart | 38400 baud on the real receiver |
| Modem lines (`ModemTransport`) | the UART, supply, power key, DTR, sound switches | the notes; the driver's own tests | the real driver on it against a scripted modem | the pulse, a call with sound |
| Clock (`DeviceClock`, `TimeSync`) | the 64-bit uptime; which time to believe | the Clock capability; judgement for the limits | the Clock contract; wrap; the rules for a source that lies | the limits against real network times |
| Card (`SdCard`, `SdConfigStore`, `SdEventLog`, `SdMessageLog`) | mount, the safe write, recovery, the logs | the notes (2026-09-30: MBR, release between attempts, descending clocks) | the ConfigStore, EventLog and MessageLog contracts; power loss at every step; full card; no card | the Arduino library's behaviour on a real volume |
| Wi-Fi and TCP (`BoardWifi`, `BoardTcpServer`) | the capabilities' rules on small seams | the notes | the Wifi and TcpServer contracts | the radio, the access point, the page |
| Display (`EinkDisplay`, `EpdWire`) | the pipeline; **no panel values** | the controller's command codes | the sequences against a recording SPI, with invented test data | **everything about the panel**: [device-display.md](device-display.md) |
| Console (`Console`) | CONFIG/BATTERY/SMS over USB | the earlier console's protocol | parsing, framing, timeouts, the whitelist | over the real USB bridge |

## The rules of hardware-lessons.md and their tests

| Rule | Test |
|---|---|
| 1. The gauge's capacity is checked at start and written only on a difference; `[battery] capacity_mah` through the schema; no percentage the voltage contradicts | `test_fuel_gauge`, `test_board_power`, `test_battery_plausibility`, `test_config_loader` |
| 2. A diagnosis reads both chips without writing, decoded with the measured values | `test_battery_diagnostic`, `test_charger`, the `BATTERY` line in `test_console` |
| 3. Both charger revisions; no charger is not a broken bus | `test_charger`, `test_board_power` |
| 4. "Cable present" is the power-good bit; nothing sleeps with a cable | `test_charger`, `test_board_power` (the bit); `test_power_policy` (the sleep rule) |
| 5. The SIM PIN is tried once per start; the device code never retries | `test_modem_driver` (the driver); `device/main.cpp` passes the PIN once and has no retry |
| 6. Opening the serial port resets the ESP32; the console needs no open port | stated in [device-console.md](device-console.md); `test_console` (a port that takes nothing does not hang it) |

## Licences

SensorLib (MIT) is a dependency of the `device` environment only. The motion sensor's firmware image comes inside it and its redistribution terms are not verified: it is not in this repository, and binaries that contain it are for the owner's own device ([THIRD_PARTY.md](../THIRD_PARTY.md)). No GPL code is used: GxEPD2 is not a dependency and no table of it was copied ([device-display.md](device-display.md)).

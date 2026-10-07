# The device's receiver, modem lines and clock

Code: `src/drivers/NmeaParser.*` and `CivilTime.h`; `src/board/` (`BoardGnss`, `ModemTransport`, `DeviceClock`, `TimeSync`, `MillisSource`, `UartPort`); the device's part in `device/Esp32UartPort.h` and `Esp32Millis.h`; tests in `test/test_nmea/`, `test_board_gnss/`, `test_modem_transport/`, `test_modem_on_board/`, `test_device_clock/`, `test_time_sync/` and, for the `Clock` and `GnssReceiver` contracts, `test/test_hardware_contracts/`. The scripted receiver is `testing/NmeaSource.h`, the scripted modem behind the UART the `FakeAtTransport` of [at-layer.md](at-layer.md).

## The receiver

`NmeaParser` reads the receiver's NMEA stream: RMC (time, validity, position) makes the fix, GGA (satellites in use, HDOP) its quality; sentences with a wrong checksum, too long ones and unknown kinds are skipped. The capability's promise ([hardware-capabilities.md](hardware-capabilities.md)) is kept at the source:

- A valid fix has a latitude within ±90, a longitude within ±180 (degrees and minutes below 60, a hemisphere that exists), and an HDOP that is finite and above zero. A receiver that says "valid" about a position that cannot exist gets no fix.
- **An HDOP that is missing, `nan`, `inf`, zero or negative is unknown and reads 99**, never NaN: a NaN compares false with everything, and a fix with it is dropped further up without a word.
- An invalid fix carries **no numbers** (the earlier code kept the last position in it, against what the capability says).
- A date that does not exist ends the fix; a date that exists but is not plausible (before 2020, from 2070 on: a receiver that has not learned the date, a week-number rollover) leaves the position and sets its time to 0 ("unknown"). The limit is `Clock::kLatestEpoch`, the same one the network time is held to.

`BoardGnss` is the `GnssReceiver` on the UART (38400 baud, pins from the processor's side). It follows the `Positioning` domain of `BoardPower`: off is `false` and leaves the caller's fix alone; **every switch-on, seen or not, starts afresh** (`BoardPower::switchOns()` counts them; the parser is reset and what the UART still held from the earlier run is thrown away), so a position from yesterday is never returned; and a fix is only as old as its last position sentence: the receiver sends one a second, and **after ten seconds without one the fix is reported as not valid**. It never blocks (at most 4096 bytes a call).

## The modem's lines

`ModemTransport` is the `AtTransport` of the modem driver on the board: the UART at 115200 baud (the processor receives on the modem's TXD, pin 11, and sends on its RXD, pin 10), the supply on expander leg 0, the power key on leg 8, DTR on a pin of the processor, and the sound path.

- **The supply never presses the key.** The modem hangs on the expander, so after a reset of the processor it still runs; a pulse on a running modem switches it off. The driver asks first and presses only when the modem stays silent, and `test_modem_on_board` runs the real driver on this transport against a scripted modem: a running modem gets no pulse, a silent one exactly one.
- **The pulse** is high for 100 ms, low for 1200 ms, high again, the shape the device was brought up with. It blocks for 1.3 s, once, at the start of a modem that did not answer.
- **Sound.** For a call the switch goes to the modem first and the amplifier comes on after; at the end the amplifier goes off first and then the switch goes back to the codec. The amplifier is off whenever no call runs.
- **DTR** starts low (the modem is awake); high lets it sleep.
- A write that the port takes only in part is given the rest; an expander that refuses a switch is said in the log, because the driver cannot be told.

## The clock

The board has no real-time clock. `DeviceClock` makes the 64-bit uptime of the `Clock` capability out of the processor's 32-bit millisecond counter (a wrap after 49 days is an ordinary step; a counter that reads lower than before holds the uptime, so it never goes back) and keeps the epoch: 0 while unknown, never guessed, a date before 2020 refused, advancing with the uptime once set, and setting it moves nothing else. The `Clock` contract runs against it.

`TimeSync` decides which time to believe. Two sources offer one, and neither always tells the truth: the network (the modem's `AT+CCLK`, what the operator says, which can be hours off or stale) and the satellites (exact when the receiver has a fix).

1. A time before 2020 or from 2070 on is ignored (said once in the log for each source).
2. The same reading offered again is ignored (the modem hands out its last reading until it reads again).
3. While the clock is unknown, any time is taken.
4. A time from the satellites corrects the clock when it differs by more than 2 s, and a clock set from them is not overruled by the network for 24 hours.
5. A network time within 30 s of the clock is taken (a drift, a small step back); one further away, **ahead or behind (a jump back)**, is held back as suspect and taken only when the next, different network reading agrees with it as much as the time between them predicts. A source that lies once does not move the clock; one that is right twice does. The zone offset is held back with the time.
6. Every change of the clock is one line in the log, **written with the zone already in force**. The log stamps a line with the local time of day; the line used to be written between "the time is set" and "the zone follows", so on a device that had just started it showed UTC (seen 2026-10-07: `16:07` among `18:07` lines in Germany in summer). The zone is the network's, or the named zone of `settings.toml` when there is one (it has precedence, and depends on the date: summer time); the host hands the named zone to `TimeSync::setZoneRule`, answered by `AppCore::zoneOffsetMinutes`. Not verified on the device.

## Not verified on the device

The receiver's UART at 38400 baud and the stale-fix rule against a real receiver, the power-key pulse and the order of the sound switches on a real call, the 1.3 s pulse's effect on a modem that was merely slow, and the clock rules against real network times (the 30 s and 24 h limits are judgements, not measurements).

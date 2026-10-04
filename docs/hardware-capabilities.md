# Hardware capabilities

Code: `src/hal/`, tests in `test/test_haptic_pattern/` and `test/test_hal_headers/`. Pure C++17 with no Arduino, no platform header and no global state; `scripts/check_layers.py` enforces it.

## The rule

The software talks to hardware only through **capabilities**: interfaces named after what the software *needs* (a clock, a modem, power, positioning, vibration), never after the part that provides it. The application and the user interface know these interfaces and nothing else. Which chip stands behind which capability is written in one place per platform (the device, the simulator), and a capability can be swapped by writing another implementation. An interface that has no second implementation (a fake in a test counts) does not exist yet.

Consequences that show in the code:

- An interface speaks the language of the need ("positioning", "radio"), not of a part. The chip's own vocabulary stays in its driver.
- The facts that make the interface usable are documented in its header: what a state means, what "unknown" means, when a fix is "valid", what a failed write leaves behind. An implementation that does not honour them is wrong, whatever it does internally.
- An interface does not know who uses it: `src/hal/` includes only standard headers and its own siblings.
- Interfaces return codes and data, never text for the user.

## The capabilities

| Capability | Header | What the software needs it for | The contract in one sentence |
|---|---|---|---|
| clock | `Clock.h` | timeouts, countdowns, idle time, time of messages, school time | uptime never goes back; the epoch is 0 until it is known and is never guessed; a date before 2020 is refused |
| power | `Power.h` | which component gets power; battery, charging, power off | a domain that cannot be switched reports failure and stays as it was; battery percent is 0–100 or -1 |
| positioning | `GnssReceiver.h`, `GnssFix.h` | the position for SOS and location replies | false when off, an invalid fix while searching, and never a position from before the last switch-on |
| modem | `Modem.h` | SMS, calls, signal, operator, SIM state | everything is polled, each event exactly once; a send failure is reported once with the modem's own reason; call events end in exactly one final event |
| log | `EventLog.h` | the record of calls, messages and every remote query | writes every line it is given, one record per call, cannot be switched off |
| vibration | `Haptic.h`, `HapticPattern.h` | telling a call, message, SOS or error apart in a pocket | `play()` returns at once; a new pattern replaces a running one; `busy()` is true until the last pulse |
| files | `ConfigStore.h` | configuration, logs and files for the web page | a failed write leaves the old content intact; unsafe paths are refused; scratch files are invisible |
| WLAN | `Wifi.h` | setting the device up from a phone or the home network | non-blocking; an access point password has 8–63 characters; starting one mode stops the other |
| TCP server | `TcpServer.h` (`TcpConnection`) | the web server | non-blocking; one connection at a time; read returns 0 for "nothing now" and -1 for "gone" |
| motion | `MotionSensor.h` | shaking, as a way to wake or to call for help | runs only between `start()` and `stop()`; no value while stopped |
| radio | `RadioLink.h` | messages where the mobile network does not reach (optional) | one delivery report per sent message, in the order of sending; "unconfirmed" is not "lost" |

The display and the input are not capabilities in this sense: the user-interface framework has its own interfaces, and the application glue that bundles the capabilities is not part of this layer.

### Clock

Two kinds of time: **uptime** (milliseconds since the start, 64 bit, monotonic) for everything that measures a duration, and the **epoch** (Unix time, UTC), which is 0 while it is not known. The board has no clock of its own; after a start without network the time stays unknown until the modem or the receiver delivers it. For every consumer "unknown" means: carry on without it. Quiet hours do not lock (see [quiet-hours.md](quiet-hours.md)); log lines and messages are written without a time, never with 1970; a position carries time 0. A modem whose own clock was never synchronised reports 1970 or 2000, so `setEpochUtc()` refuses anything before 2020-01-01. `setEpochUtc(0)` makes the time unknown again. The offset from UTC is always a valid one between -12:00 and +14:00 (0 until set); the zone rules are in [time-zones.md](time-zones.md).

### Power

Four domains: cellular, positioning, front light, radio. `setEnabled()` returns false for a component that cannot be switched. Sleeping the modem (see [power-policy.md](power-policy.md)) keeps it registered; switching the cellular domain off turns it off entirely. The radio chip is cut only while no radio software uses it. `powerOff()` is false where it is not possible (a cable feeds the board), so the interface can show the difference. `externalPower()` is "a supply is connected", not "charging": a full battery is not charged, the cable stays. `batteryMinutesRemaining()` is -1 when unknown or while charging.

### Positioning

`GnssFix`: validity, latitude, longitude, accuracy as `hdop` (smaller is better, about 5 metres per unit), satellites, UTC time. A valid fix has finite coordinates inside ±90 and ±180 and a non-negative accuracy; of two valid fixes the smaller `hdop` is the more accurate. `readFix()` is false when the receiver is switched off and true with an invalid fix while it runs without a position. A position from an earlier run is never returned after a restart, or the device would report yesterday's position.

### Modem

States: `Off`, `PoweringOn`, `Initializing`, `Registering` (searching a network), `Ready` (registered and idle), `Error` (also a locked or missing SIM), `Dialing`, `RingingIn`, `InCall`. `sendSms()` returning true means *accepted*; a refusal after that arrives through `pollSendFailure()` exactly once, with the modem's own reason (for the log, not the child). Call events are `Incoming`, `Connected`, `Ended`, `Missed`, `Busy`, `NoAnswer`, `DialFailed`:

- an incoming call produces `Incoming`, then `Connected` and `Ended`, or `Missed`;
- an outgoing call produces `Connected` and `Ended`, or one of `Busy`, `NoAnswer`, `DialFailed`;
- `Ended`, `Missed`, `Busy`, `NoAnswer` and `DialFailed` are final, and `Ended` is delivered once whoever hangs up;
- a dial we cancel ourselves ends with `Ended` and duration 0 (no `Connected` before it); a ringing call we refuse is `Missed`.

`voiceAvailable()` is false when the network registered the SIM for SMS only: writing works, calls do not, and the user must see that this is not "no network". `SimStatus` tells a SIM that wants its PIN apart from "no network". Call volume is 0–7, clamped (the modem's codec sets it; -1 when it cannot). `query()` is for the diagnosis over USB and accepts only read forms (`AT+…?`).

### Log

The log cannot be switched off: that a query was made must always be verifiable. The level of detail is applied by the caller with `loglevel::shouldWrite` (see [log-level.md](log-level.md)), so a line that must never be dropped cannot be lost in an implementation. A record is one call; the implementation adds the time when the clock knows it.

### Vibration

`HapticPattern` holds the patterns as data (a list of steps: fire an effect, then wait) and `HapticPlayer` plays one without blocking: `poll(nowMs, effect)` says when to fire, and it compares differences, so the pattern survives the wrap of the millisecond counter. The patterns: click (one short pulse), ring (one long), message (two short), sent (three short), failed (short, then long). The pauses are at least 150 ms, or two pulses blur into one. **They have not been felt on the device;** the numbers come from the motor driver's data sheet. `Haptic::service()` fires what is due, `busy()` keeps the processor awake while pulses are to come.

### Files

Paths are absolute and come from outside; every implementation checks them with the rules of [safe-path.md](safe-path.md) first. `write()` replaces a file completely: the data goes to a scratch file and replaces the old file only when it is complete, so a full card, a pulled card or a power loss leaves the old content intact. `read()` and `readTail()` distinguish a missing file (false) from an empty one (true, empty); `readTail()` gives the end of a log, where the last error is. `fileSize()` is -1 for a missing file. `remove()` is true only when a file was removed. Scratch files are never visible.

### WLAN and TCP

Station or access point; `startAccessPoint()` refuses a password outside 8–63 characters, `startStation()` an empty name. Nothing is written to flash memory: credentials stand on the card and nowhere else. `address()` is empty until there is one. The TCP server hands out one connection at a time, valid until `close()` or the next `accept()`.

### Motion

The sensor is a small computer with its own firmware and supply rail, so it runs only while a screen needs it. Values are in g, lying flat is about `az = +1`. See [shake-detector.md](shake-detector.md).

### Radio

Optional. `send()` with an empty contact means the default contact. Each sent message gets exactly one `RadioDelivery` report, in the order of sending, so the application can match it to the oldest open message; `confirmed = false` means "we do not know", not "lost".

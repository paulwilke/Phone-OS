# Test doubles

Code: `testing/fakes/` (header files), tests of their scripting in `test/test_fakes/`. The directory is **not** part of the firmware: only `-I testing` of the host tests and of the simulator puts it on an include path, as `<fakes/FakeClock.h>`.

Every capability of [hardware-capabilities.md](hardware-capabilities.md) has one scriptable fake, small enough to read in one sitting. Each is the first implementation of its interface and passes the same contract suite as any later one. A test drives it through a small **scripting surface**, documented at the top of its header; the simulator uses the same surface from outside: its console commands call these functions ([simulator.md](simulator.md)). Where the simulator needs more than a fake offers it extends the fake and its contract test, and does not write a second one.

| Fake | Scripting (what a test or the simulator can make it do) | Reading back |
|---|---|---|
| `FakeClock` | `advance(ms)`, `advanceSeconds(s)`; the software sets the epoch and offset through the real setters | the interface |
| `FakePower` | `setBattery`, `setMinutesRemaining`, `plugIn(Charging\|Full)`, `unplug`, `setChargerAnswers(false)`, `failDomain(domain, true)`, `allowPowerOff(bool)` | `powerOffCalls()`, `switchOnCount(domain)` |
| `FakeGnss` | scenarios on the power domain and the clock: `fixAfter(seconds, lat, lon, hdop)`, `never()`, `degradeAfter(seconds, hdop)`, `setSatellites` | the interface |
| `FakeModem` | `setNetwork(operator, bars, voice)` (`voice` is a `VoiceStatus`: `Ready`, `SmsOnly`, `ImsMissing`), `setSimStatus`, `injectSms`, `injectIncomingCall`, `announceCaller` (the number arrives after the ring), `remoteAnswer`, `remoteBusy`, `remoteNoAnswer`, `failDial(reason, why)` (a `DialFailReason`), `restartDuringCall`, `remoteHangup`, `failNextSms(reason, count)`, `setQueryAnswer`, `setVolumeSupported` | `sent()`, `asleep()`, `callPhone()` |
| `FakeAtTransport` | `reply(command, lines, final)`, `replyAfter`, `silence`, `replyPrompt` and `replyToBody`, `unsolicited(time, line)`, `restart()`, `setEcho`, `setCold` (silent until the power key), `advance`, `setNow` | `sent()`, `bodies()`, `unexpected()`, `powered()`, `pwrkeyPulses()`, `audioToModem()`, `audioSwitches()`, `dtr()`, `escapes()` |
| `FakeEventLog` | none | `entries()` (with the epoch the clock knew), `lines()`, `contains()` |
| `FakeHaptic` | none; it runs the real pattern player on the fake clock | `played()`, `fired()` with times |
| `FakeConfigStore` | `put`, `failNextWrite()` (half of the data reaches the scratch file, the old file stays), `failAllWrites(bool)` | `scratchFileCount()` |
| `FakeWifi` | `finishConnecting(success)`, `failAccessPoint`, `setStationAddress` | `ssid()`, `password()` |
| `FakeTcpServer` | `connect()` returns a client to script: `fromPeer(bytes)`, `peerClose()`, `limitWritesTo(n)` | `sentToPeer()`, `closedBySoftware()`, `listening()`, `port()` |
| `FakeMotion` | `setAvailable`, `failNextStart`, `tilt(x, y, z)`, `clearSample`, `shake()` | the interface |
| `FakeSdVolume` | `removeCard`/`insertCard`, `mountsOnlyAtOrBelow(hz)`, `refuseMount`, `setCapacity`, `failNextWrite`, `failAllWrites`, `failRenames`, `failRemoves`, `failReads`, `loseCardAfter(n)`/`powerBack()` (the board's card classes run on it) | `operations()`, `mountAttempts()`, `file()`, `paths()`, `readCount()` |
| `FakeWifiRadio`, `FakeTcpListener` | `finishConnecting`, `failAccessPoint`, `setStationAddress`; `connect()` returns a client to script (the board's Wi-Fi and TCP classes run on them) | `beginStationCalls()`, `shutdowns()`, the client's `received()`, `stopped()` |
| `FakeRadioLink` | `setUp`, `addPeer`, `setDefaultPeer`, `deliver`, `confirm(arrived)` for the oldest open message, `setRssi` | `sent()` |

## What they keep

A fake that cheats would let a test pass that the device then fails. They therefore keep the contracts:

- **`FakeModem`** delivers call events in a legal order (see the contract in the `Modem` header), reports a scripted send failure exactly once with the modem's reason, ends a call when it is switched off, refuses to dial while its voice status is `SmsOnly` or `ImsMissing`, fails or ends a call that a restart cuts off, accepts only read forms in `query()`, and clamps the volume to 0–7.
- **`FakeAtTransport`** answers over the byte protocol of the real modem (CR LF framing, the prompt `> ` without a line end, Ctrl-Z, `*ATREADY` after a restart) and says what it was asked that nobody scripted; see [at-layer.md](at-layer.md).
- **`FakeConfigStore`** writes through a scratch file; a failed write leaves the old content and the scratch file is invisible to `read`, `fileSize` and `list`. Names are compared without regard to case. Paths go through `SafePath`.
- **`FakeGnss`** follows the power domain: off gives `false`, and every switch-on (seen or not) starts the scenario again, so a position from an earlier run is never returned.
- **`FakeClock`** refuses dates before 2020 and offsets outside −12:00 … +14:00, like the interface says.
- **`FakeRadioLink`** reports one delivery per sent message, in the order of sending.

The scripting surface of each is tested in `test/test_fakes/` (24 tests); that every implementation, the fakes included, honours the interface contracts is the business of the contract suites.

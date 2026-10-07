# Profile

Code: `src/app/Profile.{h,cpp}`, `src/app/Logger.h`; test helper `testing/ProfileBuilder.h`; tests in `test/test_profile/`.

The `Profile` is the settings the behaviour of the device depends on, as one plain struct. The application core reads it and never parses anything: `ConfigLoader` reads the configuration files into this struct ([config-loader.md](config-loader.md)). A test builds a profile with `ProfileBuilder` (two parents and Grandma, one call action, everything else at its default) and says only what differs: `ProfileBuilder().schoolTime("tue", "14:00", "15:00").radio(true).build()`.

The defaults are those of a freshly flashed device that has no settings file: message vibration and call vibration on, vibration for a call in school time off, modem sleep allowed, the remote query written to the history, the Google Maps address template, English as the language of every SMS, all log lines written, no radio, no free text, no parent tools.

| Field | What it is |
|---|---|
| `contacts` | the contact book: who may call and write, who is a parent |
| `quietHours` | the school-time windows |
| `callTargets` | the numbers of the home-screen call actions, in order; they may be called without being contacts, and the first is whom SOS reaches by default |
| `timeZone` | the zone school time and the clock follow, or none |
| `freeText`, `radioAllowed`, `parentTools` | what the child may do |
| `messageVibration`, `callVibration`, `quietCallVibration` | the three vibration switches |
| `modemSleep` | whether the modem may sleep when idle |
| `batteryCapacityMah` | the size of the battery in mAh (default 1500); at start the device writes it into the fuel gauge when the gauge holds another one ([device-power.md](device-power.md)) |
| `modemApn` | the access point name for the modem's data context 1, which the IMS bearer for calls is built from; empty leaves the modem's own contexts alone ([modem-driver.md](modem-driver.md)) |
| `childName`, `mapTemplate`, `smsLanguage`, `extraTriggers` | what goes into the SMS the device sends and what triggers a remote query |
| `commandNotice` | how visible a remote query is on the device: log only, history, or history and a notice |
| `logLevel` | how much of the log is written (see [log-level.md](log-level.md)) |

Two questions are answered by the profile itself, because they are rules of the household and not of a component:

- `mayCall(number)`: the number is in the contact book (in any spelling: `0170 1111111` is the contact `+491701111111`) or is the target of a call action. An empty text and anything that is not a phone number is refused.
- `mayReceiveFrom(number)`: may this sender reach the child, by SMS or by call? The same notion as `mayCall`, kept as its own function so that messages and calls cannot drift apart. A name, a short code or nothing at all is never allowed. There is no setting that changes it.
- `sosRecipient(target)`: the target when one is given (it needs no list entry), otherwise the first call target; empty when there is nobody, in which case there is no SOS button.

`schoolTime(epoch, offset, &end)` asks the quiet hours and gives the minute school time ends. An unknown time (epoch 0) never locks: the device has no clock of its own, and guessing would be wrong both ways ([quiet-hours.md](quiet-hours.md)). A window that cannot be read is refused by `QuietHours::add` with a reason and leaves no lock behind.

## Logger

`Logger` writes a line to the event log unless the profile's log level says the line is not worth the space on the card. What a line is worth follows from its words ([log-level.md](log-level.md)); a line about a remote query, an SOS, a lifted lock or a restart is never dropped. The lines are English and technical: they are for the parents and for whoever repairs the device, and they are not translated.

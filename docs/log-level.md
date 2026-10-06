# Log level

Code: `src/app/LogLevel.{h,cpp}`, tests in `test/test_log_level/`. Pure C++17.

The log on the SD card can be thinned out to make the card last. The parents choose the level on the web page (`all`, `events` or `errors`); some lines are never dropped on any level.

## Levels and what they keep

| Setting | Keeps |
|---|---|
| `all` (and anything unknown) | everything |
| `events` | always-kept lines, errors and events |
| `errors` | always-kept lines and errors |

`loglevel::parse()` reads the setting. It is exact: `Errors` or a typo selects `all`, because writing too much is the safe mistake.

## What is a line worth

There is no category at the call site. `loglevel::classify()` derives the importance from the words in the line, most important class first, so a line that fits several classes gets the highest:

| Importance | Keywords | Examples |
|---|---|---|
| **Always** (never dropped) | `command`, `remote query`, `sos`, `quiet hours`, `restart`, `firmware`, `shutdown`, `config`, `web:`, `not in the contact book`, `number withheld` | "Remote query answer sent", "SOS ping started", "Quiet hours lifted by emergency", "Restart requested", "Firmware 0.1.0 started", "SMS dropped: sender not in the contact book (+491709999999)", "Call rejected: number withheld" |
| **Error** | `failed`, `rejected`, `not found`, `not responding`, `error`, `not established`, `crash`, `blocked`, `cannot` | "Modem not responding", "SMS rejected: +CMS ERROR: 500" |
| **Event** | `call`, `sent`, `received`, `power saving`, `wifi`, `cellular`, `message`, `mesh running`, `sim:` | "Call from Parent One", "Message received", "WiFi connected" |
| **Detail** | everything else, a null or empty line | "Modem woke up", "GPS fix: 9 satellites" |

A text or call that was dropped because its sender is not in the contact book is on the list for the same reason: parents must be able to see that something knocked, whatever the level. A remote query, an emergency, a lifted lock, a restart and a firmware change are on the Always list because it must be provable afterwards that somebody queried the device or that an emergency was sent. A failure of such an action stays Always: "Remote query failed" is kept on every level.

### How keywords match

- **Case does not matter** (`SOS`, `Sos`, `sos`).
- A keyword must **start a word**: `restarted` and `SOS-Grandma` match, `consent` is not `sent` and `Rosos` is not `sos`. Anything may follow the keyword.
- The matching errs on the side of keeping a line. A contact whose name contains a keyword makes any line with that name an Always line; that costs space on the card, never an audit entry.
- Whoever writes a log line must use the vocabulary above for what matters, and avoid it for noise: a line about polling the modem that contains the word "message" is an Event and survives on the `events` level.

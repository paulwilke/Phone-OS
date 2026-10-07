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
| **Always** (never dropped) | `command`, `remote query`, `sos`, `quiet hours`, `restart`, `firmware`, `shutdown`, `config`, `web:`, `not in the contact book`, `number withheld`, `not the configured radio peer`, `wi-fi: link` | "Remote query answer sent", "SOS ping started", "Quiet hours lifted by emergency", "Restart requested", "Firmware 0.1.0 started", "SMS dropped: sender not in the contact book (+491709999999)", "Call rejected: number withheld", "Wi-Fi: link not connected (status 6, ...); waiting 5 s before reconnecting" |
| **Error** | `failed`, `rejected`, `not found`, `not responding`, `error`, `not established`, `crash`, `blocked`, `cannot` | "Modem not responding", "SMS rejected: +CMS ERROR: 500" |
| **Event** | `call`, `sent`, `received`, `power saving`, `wifi`, `cellular`, `message`, `mesh running`, `sim:` | "Call from Parent One", "Message received", "WiFi connected" |
| **Detail** | everything else, a null or empty line | "Modem woke up", "GPS fix: 9 satellites" |

A text or call that was dropped because its sender is not in the contact book, and a radio message from a name that is not the configured peer, are on the list for the same reason: parents must be able to see that something knocked, whatever the level. A remote query, an emergency, a lifted lock, a restart and a firmware change are on the Always list because it must be provable afterwards that somebody queried the device or that an emergency was sent. A failure of such an action stays Always: "Remote query failed" is kept on every level.

## Dropped texts, calls and radio messages: rate and text

What the device turns away is logged on every level (the lines above) but **not every time**, and **with the text**. This replaces the earlier rule that a log line never carries a message's text; the owner decided it on 2026-10-06 so that the parents can read a balance, a tariff or a PIN notice that the network or the provider sent ([contacts.md](contacts.md), [messaging.md](messaging.md)). The code is `src/app/DropLog.{h,cpp}`, the tests `test/test_drop_log/`.

- **Dropped text can land in the log**, and the log is readable through the web page with the web password ([web-interface.md](web-interface.md)). Whoever has that password can read what strangers and providers wrote to the child's number. Nothing else of a message is written, and nothing of a call (a call has no text).
- **At most one line per sender per ten minutes.** The first drop of a sender writes the line, with the first 120 characters of the text (a control character is replaced by a space; a text of 500 characters is cut at 120 characters, never inside a character; an empty text has no text part). What is dropped from the same sender meanwhile is counted, and one more line says how many, when the ten minutes are over: `SMS dropped: sender not in the contact book (+491709999999) ... (17 more in the last 10 minutes)`. That line is a line too: the next first line of the sender comes ten minutes after it.
- **A sender is its number in canonical form**, so one number in three spellings is one sender, and a text and a call from the same number share the window. A sender that is not a number (a name such as `Provider`) is one sender by its text. A radio name is a sender of its own kind.
- **The table of senders is bounded** (32): a flood of different numbers cannot grow it, and the sender heard longest ago is forgotten first, so it may get a line sooner than ten minutes after its last. Each different number still gets its own line: the rule limits one sender, not the number of senders.
- **The clock** is the uptime, which does not jump; a time earlier than the last line would be a new window and not a log that stays silent.

### How keywords match

- **Case does not matter** (`SOS`, `Sos`, `sos`).
- A keyword must **start a word**: `restarted` and `SOS-Grandma` match, `consent` is not `sent` and `Rosos` is not `sos`. Anything may follow the keyword.
- The matching errs on the side of keeping a line. A contact whose name contains a keyword makes any line with that name an Always line; that costs space on the card, never an audit entry.
- Whoever writes a log line must use the vocabulary above for what matters, and avoid it for noise: a line about polling the modem that contains the word "message" is an Event and survives on the `events` level.

# Notices and system entries

Code: `src/app/Notice.{h,cpp}`, `src/app/SystemEntry.{h,cpp}`; tests in `test/test_notice/` and `test/test_message_store/`.

The application core contains **no sentence**. What it has to tell the user it tells as a **code with a few values**, and the interface words the code from the catalogs when it draws. Two things are told this way:

- a **notice**: the line under the status bar that shows for a few seconds ("School time until 13:30", "Papa: busy");
- a **system entry**: a row of the message history that is not a message ("Missed call from Grandma").

The reason is the language. An entry written yesterday is worded in today's language, because only a kind and a value were stored, and a contact renamed since is renamed in the history too, because only the number was stored. The only text the core produces is the body of an SMS that leaves the device, and its wording comes from the catalogs as well (see [sos-ping.md](sos-ping.md)).

## The notice line

`Notices` (`src/app/Notices.{h,cpp}`, tests in `test/test_notices/`) holds the notice that is showing. There is at most one: a newer notice replaces the older one, because the newest is what the child needs to know ("Message is out" replaces "Message waits for network"), and it takes the full time for itself. A notice expires after `Notices::kSeconds` (8) seconds of uptime and reads as none afterwards. `current()` is none or the notice; `clear()` removes it.

## Notices

A `Notice` is a `NoticeKind`, a `peer` (a phone number, for the kinds that name somebody) and a `minute` (the minute of the day, for the one kind that carries a time). The interface looks the name of `peer` up in the contact book and writes `minute` the way the language writes a time. `noticeWording(kind)` gives the catalog key of a kind and says which placeholders the code supplies: `{name}` and `{time}`.

| Kind | Meaning | Placeholders |
|---|---|---|
| `SchoolTimeUntil` | the child tried something school time forbids | time |
| `CallNotAllowed`, `CellularIsOff`, `NoNetwork` | a call attempt was refused | |
| `CallBusy`, `CallNoAnswer`, `CallNoNetwork`, `CallSmsOnly`, `CallNotReady`, `CallModemRestarted`, `CallFailed` | a call did not happen | name |
| `CallerInSchoolTime` | an incoming call from a contact was rejected in school time | name |
| `MessageWaiting`, `MessageSent`, `MessageSentRadio`, `QueueFull`, `SmsRejected`, `RadioUnreachable`, `RadioUnconfirmed` | what happened to a message | |
| `LocationQueried`, `QueryRefused` | a remote query (shown only when the profile says so); `LocationQueried` has the words of the history entry, `QueryRefused` words of its own that are short, because the one who asked is a stranger, shown by number, and a number is never shortened | name |
| `CellularSwitchedOn`, `CellularSwitchedOff`, `WifiBatteryLow`, `PowerOffRefused`, `LightOn`, `LightOff` | the device | |

A test checks that **every kind has a text in every shipped language** with exactly the placeholders it supplies, and that no language falls back to English for any of them.

## System entries

A system entry is a message whose source is `System` and whose text is `@<kind>` or `@<kind> <argument>`; the other party is the `peer` field as for every message. `sysentry::make()` writes the text and refuses anything that is not a catalog word (lower-case letters, digits, hyphens), so an entry is one word or two and passes through the log file unchanged. `sysentry::parse()` reads it back and refuses anything else: a message of another source that happens to start with `@`, and the entries of earlier versions, which hold a sentence and are shown as they are.

| Kind | Peer | Argument |
|---|---|---|
| `missed-call` | the caller | |
| `call-failed` | the number called | the reason: `busy`, `no-answer`, `no-network`, `sms-only`, `not-connected` |
| `location-queried` | the parent who asked | |
| `query-refused` | the number that asked | |
| `sms-rejected` | the recipient | |
| `location-query-failed` | the parent whose reply did not go out | |

`systemKey(kind)` gives the catalog key of a kind (`system.*`, placeholder `{name}`, and `{reason}` for a failed call) and `callReasonKey(reason)` the key of a reason (`call.reason-*`); both give `nullptr` for something this version does not know, and the interface shows the stored text.

The file format of the message history is unchanged ([message-store.md](message-store.md)).

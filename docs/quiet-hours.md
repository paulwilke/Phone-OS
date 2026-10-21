# Quiet hours (school time)

Code: `src/app/QuietHours.{h,cpp}`, tests in `test/test_quiet_hours/`. Pure C++17: the module computes whether a quiet period is running; what is allowed or blocked during one is decided elsewhere. It contains no text; the settings page builds its words from `window(i)`.

## Windows

A window is a set of weekdays, a start and an end:

| Part | Accepts | Rejected |
|---|---|---|
| days | `mon-fri`, a list `mon,wed,fri`, a range across the weekend `fri-mon`, `daily`; three-letter English names in any case, blanks around entries are ignored | other names (`monday`, `mo`), empty entries (`mon,,tue`, `mon,`), `mon-fri-sun`, `daily,mon` |
| time | `H:MM` or `HH:MM`, `0:00` to `23:59` | `24:00`, `08:60`, `8`, `8:5`, `+8:00`, surrounding blanks, anything after the minutes |

`add(days, from, to, enabled = true)` returns what happened instead of a bare yes or no: `Added`, `BadDays`, `BadTime`, `EmptyWindow` (start equals end: neither "nothing" nor "all day" is guessed) or `DayFull`. Nothing is added unless the result is `Added`.

## Switching a window off

Every window has an `enabled` flag (`enabled = true` or `false` in `[[quiet_hours]]`, the "active" checkbox on the parents' page). A window that is **not enabled stays in the list but never makes school time**: `active()` skips it, and it is the one place that decides, so calls, texts, the status bar, the lock page and the SOS five-tap rule all see the same thing. A disabled window does not shorten or extend an enabled one either (the reported end minute comes from enabled windows only). It still counts towards the two windows per day: otherwise switching it on later could break the limit.

A window without the key is **enabled**, so every card written before the key existed behaves as before.

**A card without any window** (no `[[quiet_hours]]`, or `quiet_hours = []` as the parents' page writes an empty list) gets one built-in window, **Monday to Friday, 08:00 to 12:30, disabled**. The parents see a window to adjust and tick; until they do, no school time runs. A card that lists windows gets exactly those, and a card whose listed windows are all refused (a bad day name, say) gets no default either: it has said what it wants and the loader reports it. The default lives in `config/schema.toml` (`default_entry` of the section), so the loader, the generated example file and the page's form description read the same values ([config-loader.md](core/config-loader.md)).

**At most two windows per day** (`kMaxPerDay`, so 14 in all): morning and afternoon lessons. A window counts for each day it begins on; one that crosses midnight counts for the day it begins. A window naming several days is refused as a whole when any one of those days is full.

## Is a quiet period running?

`active(epochUtc, tzOffsetMinutes, &endMinute)` takes a Unix time and the zone's offset at that instant (from `tz::offsetMinutes`, see [time-zones.md](core/time-zones.md)).

- A window is active from its start minute up to, but not including, its end minute: at `13:30` of a `08:00`–`13:30` window the phone is free again.
- A window may **cross midnight** (`22:00`–`06:00`): the evening belongs to the day the window began, the morning to the next. The morning of Saturday belongs to a Friday window even though Saturday is not in its days. An end of `00:00` means the end of that day.
- **No known time, no lock.** `epochUtc` 0 means the clock is not set and never locks, even though Unix time 0 would be a Thursday morning. The board has no clock of its own; after a restart without network the time is unknown, and without network the child cannot call anyway.
- `endMinute` receives the minute of the day at which the period ends (`13:30` = 810) and is left alone when nothing is active. With **overlapping windows** it is the end of the one that lasts longest, so that an emergency lifting the lock "until the end of school time" lifts it until the lock really ends. Windows that only **touch** (end `10:00`, next start `10:00`) are two periods: at 10:00 the second begins with its own end.
- The weekday comes from the local time, so a time zone offset can move an instant to the neighbouring day. The offset is the caller's: school time follows the change to winter time only when the offset passed in does.

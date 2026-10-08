# SOS ping

Code: `src/app/SosPing.{h,cpp}` and `src/hal/GnssFix.h`, tests in `test/test_sos_ping/`. Pure C++17: the receiver and the modem are reached through `SosPing::Host`.

The SOS ping sends the child's position to the emergency contact. Its whole subtlety is the waiting. A receiver delivers a first position within seconds, but it is off by hundreds of metres; it becomes useful only when several satellites are in view.

## Stages

| Stage | Meaning |
|---|---|
| `Idle` | nothing runs |
| `Search` | step 1: the receiver looks for satellites |
| `Refine` | step 2: there is a position, the ping waits for a better one |
| `Done` | step 3: the message was handed over (`progress()` is 3) |
| `Failed` | it could not be sent, or the modem rejected it afterwards |

`poll(now)` is called regularly. It reads a position, keeps the best one, and sends when

- the best position is accurate (`hdop < goodHdop`, 2.5 by default, about 12 metres) **and** at least `minSeconds` (25) have passed, or
- `timeoutSeconds` (120) have passed: what there is gets sent. The timeout wins if the minimum is set longer than it.

**Without any position during the search the last known one goes out** (owner, 2026-10-08: an SOS from inside a house went out without any place). The host's `lastKnownFix` (Positioning: the last valid fix the receiver had since the device started, whoever asked for it, with its age) fills `sms.sos.last-known` (or `-no-map`), which says in capitals that it is the **last known** position and how old it is ("25 min", "3 h", "2 d": `sms.sos.age-minutes`, `-hours`, `-days`), so that nobody takes it for the present place. A position found during the search always wins over the remembered one. Without a remembered one either, `sms.sos.no-gps` goes out as before. The last known position lives in memory: after switching off and on there is none until the receiver finds one.

**The more accurate position wins, not the newer one.** While searching, the HDOP fluctuates, and sending the last one would send a random, possibly the worst, position. On equal accuracy the earlier one stays. A position that the receiver calls valid but that holds NaN, a negative HDOP or coordinates off the globe is ignored; it would otherwise stay "best" for ever, because every comparison with NaN is false.

Without any position a message goes out anyway ("no GPS"): the child pressed, and the parents must hear of it.

`noteSendFailed(peer)` turns `Done` into `Failed` when the modem rejects the message after it was handed over, but only for the peer of this ping.

## Kinds

| Kind | For | Head of the message |
|---|---|---|
| `Emergency` | the child asks for help at once | "{name} sends SOS" |
| `Pickup` | the child asks to be picked up | "{name} asks: please pick me up" |
| `Query` | the reply to a parent's remote location query; no alarm for the child | "{name} is here" |

`start(peer, now, kind)` is refused for an empty peer, an incomplete wording, or while a ping runs, with one exception: **a running query reply gives way to an emergency or pickup**. Otherwise the child's call for help would be lost whenever a parent had asked for the position a moment before. A query never replaces anything, and an emergency is never replaced.

## The wording is an input

The module contains no sentence. The caller hands over templates (`Wording`), which it loads from the catalogs; all nine must be set, or `start()` refuses. The catalog keys and placeholders:

| Key | Placeholders |
|---|---|
| `sms.sos.emergency`, `sms.sos.pickup`, `sms.sos.location` | `{name}` |
| `sms.sos.emergency-anonymous`, `sms.sos.pickup-anonymous`, `sms.sos.location-anonymous` | none: used when the child has no name |
| `sms.sos.position` | `{head}` (the filled head), `{map}`, `{coordinates}`, `{accuracy}` (metres) |
| `sms.sos.position-no-map` | `{head}`, `{coordinates}`, `{accuracy}`: used when the map template is empty |
| `sms.sos.no-gps` | `{head}`, `{seconds}` |
| `sms.sos.last-known` | `{head}`, `{age}`, `{map}`, `{accuracy}` |
| `sms.sos.last-known-no-map` | `{head}`, `{age}`, `{coordinates}`, `{accuracy}`: used when the map template is empty |
| `sms.sos.age-minutes`, `-hours`, `-days` | `{n}` |

The map address is built from the parents' template by `maplink` (see [map-link.md](map-link.md)); the coordinates are always in the text, with five decimals. The text is built from the best position, the child's name (`setWho`) and the template (`setMapTemplate`).

### One SMS

An emergency must be one SMS. A text with a character outside the GSM 7-bit alphabet is sent as UCS-2, which fits 70 characters per part instead of 160. `scripts/check_i18n.py` therefore rejects such characters in every `sms.*` message (a French "ê" or "ç" would split the message), and `test_sos_ping` renders every kind, with and without name, map and position, in all shipped languages with the longest name (23 bytes), the longest accuracy (9999 m) and negative coordinates, and requires at most 160 characters and no unfilled placeholder.

## Accuracy

`accuracyMeters(hdop)` is `hdop × 5`, rounded, between 1 and 9999; NaN gives 1. The same rule serves every reply so that they all mean the same.

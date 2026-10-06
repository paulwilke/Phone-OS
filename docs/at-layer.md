# The AT layer of the modem driver

Code: `src/drivers/AtTransport.h`, `AtParser.{h,cpp}`, `Ucs2.{h,cpp}`; tests in `test/test_at_parser/`, `test/test_ucs2/`, `test/test_fake_at_transport/` and `test/test_at_fuzz/`; the scripted modem is `testing/fakes/FakeAtTransport.h` ([test-doubles.md](test-doubles.md)). This is the reading side of the AT protocol of the A7682E and the one seam to the UART. The driver that uses it (jobs, calls, messages) is not part of this document yet. **Nothing here has been run against the real modem:** the formats are those of 3GPP TS 27.005 and 27.007, of the SIMCom manual and of what the previous driver met on the device; real captures belong into the tests as further cases.

`src/drivers/` implements the interfaces of `src/hal/` and includes nothing but `hal/` and itself (`scripts/check_layers.py`). It has no MeshCore and no platform header: what stands below `AtTransport` comes with the device drivers.

## The seam

`AtTransport` has the three functions of the byte pipe: `write(bytes)`, `read()` (the next byte or -1, never blocking) and `now()` (milliseconds, wrapping after about 49 days); and three for the board: `power()` and `pwrkeyPulse()` for the modem's supply and power key, `setAudioToModem()` for the sound path. `at::Deadline` compares differences, so a limit set just before the wrap neither fires at once nor never.

## Lines

`LineReader::feed(byte)` returns true when a line is complete. A line is at most 1500 bytes, because a UCS2 text is **one line** of four hex digits per character and the longest text kept (1400 raw bytes) must arrive whole; the first version held 512 and silently cut a 160-character text to 128. A longer line is cut at a multiple of four and flagged (`truncated()`). NUL bytes are line noise and are dropped. After `AT+CMGS` the modem sends `> ` without a line end, which the prompt mode reports as the line `>`. `reset()` forgets a partial line (after a modem restart).

`classify(line)` compares **whole lines**, so a text that reads `BUSY` is no busy signal. Besides the lines of calls and messages it knows `*ATREADY:` (the modem has restarted) and the answers listed below.

## Answers

| Line | Function | Result |
|---|---|---|
| `+CLIP: "<number>",…` | `parseClip` | the caller's number |
| `+CMTI: "SM",<n>` | `parseCmti` | the storage slot |
| (a dial string) | `isValidDialString` | the only numbers that reach a command line: 1 … 23 characters of 0-9 * # and a leading + |
| `VOICE CALL: END[: d]` | `parseVoiceCallEnd` | seconds, from `m:ss`, `h:mm:ss` or `hhmmss` (the last from the manual, not seen on the device); 0 when absent or unreadable |
| `+CSQ: <rssi>,<ber>` | `parseCsq`, `csqToBars` | 0 … 5 bars |
| `+CREG: <n>,<stat>` | `parseCreg` | `registered()` (1, 5, 6, 7) and `smsOnly()` (6, 7) |
| `+CIREG: <mode>,<info>[,<ext>]` | `parseCireg` | `registered()` (info 1); `ext` 15 is the working bearer |
| `+CGDCONT: <cid>,"<type>","<apn>",…` | `parseCgdcont` | context, type, APN; false when a field does not fit |
| `+CLCC: <idx>,<dir>,<stat>,<mode>,<mpty>[,"<number>"…]` | `parseClcc` | index, direction, state 0 … 6, the number if there is one |
| `+CPIN: READY / SIM PIN / SIM PUK` | `parseCpin`, `isSimNotInserted` | the SIM state; anything else is "not known" |
| `+COPS: …` | `parseCopsName`, `parseFirstOperatorCode`, `operatorNameForCode` | the name (a code is turned into a name for the usual countries); the first code of a scan list |
| `+CCLK: "yy/MM/dd,hh:mm:ss±zz"` | `parseCclk`, `parseScts` | Unix time and zone |

The rules behind them:

- **Numbers are never shortened.** A number longer than 23 characters is refused (`parseClip`, the header of a message, `buildCmgsCommand`): a shortened number is another person's number. The same holds for an APN that does not fit a field of `parseCgdcont`: a cut APN could look equal to the configured one, and then nothing would be written.
- **Registration 6 and 7 count as registered**: the device's job is messages, and a SIM at 7 sent its SMS on 2026-09-30. Whether *calls* work is a question of the IMS (`+CIREG`), not of this status.
- **A modem without network time** runs its own clock from 1970 or 2004. `parseCclk` accepts the years 2020 … 2069 only; no time is better than the year 2070. The day is checked against the month (29 February only in a leap year).
- **`AT+CPIN?` answers `SIM failure` in the first seconds.** Only `SIM not inserted` means a missing card; the driver asks again for anything else.
- `parseCreg` and `parseCireg` read the answer of the query form. The driver asks; it does not switch the URCs on.

## Messages

`SmsResponseParser` reads the answer of `AT+CMGL` and `AT+CMGR` line by line into an array the caller owns. A message is **finished when the next header or the final OK follows**; an answer that was cut off by a timeout leaves the last message on the SIM, and the next read brings it again. The driver deletes from the SIM only what landed in the array (`index >= 0`); `dropped()` counts the messages that found no room, which must not be deleted. In UCS2 a text is pure hex, so a `RING` or `+CLIP` between two messages is recognised as a URC (`Feed::Unsolicited`); in GSM a text line that reads exactly `OK` cannot be told from the status line, which is why UCS2 is the working mode. A text longer than `kMaxSmsBytes` is cut at a character boundary and flagged (`bodyTruncated`).

Messages arrive as `+CMTI` and are read from the SIM storage, as on the device (`AT+CNMI=2,1,0,0,0`). Direct delivery with `+CMT` and PDU frames are not parsed: a message the parser fails on would be lost, where one on the SIM is read again.

## UCS2

`ucs2HexToUtf8` and `utf8ToUcs2Hex` convert between UTF-8 and the hex UTF-16BE of `AT+CSCS="UCS2"`. Surrogate pairs (emoji) are joined, a lone surrogate becomes U+FFFD, a NUL unit is dropped (it would end the string early), overlong UTF-8 forms and surrogates are refused when encoding, and a text that does not fit is cut at a character boundary and flagged.

## The scripted modem

`FakeAtTransport` answers what the driver writes from a table: `reply(command, {lines}, final)`, `silence`, `replyAfter`, `replyPrompt` with `replyToBody` for `AT+CMGS`, `unsolicited(time, line)` and `restart()`; a command nobody scripted is answered `ERROR` and recorded in `unexpected()`. Its own test pins the framing (the lines of an answer follow each other without a gap, the final line has one before it), the order of replies, the prompt, the restart (answers on the way are lost) and the wrapping clock.

## Tests

`test_at_parser` (48 cases): lines in pieces, with noise and too long, the deadline and its wrap, every parser with its edges, the SMS answers (three messages with their slots, ten full-length messages, multi-line texts, UCS2 with umlauts and emoji, a URC in between, a truncated answer, no room left, a sender that cannot be read, a long UCS2 text arriving whole, a cut inside a character). `test_ucs2`, `test_fake_at_transport`. `test_at_fuzz` feeds random bytes, random protocol characters, every prefix of valid streams, damaged valid streams, lines longer than every buffer and fields at the size of their buffers through reader and parsers; run it under `pio test -e native-asan -f test_at_fuzz` to catch what a plain run does not show.

# Message store

Code: `src/app/MessageStore.{h,cpp}`, tests in `test/test_message_store/`. Pure C++17: the file system sits behind the `MessageLog` interface (`append` a line, `readAll` the lines), so the tests use an in-memory log and the device uses the SD card.

The store holds the message history: SMS, radio (mesh) and system entries in one list. The newest 50 (`kCapacity`) are in memory; the complete history is the log file.

## The messages

A `Message` has a running number `seq` (assigned by the store, starting at 1, **never reused**), a Unix time (0 = unknown), a source (`Sms`, `Radio`, `System`), a direction, flags and two fixed text fields: `peer` (a phone number or a radio contact's name, up to 31 bytes) and `text` (up to 160 bytes).

- Outgoing messages are always "read". Incoming ones are unread until `markAllRead()`; `unread()` counts the unread ones **in memory**.
- An outgoing message can be **pending** (waiting in the queue). `markSent(seq, source)` records over which channel it finally left; it fails for a message that is not pending.
- `markDelivered(seq, confirmed)` records the radio network's confirmation of an outgoing message. SMS gets no answer from the network and stays `Unknown`.
- `remove(seq)` deletes a message, read or not. Only messages in memory can be deleted.
- When the ring is full the oldest message leaves memory (not the file).
- A failed write to the log does not lose the message on screen. `writeErrors()` counts the failures so the status page can show that the card is full or missing.

## The log file

Only appended to, never rewritten, which keeps the code simple and survives a power loss in the middle of a write: at worst the last line is damaged. Lines are separated by tabs; everything after the message line is a line of its own:

| Line | Meaning |
|---|---|
| `M seq time source direction peer text` | a message. Source `s` SMS, `m` radio, `y` system; direction `i` in, `o` out, `q` out and waiting |
| `R seq` | everything up to `seq` is read |
| `S seq source` | a waiting message was sent, and over which channel |
| `D seq 1\|0` | the radio confirmed (1) or did not confirm (0) delivery |
| `X seq` | the message is deleted: its `M` line stays in the file and is skipped on reading |

Example (invented values):

```
M	1	1789475520	s	i	+491701111111	Pick us up
M	2	1789475600	m	q	Node One	On my way
S	2	m
R	1
X	1
```

A text or a peer is written on one line: a tab becomes a blank and a line break becomes `|`. The original character is lost, which is the price of the line format.

### Restoring

`restore()` reads the whole log and rebuilds the list; the newest 50 stay. It returns false when there is no log, it cannot be read, or lines were damaged; the readable rest is kept in every case, and `damagedLines()` counts the others. A line is damaged when it is not one of the five kinds above, a number is not made of digits only (or is too large), a field is missing or unknown (a cut-off last line usually lacks its last tab), or a message number is already in memory. Empty lines are skipped, a trailing `\r` (a file touched on Windows) is ignored, and fields that are too long for their buffer are cut on a character boundary.

The read mark is applied after all lines are read, because its line can come before or after the messages it covers. Marks for messages that have already left the ring are ignored.

### Why deleted numbers are not reused

The next number is taken from the highest `M` line in the file, deleted or not. If it were taken from memory, deleting the newest message would free its number, the next message would get it, and the old `X` line would delete the new message at the next restart.

# The message history

Code: `src/ui/screens/MessageListScreen.{h,cpp}`; tests in `test/test_screen_messages/`. What came in and what went out, one list each, and one message at a time to read.

| View | Shows | Keys |
|---|---|---|
| list | two lines a row: who and when with the marks, under it the start of the text | arrows, Read (Enter), `O` the other list, `X` delete |
| message | the head, the whole text wrapped and scrolled by the arrows, the hint "X: delete message" | Back, Reply or Call back |
| delete | "Delete this message?" | Keep (Back), Delete (Enter) |

**Marks.** `✉` unread, `~` radio, `!` an entry of the device. For an outgoing message: "(waiting)" while it sits in the queue; over the radio "arrived" or "unconfirmed" when the network said so. An SMS gets no answer from the network, and nothing is written.

**Entries of the device are worded from codes** (`wording::systemEntry`): a missed call from yesterday reads in today's language, a contact renamed since is renamed in it (also while the entry is open: the view asks for its words again at every frame), a failed location reply to a parent is there too.

**Time** is that of the clock's zone (`AppCore::wallClockOf`): `14:32` for today, the date in front for an earlier day (`09/14 14:32`, `14.09. 14:32`), `--:--` for a message that arrived before the time was known.

**Names** too long for the line are shortened with "..." inside the line; a number never is.

**Reply** is offered for a message that came in from a contact — or from anybody where the profile allows free recipients (`freeText`) — and goes over the radio if it came by radio; it leaves the peer in the session and opens the compose page. **Call back** is offered for an entry of the device about a number the child may call (a missed call) and dials through the core. Nothing is offered for an outgoing message, or for a stranger where free recipients are off.

**Delete** asks first. Keep returns to where it came from; deleting returns to the list and keeps the selection inside it. The history is a record, not the queue: deleting a message that still waits to be sent does not cancel its sending (the core's rule, pinned here too).

Opening a message marks the history read (the store has one mark, "everything up to here"). The page opens on the inbox each time. The letters `O` and `X` are keyboard shortcuts, written in the hints.

Received text outside the character set (an emoji, another alphabet) is drawn as the font's box, one for each, as [i18n.md](i18n.md) says; the characters around it are as they were. A message in each language is tested with its accents, the ligatures and the euro sign.

The text of a message is kept to 160 bytes by the history.

## Tests

`test_screen_messages`: the empty lists; order and unread marks; reading marks read; the window in a long list; waiting and delivery marks (queue, radio arrived and unconfirmed, an SMS without a mark); radio in; missed call worded in three languages, also in an open entry; a failed location reply; dates and unknown time; the message view and its hint; scrolling to the end; long names and a number; reply (contact, stranger with and without free text, radio); call back (allowed, not allowed); outgoing; delete (keep, selection, from the message, a waiting message still goes); touch; the character set of what arrives; layout in four languages (the pseudo-language is exempt from the width of the action word beside the arrows only); pictures in English and German.

# Messaging

Code: `src/app/Messaging.{h,cpp}`, `src/app/Cellular.h`; tests in `test/test_messaging/`. Pure C++17 on the capabilities of [hardware-capabilities.md](../hardware/hardware-capabilities.md); uses the message history ([message-store.md](message-store.md)), the remote query ([sms-command.md](sms-command.md)), the notices ([notices.md](notices.md)) and school time ([school-time.md](school-time.md)).

Messaging owns messages in and out and the rule that decides the way out.

## Incoming

`poll()` takes every SMS the modem has. **Only the contact book may write to the child.** A text from a number that is not allowed (`Profile::mayReceiveFrom`: in the contact book in any spelling, or the target of a call action) is **dropped before anything else happens**: a stranger, a provider's short code, a sender that is a name, an empty sender, and an empty text from any of them. It is not stored, does not count as unread, does not vibrate, produces no notice and does not reach the remote query (a query comes from an admin, who is in the book anyway). The one trace is a log line, `SMS dropped: sender not in the contact book (+491709999999): <start of the text>`, with the number in its canonical form or the words `non-numeric sender` and the name, and the first 120 characters of the text (control characters replaced; no text part for an empty text); **dropped text can land in the log, which is readable through the web page with the web password**. There is at most one such line per sender per ten minutes, followed by one that counts the rest ([log-level.md](log-level.md)); it is an always-kept line so the parents can see on every log level that something knocked. A dropped text still counts as the modem being awake. There is no switch for this rule. An empty text from a contact is stored as before: it is a family member.

**A remote query is recognised first** and is never stored as a message: it is logged (on every log level, as `Remote query from …`), written to the history as a system entry (`location-queried`, or `query-refused` for a number that is not a parent) and, when the profile's command notice says *notify*, shown as a notice; a *silent* profile writes the log only. It never vibrates. A query that finds a search running is logged as busy and still recorded. The trigger words are given to `configure()`: the trigger word of every language of the catalogs (`Translator::textsInAllLanguages`) and the profile's extra words.

Any other SMS (from an allowed number) is stored unread and vibrates the *message* pattern when the profile has message vibration on — **in school time only for a parent**; the message is stored either way. **The radio is a separate path and the contact-book rule does not cover it**: a radio message comes from a name in the radio network, not from a phone number, and the book has only numbers. A message from the radio is stored only when the profile allows the radio, so a simulator that always attaches a radio behaves like a device that starts it only on permission.

`poll()` returns true when the modem did something (a message arrived, a send failed): that proves it is awake, and the power coordination uses it.

**Radio: only the configured peer may write.** When the profile allows the radio, a radio message is stored only if its sender is the node name configured as `[radio] peer` (`Profile::radioPeer`). A message from any other name is dropped like a stranger's text: not stored, not unread, no vibration, and the same log rules (`Radio message dropped: sender not the configured radio peer (<name>): <start of the text>`, an always-kept line, one per name per ten minutes and one summary, text included, [log-level.md](log-level.md)). **With no peer configured no radio message is accepted** (the line then says so). The name is compared **exactly, as the radio reports it** (case and spaces included); what the rule trusts is that name, and **a name can be spoofed by whoever is within range of the radio**, so it keeps out the noise of other nodes and not a person who knows the peer's name ([contacts.md](contacts.md)). Tests: `test_messaging` (peer, another name, no peer, a flood from another name, exact comparison), the scenarios `radio-peer` and `radio-no-peer`.

## Outgoing: cellular, else radio, else the queue

`sendText(peer, text)` sends by the first path that works:

1. **cellular**, when the device is registered (`cellular::registered`: the modem is powered, past the search for a network, and has an operator);
2. the **radio**, when it is attached, the profile allows it and it is ready. The radio knows contacts by name, not by number, so the message goes to the radio's own default contact;
3. otherwise the **queue**: the message is stored as waiting, a notice says so, and `poll()` tries again.

The queue holds `kOutboxSize` (8) messages; one more is refused with a notice and does not enter the history. A queued message keeps its **full text** (up to the modem's limit) although the history entry keeps 160 bytes, cut on a character boundary. The queue goes out in order, as paths allow; a message that goes out by radio becomes a radio entry and is then awaited like any radio message. The notice after a flush says that what was waiting is out, so "waits for network" does not stay on screen.

`hasWork()` says whether the queue could leave: the modem is powered (it will register) or the radio is usable. **A message waiting with no path at all is not work**: staying awake for it helps nobody.

`sendRadioText(peer, text)` sends over the radio and nothing else, to a named contact (empty = the default one): the radio page chooses a contact, not a number. It is refused with a notice when the radio is unusable or does not know the contact, and leaves no entry.

**Deleting a message** removes it from the history. A message still waiting in the queue is *not* cancelled: the history is a record, the queue is a promise made to the user, and an emergency text that has not left must not be lost to a tap on delete. A waiting message pushed out of the history ring by newer ones is still sent.

## After sending

A message sent by radio is later confirmed or not by the radio. The reports come in the order of sending, so the oldest awaited message gets the first report; the history entry becomes *confirmed* or *unconfirmed* (the latter with a notice and a log line). At most `kAwaitingSize` (4) messages are awaited; when a fifth is sent the oldest is forgotten and stays *unknown*. A report nobody waits for is ignored. An SMS gets no report from the network and no state is claimed for it.

A message the modem accepted and the network then refused is reported once by the modem. It is logged with the modem's reason (`SMS to Parent One rejected: …`), written to the history (`sms-rejected`), shown as a notice, vibrated with the *failed* pattern, and the SOS ping is told through the `Hooks`. The hook answers whether the refused text was the silent reply to a parent's query ([emergency.md](emergency.md)); then it stays out of the child's sight (no history entry, notice or vibration) and only the log keeps it.

## Hooks

`Messaging::Hooks` is what Messaging asks of the rest of the core: `startLocationReply(phone)` (a parent asked; false when a search is running) and `smsSendFailed(phone)`.

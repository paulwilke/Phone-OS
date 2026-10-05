# Messaging

Code: `src/app/Messaging.{h,cpp}`, `src/app/Cellular.h`; tests in `test/test_messaging/`. Pure C++17 on the capabilities of [hardware-capabilities.md](hardware-capabilities.md); uses the message history ([message-store.md](message-store.md)), the remote query ([sms-command.md](sms-command.md)), the notices ([notices.md](notices.md)) and school time ([school-time.md](school-time.md)).

Messaging owns messages in and out and the rule that decides the way out.

## Incoming

`poll()` takes every SMS the modem has. **A remote query is recognised first** and is never stored as a message: it is logged (on every log level, as `Remote query from …`), written to the history as a system entry (`location-queried`, or `query-refused` for a number that is not a parent) and, when the profile's command notice says *notify*, shown as a notice; a *silent* profile writes the log only. It never vibrates. A query that finds a search running is logged as busy and still recorded. The trigger words are given to `configure()`: the trigger word of every language of the catalogs (`Translator::textsInAllLanguages`) and the profile's extra words.

Any other SMS is stored unread and vibrates the *message* pattern when the profile has message vibration on — **in school time only for a parent**; the message is stored either way. A message from the radio is stored only when the profile allows the radio, so a simulator that always attaches a radio behaves like a device that starts it only on permission.

`poll()` returns true when the modem did something (a message arrived, a send failed): that proves it is awake, and the power coordination uses it.

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

A message the modem accepted and the network then refused is reported once by the modem. It is logged with the modem's reason (`SMS to Parent One rejected: …`), written to the history (`sms-rejected`), shown as a notice, vibrated with the *failed* pattern, and the SOS ping is told through the `Hooks`.

## Hooks

`Messaging::Hooks` is what Messaging asks of the rest of the core: `startLocationReply(phone)` (a parent asked; false when a search is running) and `smsSendFailed(phone)`.

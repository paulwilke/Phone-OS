# What Phone OS does not protect against

Phone OS is a reduced phone for a child or an elderly person. It is built to keep strangers away from that person and to let the parents find the device, not to keep secrets from someone who holds it. This page lists what the device does against strangers, and then, plainly, what it does not do. A statement here is backed by the document that describes the feature; where something is not checked on the device, it says so.

The limits of security are the owner's decision ([CONTRIBUTING.md](../CONTRIBUTING.md), rule 7). The decisions that shaped this page are in [decisions.md](decisions.md); each is named by its number and in words where it first appears.

## What the device does against strangers

- **Calls and texts only from the contact book.** A text from a number that is not in the contact book (in any spelling) is dropped and does not reach the child ([messaging.md](core/messaging.md)); an incoming call from such a number is rejected ([calls.md](core/calls.md)). A call with no number (suppressed caller identification) is hung up at once as well, because it cannot be matched to a contact.
- **Outgoing calls only to contacts.** The device dials only numbers of the contact book ([calls.md](core/calls.md), `Profile::mayCall` in [profile.md](core/profile.md)).
- **Texts to free numbers only when the parents allow it.** Writing to a number that is not a contact needs `[features] free_text = true`, which is `false` by default ([configuration-reference.md](configuration-reference.md)). With free text on, the device user can also write to short codes and premium-rate numbers, which cost money; leave it off unless the child needs it.
- **The strangers' texts are logged, not shown.** The system log records that a text was dropped, with the number and the first 120 characters ([system log](#data-on-the-card-is-in-plain-text)).

## Forged sender numbers

All of the above rests on one thing: the sender number as the network delivers it. The device cannot tell a real sender number from a forged one. That is a matter of the mobile network, not of the device.

- Calls from abroad that carry a German mobile number as the caller: to our knowledge, German operators must deliver such calls without the number since December 2022 (Telecommunications Act, TKG), so they cannot pass as a contact. This is not legal advice, it is not a promise that every call is stopped, and it says nothing about other countries.
- Text messages can still be sent with a forged sender number through gateways that let the sender choose the name or number. A text that appears to come from a parent may not come from them.
- The device has no way to check. Advice for the parents and the child: when a message from a known number is unusual (an urgent request, money, a link, a changed plan), call that person back on the number you already know, and do not answer on the number in the message.

## The remote position query

A message that consists of nothing but a trigger word (`#WhereAreYou` and the words of the other languages, and any word added by `sms.extra_trigger_words`), sent from a parent's number, makes the device find its position and text it back to that number ([sms-command.md](core/sms-command.md)). Only the sender number as the network delivers it is checked, so everything under "Forged sender numbers" applies: a forged parent number would receive the position.

- **The reply is always visible.** The reply goes out as an ordinary outgoing message, so it appears in the message history and in the card's message log (`messages.log`). The setting `remote_queries.notice` decides only how visible the system entry "query received" is: `silent` hides that entry, `history` shows it in the history, `notify` also gives a notice. `silent` does not hide the reply. The decision is D-54 (the reply to a remote position query always stays visible, and a device can refuse queries).
- **A device can refuse queries entirely.** With `remote_queries.enabled = false` (default `true`) the device answers no query, and the refusal is written to the log. A person who does not want to be found by their parents can ask them to set it; it is a setting on the card, so whoever can write the card can change it.
- **The map link is a Google Maps link by default** ([map-link.md](core/map-link.md)): it opens on most parents' phones, and it is a template the parents can change. The coordinates are in the text too, so a map service sees them only when the link is opened.
- **Positions are on the card.** The reply text and the SOS texts contain the position and are kept in the message log.

## Data on the card is in plain text

Everything on the SD card is stored as ordinary files, readable by anyone who takes the card out ([device-storage-network.md](hardware/device-storage-network.md)). This is D-55 (the data on the card stays in plain text, there is no encryption).

- `secrets.toml` holds the SIM PIN, the parents' web password, the access point password and the passwords of home networks in plain text ([config-loader.md](core/config-loader.md)).
- `messages.log` holds all messages. A deleted message stays in the file, marked as deleted, and is only skipped on reading ([message-store.md](core/message-store.md)).
- `system.log` holds phone numbers and the first 60 characters of every text sent; for texts from strangers that were dropped it holds the number and the first 120 characters.
- Positions appear in the sent SOS texts and in the replies to position queries, and so in `messages.log`.

What loss or theft means: whoever has the device or the card can read the contacts, the messages and positions, and the passwords, and knows the SIM's PIN. Report a lost device to the operator so that the SIM is blocked.

Why there is no encryption: without flash encryption the key would lie readable in the processor's flash next to the card, and the parents' web page reads the log, so it would need the key as well. The device is a child's phone, not a privacy phone, and the limit is stated here instead.

## No device PIN

The device has a pocket lock that needs two presses of Enter ([screen-lock.md](ui/screen-lock.md)). It keeps a pocket from pressing keys; it is no protection against a person: anyone can unlock it. A lock code against loss is an open point ([open-points.md](open-points.md)).

## The USB console

The console on the USB port answers any computer the device is plugged into, without a password. It can read `secrets.toml`, write the contacts and send a text in the child's name ([device-console.md](hardware/device-console.md)). This is D-28 (the USB console asks for no password, the risk is written down, a restriction is open); the restriction is open point 33. Do not plug the device into a computer you do not trust.

## The parents' web page

The page is served over HTTP, not HTTPS, in the home network or on the device's own access point ([web-interface.md](web/web-interface.md)). Anyone in that network can read what is sent, the password included. Behind the page:

- D-26 (the parents' page keeps the password for the browser tab only): the credential is dropped when the tab closes.
- D-29 (the access point's random password stays eight digits): a recording of the Wi-Fi handshake can be tried offline in minutes, and whoever gets into the access point can read the page's password, because the page speaks HTTP.
- Wi-Fi is off by default and switches itself off after 10 minutes without use.
- Five wrong passwords in a row lock everyone out for 60 seconds. That protects against guessing and also lets anyone in the same network lock the parents out for a minute at a time.

## The chip and the modem

- The ESP32-S3 runs without Secure Boot and without flash encryption. Fault-injection advisories exist for the chip; they matter little while the card is in plain text, because whoever has the device has the data anyway.
- The modem does not pass the SIM Toolkit (commands from the operator's SIM to the phone) to the processor.
- The modem's firmware is never updated (D-15: the modem firmware is not updated). A hostile modem firmware could record calls and texts, and nothing in the device guards against it.

## The mobile network

The device does not defend against being located by the network (the cell the device is in is known to the operator), nor against a false base station (an IMSI catcher) that makes the device register with it. Texts and calls are as private as the network makes them.

## Things the device does not do and this page does not promise

- No firmware binary is handed out (D-23: no firmware binary is handed out); whoever builds the firmware answers for the result.
- The radio (LoRa) has no driver yet ([open-points.md](open-points.md), point 9).

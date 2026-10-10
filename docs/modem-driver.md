# The modem driver

Code: `src/drivers/A7682EDriver.{h,cpp}` on the AT layer ([at-layer.md](at-layer.md)); tests in `test/test_modem_driver/` (start-up), `test/test_modem_calls/`, `test/test_modem_sms/` and the contract suite in `test/test_hardware_contracts/`, all against the scripted modem `testing/fakes/FakeAtTransport.h`.

**What is verified, and what is not.** Everything in this document is verified **on the host, against a script that was written from the manual and from what the previous driver met on the device**: the order of the AT commands, the state machine, the edge cases, the log lines. **Nothing has been run against the real modem**, so no answer format, no timing and no behaviour of the network is confirmed by these tests. What the VoLTE rule rests on, and what is still open, is in [volte.md](volte.md), with the checklist the owner runs on the device.

This document covers start-up, the SIM, the data bearer for the IMS, registration, the periodic questions, restarts, calls, messages, sleeping, switching off and the diagnostic questions, and at the end why each quirk is there.

The driver implements the `Modem` capability and passes its contract suite ([hardware-capabilities.md](hardware-capabilities.md)). It is a state machine: `poll()` is called from the main loop and never blocks, one **job** runs at a time (`Init`, `SimCheck`, `DataBearer`, `Register`, `ScanOperators`, `SelectOperator`, `Ims`, `Signal`, `Clock`, and for calls `Dial`, `Answer`, `Hangup`, `CallPoll`, `CallDiag`, `Volume`, and for messages `SendSms`, `ReadSms`, `DrainInbox`, and `Query`, `Sleep`, `Wake`, `Shutdown`), and a job that gets no answer in time ends itself, not the device. It includes nothing above `hal/`; its log lines are handed out through `pollLog()` and the host writes them through its log, which applies the level.

## Start-up, in order

1. **Power.** `begin()` switches the supply on and waits half a second. A pulse on the power key switches a *running* modem off, and after a reset of the processor the modem still runs, so the driver asks first: `AT` twice (five seconds to answer each, one second between); only then the power key, and six seconds of waiting. A modem that has answered once is never given the key: if it stops answering in the middle of the initialisation it is running. After twelve vain questions the state is `Error`.
2. **Init**: `AT`, `ATE0` (first, or every command comes back as an echo), `AT+CMEE=2` (errors as words), `AT+CMGF=1`, `AT+CSCS="UCS2"`, `AT+CSMP=17,167,0,8`, `AT+CNMI=2,1,0,0,0`, `AT+CTZU=1`, `AT+CLIP=1`, `AT+CSDVC=3`, `AT+COUTGAIN=4` (the A76XX series has no `AT+CLVL`), `AT+COPS=3,0`, and last `AT+CNMP=<mode>` (2 automatic by default; 13 GSM only, 38 LTE only). A refused command is no reason to give up.
3. **SimCheck**: `AT+CPIN?`. `SIM failure` in the first seconds only means "not known yet" and is asked again once a second, twelve times; `SIM not inserted` is a missing card. `SIM PIN` is answered with the stored PIN **once per start of the modem**; a rejection is never repeated, not even after a restart, because the card counts and locks after three wrong tries. A PIN that is not 4 to 8 digits is discarded. A missing, locked or PIN-less card ends in `Error` with the reason in `simStatus()`. The PIN is never logged.
4. **DataBearer**: see below.
5. **Register**: `AT+CREG?` every two seconds. Status 1, 5, 6 and 7 count as registered (6 and 7 are "SMS only"); then `AT+COPS?`, `AT+CCLK?` and `AT+CSQ`. After eight fruitless rounds the network is chosen by hand: `AT+COPS=?` and `AT+COPS=1,2,"<first code>"`; the registration window (90 s) is extended by the time the search may take, because the search once outlasted it and the modem went to `Error`. Without a network after the window: `Error`.
6. **Ims**: `AT+CIREG?` and `AT+CGDCONT?` (for the type of the IMS bearer, context 8, in the log); then the state is `Ready`.

`Ready` is reported only after the IMS question is answered or has failed, so the radio cycle of step 4 is over before anything else is sent.

## The data bearer and the IMS

Outgoing calls hung the modem two seconds after `ATD` whenever the IMS was not registered ([hardware-lessons.md](hardware-lessons.md)). The IMS bearer is built from PDP context 1, so the driver sets that up itself:

```
AT+CGDCONT?                          read; look for context 1
  context 1 is IPV4V6 with the configured APN (case does not matter)  ->  done, nothing is written
  otherwise
AT+CGDCONT=1,"IPV4V6","<apn>"        works with the radio on; with the radio off the modem answers "no network service"
AT+CFUN=0
AT+CFUN=1                            the network builds the IMS bearer from context 1 after this
```

- **Idempotent**: written only when context 1 differs (another APN, another type, missing), so the modem's memory is not worn on every start. A SIM swap leaves the old context behind; comparing the APN covers it.
- **Context 8 is never touched.** Setting it by hand did not help; its type follows context 1.
- **The APN comes from the configuration** (`[modem] apn`, [config-loader.md](config-loader.md)) and arrives with `Config::apn`; the driver reads no file. **Empty APN: the modem's own contexts are left alone** (nothing is read for it, nothing written, no radio cycle) and the log says so once per start. An APN with a character that does not belong in one (more than 100 characters, anything but letters, digits, dots, hyphens and underscores) is not sent either, and the log says that. The two other options of the proposal, a built-in table by MCC/MNC and an empty-APN `IPV4V6` context, are **not built**: that is the owner's decision.
- **The radio always comes back**: when `AT+CFUN=0` does not answer, `AT+CFUN=1` is sent anyway; a failed `AT+CFUN=1` is tried three times.
- **A refused write** (for example `no network service`) is logged with the modem's line and start-up goes on: a modem whose bearer could not be set is still a modem for messages.

`voiceStatus()` follows the answers: `Unknown` until registered and until `AT+CIREG?` has been answered, `SmsOnly` for registration 6 and 7 whatever the IMS says, otherwise `Ready` when the IMS is registered (`+CIREG: 1,1,15`) and `ImsMissing` when it is not (`+CIREG: 1,0,0`). The IMS is asked again every 30 seconds with the signal, the network and the operator, and after every registration. Each registration writes one line: `Cellular: IMS registered (PDN8 IPV4V6)` or `Cellular: IMS not registered, calls cannot be made (PDN8 IP)`; a change later writes one line, not one per poll.

## Network, operator, time, signal

`operatorName()` is the short name of `AT+COPS?` (a network that answers with its code gets the name for the usual countries) and empty without a network; `signalBars()` is 0 … 5 from `AT+CSQ` and 0 without a network; `networkTime()` is `AT+CCLK?` once an hour and after registration, and not known for a modem that has no network time yet (its clock starts in 1970 or 2004). **A blank name is no name.** Seen on the device 2026-10-08 (firmware 4.0.1, a Telekom SIM in Germany): `+COPS: 0,0," ",7`, a blank where the operator belongs, so the status line said only "Network". A name of blanks is now no name; the driver logs `Cellular: the network has no name, asking for its code` and from then on asks `AT+COPS=3,2;+COPS?` (the format switched to the numeric code, and the question in one line), at once after registration and at every periodic question; the table of `at::operatorNameForCode()` turns `26201` into "Telekom" and `20801` into "Orange", a code it does not know stays a code. The switch is kept across restarts of the modem: it is how this SIM and modem answer. The network is logged when it changes (`Cellular: network Telekom`). Host-tested; verified on the device the same day (`Cellular: network Telekom` within seconds after the blank).

A network that is lost after the start takes the operator, the bars and the voice with it, the state goes back to `Registering` and the 90-second window is **not** applied again: a device in the woods is not in error.

## A restart of the modem

`*ATREADY` after the modem had answered means it restarted by itself (a bad SIM contact, a supply that dips under the current of a transmission). The driver counts it (`restartCount()`), writes `Modem restarted by itself (n since start)` (the word "restart" keeps the line on every log level), forgets what it knew, waits three seconds and starts again from `Init`; the bearer is read and not rewritten, and a PIN that was accepted before is sent once more (one try per start of the modem, never after a rejection). The `*ATREADY` of the boot after power-on is no restart.

## Calls

Code and tests: `A7682EDriver::dial/answer/hangup/pollCallEvent/setCallVolume`, `test/test_modem_calls/`. The events and their legal order are those of the `Modem` capability ([hardware-capabilities.md](hardware-capabilities.md)).

**Dialling.** `dial()` is refused, and nothing is sent, unless the modem is `Ready`, the number is a dial string (`at::isValidDialString`: 1 … 23 characters of 0-9 * # and a leading +) and `voiceStatus()` is `Ready`. It takes effect at once (the state is `Dialing` and the sound path switches to the modem), and the job then **asks the IMS once more** (`AT+CIREG?`) before it sends `ATD<number>;`: the state of up to 30 seconds ago may be gone, and ATD without the IMS hangs the modem. A fresh "not registered" (or no answer) is a `DialFailed` with the code `NotReady` and no ATD. The limit for the answer to ATD is 15 seconds, longer than the seven seconds of the real hang, so that the restart that follows it is recognised as one.

**Connected is believed only with a call behind it.** `VOICE CALL: BEGIN` counts when ATD has gone out (which happens only with a fresh "IMS registered") or an answer to a ringing call is on its way; with no call it is a **phantom** and is ignored with a log line. While the call is being set up `AT+CLCC` is asked every second: state 2 (dialling), 3 (ringing at the other end: one log line, no event, because the capability has no event for it), 0 (active: connected, also without a BEGIN). A call that was listed and then is gone from the list, with no NO CARRIER, is over.

**How it ends.** `BUSY` → `Busy`, `NO ANSWER` → `NoAnswer`; `NO CARRIER` or `VOICE CALL: END` with a call: ringing → `Missed`, connected → `Ended` with its length (from the modem, else measured), **never connected → `DialFailed`**, because the network refused it (with the SIM of 2026-09-30, registration 7, every call ended so, and as "ended" it looked like a conversation). A failed dial is reported only after `AT+CEER` has been asked, so that the network's reason is in the event; its `reason` is the modem's line, then `lines:` (what the modem said since the call began) and the IMS facts (`CIREG <mode>,<info>,<ext>`, `PDN8 <type of context 8>`), for the log. A line of the modem with no call behind it (BUSY, NO CARRIER, END in `Ready`) is nothing.

**Incoming.** `RING` in `Ready` → `RingingIn` and the sound path switches at the first ring; `+CLIP` gives the number (known only after the first ring) and the call is **reported once**, with it (`Incoming`). A number that does not come (withheld) is waited for 1.5 seconds, and the call is then reported without one. A call that ends before it was reported is reported first, so that every call has its `Incoming` before its end. A repeated RING is the same call; a RING in any other state changes nothing. `answer()` is **not** immediate: the state changes with the modem's OK to `ATA`, and a refusal leaves the call ringing and says so in the log.

**Hanging up** ends the call for the application at once (`Ended` for a dial or a connected call, `Missed` for a ringing one) and then tells the modem (`AT+CHUP`) when its job allows; a dial cancelled before ATD went out sends nothing at all. What the modem says afterwards (NO CARRIER, a late BEGIN) finds no call and is ignored. A new `dial()` is refused while the cancelled dial's first step is still out.

**A restart in a call** (`*ATREADY`): a call being set up failed (`DialFailed`, code `ModemRestarted`), a connected one ended (`Ended`), a ringing one is missed, each once; the sound path goes back, and the start-up begins again.

**Volume.** `setCallVolume(0 … 7)` is clamped and sent as `AT+COUTGAIN=<n>` (the A76XX series has no `AT+CLVL`) at once, also in the middle of a call; the same value is not sent again; after a restart, which puts the modem back to 4, the child's choice is sent again.

**The sound path.** `AtTransport::setAudioToModem()` is switched in one place, when the state enters or leaves `Dialing`, `RingingIn` and `InCall`: on for the first dial tone and ring, off when the call is over, whichever way, or the amplifier would stay on and draw current.

## Messages

Code and tests: `A7682EDriver::sendSms/pollSms/pollSendFailure`, `test/test_modem_sms/`.

**Receiving.** `+CMTI: "SM",<n>` queues the slot; the job `AT+CMGR=<n>` reads it, and **only after it was parsed** `AT+CMGD=<n>` deletes it from the SIM. A message that does not parse stays on the SIM and is read with the next read of the whole card. After every registration (and after a restart) `AT+CMGL="ALL"` reads everything that lies on the SIM, **all of it before anything is deleted** (Meck deleted after the first message and overwrote the buffer): at most two messages per answer, what did not fit is read again at once, and only what was delivered is deleted. The previous driver listed `"REC UNREAD"`; `"ALL"` also finds a message that `AT+CMGR` already marked read and that then failed to parse. A sender whose number cannot be read is delivered with an empty number (the application drops it). The charset is UCS2, so a text that reads "BUSY" is hex and no busy signal. At most six messages wait for the application; further ones stay on the SIM.

**Sending.** `sendSms()` accepts at most four messages; it refuses (false, nothing sent) when the modem is not registered, off or going off, the number is not a dial string (`"0170 123"` is refused at once, where the previous driver accepted it and dropped it later), the text is empty, longer than `kMaxSmsBytes`, not valid UTF-8, or contains Ctrl-Z, Escape or NUL, which the modem would take for the end of the text. A text of plain ASCII goes out in the GSM alphabet (`AT+CSCS="GSM"`, `AT+CSMP=17,167,0,0`, 160 places per SMS, the characters `^ { } \ [ ] ~ |` take two), anything with a character outside it as UCS2 (`AT+CSMP=17,167,0,8`, 70 places, an emoji takes two); afterwards the charset goes back to UCS2, or incoming texts would arrive as hex. The recipient is sent with `AT+CMGS="<number>"` (in hex for UCS2; a number that does not fit is never cut), the modem answers `> `, the text follows and ends with Ctrl-Z, the result is `+CMGS: <n>` and OK.

**Fewer messages (4.1.5).** Every part costs a message, and one umlaut turns a text into UCS2 with 70 places instead of 160. When a text would take **more** messages as UCS2 than in plain ASCII, and every non-ASCII character has a stand-in (the German umlauts and sharp s, the accented letters of French and Spanish, the ligatures ae and oe, c-cedilla, n-tilde, typographic quotes and dashes, the ellipsis, the euro sign, the no-break space), the text goes out in ASCII: the umlaut becomes "oe", "ae" or "ue" and an accent is dropped. A text that fits one message as it is, and a text with a character that has no stand-in (an emoji, another script), keeps every character. On 2026-10-09 an SOS of about 130 characters with one o-umlaut arrived as three messages; it is one now. The history on the device keeps the original text, only the message on the air is folded.

**Long texts.** A text that does not fit one SMS is cut into parts (after a space where there is one, never inside a character) and the parts go out one after the other as separate messages. **There is no concatenation header** (that needs PDU mode, which is not built: the previous driver did not have it either), so the phone shows several messages. A failure in a later part is one failure for the whole text and names the part (`part 2 of 2: +CMS ERROR: 331`).

**Failures are reported exactly once, with the modem's reason** (`pollSendFailure`): the modem's own line (`+CMS ERROR: 304`), `no prompt from the modem` (after the limit the prompt is left with Escape), `no answer from the modem` (after 60 seconds for the result: the message may or may not have gone out), `modem restarted while sending` (it is **not** sent again), `modem switched off` (for what was queued). A failed message leaves the queue, and the next one is not held up. What was only queued survives a restart and goes out after the start has run again, **behind the radio cycle of the data bearer**, which would otherwise drop a message in flight.

## Sleeping, switching off, questions

**Sleep.** `sleep()` starts `AT+CSCLK=1` and DTR high when nothing else is to be done (not in a call, not with work waiting); the state stays `Ready` and the periodic questions stop. `wake()`, a message to send, a call, an announced message or a question wakes the modem first (DTR low, `AT`). A modem that is awake is not put to sleep with work waiting.

**Switching off.** `shutdown()` ends a call as the capability says (connected: `Ended`; ringing: `Missed`; being set up: `Ended`, no duration), reports what was queued and refuses new work at once, sends `AT+CPOF` and then cuts the supply; the state is `Off` when that job is done, also when the modem does not answer. The modem can be started again with `begin()`.

**Questions.** `query()` accepts only `AT+<name>?` and `AT+<name>=?` with a name of capital letters, digits and `+ ^ $`: no semicolon, no line break, no value, so that nothing can be changed this way. The answer lines are joined with `|` (and `ERROR` when the modem refused); lines that the driver reads for its own state (`+CREG:` …) are part of the answer as well. One question at a time.

## The configuration

`A7682EDriver::Config` is what the host hands over; the driver reads no file. `apn` comes from `[modem] apn` ([config-loader.md](config-loader.md)), `simPin` from `secrets.toml`, `networkMode` from the settings (2 automatic by default). The timings are the ones the device taught: `powerOnDelayMs` 6000 (three seconds were not enough), `commandTimeoutMs` 5000, `radioTimeoutMs` 30000 (`AT+CFUN` takes seconds), `dialTimeoutMs` 15000 (longer than the seven seconds of the real hang), `smsSendTimeoutMs` 60000, `registrationTimeoutMs` 90000, `operatorScanTimeoutMs` 120000, `signalIntervalMs` 30000, `clockIntervalMs` one hour. The transport (`AtTransport`) is the one seam to the board: bytes, time, the supply, the power key, DTR and the sound path; its implementation for the ESP32 and the pin map come with the other device drivers and are **not part of this work package**.

## Why each quirk is there

Nothing below was invented for the tests; each line is something the device or the manual taught, and the tests pin it.

| Quirk | Why |
|---|---|
| Ask `AT` before pressing the power key, and never press after the modem answered | a pulse on a running modem switches it off, and after a reset of the processor the modem still runs |
| `ATE0` first | otherwise every command comes back as an echo and confuses the line reading |
| `AT+COUTGAIN`, no `AT+CLVL` | the A76XX series has no `AT+CLVL` (`AT+CLVL=?` answers `ERROR` on the device) |
| `AT+CMEE=2` | `1` gives bare numbers; the reason of a failure should be words |
| Charset UCS2, back to UCS2 after every GSM sending | umlauts and emoji unharmed, and a hex text line can never be mistaken for a URC |
| `SIM failure` is asked again | the modem answers it in the first seconds and then `+CPIN: READY` |
| PIN once per start, never after a rejection | the card counts wrong tries and locks after three |
| Registration 6 and 7 count as registered | the device's job is messages; a SIM at 7 sent its SMS on 2026-09-30 |
| Manual choice of the network after eight rounds, and a longer window for it | a foreign SIM found no network in Germany automatically; the search once outlasted the window and the modem went to `Error` |
| The operator and the network are asked every 30 seconds | abroad the device kept showing the home operator for lack of a question (2026-10-03) |
| A code in place of an operator name becomes a name | some networks answer `AT+COPS?` with `26201` although the text was asked |
| A blank operator name makes the driver ask for the code | the A7682E answered `" "` with a Telekom SIM (2026-10-08) |
| `*ATREADY` after an answer is a restart | a bad SIM contact restarts the modem every 30 to 60 seconds |
| The IMS is asked right before `ATD` | the state of 30 seconds ago may be gone, and `ATD` without the IMS hangs the modem |
| A failed dial waits for `AT+CEER` | calls ended after two seconds with only "did not connect" and nobody saw why |
| `NO CARRIER` before a connection is a failed dial, not an end | a SIM for SMS only ended every call so, and it looked like a conversation |
| `AT+CHUP` after the application has hung up | the modem must be told, but the application must not wait for it |
| A message is deleted only after it was read and parsed | Meck deleted after the first message and overwrote the buffer |
| A refused sending is reported with the modem's reason | an emergency message vanished silently with `+CMS ERROR: 304` (2026-10-01) |
| A message queued before a restart goes out after the radio cycle | `AT+CFUN=0/1` drops a message in flight |

## Log lines

English, technical, no PIN, no number, no APN. They carry the words the log level looks for ([log-level.md](log-level.md)): `Cellular:` (an event), `SIM:` (an event), `Call:` (an event), `Messages:` (an event), `failed`, `cannot` (errors), `restarted` (always kept).

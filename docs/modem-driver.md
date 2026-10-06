# The modem driver

Code: `src/drivers/A7682EDriver.{h,cpp}` on the AT layer ([at-layer.md](at-layer.md)); tests in `test/test_modem_driver/` against the scripted modem `testing/fakes/FakeAtTransport.h`. **Nothing here has been run against the real modem.** This document covers start-up, the SIM, the data bearer for the IMS, registration, the periodic questions, restarts and calls; messages follow with the next part of the work package.

The driver is a state machine: `poll()` is called from the main loop and never blocks, one **job** runs at a time (`Init`, `SimCheck`, `DataBearer`, `Register`, `ScanOperators`, `SelectOperator`, `Ims`, `Signal`, `Clock`, and for calls `Dial`, `Answer`, `Hangup`, `CallPoll`, `CallDiag`, `Volume`), and a job that gets no answer in time ends itself, not the device. It includes nothing above `hal/`; its log lines are handed out through `pollLog()` and the host writes them through its log, which applies the level.

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

`operatorName()` is the short name of `AT+COPS?` (a network that answers with its code gets the name for the usual countries) and empty without a network; `signalBars()` is 0 … 5 from `AT+CSQ` and 0 without a network; `networkTime()` is `AT+CCLK?` once an hour and after registration, and not known for a modem that has no network time yet (its clock starts in 1970 or 2004). A network that is lost after the start takes the operator, the bars and the voice with it, the state goes back to `Registering` and the 90-second window is **not** applied again: a device in the woods is not in error.

## A restart of the modem

`*ATREADY` after the modem had answered means it restarted by itself (a bad SIM contact, a supply that dips under the current of a transmission). The driver counts it (`restartCount()`), writes `Modem restarted by itself (n since start)` (the word "restart" keeps the line on every log level), forgets what it knew, waits three seconds and starts again from `Init`; the bearer is read and not rewritten, and a PIN that was accepted before is sent once more (one try per start of the modem, never after a rejection). The `*ATREADY` of the boot after power-on is no restart.

## Calls

Code and tests: `A7682EDriver::dial/answer/hangup/pollCallEvent/setCallVolume`, `test/test_modem_calls/`. The events and their legal order are those of the `Modem` capability ([hardware-capabilities.md](hardware-capabilities.md)).

**Dialling.** `dial()` is refused, and nothing is sent, unless the modem is `Ready`, the number is a dial string (`at::isValidDialString`: 1 … 23 characters of 0-9 * # and a leading +) and `voiceStatus()` is `Ready`. It takes effect at once (the state is `Dialing` and the sound path switches to the modem), and the job then **asks the IMS once more** (`AT+CIREG?`) before it sends `ATD<number>;`: the state of up to 30 seconds ago may be gone, and ATD without the IMS hangs the modem. A fresh "not registered" (or no answer) is a `DialFailed` with the code `NotReady` and no ATD. The limit for the answer to ATD is 15 seconds, longer than the seven seconds of the real hang, so that the restart that follows it is recognised as one.

**Connected is believed only with a call behind it.** `VOICE CALL: BEGIN` counts when ATD has gone out (which happens only with a fresh "IMS registered") or an answer to a ringing call is on its way; with no call it is a **phantom** and is ignored with a log line. While the call is being set up `AT+CLCC` is asked every second: state 2 (dialling), 3 (ringing at the other end: one log line, no event, because the capability has no event for it), 0 (active: connected, also without a BEGIN). A call that was listed and then is gone from the list, with no NO CARRIER, is over.

**How it ends.** `BUSY` → `Busy`, `NO ANSWER` → `NoAnswer`; `NO CARRIER` or `VOICE CALL: END` with a call: ringing → `Missed`, connected → `Ended` with its length (from the modem, else measured), **never connected → `DialFailed`**, because the network refused it (with the SIM of 2026-09-30, registration 7, every call ended so, and as "ended" it looked like a conversation). A failed dial is reported only after `AT+CEER` has been asked, so that the network's reason is in the event; its `reason` is the modem's line, then `lines:` (what the modem said since the call began) and the IMS facts (`CIREG <mode>,<info>,<ext>`, `PDN8 <type of context 8>`), for the log. A line of the modem with no call behind it (BUSY, NO CARRIER, END in `Ready`) is nothing.

**Incoming.** `RING` in `Ready` → `RingingIn`, `Incoming` with no number, and the sound path switches at the first ring; `+CLIP` brings the same event again with the number (the number is known only after the first ring). A repeated RING is the same call; a RING in any other state changes nothing. `answer()` is **not** immediate: the state changes with the modem's OK to `ATA`, and a refusal leaves the call ringing and says so in the log.

**Hanging up** ends the call for the application at once (`Ended` for a dial or a connected call, `Missed` for a ringing one) and then tells the modem (`AT+CHUP`) when its job allows; a dial cancelled before ATD went out sends nothing at all. What the modem says afterwards (NO CARRIER, a late BEGIN) finds no call and is ignored. A new `dial()` is refused while the cancelled dial's first step is still out.

**A restart in a call** (`*ATREADY`): a call being set up failed (`DialFailed`, code `ModemRestarted`), a connected one ended (`Ended`), a ringing one is missed, each once; the sound path goes back, and the start-up begins again.

**Volume.** `setCallVolume(0 … 7)` is clamped and sent as `AT+COUTGAIN=<n>` (the A76XX series has no `AT+CLVL`) at once, also in the middle of a call; the same value is not sent again; after a restart, which puts the modem back to 4, the child's choice is sent again.

**The sound path.** `AtTransport::setAudioToModem()` is switched in one place, when the state enters or leaves `Dialing`, `RingingIn` and `InCall`: on for the first dial tone and ring, off when the call is over, whichever way, or the amplifier would stay on and draw current.

## Log lines

English, technical, no PIN, no number, no APN. They carry the words the log level looks for ([log-level.md](log-level.md)): `Cellular:` (an event), `SIM:` (an event), `Call:` (an event), `failed`, `cannot` (errors), `restarted` (always kept).

# Calls

Code: `src/app/Calls.{h,cpp}`; tests in `test/test_calls/`. Pure C++17 on the modem, power, clock and vibration capabilities ([hardware-capabilities.md](hardware-capabilities.md)); uses the profile ([profile.md](profile.md)), school time ([school-time.md](school-time.md)), the notices and system entries ([notices.md](notices.md)).

## Dialling

`call(number)` applies four rules, in this order, and each refusal is a notice that says which:

1. **school time**: "school time until 15:00" (`refuseInSchoolTime()` is the same question for anything else the child starts that school time forbids);
2. the **number** must be allowed: in the contact book (any spelling of the number) or the target of a call action (`CallNotAllowed`);
3. **cellular** must be switched on (`CellularIsOff`);
4. the device must be **registered** (`NoNetwork`).

A dial the modem then refuses leaves no call behind. The modem refuses when its voice status is `SmsOnly` (the network allows messages only) or `ImsMissing` (registered, but the IMS is not: dialling would hang the modem), and nothing is sent to it then; `call()` turns the status into a notice, `CallSmsOnly` or `CallNotReady`, with the number, so that the child sees why. A refusal is no failed call and writes no history entry. `answer()` and `hangup()` go to the modem; hanging up clears the other party.

## Only the contact book may call

**A call from a number that is not allowed, or with no number, is hung up at once** (`Profile::mayReceiveFrom`: the contact book in any spelling, or the target of a call action; the same rule as for incoming texts, see [messaging.md](messaging.md)). No ring, no vibration, no notice, no history entry, no missed call: the child never learns that it happened, and until it is over `state()` says `Ready`, `active()` is false and `peer()` is empty, so no screen follows it. One log line says that it did: `Call rejected: caller not in the contact book (+491709999999)` or `Call rejected: number withheld`; both are always-kept lines ([log-level.md](log-level.md)). It holds at every time of day, in school time too, and there is no switch.

- The modem reports a call twice: first without a number, then with it. The decision is taken with the number. **Nothing rings before the caller is known to be allowed**, so a contact whose number arrives late rings from then on.
- A call that never shows a number is rejected after `kQuietRejectSeconds` (3) at every time of day. A family member who withholds the number is rejected too.
- The modem's later reports about a rejected call (missed, ended) are ignored; a call the child dialled is unaffected.

## Incoming calls in school time

**Every incoming call from a contact in school time is rejected — parents included — without ringing.** What the child sees is a notice naming who tried (`CallerInSchoolTime`), and the call is in the history as a missed call (`missed-call`, with the number, so the parents can call back). A caller who is not a contact never gets this far: see above.

- A call is rejected once however often the modem reports it.
- Vibration for a call in school time is `Profile::quietCallVibration`, off by default; the everyday `callVibration` has no say in school time.

## Ringing

Outside school time a ringing call vibrates the ring pattern, once at once and then every `kRingIntervalSeconds` (2), until it stops. The device is mostly in a pocket, and a sound in a lesson is the last thing anybody wants. `callVibration` switches it off.

## A call that did not happen

The child cannot say why a call failed, and the parents find the reason in the history later, so each failure is a notice, a *failed* vibration, a history entry (`call-failed` with the reason) and a log line (with the modem's own words when it gave some):

| Reason | When |
|---|---|
| `busy`, `no-answer` | the modem says so |
| `telephony-not-ready` | the modem names it (`DialFailReason::NotReady`: the IMS was not registered) |
| `modem-restarted` | the modem restarted while the call was being set up (`DialFailReason::ModemRestarted`) |
| `no-network` | the modem names it, or only says the dial failed, and the device is not registered |
| `sms-only` | the modem names it, or ... it is registered, but the network allows SMS only |
| `not-connected` | the modem refused (`DialFailReason::Refused`), or ... none of that: plainly did not connect |

Without a code from the modem the reason is derived in this order: no network, SMS only, telephony not ready, not connected. A restart that cuts off a *connected* call is an ordinary end: nothing is reported as failed.

A call that ended without ever being connected counts as a failure of the same kind. A call the child ended while it was still dialling, and a call that was connected and then ended, are not failures.

## Volume

`changeVolume(delta)` changes the volume 0 … 7 **only during a call**: the sound runs through the modem only then, and a control without effect only confuses. A step beyond the ends is clamped; at the end it returns false. A modem that cannot set the volume reports -1, and nothing can be adjusted. `setDefaultVolume(level)` sets the volume with which the next call begins.

`active()` is true while a call is dialling, ringing or connected; the interface does not lock the device meanwhile.

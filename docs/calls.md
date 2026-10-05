# Calls

Code: `src/app/Calls.{h,cpp}`; tests in `test/test_calls/`. Pure C++17 on the modem, power, clock and vibration capabilities ([hardware-capabilities.md](hardware-capabilities.md)); uses the profile ([profile.md](profile.md)), school time ([school-time.md](school-time.md)), the notices and system entries ([notices.md](notices.md)).

## Dialling

`call(number)` applies four rules, in this order, and each refusal is a notice that says which:

1. **school time**: "school time until 15:00" (`refuseInSchoolTime()` is the same question for anything else the child starts that school time forbids);
2. the **number** must be allowed: in the contact book (any spelling of the number) or the target of a call action (`CallNotAllowed`);
3. **cellular** must be switched on (`CellularIsOff`);
4. the device must be **registered** (`NoNetwork`).

A dial the modem then refuses (for example on a network that allows SMS only) leaves no call behind. `answer()` and `hangup()` go to the modem; hanging up clears the other party.

## Incoming calls in school time

**Every incoming call in school time is rejected — parents included — without ringing.** What the child sees is a notice naming who tried, and the call is in the history as a missed call (`missed-call`, with the number, so the parents can call back).

- The modem reports a call twice: first without a number, then with it. The decision is taken with the number, so the notice can name the caller: a contact is named (`CallerInSchoolTime`), a caller who is not in the contact book gets a notice without a name (`CallInSchoolTime`). A call is rejected once however often it is reported.
- A call that never shows a number is neither rung nor rejected at first; after `kQuietRejectSeconds` (3) it is rejected anyway, or it would ring on unnoticed.
- Vibration for a call in school time is `Profile::quietCallVibration`, off by default; the everyday `callVibration` has no say in school time.

## Ringing

Outside school time a ringing call vibrates the ring pattern, once at once and then every `kRingIntervalSeconds` (2), until it stops. The device is mostly in a pocket, and a sound in a lesson is the last thing anybody wants. `callVibration` switches it off.

## A call that did not happen

The child cannot say why a call failed, and the parents find the reason in the history later, so each failure is a notice, a *failed* vibration, a history entry (`call-failed` with the reason) and a log line (with the modem's own words when it gave some):

| Reason | When |
|---|---|
| `busy`, `no-answer` | the modem says so |
| `no-network` | the modem only says the dial failed, and the device is not registered |
| `sms-only` | ... and it is registered, but the network allows SMS only |
| `not-connected` | ... and none of that: plainly did not connect |

A call that ended without ever being connected counts as a failure of the same kind. A call the child ended while it was still dialling, and a call that was connected and then ended, are not failures.

## Volume

`changeVolume(delta)` changes the volume 0 … 7 **only during a call**: the sound runs through the modem only then, and a control without effect only confuses. A step beyond the ends is clamped; at the end it returns false. A modem that cannot set the volume reports -1, and nothing can be adjusted. `setDefaultVolume(level)` sets the volume with which the next call begins.

`active()` is true while a call is dialling, ringing or connected; the interface does not lock the device meanwhile.

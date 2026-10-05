# Emergency

Code: `src/app/Emergency.{h,cpp}`; tests in `test/test_emergency/`. Joins the proven pieces — the SOS flow ([sos-flow.md](sos-flow.md)) and the SOS ping ([sos-ping.md](sos-ping.md)) — to the rest of the core: positioning ([positioning.md](positioning.md)), messaging ([messaging.md](messaging.md)), school time ([school-time.md](school-time.md)), the profile ([profile.md](profile.md)) and the catalogs ([i18n.md](i18n.md)).

## One search at a time

The SOS ping and the reply to a parent's location query are the same ping with a different text, and **only one runs**:

- a second query while a search runs is answered "busy" (`startLocationReply` returns false, and the query is logged as busy);
- an **emergency replaces a running query reply**: the child's call for help must not be lost because a parent asked a moment earlier. The parent who asked gets no answer; the reply was never promised to arrive;
- pressing SOS while an emergency or pickup ping runs changes nothing, and a second press while the choice is open keeps its countdown.

## The receiver

The receiver is held through Positioning, with a reason for each use: `SosChoice` while the flow is open (the search starts at the press, so that it is already looking when somebody has decided), then `SosPing` for an emergency or pickup or `RemoteQuery` for a reply. The flow lets go of its own reason only after the ping started, so the receiver is never off in between, and it is off again in the very pass in which the text was sent.

**The host must call `leave()` when the SOS screen closes**: in school time the flow has no timeout of its own, and the receiver would stay on. Leaving after the choice does not stop a running ping.

## School time

SOS works in school time. Pressing opens a warning instead of the choice; `SosFlow::kOverrideTaps` (5) taps, each within ten seconds of the one before, send the emergency, and the last tap **lifts the school-time lock until the end of the current window** (logged: "Quiet hours lifted by emergency until 15:00"). A child who sent an emergency from a locked phone could neither be called back nor call. The next window locks again. Fewer taps do nothing, leaving the warning has no effect, and there is no automatic emergency in school time.

## The outcome

Told once, when the stage changes: **sent** is three short pulses, **failed** is short-long. A query reply never vibrates and has no notice; it is only logged ("Remote query answer sent" / "FAILED").

A text the modem accepts and the network then refuses is reported by Messaging, which vibrates it and notes it in the history. The ping is told through `noteSendFailed`, which changes its stage without vibrating a second time. For a query reply the same call returns true, which tells Messaging to keep that failure out of the child's sight (log only).

If the text cannot even be queued (the queue is full), the ping fails at once: one *failed* vibration, a log line, the receiver released.

## Wording

The component holds no sentence. The SMS is worded from the catalogs (`sms.sos.*`) in the **profile's SMS language** — the person who reads it is the parent, not the child, so the language of the screen does not matter. A language the catalogs do not have gives English. The child's name, the map template and the wording are read at every start, so a change in the profile applies to the next press.

Without a name the anonymous wording is used ("SOS, I need help").

## Interface

| Call | Meaning |
|---|---|
| `begin(target = "")` | the press of the SOS button; false when there is nobody to reach (no target and no call target) |
| `choose(kind)`, `tap()`, `leave()` | the choice, one tap of the school-time button, the screen closes |
| `startLocationReply(phone)` | a parent asked (called by Messaging); false when a search runs |
| `noteSendFailed(phone)` | the network refused the text |
| `poll()` | the main loop: the countdown, the ping, the outcome, the receiver |
| `phase()`, `autoLeft()`, `tapsLeft()`, `ping()`, `emergencyRunning()` | what the interface draws; a query reply does not count as running (the child does not see it) |

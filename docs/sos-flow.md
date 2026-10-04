# SOS flow

Code: `src/app/SosFlow.{h,cpp}`, tests in `test/test_sos_flow/`. A clock-driven state machine in front of [the SOS ping](sos-ping.md); the surroundings (receiver, ping, school-time lock, clock) are reached through `SosFlow::Host`.

Pressing SOS **starts the location search at once** (`startSearch`), not after a choice: by the time somebody has decided, the receiver is already looking. What follows depends on whether school time is running (`begin(target, quietHours)`).

## Outside school time: the choice

`Choosing` offers *emergency*, *pick me up* and *cancel*.

- `choose(Emergency)` or `choose(Pickup)` starts the ping (`startPing`) and then lets go of the flow's own claim on the receiver (`stopSearch`), so the receiver is never unpowered in between. `choose(Query)` is refused: the location reply belongs to parents' queries.
- If nobody chooses, **the emergency goes out by itself after 40 seconds** (`kAutoSeconds`): `tick()` sends it once `autoLeft()` is 0, even when the call to `tick()` comes late. `autoLeft()` counts down from 40 and is 0 outside the choice.
- `leave()` (the back key, cancel) stops the search and **sends nothing**, now or later.

## In school time: five taps

`QuietConfirm` opens a warning with a button that needs 5 taps (`kOverrideTaps`).

- Four taps do nothing: no ping, no lift.
- **The fifth tap sends the emergency and lifts the school-time lock until the end of that school time** (`liftQuietLock`, called before the ping starts). Otherwise the child could neither call nor be called after sending the alarm. The host knows when the school time ends (`QuietHours::active` reports the end minute) and does nothing if the lock is already off.
- There is no countdown and no pickup here: in school time the press must be deliberate. Taps are counted without a time window, as before.
- `leave()` closes the warning; the taps made are forgotten.

## Rules for every path

- The search is held exactly while the flow is open; `startSearch` and `stopSearch` are balanced (a randomised test checks this over many sequences).
- A press while a ping is searching, or while the flow is open, changes nothing and returns true: an open choice keeps its countdown, an open warning its taps. An empty target returns false and does nothing.
- If the ping cannot start, the flow still closes and releases the search, and `choose()` / `tap()` return false.
- A clock that jumps back keeps the countdown at its full length instead of firing.
- Nothing is sent from a flow that is not open.

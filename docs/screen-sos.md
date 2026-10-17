# The SOS page

Code: `src/ui/screens/SosScreen.{h,cpp}`; tests in `test/test_screen_sos/`. The page draws what the core's SOS flow and ping say (`AppCore::State`: `sosPhase`, `sosSecondsLeft`, `sosTapsLeft`, `ping*`) and calls `beginSos`, `chooseSos`, `tapSos`, `leaveSos`. It owns no timer and no state beyond which field is in focus.

## What the page shows

| Phase | Shown | Action |
|---|---|---|
| choosing | "Emergency in N s" and three fields: 1 emergency, 2 come and get me, 3 cancel | the arrows move the focus, Pick chooses |
| school time | why the device is locked, until when, how many taps are needed, one big button with the taps still to go | Tap |
| searching | what was chosen, three bars (one filled), "Locating", seconds so far | |
| position found | two bars, "Position found", seconds so far and the accuracy to date | |
| sent | three bars, "Sent", the accuracy of the position (or that there was none), to whom | |
| not sent | two bars, "Not sent", why | Try again |
| nothing running | "No emergency is running" | |

**The search is already running when the page opens.** Pressing the SOS tile calls `beginSos`; the receiver warms up while the child decides. The countdown is the core's: when it runs out the emergency goes out by itself.

**Choosing:** the keys 1, 2 and 3 choose at once, a touch on a field chooses it, Enter chooses the field in focus. Back is "cancel". The words of a field wrap inside its box and are never cut.

**School time:** the core asks for five taps, each within ten seconds of the last (`SosFlow::kOverrideTaps`, `kTapWindowMs`); the page shows the counts it is given and does not know the rules. A tap is a touch on the button or Enter.

**The bars:** one per step, the third only when the text is really out. The core counts a failed ping as three of three; the page draws two, because three filled bars would look like a success for a text that never left.

**A reply to a parent's query is not shown.** The ping object is shared with the remote query; when the running one is a query the page says nothing runs.

**Leaving** the page by any way (Back, home, lock) calls `leaveSos()` from `onLeave()`, so the receiver is released. An emergency that has been chosen is not cancelled by this: the text goes out all the same.

**A call is no way out** (decision [D-39](decisions.md)): a call that rings covers the page with the call page, and the choice stays open. The child can answer; the countdown goes on and the emergency goes out by itself during the call when nobody chose. When the call is over the stack comes back to this page, to the countdown or to what the ping did.

**Lock:** the page keeps the device from locking while an emergency is open or running (`preventsAutoLock`).

## Wording

Every word is from the catalogs (`sos.*`, `home.sos`, `key.*`); the sentences with numbers take them as placeholders. The text that goes to the parents (`sms.sos.*`) is the core's and not drawn here.

## Tests

`test_screen_sos`: the choice and its countdown; every way to choose (keys, touch, Enter, Back); school time and the five taps; each phase of the ping; the bars; a failed text and trying again; a remote query not shown; every way out releasing the search, through a real stack; a call during the choice (`test_runtime`, the scenario `sos-during-a-call`); the lock; the page in French and German; layout of every phase in four languages; pictures in English and German.

# Positioning

Code: `src/app/Positioning.{h,cpp}`; tests in `test/test_positioning/`. It uses the clock, power and positioning capabilities ([hardware-capabilities.md](../hardware/hardware-capabilities.md)).

**The satellite receiver does not run all the time.** It costs battery, and a device that locates in secret is what this one must not be. There are exactly six reasons to switch it on, and each is its own switch:

| Reason | While it holds |
|---|---|
| `StartupFix` | once after start: the time, when the network gives none, and a first position |
| `SosChoice` | the SOS button was pressed and the choice is open: the search starts at once |
| `SosPing` | an SOS ping is searching |
| `RemoteQuery` | a parent's location query is being answered |
| `PositionPage` | the position page is open |
| `TestPage` | the self-test page is open |

The receiver is on while **any** reason holds. That is the point of keeping them apart: two sources that once wanted the receiver shared one switch, and whichever finished first took it away from the other (the position page closing while a parent's query was still waiting for its fix). `want(reason, on)` holds or releases one reason; holding twice is holding once. The switch goes to the power capability, and the answer to "is it on" is asked of it, not remembered: when a switch fails it is logged and tried again at the next `poll()`, in both directions, so a failed switch-off cannot leave the receiver running.

## The start-up fix

`startStartupFix()` holds the `StartupFix` reason and it ends by itself:

- with the **first usable fix**: valid, with an HDOP of at most 5.0 (5.0 is usable, 5.1 is not);
- with **any valid fix after one minute**: a poor position is better than none;
- **without a result after three minutes**: in a building no fix comes, and the battery counts.

It is logged when it starts and when it ends. It runs out its time even when the receiver cannot be switched on (a failed power switch), so it is not retried for ever. Another reason that holds the receiver is not affected when the start-up fix ends.

## The last known fix

While the receiver runs, every valid fix is remembered with the time it was seen, whoever asked for it. `lastKnownFix(fix, &age)` returns it, with its age in seconds, even when the receiver has long been off; it is false while there has been none. A fix from before the last switch-on is never taken for a current one: the capability does not return it, and a remembered fix does not end a new start-up fix.

`readFix()` passes the receiver's answer through, for the SOS ping.

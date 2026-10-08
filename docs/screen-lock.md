# The lock screen

Code: `src/ui/screens/LockScreen.{h,cpp}`; tests in `test/test_screen_lock/`. A key lock without a PIN. The stack shows it while the device is locked (`ScreenStack::lock`), and unlocking goes to the home screen (`ScreenStack::unlock`).

## Unlocking

**Two presses of the keyboard's Enter key in a row.** A tap on the screen and the three sensor keys under it never unlock — in a pocket the cloth touches the screen, and two taps came together too easily on the device. Any key between the two presses, a typed character included, starts again; a touch neither unlocks nor breaks a first press. Every input is used up here: nothing gets through to the device behind. The hardware keys do nothing while locked (the stack's rule); the screen would ignore them too.

The screen has **no soft keys**: a button that did nothing when tapped would be a lie. The hint says what to do instead and names the key by its symbol, as it is printed on the keyboard: "Press ↵ twice to unlock", and after the first press "Press ↵ once more". Entering the screen again forgets a first press of an earlier lock.

## What it says

No clock. The lock is the permanent state, and a clock would be the one thing that changes by itself: a redraw of the panel every minute, visible flicker, and current for nothing. (The status bar shows the date, which changes once a day, and the battery in bars.) What one wants to know without unlocking instead:

- **what the device is connected to**, in the large font: the operator, or "Radio", "Network" (a network without a name) or "No network" — `wording::networkLabel`;
- **how**: "Connected", "SMS only" (a network that does not carry calls), "No calls yet" (the network is there, the IMS is not) or "Not connected";
- **what is new**: `✉ 2 new messages`, the plural of the language, when anything is unread (system entries included).

A test renders the lock, lets two minutes go by and asserts that the picture is the same.

## The front light

Entering the screen switches the front light off: a light that burns on in a pocket empties the battery over a night.

## Tests

`test/test_screen_lock` — one press does not unlock and two do; every other key between the two (arrows, back, the three keys under the screen, letters, the euro sign) starts again; taps never unlock and do not disturb a first press; no soft keys; entering again forgets a first press; the light; the network and the way, in three network states and in German and French; the hint and its change; the unread count in the right number (French counts differently); no change as time goes by; the panel budget — an hour on the lock costs no refresh after the first picture, and two minutes on the home screen until it locks cost one refresh, the lock itself ([ui-refresh.md](ui-refresh.md)); the real stack — unlocking lands on the home screen, the three sensor keys and taps do nothing while locked; the longest operator label fits; layout in all four languages; pictures in English and German (`lock_*`, `lock_armed_*`).

# Power coordination

Code: `src/app/PowerCoordination.{h,cpp}`; tests in `test/test_power_coordination/`. Applies the power policy ([power-policy.md](power-policy.md)) to the hardware. The policy only decides; this component acts on the decision and owns the one piece of state it needs: whether the modem has been put to sleep.

What the policy needs to know about the rest of the device comes in as a plain `Needs` (a call, work that could be done, a usable radio, Wi-Fi), so that this component depends on none of the others.

## What `apply` does

- The **modem** is put to sleep after a quiet minute, once, and woken when the policy says so (a key press, queued work, a call). Each is logged. With **cellular off** it is neither put to sleep nor woken. A modem without a network never sleeps.
- The **radio chip** gets power only while the radio software uses it, and the change is logged; a switch that fails is not claimed and is tried again at the next call.
- The decision for the **processor** goes back to the main program, which sleeps it (that is hardware). On the cable, with a lit display, or with Wi-Fi running it does not sleep; with the display off and the modem asleep it may, for at most the time until the next redraw.

`noteInput()` is a key press; `noteModemAwake()` says the modem did something (a message, a call, a switch of its power).

## Also here

The same kind of thing, what the device does with its own power:

- the **front light** (`toggleFrontlight`, `setFrontlight`): it draws current, so the interface switches it off when the device locks. Toggling is a notice; a switch that fails says nothing.
- the **request to switch the device off**: made, not done here — the display must be cleared first. If it does not work (a cable feeds the board) `notePowerOffFailed()` drops the request and says so with a notice and a log line.
- battery level, charge state and minutes remaining, read through.

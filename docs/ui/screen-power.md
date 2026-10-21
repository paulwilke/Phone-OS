# The power page

Code: `src/ui/screens/PowerScreen.{h,cpp}` with the widget `src/ui/OptionRows.{h,cpp}`; tests in `test/test_screen_power/`. The page behind the power-symbol tile (a circle with a bar, no word on it), the last tile of the first page of the home screen ([screen-home.md](screen-home.md)). The device is often switched off in the pocket and on when it is used, which makes the battery last; the page makes that easy to find and hard to do by accident (decisions D-11 and D-12).

## The two views

| View | Shows | Select does |
|---|---|---|
| first view | one button, **Switch off**; on a cable the box says "unplug USB first" instead | opens the question; on a cable nothing: USB keeps the board alive, so the battery switch would change nothing |
| the question | the title **Switch off?** and two buttons: **Cancel** (the focus starts here) and **Yes, switch off** | Cancel goes back to the first view; Yes asks the core to switch off |

**Back** from the question goes back to the first view, and from the first view leaves the page. Leaving the page forgets the question, so it never opens on "Yes". A tap on a button works like Select on it.

**What happens after Yes.** `AppCore::requestPowerOff` sets the request; the firmware's loop answers it as for the entry under Settings and the status page: the display shows the "Switched off" picture, the modem says goodbye to the network (up to ten seconds), the rails are cut and the battery is disconnected ([device-console.md](../hardware/device-console.md), [device-power.md](../hardware/device-power.md)). With a cable plugged in after the question the firmware refuses and the notice says "Switch off only without cable".

**The same switch-off is also under Settings** ("press twice") and on the parents' status page; this page is the one that is easy to find.

## Not on this page

**Power save.** What it should do is decision D-13, which is open ([open-points.md](../open-points.md), point 25); a button that promised to save power would be wrong while the setting underneath (`modem_sleep`) decides whether anything sleeps.

**The switch on the side of the case.** The firmware does not read it (decision D-11).

## Tests

`test_screen_power`: the first view and no question yet; the question with the focus on Cancel; Cancel changing nothing; Yes and only Yes switching off; Back from the question and then from the page; leaving the page forgetting the question; the cable (the box says to unplug and nothing is asked); a tap like Select; the soft keys; every view drawn inside the display in all four languages, with and without a cable; pictures in English and German (`power_*`, `power_ask_*`).

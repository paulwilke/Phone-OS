# The Wi-Fi page

Code: `src/ui/screens/WifiScreen.{h,cpp}`; tests in `test/test_screen_wifi/`. Three buttons — the home network, the device's own access point, off — and under them what somebody needs to join, or why it did not work. Enter or a tap switches to the button in focus; the core refuses what it cannot do (a battery too low for Wi-Fi) and says so in the notice line.

**The mark.** The button of what runs now has a second outline inside the first (white on the focus). "Off" is marked while Wi-Fi is off, including after a failure.

| State | Under the buttons |
|---|---|
| off | what Wi-Fi is for: the parents' page |
| off, with a reason | the reason only: "Switched off: not used", "No home network saved", "Access point does not start" |
| connecting | "Connecting to Home ..." |
| connected / access point | the network; for the access point its **password**; the address of the page; "Off in N min" (a started minute counts as a whole one) |
| failed | the reason ("Home network not reachable") and that the access point still works |

The reasons are the core's codes worded by `wording::wifiReason`, so a language change also changes a reason that is already on show. The password of the access point is shown here and nowhere else (the core hands it to the state only while the access point runs); the password of a home network is never shown.

## Not ported

The previous page also said whether the web server runs or starts. The web code is a later package and the core has no such state, so the line is left out until there is something to tell. The sentence under "off" names the parents' page, which that package will serve; decision to confirm whether the sentence should wait for it.

## Tests

`test_screen_wifi`: the buttons and the mark, the access point with its password and address and the minutes, a home network connecting and connected (without a password), one that cannot be reached, none stored, a Wi-Fi that switched itself off, a failing access point, switching off, a low battery in the notice, soft keys, touch, the ends, the focus on open, layout in four languages for each state, pictures in English and German.

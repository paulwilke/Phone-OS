# The radio page

Code: `src/ui/screens/RadioScreen.{h,cpp}`; tests in `test/test_screen_radio/`. The long-range radio that carries messages where the mobile network does not reach. The page says "radio" everywhere and never the name of the software underneath.

A head line says in one sentence how it is: not allowed, off, in save mode, nobody in range, ready (with the strength in dBm when it is known). Under it the controls the state offers, as buttons, and when the radio runs the contacts it knows:

| State | Controls |
|---|---|
| not allowed by the profile | none; the page says so |
| off | "Switch radio on": asks the application to start it and attach it (`requestRadioStart`) |
| running | "Save mode: radio only" (cellular and Wi-Fi off; the core refuses without a radio), "Radio off (restart)" (`requestRestart`) |
| in save mode | "Check cellular" (a probe of a few minutes; the button counts it down as "Network probe 2:59"), "End save mode" |

A contact is a row with its name (shortened with "..." like every name) and under it its way (direct, or via detours) and when it was last heard ("3 min ago"; "heard" when the radio gave no time or the device has none). **Enter on a contact writes to it** over the radio — it is left in the session (`Session::recipient`, `viaRadio`) and the compose page opens — **only where the profile lets the child choose any recipient** (`freeText`); without that the contacts are shown, the soft key has no action word and Enter does nothing. The previous page showed the action word only with that switch but let Enter write in any case; here the word and the key agree.

Buttons and sub-lines wrap inside their rows, so a row is as high as its words need; the list scrolls with the focus (one row at a time, stopping at both ends) and shows a scroll bar when it is longer than the page.

**Layout.** Every state is drawn inside the screen in English, German, French and the pseudo-language (40 % longer than any real one); only the one action word beside the arrows may reach into the soft-key bar in the pseudo-language.

## Tests

`test_screen_radio`: not allowed; off and the start request; running with nobody; ready with strength, way and age; no strength and detours without a time; German words; save mode and its buttons; the probe counting down (two digits of seconds); the restart; writing to a contact with and without free text; a long name; twelve contacts scrolling to the last; the first row; touch; back and the arrows; layout in four languages in six states; pictures in English and German.

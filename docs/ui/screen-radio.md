# The radio page

Code: `src/ui/screens/RadioScreen.{h,cpp}`; tests in `test/test_screen_radio/`. The long-range radio that carries messages where the mobile network does not reach. The page says "radio" everywhere and never the name of the software underneath.

A head line says in one sentence how it is: not allowed, off, nobody in range, ready (with the strength in dBm when it is known). Under it the controls the state offers, as buttons, and when the radio runs the contacts it knows:

| State | Controls |
|---|---|
| not allowed by the profile | none; the page says so |
| off | "Switch radio on": asks the application to start it and attach it (`requestRadioStart`) |
| running | "Radio off (restart)" (`requestRestart`) |

There is no save mode on this page: it is the battery saving of the radio's emergency and nobody switches it ([emergency-radio.md](../core/emergency-radio.md)).

**The foot of the page** says where the radio setting holds: "The radio setting holds for Europe only (EU, 869 MHz)." (`radio.europe`, in English, German and French; D-61). It is drawn under the list where the profile allows the radio, and the list gives up the room for it. The words say Europe and not the name of the software. Under it, a second note, "Ideally a LoRa antenna is screwed on for the radio." (`radio.antenna`), in the same three languages.

**The control "Check mobile network now"** (`radio.search`) is the second row during the radio's emergency only (D-67): it asks for one search of the mobile network (`AppCore::searchMobileNetwork()`: the modem is switched on for at most 90 seconds and stays on if it finds a network, [connectivity.md](../core/connectivity.md)). Outside the emergency it is not offered.

**During the radio's emergency** a contact can be written to whatever the free-text switch says (the compose page then offers the short texts), and a radio entry of the message list can be answered ([emergency-radio.md](../core/emergency-radio.md)).

A contact is a row with its name (shortened with "..." like every name) and under it its way (direct, or via detours) and when it was last heard ("3 min ago"; "heard" when the radio gave no time or the device has none). **Enter on a contact writes to it** over the radio — it is left in the session (`Session::recipient`, `viaRadio`) and the compose page opens — **only where the profile lets the child choose any recipient** (`freeText`); without that the contacts are shown, the soft key has no action word and Enter does nothing. The previous page showed the action word only with that switch but let Enter write in any case; here the word and the key agree.

Buttons and sub-lines wrap inside their rows, so a row is as high as its words need; the list scrolls with the focus (one row at a time, stopping at both ends) and shows a scroll bar when it is longer than the page.

**Layout.** Every state is drawn inside the screen in English, German, French and the pseudo-language (40 % longer than any real one); only the one action word beside the arrows may reach into the soft-key bar in the pseudo-language.

## Tests

`test_screen_radio`: not allowed; off and the start request; running with nobody; ready with strength, way and age; no strength and detours without a time; German words; no save-mode button; the note on Europe and the one on the antenna in three languages; the control that checks the mobile network, offered during the emergency only; writing to a contact during the emergency without free text; the restart; writing to a contact with and without free text; a long name; twelve contacts scrolling to the last; the first row; touch; back and the arrows; layout in four languages in four states; pictures in English and German.

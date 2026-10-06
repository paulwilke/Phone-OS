# The status page

Code: `src/ui/screens/StatusScreen.{h,cpp}`; tests in `test/test_screen_status/`. What the parents come here for and no more: the battery and how long it lasts, the child's name and the number of contacts, how visible a remote query is, the firmware and the hardware, and the way to switch the device off. The firmware and hardware names are the application's to say (build constants), so it hands them to the page as a `DeviceInfo`; an empty hardware gives no line.

**Remote queries.** Enter steps through silent, in the history, with a notice. The runtime writes the change to `settings.toml` on the card, like the other settings the child changes ([runtime.md](runtime.md)).

**Switching off** is written on the page and takes two presses of x (capital too); any other key takes the first back. The page only asks: the application clears the display and switches off, and with the cable plugged in it cannot, because USB keeps the board alive; it then tells the core (`notePowerOffFailed`), whose notice says so. The page says "unplug USB first" beforehand while the cable is in.

The page is a text and scrolls with the arrows when it is longer than the page (a long hardware name, or a language); the position is kept while the words stay the same and the page opens at the top.

## Not ported

The previous page also showed the warnings from reading the configuration (persona and contact files). Reading the configuration is a later package; the core gets a finished profile, so there are no warnings to show yet. "Persona" is replaced by the child's name, since the profile has no persona. Decision to confirm.

## Tests

`test_screen_status`: the lines, unknown and empty values, the cable with the battery, the number of contacts, the step through the queries, the two presses of x and what takes the first back, the cable refusing, scrolling and its soft keys, the position being kept, layout in four languages for each state, pictures in English and German.

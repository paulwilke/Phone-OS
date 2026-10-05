# The position page

Code: `src/ui/screens/GnssScreen.{h,cpp}`; tests in `test/test_screen_gnss/`. What the satellite receiver knows. **While the page is open it holds the receiver** (`setPositionPageOpen(true)` in `onEnter`, released in `onLeave`, so by every way out of the page, proven with a real stack); the position is searched for as long as somebody looks, and the receiver goes off again when they stop.

**A current fix** (the receiver runs and the position is at most five seconds old) gives four values, each a small label above a large number: latitude and longitude with five decimals (about a metre; the sign is kept, and a value that rounds to zero has none), the accuracy in metres (from the HDOP, as everywhere else the device tells accuracy), the satellites in use.

**Without a current fix** the page says what is going on: searching (and that in the open it takes up to two minutes, and indoors there is often no reception) or that positioning is off. **The last known position** is shown under it with its age and coordinates, so that an old position is never taken for the present one; the core remembers it whether the receiver runs or not. A fix that stops coming turns into the last known one after five seconds, while the search goes on. The text scrolls with the arrows when a language makes it longer than the page, and the soft keys offer them only then.

Numbers are written by `ui::format` (`degrees`, `tenths`, `age`; see [ui-framework.md](ui-framework.md)): digits only, so a coordinate reads the same in every language.

**Layout.** Every state is drawn inside the screen in English, German, French and the pseudo-language (40 % longer than any real one); only the one action word beside the arrows may reach into the soft-key bar in the pseudo-language.

## Not ported

The previous page also showed the time of the fix ("as of 14:31"). The age of the last known position says what the time of day was meant to say, and the page has no time zone of its own. Decision to confirm.

## Tests

`test_screen_gnss`: the receiver held and given back (directly and through a real stack by back, home and lock); searching with the hint; positioning off with nothing known; the four values; the words in German; a southern and western position with its signs; the arrows doing nothing while the values fit; the last known position after leaving, with its age, never as the present one; a fix that stops coming; the age in German; scrolling in the pseudo-language; layout in four languages in four states; pictures in English and German. `test_ui_formats`: the formats themselves.

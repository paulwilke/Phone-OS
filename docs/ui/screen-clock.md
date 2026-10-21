# The clock screen

Code: `src/ui/screens/ClockScreen.{h,cpp}`; tests in `test/test_screen_clock/`. The page where the time is, on purpose: it left the status bar because every minute there cost a redraw of the panel, and here the redraw is wanted, because whoever opens the page wants the time.

## What it shows

From the core's wall clock (`AppCore::State::wall`, in the clock's zone): the time `HH:MM` in the large font, the weekday written out, the date as the language writes it, and a note that says where the time comes from.

| | en | de | fr |
|---|---|---|---|
| weekday | Tuesday | Dienstag | mardi |
| date | September 15, 2026 | 15. September 2026 | 15 septembre 2026 |
| note | Time from the network | Zeit aus dem Netz | Heure du réseau |

The year and the day change with the wall clock, not with UTC: half past eleven in the evening of New Year's Eve in UTC is already January 1 two hours east of it. The weekday and the date are `wording::weekdayName` and `wording::dateLine`.

**Without a time** (the network or the receiver has not told it yet) the page shows `--:--` and says "No time from the network yet": no weekday, no date, nothing invented.

Every part wraps to the width of the page instead of running over the edge, so a language that needs more room (the pseudo-language included) is never cut off.

## Keys

The soft key is Back, and Back goes back. Nothing else is used.

## Tests

`test/test_screen_clock` — time, weekday, date and note in English, German and French (found on the picture with `testing/Shows.h`); the zone and the day change; the year change; without a time; Back and the soft key; every weekday and month fits the width in the three languages; drawn inside the display in all four languages, with and without a time; pictures in English and German (`clock_*`, `clock_no_time_*`). The words of every weekday and month, and that none repeats, are tested with the wording (`test/test_ui_wording`).

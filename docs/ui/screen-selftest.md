# The self-test page

Code: `src/ui/screens/SelfTestScreen.{h,cpp}`; tests in `test/test_screen_selftest/`. Eight checks for a person with the device in hand, as a grid of buttons (the arrows wrap, Enter or a tap runs the one in focus). Above the grid: the state now (light, receiver, battery) and the result of the last check, two lines high even when empty so that the grid never moves.

| Check | What it does |
|---|---|
| Vibration | plays the five patterns in turn (click, ring, message, sent, failed) and names the one played; "Felt it?" |
| Light | switches the front light; "Brighter?"; the state line follows |
| Display | stripes of changing width above, a chequerboard below, over the whole content area, to see whether the panel shows evenly or leaves ghosts; a tap, Back or Enter ends it |
| Keys | names every key as it is pressed: the arrows, Enter, Backspace, the three keys under the screen (the page asks the stack for them while the test runs), letters by their character, others by their code. **No soft key**: Backspace is one of the keys to test, so the test ends by itself after six seconds without input; the page says how long is left |
| Touch | the place of every tap as numbers and a cross; "Done" (Back) ends it, and so do six seconds without a tap |
| GPS | holds the receiver with the reason of the test page and shows how it does, live: searching, or the fix with satellites, metres and HDOP; again to stop |
| Motion, Network | open those pages |

A result is kept as a code and worded when drawn, so it stays right after a change of language. **What the page holds it gives back whenever it is left** (`onLeave`): the receiver, by every way out, proven with a real stack; leaving without a GPS check releases nothing another reason holds. The idle ends of the key and touch tests are measured in `onTick` against the core's uptime.

**Layout.** Every state is drawn inside the screen in English, German, French and the pseudo-language (40 % longer than any real one); only the one action word beside the arrows may reach into the soft-key bar in the pseudo-language. The header and the result keep the room of the longest state they can have in the language, so the grid never moves when a check reports.

## Not ported

The previous page showed the self-test summary of the start-up above the state line. Nothing in the application produces such a summary, and the page would have had to print a sentence made elsewhere in no language in particular, so the line is not there.

## Tests

`test_screen_selftest`: the menu, the state lines, the arrows and a tap; back; the five patterns in turn and again; a result worded again in German; the light; the display check and how it ends; every key named; typed keys by character and code; the key test without a soft key and its idle end and restart; the touch test with its cross, its end by Done and by idle, and its wait starting afresh; the GPS check, live, and giving the receiver back by every way; the motion and network checks; layout in four languages in nine states; pictures in English and German.

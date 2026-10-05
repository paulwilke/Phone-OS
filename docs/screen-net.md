# The network page

Code: `src/ui/screens/NetScreen.{h,cpp}`; tests in `test/test_screen_net/`. What the status bar says in a word, said in full, for adults. One line each:

- the SIM, **only when there is a problem** (no card, PIN missing, PIN wrong, locked);
- the network (the operator's name, or the words for none or the radio, as the status bar has them);
- the status: connected, SMS only, not connected;
- the signal as a number ("4/5" reads even when the display is small);
- cellular power, the modem's sleep, whether the time is known, how many messages wait.

Under the lines a paragraph explains a SIM problem, or why a network that allows messages only is no fault of the device. A wrong PIN says not to restart before it is right, because three wrong tries lock the SIM. The explanations name no file or setting that this repository does not have: the parents are told to enter the PIN, not where.

The page is a text and scrolls with the arrows when a language makes it longer than the page; the soft keys offer the arrows only then. It opens at the top.

## Tests

`test_screen_net`: a healthy network, no network, cellular off and waiting messages and a sleeping modem, an unknown time, every SIM problem with its explanation, the wrong-PIN warning in German and French, SMS only and its explanation, no explanation where nothing is wrong, scrolling and its soft keys, opening at the top, back, layout in four languages for each state, pictures in English and German.

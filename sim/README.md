# The simulator

The device's application — the same `AppCore`, screens, configuration loader and web service the firmware will run — on the fakes of `testing/fakes/`, with a console in which you play the other side. Details of what exists are in the code headers; the user-facing documentation grows with the simulator.

## Build and run

```sh
pio run -e sim                                            # console only, needs a compiler and nothing else
.pio/build/sim/program --script sim/scripts/smoke.txt     # a scenario, headless; exit status 1 when a command failed
.pio/build/sim/program --data sim/data-parent             # interactive: type `help`
```

With a window (SDL2: `brew install sdl2`): `pio run -e sim-sdl && .pio/build/sim-sdl/program`. The arrows, Enter and Backspace are the device's keys, F1 / F2 / F3 the three keys under the screen, typed characters are text, a click is a touch. A partial refresh keeps the old picture on show for 300 ms and a full refresh flashes. Without SDL (`sim`) there is no window and `--headless` is the only mode; CI uses it.

## The card

`--data <dir>` is the card: a directory with `config/` (and `lang/` for a user catalog). It is read into memory; nothing in it is ever written. `sim/data` is the child's device, `sim/data-parent` a parent's, `sim/data-school` one on which school time never ends. All values are invented.

## The parents' web page

`wifi ap` (or `wifi station`, then `wifi connected`) switches Wi-Fi on, and the real web service answers on `http://127.0.0.1:8080/` (`--port` changes it; the simulator says the address when it opens). Only this computer can reach it. The password of the demo cards is `test-password-1`, an invented value in their `secrets.toml`; the page is subject to every rule of the device (lock-out after five wrong passwords, the `X-Phoneplus` header, the path check). The language of the page is chosen on the page. A language saved in the settings is the device's after a restart, which the page asks for and the simulator carries out.

The card is a directory: by default a fresh copy of `--data` in `out/sim-card/` (emptied at every start, and only if the simulator made it), so what the page saves never reaches the repository. `--in-place` uses the data directory itself, for the owner who wants it. Bound to the loopback address only; port 80 is replaced by the port above because a user may not open it.

## Scenarios

`sim/scripts/*.txt` are stories, each run headless by CI and by `test_simulator`: a normal day, school time, a remote query during an SOS, the network dropping, save mode and the cell probe, a language switch during an emergency, a missed call in three languages, a user catalog, a bad user catalog. A line `# data: <dir>` at the top names the card the story needs (`sim/data-school`, `test/data/usercatalog`, `test/data/badcatalog`). They assert with `see <catalog key> [name=value]` (the screen says that text, in the language of the moment, so the story works in every language), `say <text>`, `check <what> <value>` and `fails <command>`; a failing assertion is an error and the exit status is 1. `python3 scripts/counter_check_scenarios.py` breaks the behaviour each story is about and expects the story to fail; run it when a story is written or changed.

The SOS page, the settings and the Wi-Fi page do not exist yet, so a story presses the core's entry points with `sos`, `cellular`, `savemode` and `probe`, as those pages will.

## The click-through demo

`python3 scripts/make_demo.py` (after `pio run -e sim`) writes `out/demo/` (not committed; CI uploads it as the artifact `demo`): the simulator walks the interface by its keys from a world with a few messages in it (`program --crawl`, about 150 states: every screen the keys lead to, found in English), takes the pictures the keys do not lead to (`sim/demo/scenes*.txt`), each in English, German and French, and builds `index.html`, a static page with the keys under the picture and a language selector that swaps every picture and word. The page has no words of its own: they are the `demo.*` keys of the catalogs. The script fails when a picture, a translation or a screen is missing. Open `out/demo/index.html` in a browser.

The walk keeps at most 30 states of one screen (they differ in a selection or a scroll position) and says how many more it left out; it does not press keys that would send a message or end a call.

## Pictures

`golden <name>` compares the panel with `sim/golden/<name>.txt` (`#` black, `.` white, the format of `test/golden/`); `UPDATE_GOLDEN=1` writes the file, to be reviewed like code. `screenshot <file.png>` writes the panel as a picture.

The console's own words (help, errors) are English and are not translated.

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

## Pictures

`golden <name>` compares the panel with `sim/golden/<name>.txt` (`#` black, `.` white, the format of `test/golden/`); `UPDATE_GOLDEN=1` writes the file, to be reviewed like code. `screenshot <file.png>` writes the panel as a picture.

The console's own words (help, errors) are English and are not translated.

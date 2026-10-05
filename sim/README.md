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

## Pictures

`golden <name>` compares the panel with `sim/golden/<name>.txt` (`#` black, `.` white, the format of `test/golden/`); `UPDATE_GOLDEN=1` writes the file, to be reviewed like code. `screenshot <file.png>` writes the panel as a picture.

The console's own words (help, errors) are English and are not translated.

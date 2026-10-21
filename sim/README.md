# The simulator

The device's application on the fakes, with a console, scripted scenarios, a simulated e-ink panel and the real web page. The documentation is in [docs/simulator.md](../docs/simulator.md); the generated demo is in [docs/process/demo.md](../docs/process/demo.md); trying a translation is in [docs/translating.md](../docs/translating.md).

```sh
pio run -e sim                                            # console only, needs a compiler and nothing else
.pio/build/sim/program --script sim/scripts/smoke.txt     # a scenario, headless; exit status 1 when a command failed
.pio/build/sim/program --data sim/data-parent             # interactive: type `help`
pio run -e sim-sdl                                        # with a window (SDL2: brew install sdl2)
python3 scripts/make_demo.py                              # the pictures and the click-through demo into out/demo/
```

| Directory | What it holds |
|---|---|
| `data/`, `data-parent/`, `data-school/`, `data-school-off/` | the demo cards: the child's device, a parent's, one on which school time never ends, and the same with its windows switched off (invented values only) |
| `scripts/` | the scenarios, one story each, run by CI headless |
| `golden/` | the pictures the smoke scenario compares with (`#` and `.`, like `test/golden/`) |
| `demo/` | the world, the scenes and the page of the generated demo |

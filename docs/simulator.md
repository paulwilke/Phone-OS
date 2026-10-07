# The simulator

Code: `sim/` (`Simulator.h`, `main.cpp`, `SimPanel.h`, `Png.h`, `DirectoryStore.h`, `SocketTcp.h`, `Crawler.h`, `SdlWindow.h`), the composition it runs in `src/boot/` ([runtime.md](runtime.md)); tests in `test/test_simulator/`, `test/test_sim_panel/`, `test/test_sim_platform/` and `test/test_sim_web/`.

The simulator runs the **same** application core, screen stack, configuration loader and web service as the firmware will, on the fakes of [test-doubles.md](test-doubles.md) made interactive: it is a platform below the capability interfaces, not a second implementation of the application. `sim/` may include `src/` and `testing/`; `src/` never includes `sim/` (`scripts/check_layers.py` checks it, and that no file includes MeshCore).

It serves four purposes: looking at the device in three languages before there is any hardware, generating pictures and the demo instead of making them by hand ([demo.md](demo.md)), trying a translation on a simulated screen ([translating.md](translating.md)), and a second level of tests in which scripted scenarios run through the real wiring.

## Build and run

```sh
pio run -e sim                                            # the console, headless; a compiler is all it needs
.pio/build/sim/program --script sim/scripts/smoke.txt     # a scenario, headless; exit status 1 when a command failed
.pio/build/sim/program                                    # interactive: type `help`
.pio/build/sim/program --data sim/data-parent             # another card
```

Options: `--data <dir>` (the card; default `sim/data`), `--lang <dir>` (the shipped catalogs; `lang`, the page's are in its `web/` folder and are read when the page asks for them), `--host-lang <dir>` (the words that only host tools show; `tools/lang`, merged into the languages of the same code), `--web <file>` (the page; `web/index.html`), `--golden <dir>` (`sim/golden`), `--work <dir>` and `--in-place` (the card, below), `--port <n>` (the web page, 8080), `--script <file>`, `--interactive`, `--headless`, `--scale <n>`, `--lapse <n>`, and the options of the crawl ([demo.md](demo.md)). `--help` lists them.

A script without `--interactive` runs and exits. A line `# data: <dir>` among its first lines names the card the script needs, when `--data` is not given.

### The window

`pio run -e sim-sdl` builds the same program with a window, which needs SDL2: on macOS `brew install sdl2`, on Debian and Ubuntu `apt install libsdl2-dev`. Without SDL the `sim` environment still builds and runs; it has no window and `--headless` is the only mode (CI runs that way, on a plain Linux runner). The window shows what the panel shows: a partial refresh keeps the old picture on show for 300 ms and a full refresh flashes. The keys of the Mac keyboard are the device's keys: arrows, Enter, Backspace, and F1, F2 and F3 for the three keys under the screen; typed characters are text, and a click is a touch.

## The card

The card is a directory with `config/` (`settings.toml`, `contacts.toml`, `secrets.toml`) and, for a user catalog, `lang/` (and `lang/web/` for the page's words), behind `DirectoryStore`, a `ConfigStore` that is held to the same contract suite as the fake and the real card: paths are checked with `SafePath`, names are compared without regard to case, a write goes through a scratch file, and scratch files are never visible. By default the simulator copies the data directory to `out/sim-card/card` at every start, so **what the web page saves never reaches the repository**; it empties that directory only if it made it (it holds a marker file), and refuses a work directory that is, holds or lies in the data directory. `--in-place` uses the data directory itself, for the owner who wants the changes kept. A restart that the device asks for (the page's restart button) starts the device again on the same card.

`sim/data` is the child's device, `sim/data-parent` a parent's (free text, the parents' tools), `sim/data-school` one on which school time never ends, `sim/data-school-off` the same windows switched off (`enabled = false`). Every value is invented: the numbers `+491701111111`, `+491702222222` and `+493012345678`, the child Alex, the contacts Parent One and Parent Two, and a convenience password, `test-password-1`, which is an invented value in their `secrets.toml` and weakens no rule. A test loads each card and expects no diagnostic.

## Console commands

The console's own words (`help`, prompts, errors) are **English and are not translated**; the words of the device come from the catalogs. A command that fails prints `error: ...` and is an error in a script. Everything after `#` is a comment, except in what is said or typed (`sms`, `type`, `echo`, `radio`).

| Command | What it does |
|---|---|
| `sms <number> <text>` | a text message arrives |
| `call <number>` | a call comes in |
| `remote answer`, `busy`, `noanswer`, `hangup`, `fail <why>` | what the other end of a call does |
| `radio start` | the child asks for the radio; the simulator starts it, as a device does |
| `radio up`, `radio down`, `radio peer <name>`, `radio msg <name> <text>`, `radio confirm yes`, `radio confirm no` | the radio network: the chip, a contact, a message (the name is one word), the confirmation of the oldest message |
| `battery <0-100>`, `plug charging`, `plug full`, `unplug` | the battery and the charger |
| `signal <0-5>`, `network <name>`, `network none`, `simcard ready`, `missing`, `pin`, `pinwrong`, `puk` | the mobile network and the SIM |
| `gps fix [lat lon hdop]`, `gps none`, `gps bad` | the receiver: a fix (default 48.13745, 11.57550, 1.5), never a fix, a fix with poor accuracy |
| `motion tilt left\|right\|forward\|back\|flat`, `motion tilt <x> <y> <z>`, `shake`, `off`, `on` | the motion sensor: the named tilts put the side that is low (`forward` is the top edge) in the sensor's own axes, so the ball of the motion page rolls there; the numbers are the sensor's raw values in g |
| `wifi ap`, `wifi station`, `wifi off`, `wifi connected`, `wifi failed` | Wi-Fi as the device's Wi-Fi page switches it (the core's one controller, with save mode and battery in front): the access point, or the station and the home network's answer |
| `web [as <password>\|anonymous] get\|post\|put\|delete <path> [body]` | one request to the parents' page through the real socket, as a browser would send it (the card's password by default; the changing methods carry the `X-Phoneplus` header); the simulated time runs until it is answered |
| `date <YYYY-MM-DD> <HH:MM>` | sets the clock to the local time of the device's zone, summer time included |
| `advance <n>[ms\|s\|m\|h]` | lets time pass, one second at a time, the device running |
| `lapse <factor>` | the speed-up of the real-time loop (window and terminal) |
| `key <name>...` | keys in order: `up`, `down`, `select` (or `enter`), `back`, `home`, `inbox`, `compose` |
| `type <text>`, `tap <x> <y>` | typed characters (UTF-8); a touch at a pixel of the 240 × 320 panel |
| `language <code>` | the interface language: `en`, `de`, `fr`, the pseudo-language `qps-ploc`, or a user catalog |
| `sos begin [number]`, `sos choose emergency`, `sos choose pickup`, `sos tap`, `sos leave` | the entry points of the SOS button on the core; the SOS page itself is driven with keys (`sos-page` scenarios) |
| `cellular on\|off`, `savemode on\|off`, `probe` | what the network page and its options do |
| `state`, `log [n]` | a summary of the device; the last lines of its log |
| `screenshot <file.png>`, `shotdir <dir>`, `shot <name>`, `ascii [file]` | the panel as a PNG, as pictures in a directory, as `#` and `.` |
| `golden <name>` | compares the panel with `sim/golden/<name>.txt`; `UPDATE_GOLDEN=1` writes it, to be reviewed like code |
| `see <key> [name=value...]`, `see-not`, `say <text>`, `say-not <text>` | assertions on the screen, below |
| `check <what> <value>`, `fails <command>` | assertions on the state and on refusals, below |
| `run <script>`, `echo <text>`, `help`, `quit` | control |

Commands that change the world let the device catch up before they return; `screenshot`, `shot`, `ascii` and `golden` draw a frame first, so they show what the state says and not the picture of up to a second ago.

## Scripts and assertions

A scenario is a text file under `sim/scripts/`. CI and `test_simulator` run every file there headless. The assertions make a script a test:

- `see <catalog key> [name=value ...]` is true when the screen says the catalog's text for that key, with the arguments filled in, **in the language of the moment**: the scenario works in every language. A value may have spaces (`name=Parent One`). A text that the screen wraps over lines is found line by line. The match is exact over the whole word and a margin, on white or on black, in any of the four fonts (`testing/Shows.h`), so a word that is cut off, in the wrong font or covered is not found, and a part of a word is not the word.
- `say <text>` is the same for a literal text (a contact's name, an operator).
- `check` compares a value of the state: `screen`, `locked`, `unread`, `school`, `call` (`idle`, `dialing`, `ringing`, `incall`), `sos` (`idle`, `choosing`, `quiet-confirm`, `countdown`, `running`), `registered`, `cellular`, `savemode`, `battery`, `language`, `boots`, `sms-sent` (how many texts left), `last-sms-to`, `last-sms-has` (the text contains the words), `log-has`, `wifi` (`off`, `connecting`, `station`, `access-point`, `failed`: the state the device's screens read), `wifi-seconds-left`, `settings-has` (the text is in settings.toml *on the card*: a change counts when it was written back), `web-status` (0: nobody answered), `web-has` (the last answer contains the words), and `haptic-click`, `haptic-ring`, `haptic-message`, `haptic-sent`, `haptic-failed` (how often a pattern was played).
- `fails <command>` is true when the command is refused.

The scenarios: a normal day, school time, a remote query during an SOS, the network dropping and returning, save mode and the cell probe, a language switch during an emergency, a missed call in three languages, a stranger's text and a stranger's call that leave no trace, the same card with its windows switched off, where nothing is locked (`school-time-off`), the parents' page and the one Wi-Fi controller, a user catalog adding a fourth language, and a bad user catalog that is rejected without a crash. Seven scenarios go through the **real pages with key presses**, none through a core entry point: `sos-page` (the tile, the choice and its countdown, the text and its recipient, a text that waits for the network), `sos-page-school-time` (the five taps with the keys, the lock lifted), `language-page` (German, French and English chosen from the options page: the home screen, the status bar and an open history entry change, and `settings.toml` on the card says it), `settings-page` (message vibration switched off, no vibration for the next text, still off after the restart the parents' page asks for), `wifi-page` (the access point from the page, the parents' page answering, kept on by its own use and not by silence, off by itself and by the third row), `network-status-pages` (operator, signal, SIM, battery and charger as the simulator was told) and, from the tests, `test_runtime`'s walk. The pages a card without parents' tools does not have use `sim/data-parent`. The last two use cards in `test/data/`. **Each has a counter-check**: `python3 scripts/counter_check_scenarios.py` breaks the behaviour the story is about, builds the simulator and expects the story to fail; it names what it broke.

## The web page

`wifi ap` (or `wifi station` and then `wifi connected`) switches Wi-Fi on, and the real `WebService` answers on `http://127.0.0.1:8080/`. `SocketTcpServer` listens on the loopback address only, so the page is reachable from this computer and from nowhere else; port 80, which the web server asks for, is replaced by `--port` because a user may not open it. The page, the API, the password, the lock-out after five wrong passwords, the `X-Phoneplus` header and the path check are the device's: the simulator weakens nothing. Wi-Fi is the device's one controller: `wifi ap` switches it as the device's Wi-Fi page will, the page runs while it runs, every authenticated request restarts its idle time, and the scenario `wifi-admin-page` checks that the device's state and the page's agree throughout. A language saved in the settings is the device's after a restart. `test/test_sim_web` starts the simulator headless, logs in, reads the languages, saves a setting and checks the file of the card; it tries the lock-out, the missing header and the path escape through the real socket.

## What the simulator cannot prove

It proves that the application does what it should **given what the capabilities say**. It does not prove:

- **timing**: the clock is a counter that the simulator moves, and a second costs no time; the real loop, the modem's delays, the length of a refresh (the 300 ms and 1200 ms are round figures that make it visible), and anything that depends on how fast the processor is;
- **the radio**: there is no radio propagation, no range, no loss; the fake delivers and confirms when told;
- **the battery**: the level is a number somebody sets; there is no discharge curve, no current;
- **e-ink behaviour**: no ghosting, no temperature effects, and not the true look of the panel;
- **the modem and the receiver**: their answers are scripted, so a real operator's surprises, a real SIM and a real sky are not here (the contract suites say what every implementation promises, and a real driver has to pass them);
- **power and sleep**: the power policy's decisions are checked, not what a board does with them;
- **a user catalog bigger than the limit** of 32,768 bytes (each file, the device's and the page's) is rejected as on the device ([translating.md](translating.md)).

A scenario that passes here and a device that fails are a reason to look at the capability that was assumed.

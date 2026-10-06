# The runtime: one composition for the device and the simulator

Code: `src/boot/Runtime.{h,cpp}`; tests in `test/test_runtime/`. The wiring is written once, as library code, so that the firmware's main loop and the simulator's run the same application core, screen stack, configuration loader and web service. Each host only creates its capabilities, builds a `Runtime` and loops on `step()`.

## What the host hands over

`boot::Capabilities` holds the clock, power, modem, GNSS receiver, haptic, event log, configuration store, Wi-Fi, TCP server, web assets and display (references), the function that draws random numbers, the firmware version string, and, for hardware a device may lack, the radio and the motion sensor (pointers; the motion sensor's presence puts the motion tile on the home screen). The translator is passed separately and already holds the **shipped** catalogs: the device embeds them, the simulator reads them from `lang/`. The interface takes input as events (`Runtime::input`), so there is no input capability.

## Building a Runtime is booting

In this order, in the constructor:

1. the user's catalogs from `/lang/` are added to the translator (`ConfigLoader::loadUserCatalogs`), so a language the card brings can be the one the configuration asks for;
   (The page's catalogs, `/lang/web/`, are not read here: the runtime registers them as the translator's user source for the web set, read when the page first needs them and let go when Wi-Fi goes off; the destructor withdraws the source.)
2. `/config/` is read into the `Profile` (`ConfigLoader::load`);
3. the languages are checked (`ConfigLoader::checkLanguages`). The interface language is selected when a catalog has it; otherwise the translator stays on English and there is a `BadCatalogCode` diagnostic;
4. the core is built from the profile, told the firmware version, the radio and Wi-Fi, and started (`AppCore::start`); the core's Wi-Fi controller is the only one, and the web service follows it ([wifi-controller.md](wifi-controller.md));
5. every diagnostic is written to the event log as one English line, `config: <name> <arguments>`, and kept for `diagnostics()`;
6. every screen of `src/ui/ScreenIds.h` is registered (home, lock, clock, phone, number, call, messages, compose, recipient, SOS, options, Wi-Fi, network, status, language, GPS, radio, motion and the self-test), the stack is given the lock screen, the auto-lock time, the three keys under the screen, and begins on the home screen.

A problem on the card is a diagnostic, never a refusal to start: an empty card boots a device with the defaults and three `MissingFile` diagnostics.

## One pass: `step(nowMs)`

`nowMs` is the host's millisecond counter. A pass runs the core (`poll`), the web service, the call hand-over, the stack's auto-lock, and draws a frame when one is due. It returns the core's `PowerDecision`, which the host carries out; the host also does what only hardware can (starting the radio on `core().takeRadioStartRequest()`, switching off on `core().powerOffRequested()`, restarting on `restartRequested()`).

- **Frames.** A frame is drawn when input arrived and otherwise at most once a second (`kFrameIntervalMs`), always from the state and always the whole screen. Whether the panel works is the refresh policy's decision ([ui-refresh.md](ui-refresh.md)), not the runtime's: a second of nothing new costs a draw and no refresh.
- **The display is "on" while the device is not locked.** The panel keeps its picture without current, so "display on" means somebody may be looking. A locked device (in a pocket) may let the processor sleep when the rest of the power policy allows ([power-policy.md](power-policy.md)).
- **Calls.** A call that comes in takes the screen, also from the lock, and the call page goes away again when the call is over. A call the child started is on the screen because the phone page opened it, and stays until the child has read how it ended ([screen-call.md](screen-call.md)).
- **Auto-lock** after `kAutoLockSeconds` (120) without input; a call in progress prevents it.

## Input and language

`input(input, nowMs)` tells the power logic that somebody pressed something, hands the event to the stack and draws the next frame at once. The three keys under the screen are bound as on every screen: Home goes to the home screen, Inbox opens the history, Compose opens the choice of recipient.

`setLanguage(code)` is what a language page or the configuration calls: the translator switches and the next frame is in the new language, history entries included, because no screen keeps words. False for a code no catalog has, and nothing changes. The choice lasts until the next start; writing it back is the configuration's job.

## Every page is in the product

Every name in `src/ui/ScreenIds.h` is registered, and two tests keep it so (`test_runtime`): one asks the stack for each name (`ScreenStack::registered`), the other walks the real runtime with the keys of the device, from the home screen, with every feature on, and fails for a page it cannot reach (the lock and the call page are left out: the first needs two minutes of silence, the second a call). They were written because three packages of pages were once merged late and nothing registered them: the tiles opened nothing. Taking one `stack_.add` out of `Runtime.cpp` makes both fail.

The motion page gets the sensor through `SensorMotionSource` (`boot/`), which turns the hardware's `MotionSensor` into the interface's `MotionSource`; without a sensor it says so. The status page is given the firmware version and `Capabilities::hardware` (the board's name; empty: no line).

**What the child changes is written back.** After a pass and after a key, the runtime writes `settings.toml` through `ConfigWriter` when the core says something changed (`settingsNeedWrite`): message vibration, the notice of a remote query and the language of the screen. A failed write is a line in the log and the old file stays.

## Tests

`test/test_runtime` — boot on the fakes in a chosen language; the frames through the refresh policy (one full refresh, none for a quiet minute); an empty card; an unknown interface language; a user catalog on the card chosen by the configuration; an incoming message (counted, one partial refresh); an incoming call (takes the screen, through the lock, goes again); auto-lock; the keys under the screen; input drawn at once; a key press starts the quiet time again; the processor sleeps only while locked; an SOS from beginning to the parent's phone; a change of language at run time (the picture is the one a device configured in that language draws; an unknown code changes nothing); the three shipped languages draw the home screen without cut-off and differently; the real web service answers through the TCP fake; a restart asked for by the web interface; every screen name registered, and every page reachable by keys. Each of the behaviours above was broken on purpose and its test watched to fail.

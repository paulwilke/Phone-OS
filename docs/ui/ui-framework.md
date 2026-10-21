# The user-interface framework

The parts every screen is built from: the input model, the screen stack, and the widgets. The base of the screens (the frame around the content, the wording of the core's codes) is in [ui-screens.md](ui-screens.md); the framework is what the screens are written against, and everything in it is tested on the host, in every language, without a panel.

Everything lives in `src/ui/`, depends only on the display ([ui-display.md](ui-display.md)), the text engine ([ui-text.md](ui-text.md)) and the translation library ([i18n.md](../core/i18n.md)), and draws only through `ui::Display`.

## Input

`ui::Input` (`Input.h`) is what the platform turns key presses and touches into:

| Kind | Meaning |
|---|---|
| `Key::Up`, `Key::Down` | scroll, or move the selection one step; the device has no arrow keys, so these are the two keys that scroll |
| `Key::Select` | the main action (Enter) |
| `Key::Back` | back (backspace) |
| `Key::Home`, `Key::Inbox`, `Key::Compose` | the three keys under the screen; they act the same on every screen, so the stack owns them (below) |
| text | a typed character, as a Unicode code point |
| touch | a tap at a point of the screen, in pixels |

A tap is turned into a key or a selection by the widget under it, so a screen has one input logic for keys and touches alike.

## Screens and the screen stack

A `ui::Screen` (`Screen.h`) has `handle(Input)`, `render(Display&)` and two calls that bracket the time it is visible:

- **`onEnter()`** — the screen became the visible one.
- **`onLeave()`** — it stopped being the visible one, **for whatever reason**.

They always alternate, and they are the only place a screen starts or stops anything that runs because it is visible: the position fix while the position page is open, the motion sensor, a timer. A screen never has to guess from the outside whether it is still on show, and nothing is left running because one of the ways of leaving was forgotten. (The earlier design polled from the stack, which turned the position fix on and off by comparing the current screen after every key; leaving by lock, home key or incoming call each needed its own case.)

`ui::ScreenStack` (`ScreenStack.h`) keeps a short path from the root screen. **Visible** is the top of the path, or the lock screen while locked. Every change of the visible screen calls `onLeave()` on the old and `onEnter()` on the new one, and nothing else calls them:

| Operation | Effect on the path | Calls |
|---|---|---|
| `open(id)`, id new | pushed; replaces the newest screen when the path is full (4) | leave visible, enter id |
| `open(id)`, id already on the path | goes back to it (it is not put on twice) | leave visible, enter id |
| `open(id)`, id visible | nothing | none |
| `open(id)`, id unknown, refused by the filter, or locked | nothing | none |
| `back()` | pops; nothing at the root | leave visible, enter the one below |
| `home()` | back to the root | leave visible, enter root |
| `lock()` | the path is kept | leave visible, enter the lock screen |
| `unlock()` | back to the root | leave the lock screen, enter root |
| `interrupt(id)` (an incoming call) | pushed, also from the lock screen, which is left; the stack remembers that it was locked | leave visible, enter id |
| `endInterrupt(id)` | pops, only while id is still the visible one, and locks again when the interrupt found the device locked | leave id, enter the one below (or the lock screen) |
| `begin()` | the root | enter root (leave the visible one first if started before) |

The open filter (`setOpenFilter`) lets a role that has no messages refuse the messages screen; the root, the lock screen and interrupts are not subject to it. A screen must not navigate from inside `onEnter()` or `onLeave()`: such a call is ignored, so the path stays consistent while a transition runs. An action bound to a hardware key may make several navigation calls; each is a transition of its own.

**Input** goes to the visible screen. The three keys under the screen run the action bound with `setHardwareKey` instead, unless the screen asks for them (`wantsHardwareKeys`, for a key test) or the device is locked, where they do nothing (the lock screen sees them and may ignore them): unlocking stays a deliberate gesture, or the pocket would open the device.

**Ticks.** `poll(now)` first calls `onTick()` on the visible screen (the lock screen while locked), then does the auto-lock check; a tick that changed what is visible is followed by the check against the screen visible *now*. `onTick` is for what happens while the screen is shown — reading a sensor, or starting one once the frame that says "starting" has been drawn; starting and stopping what runs because the screen is visible stays in `onEnter` and `onLeave`.

**Auto-lock.** `poll(now)` locks the device after `setAutoLockSeconds` without input, unless the visible screen says `preventsAutoLock()` (a call in progress). The clock is any steady counter of seconds; wrapping is handled.

`Navigation` is the part of the stack a screen may use (`open`, `back`, `home`, `lock`, `unlock`).

## Layout

`ScreenLayout::standard()` (`Layout.h`) splits the 240 × 320 display into the status bar (22 pixels), the alert bar (20), the content, and the soft keys (32; 28 until 4.0, a little taller to be hit more easily), with two pixels between content and soft keys. The alert bar keeps its place when it is empty: a bar that appears and disappears moves the content down and up, which on e-ink is two full redraws and a finger that hits another tile than the one it aimed at.

## Widgets

Every widget draws inside the rectangle it is given and nothing outside it; the tests declare that rectangle as a region of the frame buffer and fail on any pixel outside. **Fixed labels** (words from the catalog) are never shortened: one that does not fit is a layout bug, found by the layout tests in every language ([ui-layout-testing.md](ui-layout-testing.md)). **User data** (a contact's name, a message, the operator's name) is shortened with `...` where a widget says so.

| Widget | What it is | User data it shortens |
|---|---|---|
| `SoftKeys` | the key bar: back on the left, optional ▲ ▼, the main action on the right; each button is a touch target (`softKeyAt`). **Back has a fixed place** on every screen, 94 pixels at the left whether arrows or an action are shown or not; the action ends at the right edge and also takes the arrows' place when there are none; a `wide` bar with one button (Hang up) gives it all of the bar. When a label does not fit with its key symbol and a space, the symbol goes (it is never pressed against the word) | none |
| `StatusBar` | operator or status word, "GPS" while locating, date, battery as five bars; built from a `StatusModel`; changes rarely, because every change is a panel redraw | the operator's name (the GPS note and the date are never pushed out; the weekday goes first when space is short) |
| alert bar | an inverted line with a message, or the reserved empty space | the whole message |
| `TileGrid` | symbol above a short label in a grid of tiles over pages; selected tile inverted, others raised; page dots; Up/Down move through the tiles in reading order and wrap | none (a tile without a symbol wraps its label) |
| `ListView` | one-line rows with a moving window and a scroll bar; Up/Down stop at the ends; `select` puts the focus on a row | rows marked as user text |
| `OptionRows` | boxes that each set one thing: the question in the small font, the answer under it; a row without an answer is a button, and one marked `current` has a second outline; Up/Down stop at the ends, the window follows the focus | none |
| `PageTitle` | the title of an options or instrument page and its rule; wraps in a long language, so the page lays out what is left below it | none |
| `TextBlock` | wrapped paragraph, scrolled by line, with a scroll bar | none (scrolling reaches the end) |
| `shapes` (`src/ui/Shapes.h`) | rounded rectangles and a scroll bar | |

`ui::format` (`Formats.h`) writes what the instrument pages share: a coordinate with five decimals, one decimal for a quality figure, and an age in words ("just now", "3 min ago", "2 days ago") from `age.*` of the catalogs; a coordinate reads the same in every language, an age is worded.

The status bar's wording lives in the catalogs under `status.*` and `weekday.*`; the standard soft-key words under `key.*`. The grid size of a `TileGrid` is the screen's decision: with three columns a tile is 74 pixels wide, too narrow for the longer words of the pseudo-language; the tests use two.

## Tests

- `test/test_screen_stack` — every row of the table above with its exact `onEnter`/`onLeave` sequence, the hardware-key rules, auto-lock including the clock wrapping, re-entrant navigation, and a property test: after any sequence of 60 random operations, exactly one screen is entered and it is the visible one.
- `test/test_widgets` — the logic of every widget (selection, wrapping, scrolling, taps, the battery bars, which status word wins, the date in each language, the weekday dropping out), and for the layouts: `languages::forEach` runs each in English, German, French and the pseudo-language, asserts no pixel leaves its cell in any of them, and compares pictures (`test/golden/*_en.txt`, `*_pseudo.txt`) in English and in the pseudo-language.
- `python3 scripts/check_i18n.py --usage` — every key of `lang/en.toml` is used by the code or a test, and every string in the code that looks like a catalog key is one.

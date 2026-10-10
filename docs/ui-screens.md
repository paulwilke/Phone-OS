# The frame of the screens, and the wording of the core's codes

Code: `src/ui/AppScreen.{h,cpp}`, `src/ui/AppStatus.{h,cpp}`, `src/ui/ScreenIds.h`, `src/ui/Wording.{h,cpp}`; tests in `test/test_ui_app_screen/` and `test/test_ui_wording/`, the rig in `testing/DeviceRig.h`. The concrete screens are written against these; each has a page of its own (listed in [README.md](README.md)). The widgets they use are in [ui-framework.md](ui-framework.md).

## What a screen may know

A screen includes `src/ui/`, `src/i18n/` and, from the application, **only `src/app/AppCore.h`**: the facade, and the plain data it hands out — the state, notices, the message history, the contact book, the enums of the network and Wi-Fi — **and `src/app/ShakeDetector.h`**, a value class that counts shakes from the samples a screen is given and has no device behind it. The acceleration sensor itself reaches the motion page through `ui::MotionSource`, which the application implements. It never includes a hardware interface, a board, a driver or any other part of the application directly, so it can be drawn on any `Display` and tested without a device. `scripts/check_layers.py` fails the build on a violation (it resolves an include next to the including file first and from `src/` second, the way the build does, so `"i18n/Translator.h"` is judged as the i18n layer).

A screen that needs something the core does not hand out as data — the acceleration sensor of the motion page — gets a small interface of its own in `src/ui/`, and the application wires the hardware to it.

## `AppScreen`

`AppScreen` is the base of every screen. It owns the frame around the content, so a concrete screen writes only what is its own:

| Frame part | Where it comes from |
|---|---|
| status bar | `statusModelOf(state)`, drawn with `drawStatusBar` |
| notice line | the core's notice, worded by `wording::notice`, drawn with `drawAlertBar`; the space is reserved when empty |
| content | `draw(display, area)` of the screen |
| soft keys | `keys()` of the screen |

`render()` first paints the whole display white, so nothing of an earlier frame shows through, and takes a snapshot of the core's state (`state()`), which the screen draws from. `handle()` takes a snapshot again, so a decision is taken on the numbers of the moment of the input. A tap on a soft key never reaches the screen as a touch: it arrives as the key it stands for (`onInput` with `Key::Back`, `Up`, `Down` or `Select`); a tap on the content arrives as a touch, with the content area; a tap on the status bar, the notice line or the gap above the keys is nobody's and returns false. A screen keeps no words: every text is asked of the catalogs when it is drawn, so a change of language is complete at the next frame.

`ScreenContext` is what the application hands every screen: the core, the translator, the navigation, the `Session` (the little data the screens of one task hand to each other: who a message goes to — the screen that asks writes it, the screen that composes reads it) and whether the device has a motion sensor (the core does not wrap the sensor, so the home screen learns it here).

## The GPS note

"GPS" is drawn in the middle of the status bar while the position is being determined, so that it stays in one place whatever the network is called and whatever day it is (owner, 2026-10-09). The left text gets the room up to it (an operator's name is shortened to fit, at least 40 pixels or the note gives way), the date the room after it (the weekday goes first). Only when a fixed word such as "SIM verrouillee" or the date would run into the middle does the note follow the left text, as it did before 4.1.6; nothing is ever drawn over anything. `test_widgets` pins the middle for four names and three weekdays and the fallback in French.

## The status bar from the state

`statusModelOf(state)` turns the core's state into the bar's own small model:

| Bar | From the state |
|---|---|
| SIM problem | `SimStatus`: missing, PIN required or wrong (`PinWrong` is the same word: the stored PIN was rejected and is never tried again), locked (PUK). "Unknown" is a modem that was not asked yet: it tries anyway, so the bar says nothing |
| network | the network label as data: operator, radio, connecting, none. **Connecting** (`status.connecting`, "Connecting ...") is a modem on its way: switched on and searching for its first network since then, or registered while the network's name has not come yet; a network lost after the first one is "No network", so that the child sees that nobody can reach the device. A network whose name is empty after shortening is shown by the general word `status.network` ("Network") |
| school time | `schoolTime` |
| "SMS only", "Connecting ..." | `voice`: `SmsOnly` is a restriction of the network, `ImsMissing` is telephony that is not ready and usually passes within half a minute, shown as "Connecting ..." (owner, 2026-10-08: "No calls yet" was too much text for it; the network page still says which of the two is missing); `Unknown` says nothing, like an unknown SIM |
| "GPS" | `positioningActive`: a device that locates in secret is what this one must not be |
| battery | `batteryPercent`, five bars |
| date | `wall`: month, day and weekday, or the "unknown" form when the time is not known |

The **signal strength is not on the bar**. The bar changes rarely on purpose, because every change is a redraw of the e-ink panel, and the strength changes with every step the child takes. It is on the network and status pages, which are opened on purpose.

## The notice line

The core's notice is a kind with a number (for who it concerns) and a minute (for the one kind that names a time). `wording::notice(notice, contacts, t, maxWidth)` words it in the language of the moment:

- The name of the number comes from the contact book when the line is drawn, so a contact renamed since is renamed. A number that is in no book is shown as it is; no number at all is the word `peer.unknown`.
- A time is written `HH:MM` (`wording::clockTime`), digits only, the same in every language. A minute outside the day is written as midnight, never as a wrong time.
- **A line that is too wide is not cut at the end** (where "busy" or "refused" is), but its name is shortened with "..." inside itself. A phone number is **never** shortened: the digits are the information, which is why `QueryRefused` — the one notice whose subject is a stranger — has short words of its own (`notice.query-refused`: "Refused: +49…"). If even the name's "..." does not fit, the bar's own shortening is the last resort.
- The words the device itself writes fit the bar in every shipped language: `test_every_notice_fits_the_alert_bar_...` measures every notice kind with a name, a very long name and (for the refused query) a number. The pseudo-language is exempt there on purpose; it is checked for clipping only.

## `wording`: every code, in one place

| Function | Turns | Into |
|---|---|---|
| `notice` | a `Notice` | the line under the status bar, or nothing |
| `systemEntry` | a message of source System | the row of the history ("Missed call from Grandma"); an entry from an earlier version (a sentence) or from a later one (a kind this version does not know) is shown as it was stored |
| `callReason` | `busy`, `no-answer`, `no-network`, `sms-only`, `not-connected` | a short phrase; an unknown code is shown as given |
| `wifiReason` | `WifiReason` | why Wi-Fi is off or failed, nothing for `None`; a `switch` without a default, so a new reason is a compiler warning |
| `networkLabel` | the network label | the operator's name or the word for none, radio, connecting, or "Network" |
| `peerName` | a phone number | the contact's name, the number, or `peer.unknown` |
| `clockTime` | a minute of the day | `HH:MM` |

The tests walk **every** notice kind, system entry kind, call reason, Wi-Fi reason and network label in English, German and French and assert real words (no catalog key, no unfilled placeholder) and that no language fell back to English or asked for a missing key. A code without a catalog entry therefore fails a test and never shows a key on the child's screen.

## Testing a screen

`testing/DeviceRig.h` is a whole device for screen tests: the core on the fakes with all catalogs and the pseudo-language loaded, the clock at a Tuesday afternoon, `RecordingNavigation` for what a screen asked the stack to do, and a frame buffer. `testing/Shows.h` answers "does this screen say that?": `shows::text(fb, "School time until 15:00", FontSize::Small, region, onBlack)` draws the text into a small buffer with the text engine and looks for exactly that pattern in the picture, so a test asserts on what the child reads in the language of the moment — and a word drawn in the wrong font, cut off or overlapped is not found. A word is not found inside a longer one (three clear columns on either side). A screen test scripts the world through the fakes, drives the screen with keys and touches, and asserts on what the screen did to the core, to the navigation and to the picture. Layouts run in every language (`languages::forEach`, see [ui-layout-testing.md](ui-layout-testing.md)); pictures are kept in English and German for whole screens.

## Tests

- `test/test_ui_app_screen` — the text finder (`testing/Shows.h`); the state-to-bar mapping (every SIM status, every network kind, school time, position, battery, date); the frame (status bar, notice line and soft keys around the content; the reserved empty notice line; the language followed at the next frame; the whole screen repainted; no clipping in any language; pictures in English and German); the notice never moves the content; school time and a locked SIM named on the bar in every language; every notice fits the bar; a long name is shortened inside the line; a number is not; input translation (keys, soft-key taps, content taps, taps on the bars, the state of the moment of the input).
- `test/test_ui_wording` — every code in every language; exact words for the examples; entries of other versions; the language and a renamed contact followed.

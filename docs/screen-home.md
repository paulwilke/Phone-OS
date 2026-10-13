# The home screen

Code: `src/ui/screens/HomeScreen.{h,cpp}`; tests in `test/test_screen_home/`. A grid of tiles, three columns, two pages. The first page is the child's, the second is the device's; the dots under the tiles say there is a second page, and they can be tapped. It is the root of the screen stack ([ui-screens.md](ui-screens.md)): `unlock`, `home()` and a key under the screen all end here.

## The tiles

| Page | Tile | Label key | Exists when | Select does |
|---|---|---|---|---|
| 1 | phone | `home.phone` | there is somebody to call: a contact or a call target | school time: says "School time until 15:00" and stays; otherwise opens the phone page. It dials nothing: the child chooses whom first |
| 1 | text | `home.text` | the same | opens the recipient page |
| 1 | SOS | `home.sos` | `Profile::sosRecipient` is not empty | `beginSos()`, then the SOS page. In school time the core opens its warning instead of the choice |
| 1 | inbox | `home.inbox` | always | opens the messages |
| 1 | radio | `home.radio` | the profile allows the radio | opens the radio page |
| 1 | light | `home.light` | always | switches the front light; the notice line says "Light on" / "Light off" |
| 1 | power | the power symbol alone, no word (D-21) | always | opens the power page ([screen-power.md](screen-power.md)), where the device is switched off after a question |
| 2 | network, GPS, clock | `home.network`, `home.gps`, `home.clock` | `[home]` does not switch it off | open their pages |
| 2 | settings | `home.settings` | the same | opens the options |
| 2 | Wi-Fi | `home.wifi` | the same, `parentTools` and the core has a Wi-Fi | opens the Wi-Fi page |
| 2 | motion | `home.motion` | the same, and the device has a motion sensor (`ScreenContext::motionSensor`) | opens the motion page |
| 2 | tests | `home.tests` | `[home]` does not switch it off | opens the self test |
| 2 | status | `home.status` | the same, and `parentTools` | opens the status page |

**The light is the child's** (owner, 2026-10-08): it is on the first page, behind the inbox and the radio. **The power tile is the last of the first page** (owner, 2026-10-12, D-12): the device is often switched off in the pocket and on when it is used, and a switch-off hidden on the second page was too hidden. **The parents choose the tiles of the second page**: `[home]` in `settings.toml` has a switch per tile (`network`, `gps`, `clock`, `settings`, `wifi`, `motion`, `tests`, `status`, all `true` by default; on the parents' page under **Advanced**). A switch only hides; Wi-Fi and Status still need the parent tools, Motion the sensor. With every tile of the second page off there is no second page and no dots. Without the Wi-Fi tile (or the options, which lead to it) the parents' page is still reached over the USB cable: `scripts/webbridge.py` ([device-console.md](device-console.md)).

**The order is fixed.** The profile holds the numbers the call actions reach, not an order of tiles, so the order is the one in the table. Everything a child can change leaves the tile set alone: only the profile and the device decide which tiles exist. A device with nobody to reach has no phone, text or SOS tile (there is nothing to send the position to), and no tile is ever missing for another reason.

All tools together are eight, and a page holds nine; a tenth would be unreachable, which a test pins.

Only the symbols of the first page are pictures (☎ ✈ 💬 📍 📡); the tools are words, wrapped when they need two lines.

## What a tile shows

- **Badge.** The inbox tile shows `✉` and the number of unread entries in its top right corner when there is something unread, and nothing otherwise. The count is the core's, which counts system entries (a missed call, a refused query) too.
- **Words.** Every label is a catalog word that fits the inside of a tile (68 pixels in the small bold font) in English, German and French; a test measures all of them. The pseudo-language is longer than any real language on purpose and is the one exception: it is only checked for staying on the display.

## Selection and keys

Up and Down move the selection one tile in reading order through both pages and wrap; the page follows the selection. Select opens the tile. **Back locks the device**: without it the child could not lock it before it goes into the pocket. A typed character is not used. The selection is **kept** when the screen is left and come back to, so returning from a tool lands on the same tile and the same page.

**The soft keys have no arrows** (owner, 2026-10-08: the arrows only took room), **but one button: Lock** ("Lock", "Sperren", "Verrouiller"), in the place of Back on the left. Back locks the device on this screen, and since 4.1.0 nothing on it said so: the bar was empty, and the only way to lock was a key a child does not know. The word is the same whatever tile is selected, so the bar never changes and costs no redraw. A tap on it is the Back key. The tiles are the buttons and the dots turn the page; a bar that named the action of the tile under the selection would change with every step. The keys Up and Down still move the selection, so every tile is reached without touch (a test walks them all). The button is in the bar of every language; a test measures that the word fits the slot in each.

## Touch

A tap on a tile selects and opens it. **The page dots are wide targets**: the strip under the tiles (18 pixels, as in 4.0) is cut into one part per page, and a tap anywhere in a part goes to the first tile of that page and opens nothing; with two pages the left half is the first page and the right half the second. Each dot is drawn as a bar of 28 × 7 pixels in the middle of its part. In 4.0 the dots were six pixels side by side in the middle with twelve pixels of target each, too small for a finger. A test sends the touch chip's own report for a finger at the middle, the right edge and beyond the panel through the device's translation (`TouchDecoder`). A tap beside the tiles does nothing; only the tiles of the page on show can be hit.

## Tests

`test/test_screen_home` — which tiles exist for each switch of the profile (child, radio, parents' tools, Wi-Fi, motion sensor, nobody to reach, a call target outside the book, contacts without a call action); that the tools fit one page; the walk through both pages; the selection kept; every tile opening its screen; the phone tile dialling nothing; the light; the power tile opening the power page; SOS and its school-time warning; school time (the phone tile stays and says until when, in two languages; text and SOS work; after school time the phone opens again); the badge, system entries counted; no arrows and every tile reached with the keys; taps on tiles and on the wide dot targets through the touch chip's report; the tiles of `[home]` switched off, and all of them; every label found on the screen in the three languages; every label fits its tile; both pages drawn inside the display in all four languages; pictures in English and German (`home_front_*`, `home_tools_*`); a notice does not move the tiles.

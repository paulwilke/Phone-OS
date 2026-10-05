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
| 2 | network, GPS, clock | `home.network`, `home.gps`, `home.clock` | always | open their pages |
| 2 | settings | `home.settings` | always | opens the options |
| 2 | Wi-Fi | `home.wifi` | `parentTools` and the core has a Wi-Fi | opens the Wi-Fi page |
| 2 | motion | `home.motion` | the device has a motion sensor (`ScreenContext::motionSensor`) | opens the motion page |
| 2 | tests | `home.tests` | always | opens the self test |
| 2 | status | `home.status` | `parentTools` | opens the status page |
| 2 | light | `home.light` | always | switches the front light; the notice line says "Light on" / "Light off" |

**The order is fixed.** The profile holds the numbers the call actions reach, not an order of tiles, so the order is the one in the table. Everything a child can change leaves the tile set alone: only the profile and the device decide which tiles exist. A device with nobody to reach has no phone, text or SOS tile (there is nothing to send the position to), and no tile is ever missing for another reason.

All tools together are nine, and a page holds nine; a tenth would be unreachable, which a test pins.

Only the symbols of the first page are pictures (☎ ✈ 💬 📍 📡); the tools are words, wrapped when they need two lines.

## What a tile shows

- **Badge.** The inbox tile shows `✉` and the number of unread entries in its top right corner when there is something unread, and nothing otherwise. The count is the core's, which counts system entries (a missed call, a refused query) too.
- **Words.** Every label is a catalog word that fits the inside of a tile (68 pixels in the small bold font) in English, German and French; a test measures all of them. The pseudo-language is longer than any real language on purpose and is the one exception: it is only checked for staying on the display.

## Selection and keys

Up and Down move the selection one tile in reading order through both pages and wrap; the page follows the selection. Select opens the tile. **Back locks the device**: without it the child could not lock it before it goes into the pocket. A typed character is not used. The selection is **kept** when the screen is left and come back to, so returning from a tool lands on the same tile and the same page.

**The soft keys show the arrows only.** The tiles are the buttons, and a bar that named the action of the tile under the selection would change with every step, which costs a redraw of the panel each time. Tapping an arrow moves the selection like the key.

## Touch

A tap on a tile selects and opens it. A tap on a page dot goes to the first tile of that page and opens nothing. A tap beside the tiles does nothing; only the tiles of the page on show can be hit.

## Tests

`test/test_screen_home` — which tiles exist for each switch of the profile (child, radio, parents' tools, Wi-Fi, motion sensor, nobody to reach, a call target outside the book, contacts without a call action); that the tools fit one page; the walk through both pages; the selection kept; every tile opening its screen; the phone tile dialling nothing; the light; SOS and its school-time warning; school time (the phone tile stays and says until when, in two languages; text and SOS work; after school time the phone opens again); the badge, system entries counted; the arrows; taps on tiles, dots and arrows; every label found on the screen in the three languages; every label fits its tile; both pages drawn inside the display in all four languages; pictures in English and German (`home_front_*`, `home_tools_*`); a notice does not move the tiles.

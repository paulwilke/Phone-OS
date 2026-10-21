# Map link

Code: `src/app/MapLink.{h,cpp}`, tests in `test/test_map_link/`. Pure C++17.

A position message carries the coordinates and, when wanted, a link that opens the place on the parents' phone. The link is built from a **template** that the parents can change, because what works depends on their phone and on the networks in between. Two obvious choices failed on a real device:

- an OpenStreetMap link never arrived: German networks filter links in SMS from foreign numbers;
- a `geo:` address arrived but could not be opened: not every phone (iOS, for one) knows it.

The default, `https://maps.google.com/?q={lat},{lon}` (`maplink::defaultTemplate()`), opens on both phone systems. Others that are used:

| Template | Service |
|---|---|
| `https://waze.com/ul?ll={lat},{lon}` | Waze |
| `https://www.openstreetmap.org/?mlat={lat}&mlon={lon}` | OpenStreetMap |
| `geo:{lat},{lon}` | the phone's default map app, where it understands `geo:` |
| empty | no link, the coordinates alone |

## Building the link

`maplink::build(template, lat, lon, out, size)` replaces `{lat}` and `{lon}` by the coordinate with five decimals (about one metre, more than any receiver delivers) and copies everything else as it is. Braces that are not one of the two placeholders stay, so a typo such as `{latt}` is visible in the message and does not silently produce another address. The placeholders are exact lower case and may repeat.

The result says what happened, and `out` is empty for everything but `Ok`:

| Result | Meaning |
|---|---|
| `Ok` | `out` holds the address |
| `NoTemplate` | the template is empty or null: no link wanted. It does **not** fall back to the default; whoever empties the setting does not want one |
| `InvalidCoordinates` | not a finite latitude within ±90 and longitude within ±180 (a receiver without a fix, garbage). The edges of the globe are valid |
| `BufferTooSmall` | the address does not fit |

A link that does not fit is **never shortened**: a link cut inside a coordinate points to a wrong place, and in an emergency a missing link (the coordinates are still in the message) is better than a misleading one.

## Numbers

`maplink::formatDegrees()` writes five decimals with integer arithmetic only, because floating-point `printf` is not available in every embedded toolchain. Rounding is half away from zero. A value that rounds to zero has no sign (`-0.000004` is `0.00000`, not `-0.00000`). NaN, infinity, a magnitude beyond 1e9 and a buffer that is too small are refused.

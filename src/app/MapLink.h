#pragma once

// The map address in a position message, built from a template. The template is
// the parents' choice because what opens on their phone depends on the phone and
// on the networks in between. Measured on a real device:
//   * an OpenStreetMap link did not arrive at all: German networks filter links
//     in SMS from foreign numbers;
//   * a `geo:` address arrived but could not be opened: not every phone, iOS for
//     one, knows RFC 5870.
// The default is Google Maps because its address opens on both phone systems.
//
//   "https://maps.google.com/?q={lat},{lon}"
//   "https://waze.com/ul?ll={lat},{lon}"
//   "https://www.openstreetmap.org/?mlat={lat}&mlon={lon}"
//   "geo:{lat},{lon}"
//   ""                    no address at all, the coordinates alone
//
// `{lat}` and `{lon}` are replaced by the coordinate with five decimals, which is
// about one metre and more than any GPS receiver delivers. Everything else in the
// template is copied as it is, including braces that are not one of the two
// placeholders: somebody who writes `{latt}` should see it in the message and not
// silently get a different address.

#include <cstddef>

namespace maplink {

enum class Result {
  Ok,                 // `out` holds the address
  NoTemplate,         // the template is empty or null: no address wanted, `out` is empty
  InvalidCoordinates, // not a finite latitude in [-90, 90] and longitude in [-180, 180]; `out` is empty
  BufferTooSmall,     // the address does not fit; `out` is empty
};

// Fills `out` (at most `size` bytes with the terminator) from the template.
//
// On every result other than Ok `out` is empty, never a shortened address: a link
// cut in the middle of a coordinate would point to a wrong place, and in an
// emergency a missing link (the message still carries the coordinates) is better
// than a misleading one.
Result build(const char* tmpl, double lat, double lon, char* out, size_t size);

// A coordinate with five decimals, "-12.34500", rounded half away from zero.
// Done with integers because printf of floating-point numbers is not available in
// every embedded toolchain. A value that rounds to zero has no minus sign. Returns
// false, with `out` empty, for NaN, infinity, a magnitude beyond 1e9 or a small buffer.
bool formatDegrees(double value, char* out, size_t size);

// Used when the settings say nothing.
const char* defaultTemplate();

}  // namespace maplink

#pragma once

// The interface symbols as UTF-8, for code that draws them (the soft keys, tiles). They are
// the symbols of assets/fonts/charset.txt; a test checks that the two agree. Catalog texts
// write the same characters directly.

namespace ui {
namespace symbol {

constexpr const char* kEnter = "\xE2\x86\xB5";           // U+21B5
constexpr const char* kBackspace = "\xE2\x8C\xAB";       // U+232B
constexpr const char* kUp = "\xE2\x96\xB2";              // U+25B2
constexpr const char* kDown = "\xE2\x96\xBC";            // U+25BC
constexpr const char* kTelephone = "\xE2\x98\x8E";       // U+260E
constexpr const char* kEnvelope = "\xE2\x9C\x89";        // U+2709
constexpr const char* kAeroplane = "\xE2\x9C\x88";       // U+2708
constexpr const char* kBubble = "\xF0\x9F\x92\xAC";      // U+1F4AC
constexpr const char* kRadio = "\xF0\x9F\x93\xA1";       // U+1F4E1
constexpr const char* kPin = "\xF0\x9F\x93\x8D";         // U+1F4CD

}  // namespace symbol
}  // namespace ui

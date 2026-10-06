#pragma once

// Conversion between UTF-8 and the hex-coded UCS-2 / UTF-16BE that the A7682E uses for
// numbers and SMS texts after AT+CSCS="UCS2". The GSM alphabet cannot carry umlauts or
// emoji unharmed, and "1234" is both valid GSM text and valid hex, so the charset is
// always chosen, never guessed.

#include <cstddef>

namespace text {

// True when s[0..len) is a multiple of four hex digits. The empty string is valid.
bool isUcs2Hex(const char* s, size_t len);

// Decodes hex UCS-2 to UTF-8. Surrogate pairs (emoji) are joined, a lone surrogate
// becomes U+FFFD, a NUL unit is dropped (it would end the C string early). False for
// invalid hex; `out` is empty then. When the text does not fit it is cut at a character
// boundary and *truncated is set. `out` is always terminated (outSize > 0 required).
bool ucs2HexToUtf8(const char* hex, size_t hexLen, char* out, size_t outSize, bool* truncated);

// Encodes UTF-8 to hex UTF-16BE in capitals. Returns the number of UTF-16 units, or -1
// for invalid UTF-8 (overlong forms and surrogates included) or a buffer that is too
// small; `out` is empty then.
int utf8ToUcs2Hex(const char* utf8, char* out, size_t outSize);

}  // namespace text

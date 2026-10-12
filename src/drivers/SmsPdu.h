#pragma once

// SMS-SUBMIT in PDU mode (decision D-10). The modem's text mode sends a text that has one character outside
// ASCII as UCS2 (70 places) and has no way to join the parts of a long text; in PDU mode the driver builds the
// message itself:
//   * a text whose characters are all in the GSM alphabet (ITU 03.38: the German umlauts and the sharp s,
//     the common accented letters, the euro sign) goes out as 7-bit, 160 places in one message;
//   * any other text goes out as UCS2, 70 places in one message (an emoji takes two);
//   * a longer text is cut into parts of 153 (7-bit) or 67 (UCS2) places that carry a concatenation header,
//     and the receiving phone shows them as one message.
// The extension characters of the alphabet (^ { } \ [ ] ~ | and the euro sign) take two places.
//
// Pure functions of their arguments, tested on the host against the example PDU that every SMS tutorial uses
// and by decoding what they encode (test/test_sms_pdu). The validity period is 12 hours (the 167 that the text
// mode was set to), the message reference is left to the modem, and the service centre is the one stored in the
// modem (a leading 00).

#include <cstdint>
#include <string>
#include <vector>

namespace sms {

enum class Coding : uint8_t { Gsm7, Ucs2 };

// One message ready for AT+CMGS=<tpduLength>: the hex PDU with the leading service-centre octet, and the length
// of the TPDU behind it in octets, which is the number the command wants.
struct Part {
  std::string hex;
  int tpduLength = 0;
};

// The 7-bit places of a text (an extension character is two entries: 0x1B and its code), or false when a
// character is outside the alphabet or the text is not valid UTF-8.
bool toGsm7(const std::string& utf8, std::vector<uint8_t>& places);

// Encodes a text for a number ("+491701111111", digits, * and #). `reference` is the concatenation reference
// of a text that needs several parts. False for a number that is not a dial string or has more than 20 digits
// (never shortened: a shortened number is someone else's), an empty text, invalid UTF-8 and a NUL.
bool encode(const std::string& phone, const std::string& utf8, uint8_t reference, Coding& coding, std::vector<Part>& parts);

}  // namespace sms

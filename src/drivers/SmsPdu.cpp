#include "SmsPdu.h"

#include <algorithm>
#include <utility>

namespace sms {

namespace {

constexpr int kMaxDigits = 20;
constexpr size_t kSinglePlaces7 = 160, kPartPlaces7 = 153;
constexpr size_t kSingleUnits = 70, kPartUnits = 67;
constexpr uint8_t kEscape = 0x1B;

// The basic table of ITU 03.38 as Unicode (the entry of 0x1B is the escape and has no character).
constexpr uint16_t kBasic[128] = {
    0x40,  0xA3,  0x24,  0xA5,  0xE8,  0xE9,  0xF9,  0xEC,  0xF2,  0xC7,  0x0A,  0xD8,  0xF8,  0x0D,  0xC5,  0xE5,
    0x394, 0x5F,  0x3A6, 0x393, 0x39B, 0x3A9, 0x3A0, 0x3A8, 0x3A3, 0x398, 0x39E, 0,     0xC6,  0xE6,  0xDF,  0xC9,
    0x20,  0x21,  0x22,  0x23,  0xA4,  0x25,  0x26,  0x27,  0x28,  0x29,  0x2A,  0x2B,  0x2C,  0x2D,  0x2E,  0x2F,
    0x30,  0x31,  0x32,  0x33,  0x34,  0x35,  0x36,  0x37,  0x38,  0x39,  0x3A,  0x3B,  0x3C,  0x3D,  0x3E,  0x3F,
    0xA1,  0x41,  0x42,  0x43,  0x44,  0x45,  0x46,  0x47,  0x48,  0x49,  0x4A,  0x4B,  0x4C,  0x4D,  0x4E,  0x4F,
    0x50,  0x51,  0x52,  0x53,  0x54,  0x55,  0x56,  0x57,  0x58,  0x59,  0x5A,  0xC4,  0xD6,  0xD1,  0xDC,  0xA7,
    0xBF,  0x61,  0x62,  0x63,  0x64,  0x65,  0x66,  0x67,  0x68,  0x69,  0x6A,  0x6B,  0x6C,  0x6D,  0x6E,  0x6F,
    0x70,  0x71,  0x72,  0x73,  0x74,  0x75,  0x76,  0x77,  0x78,  0x79,  0x7A,  0xE4,  0xF6,  0xF1,  0xFC,  0xE0};

struct Extension {
  uint8_t code;
  uint32_t codePoint;
};
constexpr Extension kExtension[] = {{0x0A, 0x0C}, {0x14, '^'}, {0x28, '{'}, {0x29, '}'}, {0x2F, '\\'},
                                    {0x3C, '['},  {0x3D, '~'}, {0x3E, ']'}, {0x40, '|'}, {0x65, 0x20AC}};

// The next code point of a UTF-8 text at s[i]; false for anything that is not well formed (a stray
// continuation byte, an overlong form, a surrogate, beyond U+10FFFF).
bool next(const std::string& s, size_t& i, uint32_t& cp) {
  const unsigned char c = (unsigned char)s[i];
  size_t n;
  if (c < 0x80) { cp = c; n = 1; }
  else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; n = 2; }
  else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; n = 3; }
  else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; n = 4; }
  else return false;
  if (i + n > s.size()) return false;
  for (size_t k = 1; k < n; ++k) {
    const unsigned char d = (unsigned char)s[i + k];
    if ((d & 0xC0) != 0x80) return false;
    cp = (cp << 6) | (d & 0x3F);
  }
  if ((n == 2 && cp < 0x80) || (n == 3 && cp < 0x800) || (n == 4 && cp < 0x10000)) return false;
  if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return false;
  i += n;
  return true;
}

// The code points of a text; false for invalid UTF-8 or a NUL.
bool codePoints(const std::string& s, std::vector<uint32_t>& out) {
  for (size_t i = 0; i < s.size();) {
    uint32_t cp;
    if (!next(s, i, cp) || cp == 0) return false;
    out.push_back(cp);
  }
  return true;
}

bool gsmPlaces(uint32_t cp, uint8_t (&places)[2], size_t& n) {
  for (int code = 0; code < 128; ++code) {
    if (code != kEscape && kBasic[code] == cp) {
      places[0] = (uint8_t)code;
      n = 1;
      return true;
    }
  }
  for (const Extension& e : kExtension) {
    if (e.codePoint == cp) {
      places[0] = kEscape;
      places[1] = e.code;
      n = 2;
      return true;
    }
  }
  return false;
}

bool addressOctets(const std::string& phone, std::string& hex, int& digits) {
  hex.clear();
  size_t start = 0;
  bool international = false;
  if (!phone.empty() && phone[0] == '+') {
    international = true;
    start = 1;
  }
  std::vector<uint8_t> nibbles;
  for (size_t i = start; i < phone.size(); ++i) {
    const char c = phone[i];
    if (c >= '0' && c <= '9') nibbles.push_back((uint8_t)(c - '0'));
    else if (c == '*') nibbles.push_back(0xA);
    else if (c == '#') nibbles.push_back(0xB);
    else return false;
  }
  if (nibbles.empty() || nibbles.size() > (size_t)kMaxDigits) return false;
  digits = (int)nibbles.size();
  static const char* const hexDigits = "0123456789ABCDEF";
  hex += international ? "91" : "81";
  for (size_t i = 0; i < nibbles.size(); i += 2) {
    const uint8_t low = nibbles[i];
    const uint8_t high = i + 1 < nibbles.size() ? nibbles[i + 1] : 0xF;
    hex += hexDigits[high];
    hex += hexDigits[low];
  }
  return true;
}

void appendHex(std::string& out, const std::vector<uint8_t>& bytes) {
  static const char* const hexDigits = "0123456789ABCDEF";
  for (uint8_t b : bytes) {
    out += hexDigits[b >> 4];
    out += hexDigits[b & 0x0F];
  }
}

// The 7-bit places packed from bit `startBit` of a buffer that already holds `prefix` bytes.
std::vector<uint8_t> pack7(const std::vector<uint8_t>& prefix, const uint8_t* places, size_t count, size_t startBit) {
  std::vector<uint8_t> out(prefix);
  out.resize((startBit + count * 7 + 7) / 8, 0);
  for (size_t i = 0; i < count; ++i) {
    for (size_t b = 0; b < 7; ++b) {
      if ((places[i] >> b) & 1) {
        const size_t bit = startBit + i * 7 + b;
        out[bit / 8] |= (uint8_t)(1u << (bit % 8));
      }
    }
  }
  return out;
}

// The message around a user data field: service centre (the modem's own), first octet, reference, address,
// protocol, coding, validity, length, data.
Part build(const std::string& addressHex, int digits, bool concatenated, uint8_t dcs, int udl, const std::vector<uint8_t>& ud) {
  std::string tpdu;
  appendHex(tpdu, {(uint8_t)(concatenated ? 0x51 : 0x11), 0x00, (uint8_t)digits});
  tpdu += addressHex;
  appendHex(tpdu, {0x00, dcs, 0xA7, (uint8_t)udl});
  appendHex(tpdu, ud);
  Part part;
  part.tpduLength = (int)(tpdu.size() / 2);
  part.hex = "00" + tpdu;
  return part;
}

std::vector<uint8_t> header(uint8_t reference, size_t total, size_t sequence) {
  return {0x05, 0x00, 0x03, reference, (uint8_t)total, (uint8_t)sequence};
}

}  // namespace

bool toGsm7(const std::string& utf8, std::vector<uint8_t>& places) {
  places.clear();
  std::vector<uint32_t> cps;
  if (!codePoints(utf8, cps)) return false;
  for (uint32_t cp : cps) {
    uint8_t p[2];
    size_t n = 0;
    if (!gsmPlaces(cp, p, n)) return false;
    for (size_t k = 0; k < n; ++k) places.push_back(p[k]);
  }
  return true;
}

bool encode(const std::string& phone, const std::string& utf8, uint8_t reference, Coding& coding, std::vector<Part>& parts) {
  parts.clear();
  std::string address;
  int digits = 0;
  if (!addressOctets(phone, address, digits) || utf8.empty()) return false;
  std::vector<uint32_t> cps;
  if (!codePoints(utf8, cps)) return false;

  std::vector<uint8_t> places;
  if (toGsm7(utf8, places)) {
    coding = Coding::Gsm7;
    if (places.size() <= kSinglePlaces7) {
      parts.push_back(build(address, digits, false, 0x00, (int)places.size(), pack7({}, places.data(), places.size(), 0)));
      return true;
    }
    // Parts of at most 153 places; a pair of escape and code is never cut.
    std::vector<std::pair<size_t, size_t>> cuts;
    for (size_t at = 0; at < places.size();) {
      size_t take = std::min(kPartPlaces7, places.size() - at);
      if (take == kPartPlaces7 && places[at + take - 1] == kEscape) --take;
      cuts.push_back({at, take});
      at += take;
    }
    if (cuts.size() > 255) return false;
    for (size_t k = 0; k < cuts.size(); ++k) {
      // The header takes 48 bits; the text starts at the next septet boundary, bit 49, and the length counts
      // the seven places that the header and the filler bit take.
      const std::vector<uint8_t> udh = header(reference, cuts.size(), k + 1);
      parts.push_back(build(address, digits, true, 0x00, (int)(7 + cuts[k].second),
                            pack7(udh, places.data() + cuts[k].first, cuts[k].second, 49)));
    }
    return true;
  }

  coding = Coding::Ucs2;
  std::vector<uint16_t> units;
  for (uint32_t cp : cps) {
    if (cp >= 0x10000) {
      cp -= 0x10000;
      units.push_back((uint16_t)(0xD800 + (cp >> 10)));
      units.push_back((uint16_t)(0xDC00 + (cp & 0x3FF)));
    } else {
      units.push_back((uint16_t)cp);
    }
  }
  const auto octets = [&](size_t from, size_t count, std::vector<uint8_t> prefix) {
    for (size_t i = from; i < from + count; ++i) {
      prefix.push_back((uint8_t)(units[i] >> 8));
      prefix.push_back((uint8_t)(units[i] & 0xFF));
    }
    return prefix;
  };
  if (units.size() <= kSingleUnits) {
    parts.push_back(build(address, digits, false, 0x08, (int)(units.size() * 2), octets(0, units.size(), {})));
    return true;
  }
  std::vector<std::pair<size_t, size_t>> cuts;
  for (size_t at = 0; at < units.size();) {
    size_t take = std::min(kPartUnits, units.size() - at);
    if (take == kPartUnits && units[at + take - 1] >= 0xD800 && units[at + take - 1] < 0xDC00) --take;   // not between the halves of a pair
    cuts.push_back({at, take});
    at += take;
  }
  if (cuts.size() > 255) return false;
  for (size_t k = 0; k < cuts.size(); ++k) {
    const std::vector<uint8_t> ud = octets(cuts[k].first, cuts[k].second, header(reference, cuts.size(), k + 1));
    parts.push_back(build(address, digits, true, 0x08, (int)ud.size(), ud));
  }
  return true;
}

}  // namespace sms

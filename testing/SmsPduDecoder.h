#pragma once

// An SMS-SUBMIT read back as ITU 23.040 describes it, written from the specification and not from the encoder
// (src/drivers/SmsPdu.cpp), so that a test can say what a PDU means: the number, the coding, the concatenation
// header, the places or the UTF-16 units, and the text. For the PDUs that the modem driver is asked to send.
// allow-non-english: the texts in the comments contain umlauts.

#include <cstdint>
#include <string>
#include <vector>

namespace smsdecode {

inline int hexValue(char c) { return c <= '9' ? c - '0' : c - 'A' + 10; }

struct Decoded {
  bool ok = false;
  bool udhi = false;
  std::string number;
  int dcs = -1;
  int validity = -1;
  int udl = 0;
  int reference = -1, total = 0, sequence = 0;
  int tpduLength = 0;
  std::vector<uint8_t> places;   // 7-bit
  std::vector<uint16_t> units;   // UCS2
};

// Reads an SMS-SUBMIT as ITU 23.040 describes it.
inline Decoded decode(const std::string& hex) {
  Decoded d;
  std::vector<uint8_t> b;
  for (size_t i = 0; i + 1 < hex.size(); i += 2) b.push_back((uint8_t)(hexValue(hex[i]) * 16 + hexValue(hex[i + 1])));
  size_t i = 0;
  if (b.empty() || b[i++] != 0x00) return d;   // the service centre of the modem
  const size_t tpduStart = i;
  const uint8_t first = b[i++];
  d.udhi = (first & 0x40) != 0;
  if ((first & 0x03) != 0x01 || ((first >> 3) & 0x03) != 0x02) return d;   // SUBMIT with a relative validity
  ++i;                                                                       // the reference is the modem's
  const int digits = b[i++];
  const uint8_t type = b[i++];
  if (type == 0x91) d.number = "+";
  for (int k = 0; k < digits; ++k) {
    const uint8_t octet = b[i + k / 2];
    const int nib = k % 2 == 0 ? (octet & 0x0F) : (octet >> 4);
    d.number += nib <= 9 ? (char)('0' + nib) : nib == 0xA ? '*' : '#';
  }
  i += (size_t)(digits + 1) / 2;
  ++i;   // protocol
  d.dcs = b[i++];
  d.validity = b[i++];
  d.udl = b[i++];
  d.tpduLength = (int)(b.size() - tpduStart);
  const std::vector<uint8_t> ud(b.begin() + (long)i, b.end());
  size_t headerOctets = 0;
  if (d.udhi) {
    headerOctets = (size_t)ud[0] + 1;
    if (ud[1] != 0x00 || ud[2] != 0x03) return d;
    d.reference = ud[3];
    d.total = ud[4];
    d.sequence = ud[5];
  }
  if (d.dcs == 0x00) {
    const size_t headerSeptets = d.udhi ? (headerOctets * 8 + 6) / 7 : 0;
    const size_t n = (size_t)d.udl - headerSeptets;
    if (ud.size() != ((headerSeptets + n) * 7 + 7) / 8) return d;
    for (size_t k = 0; k < n; ++k) {
      uint8_t septet = 0;
      for (size_t bit = 0; bit < 7; ++bit) {
        const size_t pos = (headerSeptets + k) * 7 + bit;
        if ((ud[pos / 8] >> (pos % 8)) & 1) septet |= (uint8_t)(1u << bit);
      }
      d.places.push_back(septet);
    }
  } else if (d.dcs == 0x08) {
    if ((size_t)d.udl != ud.size()) return d;
    for (size_t k = headerOctets; k + 1 < ud.size(); k += 2) d.units.push_back((uint16_t)(ud[k] * 256 + ud[k + 1]));
  } else {
    return d;
  }
  d.ok = true;
  return d;
}

// The places a text must have, written from the table of the specification for the characters the tests use.
inline std::vector<uint8_t> expectedPlaces(const std::string& s) {
  std::vector<uint8_t> out;
  for (size_t i = 0; i < s.size();) {
    uint32_t cp = (unsigned char)s[i];
    size_t n = 1;
    if (cp >= 0xC0 && cp < 0xE0) { cp = ((cp & 0x1F) << 6) | ((unsigned char)s[i + 1] & 0x3F); n = 2; }
    else if (cp >= 0xE0) { cp = ((cp & 0x0F) << 12) | (((unsigned char)s[i + 1] & 0x3F) << 6) | ((unsigned char)s[i + 2] & 0x3F); n = 3; }
    i += n;
    switch (cp) {
      case '@': out.push_back(0x00); break;
      case '$': out.push_back(0x02); break;
      case '_': out.push_back(0x11); break;
      case 0xF6: out.push_back(0x7C); break;
      case 0xE4: out.push_back(0x7B); break;
      case 0xFC: out.push_back(0x7E); break;
      case 0xDF: out.push_back(0x1E); break;
      case 0xC4: out.push_back(0x5B); break;
      case 0xD6: out.push_back(0x5C); break;
      case 0xDC: out.push_back(0x5E); break;
      case 0xE9: out.push_back(0x05); break;
      case 0x20AC: out.push_back(0x1B); out.push_back(0x65); break;
      case '[': out.push_back(0x1B); out.push_back(0x3C); break;
      case ']': out.push_back(0x1B); out.push_back(0x3E); break;
      case '{': out.push_back(0x1B); out.push_back(0x28); break;
      case '}': out.push_back(0x1B); out.push_back(0x29); break;
      case '|': out.push_back(0x1B); out.push_back(0x40); break;
      case '\\': out.push_back(0x1B); out.push_back(0x2F); break;
      case '~': out.push_back(0x1B); out.push_back(0x3D); break;
      case '^': out.push_back(0x1B); out.push_back(0x14); break;
      default: out.push_back((uint8_t)cp);   // letters, digits, space and punctuation are where ASCII has them
    }
  }
  return out;
}


// The UTF-16 units a text must have.
inline std::vector<uint16_t> expectedUnits(const std::string& s) {
  std::vector<uint16_t> out;
  for (size_t i = 0; i < s.size();) {
    uint32_t cp = (unsigned char)s[i];
    size_t n = 1;
    if (cp >= 0xF0) { cp = ((cp & 0x07) << 18) | (((unsigned char)s[i + 1] & 0x3F) << 12) | (((unsigned char)s[i + 2] & 0x3F) << 6) | ((unsigned char)s[i + 3] & 0x3F); n = 4; }
    else if (cp >= 0xE0) { cp = ((cp & 0x0F) << 12) | (((unsigned char)s[i + 1] & 0x3F) << 6) | ((unsigned char)s[i + 2] & 0x3F); n = 3; }
    else if (cp >= 0xC0) { cp = ((cp & 0x1F) << 6) | ((unsigned char)s[i + 1] & 0x3F); n = 2; }
    i += n;
    if (cp >= 0x10000) {
      cp -= 0x10000;
      out.push_back((uint16_t)(0xD800 + (cp >> 10)));
      out.push_back((uint16_t)(0xDC00 + (cp & 0x3FF)));
    } else {
      out.push_back((uint16_t)cp);
    }
  }
  return out;
}

// The text of the PDUs of one message, in order, as UTF-8; for the texts the driver tests send (ASCII and the
// characters expectedPlaces knows). An empty string when a PDU does not decode.
inline std::string textOf(const std::vector<std::string>& pdus) {
  std::string out;
  for (const std::string& hex : pdus) {
    const Decoded d = decode(hex);
    if (!d.ok) return std::string();
    if (d.dcs == 0x08) {
      for (size_t k = 0; k < d.units.size(); ++k) {
        uint32_t cp = d.units[k];
        if (cp >= 0xD800 && cp < 0xDC00 && k + 1 < d.units.size()) cp = 0x10000 + ((cp - 0xD800) << 10) + (d.units[++k] - 0xDC00);
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
        else { out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
      }
      continue;
    }
    for (size_t k = 0; k < d.places.size(); ++k) {
      const uint8_t p = d.places[k];
      switch (p) {
        case 0x00: out += '@'; break;
        case 0x02: out += '$'; break;
        case 0x11: out += '_'; break;
        case 0x7C: out += "\xC3\xB6"; break;
        case 0x7B: out += "\xC3\xA4"; break;
        case 0x7E: out += "\xC3\xBC"; break;
        case 0x1E: out += "\xC3\x9F"; break;
        case 0x1B: {
          const uint8_t e = k + 1 < d.places.size() ? d.places[++k] : 0;
          out += e == 0x3C ? '[' : e == 0x3E ? ']' : e == 0x28 ? '{' : e == 0x29 ? '}' : e == 0x40 ? '|' : e == 0x2F ? '\\' : e == 0x3D ? '~' : e == 0x14 ? '^' : '?';
          break;
        }
        default: out += (char)p;
      }
    }
  }
  return out;
}

}  // namespace smsdecode

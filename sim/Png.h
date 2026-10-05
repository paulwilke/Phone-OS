#pragma once

// A PNG writer for the one kind of picture the simulator has: black and white, one bit per pixel.
// Written here because the picture is tiny and the dependency would be larger than the writer.
//
// The pixel data is stored, not compressed (deflate "stored" blocks): a 240 x 320 panel is 9.9 KB
// that way, a screenshot needs no more, and a stored block is the one part of deflate that cannot
// be got wrong. Any viewer, browser and image library reads the result.

#include <cstddef>
#include <cstdint>
#include <vector>

#include <ui/Crc32.h>

namespace sim {

namespace png_detail {

inline void put32(std::vector<uint8_t>& out, uint32_t v) {
  out.push_back(static_cast<uint8_t>(v >> 24));
  out.push_back(static_cast<uint8_t>(v >> 16));
  out.push_back(static_cast<uint8_t>(v >> 8));
  out.push_back(static_cast<uint8_t>(v));
}

// A chunk is its length, its four-letter type, its data, and the CRC of type and data.
inline void chunk(std::vector<uint8_t>& out, const char (&type)[5], const std::vector<uint8_t>& data) {
  put32(out, static_cast<uint32_t>(data.size()));
  const size_t start = out.size();
  out.insert(out.end(), type, type + 4);
  out.insert(out.end(), data.begin(), data.end());
  put32(out, ui::crc32(out.data() + start, out.size() - start));
}

inline uint32_t adler32(const std::vector<uint8_t>& data) {
  uint32_t a = 1, b = 0;
  for (uint8_t byte : data) {
    a = (a + byte) % 65521;
    b = (b + a) % 65521;
  }
  return (b << 16) | a;
}

}  // namespace png_detail

// `packed` is a picture as ui::Display::drawBitmap reads it: rows padded to whole bytes, the
// leftmost pixel in the top bit, a set bit is a black pixel. The size must be at least 1 x 1.
inline std::vector<uint8_t> encodePng(const std::vector<uint8_t>& packed, int width, int height) {
  const size_t stride = (static_cast<size_t>(width) + 7) / 8;

  // Every row starts with its filter type, 0 (none). PNG counts 0 as black, the bitmap 1.
  std::vector<uint8_t> raw;
  raw.reserve((stride + 1) * static_cast<size_t>(height));
  for (int y = 0; y < height; ++y) {
    raw.push_back(0);
    for (size_t i = 0; i < stride; ++i) raw.push_back(static_cast<uint8_t>(~packed[static_cast<size_t>(y) * stride + i]));
  }

  // zlib: a two-byte header, stored blocks of at most 65535 bytes, and the Adler-32 of the data.
  std::vector<uint8_t> zlib = {0x78, 0x01};
  size_t at = 0;
  do {
    const size_t n = raw.size() - at < 65535 ? raw.size() - at : 65535;
    const bool last = at + n == raw.size();
    zlib.push_back(last ? 1 : 0);
    zlib.push_back(static_cast<uint8_t>(n));
    zlib.push_back(static_cast<uint8_t>(n >> 8));
    zlib.push_back(static_cast<uint8_t>(~n));
    zlib.push_back(static_cast<uint8_t>(~n >> 8));
    zlib.insert(zlib.end(), raw.begin() + static_cast<long>(at), raw.begin() + static_cast<long>(at + n));
    at += n;
  } while (at < raw.size());
  png_detail::put32(zlib, png_detail::adler32(raw));

  std::vector<uint8_t> out = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
  std::vector<uint8_t> header;
  png_detail::put32(header, static_cast<uint32_t>(width));
  png_detail::put32(header, static_cast<uint32_t>(height));
  header.insert(header.end(), {1, 0, 0, 0, 0});  // 1 bit deep, greyscale, deflate, no filter scheme, no interlace
  png_detail::chunk(out, "IHDR", header);
  png_detail::chunk(out, "IDAT", zlib);
  png_detail::chunk(out, "IEND", {});
  return out;
}

}  // namespace sim

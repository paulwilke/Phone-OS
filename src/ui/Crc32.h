#pragma once

// CRC-32 (the common polynomial 0xEDB88320). The e-ink logic compares the CRC of the
// frame it is about to show with the one of the frame on the panel: an unchanged picture
// costs no refresh. One definition, so the driver and the tests agree.

#include <cstddef>
#include <cstdint>

namespace ui {

inline uint32_t crc32(const uint8_t* data, size_t size, uint32_t crc = 0) {
  crc = ~crc;
  for (size_t i = 0; i < size; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

}  // namespace ui

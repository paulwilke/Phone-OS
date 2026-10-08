#include "Zip.h"

#include "../app/Crc32.h"

namespace zip {
namespace {

constexpr uint32_t kLocalHeader = 0x04034b50;
constexpr uint32_t kCentralHeader = 0x02014b50;
constexpr uint32_t kEndRecord = 0x06054b50;
constexpr uint16_t kVersion = 10;  // 1.0: stored entries, nothing newer is used

void put16(std::string& out, uint16_t v) {
  out += static_cast<char>(v & 0xFF);
  out += static_cast<char>(v >> 8);
}
void put32(std::string& out, uint32_t v) {
  put16(out, static_cast<uint16_t>(v & 0xFFFF));
  put16(out, static_cast<uint16_t>(v >> 16));
}
uint16_t get16(const unsigned char* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
uint32_t get32(const unsigned char* p) { return get16(p) | (static_cast<uint32_t>(get16(p + 2)) << 16); }

// The MS-DOS date and time a ZIP entry carries: local time is unknown here, so UTC; two-second steps.
void dosDateTime(uint32_t epoch, uint16_t& date, uint16_t& time) {
  if (epoch < 315532800u) epoch = 315532800u;  // 1980-01-01, the earliest a ZIP date can say
  const uint32_t days = epoch / 86400u, secs = epoch % 86400u;
  // Civil date from days since 1970-01-01 (Howard Hinnant's algorithm).
  const int64_t z = static_cast<int64_t>(days) + 719468;
  const int64_t era = z / 146097;
  const int64_t doe = z - era * 146097;
  const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const int64_t mp = (5 * doy + 2) / 153;
  const int day = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
  const int month = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
  const int year = static_cast<int>(yoe + era * 400 + (month <= 2 ? 1 : 0));
  date = static_cast<uint16_t>(((year - 1980) << 9) | (month << 5) | day);
  time = static_cast<uint16_t>(((secs / 3600) << 11) | (((secs / 60) % 60) << 5) | ((secs % 60) / 2));
}

}  // namespace

std::string write(const std::vector<Entry>& entries, uint32_t epochUtc) {
  uint16_t date = 0, time = 0;
  dosDateTime(epochUtc, date, time);
  std::string out, central;
  for (const Entry& e : entries) {
    const uint32_t crc = checksum::crc32(reinterpret_cast<const uint8_t*>(e.data.data()), e.data.size());
    const uint32_t offset = static_cast<uint32_t>(out.size());
    const auto common = [&](std::string& to) {
      put16(to, kVersion);   // version needed
      put16(to, 0);          // flags: none
      put16(to, 0);          // method: stored
      put16(to, time);
      put16(to, date);
      put32(to, crc);
      put32(to, static_cast<uint32_t>(e.data.size()));
      put32(to, static_cast<uint32_t>(e.data.size()));
      put16(to, static_cast<uint16_t>(e.name.size()));
      put16(to, 0);          // extra field
    };
    put32(out, kLocalHeader);
    common(out);
    out += e.name;
    out += e.data;

    put32(central, kCentralHeader);
    put16(central, kVersion);  // made by: MS-DOS, 1.0
    common(central);
    put16(central, 0);         // comment
    put16(central, 0);         // disk
    put16(central, 0);         // internal attributes
    put32(central, 0);         // external attributes
    put32(central, offset);
    central += e.name;
  }
  const uint32_t centralOffset = static_cast<uint32_t>(out.size());
  out += central;
  put32(out, kEndRecord);
  put16(out, 0);
  put16(out, 0);
  put16(out, static_cast<uint16_t>(entries.size()));
  put16(out, static_cast<uint16_t>(entries.size()));
  put32(out, static_cast<uint32_t>(central.size()));
  put32(out, centralOffset);
  put16(out, 0);
  return out;
}

bool read(const char* data, size_t length, size_t maxEntrySize, size_t maxEntries, std::vector<Entry>& out,
          std::string& why) {
  out.clear();
  const auto* p = reinterpret_cast<const unsigned char*>(data);
  size_t at = 0;
  std::vector<Entry> found;
  while (true) {
    if (at + 4 > length) {
      why = found.empty() ? "Not a ZIP archive" : "The archive is cut off";
      return false;
    }
    const uint32_t signature = get32(p + at);
    if (signature == kCentralHeader || signature == kEndRecord) break;
    if (signature != kLocalHeader) {
      why = "Not a ZIP archive";
      return false;
    }
    if (at + 30 > length) {
      why = "The archive is cut off";
      return false;
    }
    const uint16_t flags = get16(p + at + 6), method = get16(p + at + 8);
    const uint32_t crc = get32(p + at + 14), packed = get32(p + at + 18), size = get32(p + at + 22);
    const uint16_t nameLength = get16(p + at + 26), extraLength = get16(p + at + 28);
    // Bit 0 is encryption, bit 3 a data descriptor after the data (sizes unknown in the header); bit 11 only
    // says the name is UTF-8, which is harmless.
    if ((flags & 0x0009) != 0 || method != 0 || packed != size) {
      why = "Only an archive saved by the device can be restored (this one is compressed or encrypted)";
      return false;
    }
    if (size > maxEntrySize) {
      why = "A file in the archive is too large";
      return false;
    }
    if (found.size() >= maxEntries) {
      why = "Too many files in the archive";
      return false;
    }
    const size_t nameAt = at + 30, dataAt = nameAt + nameLength + extraLength;
    if (dataAt + size > length) {
      why = "The archive is cut off";
      return false;
    }
    Entry e;
    e.name.assign(data + nameAt, nameLength);
    e.data.assign(data + dataAt, size);
    if (checksum::crc32(reinterpret_cast<const uint8_t*>(e.data.data()), e.data.size()) != crc) {
      why = "The archive is damaged (checksum)";
      return false;
    }
    found.push_back(std::move(e));
    at = dataAt + size;
  }
  if (found.empty()) {
    why = "The archive is empty";
    return false;
  }
  out = std::move(found);
  return true;
}

}  // namespace zip

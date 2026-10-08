#pragma once

// The backup archive of the parents' page: a ZIP file whose entries are stored, not compressed.
//
// Why ZIP and stored: the files go out and come back byte for byte, so no character of a file can break
// the format (the JSON bundle of 4.0 had to escape them); a Mac opens a ZIP file with a double click; the
// device writes and reads it with nothing but a CRC-32 (app/Crc32.h), and compression would need a library
// the build does not have. A stored ZIP adds about 120 bytes per file, much less than tar (a 512-byte header
// and padding to 512 bytes per file, 1024 bytes at the end), which matters because the archive must come back
// in one request body of at most 8192 bytes (http::kMaxBody).
//
// Reading is strict: local file headers one after another, each stored (method 0), without encryption and
// without a data descriptor, its size within the limit and its CRC right; then the central directory or the
// end record. Anything else (a compressed entry, as the Mac's own "Compress" makes) is refused with a reason
// and nothing of it is returned.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zip {

struct Entry {
  std::string name;  // the path inside the archive, "config/settings.toml"
  std::string data;  // the bytes of the file
};

// The archive of `entries`, dated `epochUtc` (0: 1980-01-01, the earliest a ZIP date can say).
std::string write(const std::vector<Entry>& entries, uint32_t epochUtc);

// The entries of `data`. False with `why` (English, for the page and the log) when the archive is not one
// this device wrote: not a ZIP file, a compressed or encrypted entry, an entry larger than `maxEntrySize`,
// more than `maxEntries`, a wrong CRC, or a cut file.
bool read(const char* data, size_t length, size_t maxEntrySize, size_t maxEntries, std::vector<Entry>& out,
          std::string& why);

}  // namespace zip

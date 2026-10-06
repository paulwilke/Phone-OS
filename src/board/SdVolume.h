#pragma once

// The seam below everything that lives on the SD card: one mounted FAT volume, as plain file operations. The
// device implements it with the Arduino SD library (device/Esp32SdVolume.h); the tests with an in-memory
// volume that can be told to run full, to fail halfway, to lose power at a chosen step or to be pulled out
// (testing/fakes/FakeSdVolume.h). Everything above it (the safe write, recovery after a power loss, the log
// that does not fill the card, the missing card) is plain C++ and tested on the host.
//
// Paths are absolute and arrive already checked by SafePath; names are compared without regard to case
// (FAT). `rename` does not replace: FAT refuses a target that exists, which is why a safe write removes the
// old file first. Every call returns instead of hanging when the card is gone.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "../hal/ConfigStore.h"

namespace board {

class SdVolume {
public:
  virtual ~SdVolume() = default;

  // Asks the card at this bus clock (the card hangs on the bus that the display and the radio share, and a
  // long line may not stand 4 MHz). False when there is no card or no readable FAT volume: FAT32 with an MBR
  // partition table is read, a card with a GPT table (the default of macOS) is not.
  virtual bool mount(uint32_t clockHz) = 0;
  // Releases the volume so that a further mount() asks the card again; without it the library keeps its
  // failure state and the second attempt fails for the same reason as the first.
  virtual void unmount() = 0;
  virtual bool mounted() const = 0;

  // Bytes free on the volume; -1 when unknown.
  virtual int64_t freeBytes() = 0;

  virtual bool exists(const std::string& path) = 0;
  virtual bool isDirectory(const std::string& path) = 0;
  // Size of a file; -1 when there is none or the path is a directory.
  virtual int64_t fileSize(const std::string& path) = 0;
  // Reads up to `len` bytes from `offset`; the number read (0 at the end), -1 on failure.
  virtual int readAt(const std::string& path, uint64_t offset, uint8_t* out, size_t len) = 0;
  // Creates the file or empties it, and writes all of `data`. False on any failure, a short write included;
  // a failed write may leave part of the data in the file.
  virtual bool writeFile(const std::string& path, const uint8_t* data, size_t len) = 0;
  virtual bool appendFile(const std::string& path, const uint8_t* data, size_t len) = 0;
  virtual bool rename(const std::string& from, const std::string& to) = 0;
  virtual bool removeFile(const std::string& path) = 0;
  // One level only: the parent must exist.
  virtual bool makeDirectory(const std::string& path) = 0;
  // The entries of a directory, with the name within it; false when it does not exist.
  virtual bool listDirectory(const std::string& path, std::vector<FileEntry>& out) = 0;
};

}  // namespace board

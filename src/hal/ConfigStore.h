#pragma once

// Capability: read, write, list and delete the files on the card. The web page keeps
// the configuration, the logs and arbitrary files through it; whether an SD card, a
// directory of the simulator or a map in a test stands behind it, it does not know.
//
// Paths are absolute and come from outside ("/config/settings.toml"). **Every
// implementation checks them with the rules of SafePath before it touches anything**
// and refuses a path that fails (read and size report "no such file", the others
// return false). Names are compared without regard to case: the card is FAT.
//
// Scratch files of an implementation (the file a safe write goes through, SafePath's
// kTempSuffix) are never visible: not in list(), not to read, not to size.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

struct FileEntry {
  std::string name;  // the name within the directory, without the path
  size_t size = 0;
  bool isDir = false;
};

class ConfigStore {
public:
  virtual ~ConfigStore() = default;

  // Reads the file from the start, at most `maxBytes`. False when there is no such
  // file (`out` is cleared); true for a file that exists, even an empty one. A larger
  // file is cut; fileSize() says how large it is.
  virtual bool read(const std::string& path, std::string& out, size_t maxBytes) = 0;
  // The **end** of the file, for logs: what just went wrong is at the back. The last
  // `maxBytes` bytes, or the whole file if it is smaller.
  virtual bool readTail(const std::string& path, std::string& out, size_t maxBytes) = 0;
  // The bytes from `offset` on, at most `maxBytes`: a long log is read in pieces, one after the other. An offset
  // at or past the end gives an empty `out` and true, because the file exists; fileSize() says where the end is.
  virtual bool readAt(const std::string& path, uint64_t offset, std::string& out, size_t maxBytes) = 0;
  // Size in bytes; -1 when there is no such file.
  virtual int64_t fileSize(const std::string& path) = 0;

  // Replaces the file completely. **A write that fails — a full card, a card pulled
  // out, a power loss — leaves the old content intact**: the data goes to a scratch
  // file first and replaces the old file only when it is complete. Missing directories
  // of one level are created. Empty content creates an empty file. False on any
  // failure; the old file is unchanged then.
  virtual bool write(const std::string& path, std::string_view data) = 0;
  // Deletes a file. True when a file was removed; false when there was none or it could
  // not be removed.
  virtual bool remove(const std::string& path) = 0;
  // The entries of a directory in `out`, in no particular order. False when the
  // directory does not exist.
  virtual bool list(const std::string& dir, std::vector<FileEntry>& out) = 0;
};

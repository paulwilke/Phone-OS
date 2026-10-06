#pragma once

// The SD volume through the Arduino SD library on the board's shared SPI bus. FAT32 with an MBR partition table
// is read; a card with a GPT table (the default of macOS) is not, and mounting fails with "no valid FAT
// volume" although the card answers perfectly. Everything above this class is board/Sd*.h and is tested on
// the host; this class is only the translation into the library's calls.

#include <SD.h>

#include <cstring>

#include <board/PinMap.h>
#include <board/SdVolume.h>

#include "Esp32Spi.h"

class Esp32SdVolume : public board::SdVolume {
public:
  bool mount(uint32_t clockHz) override {
    mounted_ = SD.begin(board::pin::kSdCs, boardSpi(), clockHz);
    return mounted_;
  }
  void unmount() override {
    SD.end();
    mounted_ = false;
  }
  bool mounted() const override { return mounted_ && SD.cardType() != CARD_NONE; }

  int64_t freeBytes() override {
    if (!mounted_) return -1;
    return (int64_t)SD.totalBytes() - (int64_t)SD.usedBytes();
  }

  bool exists(const std::string& path) override { return mounted_ && SD.exists(path.c_str()); }
  bool isDirectory(const std::string& path) override {
    if (!mounted_) return false;
    File f = SD.open(path.c_str(), FILE_READ);
    const bool dir = f && f.isDirectory();
    if (f) f.close();
    return dir;
  }
  int64_t fileSize(const std::string& path) override {
    if (!mounted_) return -1;
    File f = SD.open(path.c_str(), FILE_READ);
    if (!f || f.isDirectory()) return -1;
    const int64_t n = (int64_t)f.size();
    f.close();
    return n;
  }
  int readAt(const std::string& path, uint64_t offset, uint8_t* out, size_t len) override {
    if (!mounted_) return -1;
    File f = SD.open(path.c_str(), FILE_READ);
    if (!f || f.isDirectory()) return -1;
    if (!f.seek((uint32_t)offset)) {
      f.close();
      return offset >= f.size() ? 0 : -1;
    }
    const int n = (int)f.read(out, len);
    f.close();
    return n;
  }
  bool writeFile(const std::string& path, const uint8_t* data, size_t len) override {
    if (!mounted_) return false;
    File f = SD.open(path.c_str(), FILE_WRITE);  // creates or empties
    if (!f) return false;
    const size_t written = len ? f.write(data, len) : 0;
    f.flush();
    f.close();
    return written == len;
  }
  bool appendFile(const std::string& path, const uint8_t* data, size_t len) override {
    if (!mounted_) return false;
    File f = SD.open(path.c_str(), FILE_APPEND);
    if (!f) return false;
    const size_t written = f.write(data, len);
    f.close();
    return written == len;
  }
  bool rename(const std::string& from, const std::string& to) override { return mounted_ && SD.rename(from.c_str(), to.c_str()); }
  bool removeFile(const std::string& path) override { return mounted_ && SD.remove(path.c_str()); }
  bool makeDirectory(const std::string& path) override { return mounted_ && SD.mkdir(path.c_str()); }
  bool listDirectory(const std::string& path, std::vector<FileEntry>& out) override {
    out.clear();
    if (!mounted_) return false;
    File d = SD.open(path.c_str());
    if (!d || !d.isDirectory()) return false;
    for (File f = d.openNextFile(); f; f = d.openNextFile()) {
      const char* name = f.name();
      const char* slash = std::strrchr(name, '/');  // the full path, depending on the core
      FileEntry e;
      e.name = slash ? slash + 1 : name;
      e.isDir = f.isDirectory();
      e.size = e.isDir ? 0 : (size_t)f.size();
      f.close();
      out.push_back(e);
    }
    d.close();
    return true;
  }

private:
  bool mounted_ = false;
};

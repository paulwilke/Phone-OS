#pragma once

// The ConfigStore on the SD card. The promises of the capability (src/hal/ConfigStore.h) are kept here on top of
// the plain volume:
//
//   * Every path is checked with SafePath before the card is touched; unsafe ones are "no such file" to
//     reading and false to the rest. Names are compared without regard to case (FAT).
//   * **A write that fails leaves the old content intact.** The data goes to a scratch file next to the target
//     (the name plus ".tmp", reserved by SafePath), is checked to be complete, and only then replaces the old
//     file: the old file is removed and the scratch file renamed (FAT does not rename over an existing
//     file). A failure before the removal changes nothing. A power loss between the removal and the rename
//     leaves the complete new content in the scratch file and no target: recover() at the next start
//     renames it into place; a scratch file next to an existing target is leftover of an unfinished write
//     and is deleted.
//   * A write does not start when the card has less room than the data and a reserve of 8 KB (the cluster
//     and the directory entry): a full card refuses before it has touched anything.
//   * Scratch files are never visible, and neither are entries whose names SafePath would refuse (a path
//     the web page could not operate on anyway).
//   * Missing directories of one level are created.
//   * Reading takes at most the bytes asked for, in blocks of 1 KB; a card that fails in the middle is
//     "no such file" and not half a file.
//
// Without a card every call fails (read and size say "no such file"); three failures in a row make the card
// count as pulled out (SdCard).

#include "../hal/ConfigStore.h"
#include "SdCard.h"

namespace board {

constexpr int64_t kSdWriteReserveBytes = 8192;
constexpr size_t kSdReadBlock = 1024;

class SdConfigStore : public ConfigStore {
public:
  explicit SdConfigStore(SdCard& card) : card_(card) {}

  // After a power loss: finishes or removes what an interrupted write left behind, in the root and in each
  // directory below it. Returns the number of scratch files dealt with.
  int recover();

  bool read(const std::string& path, std::string& out, size_t maxBytes) override;
  bool readTail(const std::string& path, std::string& out, size_t maxBytes) override;
  int64_t fileSize(const std::string& path) override;
  bool write(const std::string& path, std::string_view data) override;
  bool remove(const std::string& path) override;
  bool list(const std::string& dir, std::vector<FileEntry>& out) override;

private:
  bool readRange(const std::string& path, bool fromEnd, std::string& out, size_t maxBytes);
  bool ensureParent(const std::string& path);
  int recoverIn(const std::string& dir);

  SdCard& card_;
};

}  // namespace board

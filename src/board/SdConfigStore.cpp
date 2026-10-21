#include "SdConfigStore.h"

#include <algorithm>
#include <cstring>

#include "../app/SafePath.h"

namespace board {

namespace {
bool isScratch(const std::string& name) { return safepath::endsWith(name.c_str(), safepath::kTempSuffix); }
std::string scratchOf(const std::string& path) { return path + safepath::kTempSuffix; }
std::string withoutSlash(const std::string& dir) { return dir.size() > 1 && dir.back() == '/' ? dir.substr(0, dir.size() - 1) : dir; }
}  // namespace

bool SdConfigStore::readRange(const std::string& path, From from, uint64_t start, std::string& out, size_t maxBytes) {
  out.clear();
  if (!card_.ready() || !safepath::valid(path.c_str())) return false;
  SdVolume& v = card_.volume();
  const int64_t size = v.fileSize(path);
  if (size < 0) return false;
  const uint64_t total = (uint64_t)size;
  const uint64_t begin = from == From::Offset ? std::min(start, total) : 0;
  const uint64_t want = std::min<uint64_t>(total - begin, maxBytes);
  uint64_t offset = from == From::End ? total - want : begin;
  uint8_t block[kSdReadBlock];
  while (out.size() < want) {
    const size_t chunk = (size_t)std::min<uint64_t>(kSdReadBlock, want - out.size());
    const int n = v.readAt(path, offset, block, chunk);
    if (n <= 0) {  // a card that fails in the middle: no half file
      out.clear();
      card_.operationFailed();
      return false;
    }
    out.append(reinterpret_cast<const char*>(block), (size_t)n);
    offset += (uint64_t)n;
  }
  card_.operationSucceeded();
  return true;
}

bool SdConfigStore::read(const std::string& path, std::string& out, size_t maxBytes) { return readRange(path, From::Start, 0, out, maxBytes); }
bool SdConfigStore::readTail(const std::string& path, std::string& out, size_t maxBytes) { return readRange(path, From::End, 0, out, maxBytes); }
bool SdConfigStore::readAt(const std::string& path, uint64_t offset, std::string& out, size_t maxBytes) {
  return readRange(path, From::Offset, offset, out, maxBytes);
}

int64_t SdConfigStore::fileSize(const std::string& path) {
  if (!card_.ready() || !safepath::valid(path.c_str())) return -1;
  return card_.volume().fileSize(path);
}

bool SdConfigStore::ensureParent(const std::string& path) {
  const size_t slash = path.rfind('/');
  if (slash == 0) return true;
  const std::string parent = path.substr(0, slash);
  SdVolume& v = card_.volume();
  if (v.isDirectory(parent)) return true;
  // One level only: the parent of the parent must be there.
  const size_t up = parent.rfind('/');
  if (up != 0 && !v.isDirectory(parent.substr(0, up))) return false;
  return v.makeDirectory(parent);
}

bool SdConfigStore::write(const std::string& path, std::string_view data) {
  if (!card_.ready() || !safepath::valid(path.c_str()) || path.back() == '/') return false;
  SdVolume& v = card_.volume();
  if (v.isDirectory(path)) return false;
  const int64_t free = v.freeBytes();
  if (free >= 0 && free < (int64_t)data.size() + kSdWriteReserveBytes) return false;  // nothing touched
  if (!ensureParent(path)) {
    card_.operationFailed();
    return false;
  }
  const std::string scratch = scratchOf(path);
  if (v.exists(scratch)) v.removeFile(scratch);
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data.data());
  if (!v.writeFile(scratch, bytes, data.size()) || v.fileSize(scratch) != (int64_t)data.size()) {
    v.removeFile(scratch);  // best effort; a scratch file that stays is invisible and overwritten next time
    card_.operationFailed();
    return false;
  }
  if (v.exists(path) && !v.removeFile(path)) {  // the old file stays whole
    v.removeFile(scratch);
    card_.operationFailed();
    return false;
  }
  if (!v.rename(scratch, path) && !v.rename(scratch, path)) {
    // The old file is gone and the new one is complete under its scratch name: recover() puts it right at the
    // next start. The write is reported as failed, because the name is not in place yet.
    card_.operationFailed();
    return false;
  }
  card_.operationSucceeded();
  return true;
}

bool SdConfigStore::remove(const std::string& path) {
  if (!card_.ready() || !safepath::valid(path.c_str())) return false;
  SdVolume& v = card_.volume();
  if (!v.exists(path) || v.isDirectory(path)) return false;  // directories are not deleted through this interface
  const bool ok = v.removeFile(path);
  if (ok) card_.operationSucceeded();
  else card_.operationFailed();
  return ok;
}

bool SdConfigStore::list(const std::string& dir, std::vector<FileEntry>& out) {
  out.clear();
  if (!card_.ready() || !safepath::valid(dir.c_str())) return false;
  std::vector<FileEntry> all;
  if (!card_.volume().listDirectory(withoutSlash(dir), all)) return false;
  for (const FileEntry& e : all) {
    if (isScratch(e.name)) continue;
    const std::string asPath = "/" + e.name;
    if (!safepath::valid(asPath.c_str())) continue;  // a name the page could not use
    out.push_back(e);
  }
  return true;
}

int SdConfigStore::recoverIn(const std::string& dir) {
  SdVolume& v = card_.volume();
  std::vector<FileEntry> entries;
  if (!v.listDirectory(dir, entries)) return 0;
  int handled = 0;
  const std::string prefix = dir == "/" ? "/" : dir + "/";
  for (const FileEntry& e : entries) {
    if (e.isDir || !isScratch(e.name)) continue;
    const std::string scratch = prefix + e.name;
    const std::string target = scratch.substr(0, scratch.size() - std::strlen(safepath::kTempSuffix));
    // A scratch file next to its target is an unfinished write (the old file is whole); one without a target is
    // a finished write whose rename did not happen, because the old file is removed only after the new one is
    // complete.
    if (v.exists(target)) v.removeFile(scratch);
    else v.rename(scratch, target);
    ++handled;
  }
  return handled;
}

int SdConfigStore::recover() {
  if (!card_.ready()) return 0;
  int handled = recoverIn("/");
  std::vector<FileEntry> root;
  if (card_.volume().listDirectory("/", root))
    for (const FileEntry& e : root)
      if (e.isDir) handled += recoverIn("/" + e.name);
  return handled;
}

}  // namespace board

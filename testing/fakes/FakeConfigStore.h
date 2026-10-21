#pragma once

// An in-memory file store that keeps the promises of the contract, including the hard
// one: a write that fails leaves the old content intact.
//
// Writes go through a scratch file (SafePath's kTempSuffix) like the real ones, so a
// failure can leave a half-written scratch file behind; it is invisible to the
// interface, as the contract says. Names are compared without regard to case (FAT).
//
// Scripting:
//   failNextWrite()      the next write fails halfway: half of the data reaches the
//                        scratch file, the old file stays
//   failAllWrites(bool)  every write fails before it starts (a full or missing card)
//   put(path, data)      sets a file without any checks, for the start of a test
// Reading back: scratchFileCount() (hidden scratch files in the store).

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <app/SafePath.h>
#include <hal/ConfigStore.h>

class FakeConfigStore : public ConfigStore {
public:
  bool read(const std::string& path, std::string& out, size_t maxBytes) override {
    out.clear();
    const File* f = file(path);
    if (!f) return false;
    out = f->data.substr(0, maxBytes);
    return true;
  }
  bool readTail(const std::string& path, std::string& out, size_t maxBytes) override {
    out.clear();
    const File* f = file(path);
    if (!f) return false;
    out = f->data.size() > maxBytes ? f->data.substr(f->data.size() - maxBytes) : f->data;
    return true;
  }
  bool readAt(const std::string& path, uint64_t offset, std::string& out, size_t maxBytes) override {
    out.clear();
    const File* f = file(path);
    if (!f) return false;
    if (offset < f->data.size()) out = f->data.substr((size_t)offset, maxBytes);
    return true;
  }
  int64_t fileSize(const std::string& path) override {
    const File* f = file(path);
    return f ? (int64_t)f->data.size() : -1;
  }

  bool write(const std::string& path, std::string_view data) override {
    if (!safepath::valid(path.c_str()) || path.back() == '/') return false;
    if (failAll_) return false;
    if (!makeParent(path)) return false;
    const std::string scratch = key(path + safepath::kTempSuffix);
    if (failNext_) {
      failNext_ = false;
      files_[scratch] = {path + safepath::kTempSuffix, std::string(data.substr(0, data.size() / 2))};
      return false;
    }
    // The scratch file is complete, then it replaces the old file in one step.
    files_.erase(scratch);
    files_[key(path)] = {path, std::string(data)};
    return true;
  }
  bool remove(const std::string& path) override {
    if (!safepath::valid(path.c_str())) return false;
    return files_.erase(key(path)) > 0;
  }
  bool list(const std::string& dir, std::vector<FileEntry>& out) override {
    out.clear();
    if (!safepath::valid(dir.c_str())) return false;
    std::string base = dir;
    if (base.back() != '/') base += '/';
    if (base != "/" && !dirs_.count(key(base.substr(0, base.size() - 1)))) return false;
    const std::string lowerBase = key(base);
    std::set<std::string> subdirs;
    for (const auto& [k, f] : files_) {
      if (isScratch(f.path) || k.compare(0, base.size(), lowerBase) != 0) continue;
      const std::string rest = f.path.substr(base.size());
      const size_t slash = rest.find('/');
      if (slash == std::string::npos) out.push_back({rest, f.data.size(), false});
      else subdirs.insert(rest.substr(0, slash));
    }
    for (const std::string& d : subdirs) out.push_back({d, 0, true});
    for (const auto& [lowerDir, original] : dirs_) {  // directories without files
      if (lowerDir.size() > base.size() && lowerDir.compare(0, base.size(), lowerBase) == 0 &&
          lowerDir.find('/', base.size()) == std::string::npos && !subdirs.count(original.substr(base.size()))) {
        out.push_back({original.substr(base.size()), 0, true});
      }
    }
    return true;
  }

  // Scripting.
  void failNextWrite() { failNext_ = true; }
  void failAllWrites(bool fail) { failAll_ = fail; }
  void put(const std::string& path, const std::string& data) {
    makeParent(path);
    files_[key(path)] = {path, data};
  }
  size_t scratchFileCount() const {
    size_t n = 0;
    for (const auto& kv : files_) n += isScratch(kv.second.path);
    return n;
  }

private:
  struct File {
    std::string path;  // as it was written, for listing
    std::string data;
  };

  static std::string key(std::string s) {
    for (char& c : s) {
      if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    }
    return s;
  }
  static bool isScratch(const std::string& path) { return safepath::endsWith(path.c_str(), safepath::kTempSuffix); }

  const File* file(const std::string& path) const {
    if (!safepath::valid(path.c_str())) return nullptr;
    const auto it = files_.find(key(path));
    return it == files_.end() ? nullptr : &it->second;
  }
  // Creates the directory a file lives in when its own parent exists: one level.
  bool makeParent(const std::string& path) {
    const size_t slash = path.rfind('/');
    if (slash == 0) return true;
    const std::string parent = path.substr(0, slash);
    if (dirs_.count(key(parent))) return true;
    const size_t up = parent.rfind('/');
    if (up != 0 && !dirs_.count(key(parent.substr(0, up)))) return false;
    dirs_[key(parent)] = parent;
    return true;
  }

  std::map<std::string, File> files_;
  std::map<std::string, std::string> dirs_;  // lower case -> as written
  bool failNext_ = false;
  bool failAll_ = false;
};

#pragma once

// An in-memory FAT volume with everything that goes wrong on a real card.
//
// The card:    removeCard() / insertCard()            no card, a card (every call fails while it is out)
//              mountsOnlyAtOrBelow(hz)                 a card that stands only a slow bus clock (default: any)
//              refuseMount(bool)                       a card that is there and cannot be read (a GPT table)
//              setCapacity(bytes)                      the volume's size; free space is that minus what files hold
// A write:     failNextWrite()                         the next writeFile/appendFile takes half of the data and fails
//              failAllWrites(bool)                     every write fails before it starts (a write-protected card)
//              failRenames(bool), failRemoves(bool)    renames/removes refuse
//              failReads(bool)                         every readAt fails (a card that gave up in the middle)
//              failReadsAt(offset, times)              the next `times` readAt calls at that offset fail, then it reads
//                                                      (a block whose open fails now and then); they count in readCount()
// The failure state: a mount that failed leaves the volume stuck until unmount(), like the library.
// Power loss:  loseCardAfter(n)                        the n-th mutating operation from now is the last one that happens;
//                                                      after it every call fails until powerBack()
//              powerBack()                             the card works again, with what it held
// Reading back: mountAttempts() (the clock of each), operations() (the mutating calls in order, "write /a" …),
// readCount() (how many readAt calls), file(path) (the bytes or a missing marker), has(path).

#include <algorithm>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <board/SdVolume.h>

class FakeSdVolume : public board::SdVolume {
public:
  bool mount(uint32_t clockHz) override {
    attempts_.push_back(clockHz);
    mounted_ = false;
    if (stuck_) return false;
    if (!present_ || refuseMount_ || clockHz > maxClock_) {
      stuck_ = true;
      return false;
    }
    mounted_ = true;
    return true;
  }
  // Like the library: a volume that was not released keeps its failure state and fails the next mount for the
  // same reason as the last one (a card that was "not found" once stays "not found" until it is released).
  void unmount() override {
    mounted_ = false;
    stuck_ = false;
  }
  bool mounted() const override { return mounted_ && present_; }

  int64_t freeBytes() override {
    if (!usable()) return -1;
    int64_t used = 0;
    for (const auto& kv : nodes_) used += (int64_t)kv.second.data.size();
    return capacity_ - used;
  }

  bool exists(const std::string& path) override { return usable() && nodes_.count(key(path)) > 0; }
  bool isDirectory(const std::string& path) override {
    if (!usable()) return false;
    if (path == "/") return true;
    const auto it = nodes_.find(key(path));
    return it != nodes_.end() && it->second.isDir;
  }
  int64_t fileSize(const std::string& path) override {
    if (!usable()) return -1;
    const auto it = nodes_.find(key(path));
    return it == nodes_.end() || it->second.isDir ? -1 : (int64_t)it->second.data.size();
  }
  int readAt(const std::string& path, uint64_t offset, uint8_t* out, size_t len) override {
    if (!usable() || failReads_) return -1;
    const auto it = nodes_.find(key(path));
    if (it == nodes_.end() || it->second.isDir) return -1;
    ++reads_;
    const auto failing = readFailures_.find(offset);
    if (failing != readFailures_.end() && failing->second > 0) {
      --failing->second;
      return -1;
    }
    const std::string& d = it->second.data;
    if (offset >= d.size()) return 0;
    const size_t n = std::min(len, d.size() - (size_t)offset);
    std::memcpy(out, d.data() + offset, n);
    return (int)n;
  }
  bool writeFile(const std::string& path, const uint8_t* data, size_t len) override {
    if (!mutate("write " + path)) return false;
    if (failAll_ || !parentExists(path)) return false;
    Node& n = nodes_[key(path)];
    if (n.isDir) return false;
    n.path = path;
    if (failNext_) {
      failNext_ = false;
      n.data.assign((const char*)data, len / 2);
      return false;
    }
    if ((int64_t)len > capacity_ - usedExcept(key(path))) {
      n.data.assign((const char*)data, (size_t)std::max<int64_t>(0, capacity_ - usedExcept(key(path))));
      return false;
    }
    n.data.assign((const char*)data, len);
    return true;
  }
  bool appendFile(const std::string& path, const uint8_t* data, size_t len) override {
    if (!mutate("append " + path)) return false;
    if (failAll_ || !parentExists(path)) return false;
    Node& n = nodes_[key(path)];
    if (n.isDir) return false;
    n.path = path;
    if (failNext_) {
      failNext_ = false;
      n.data.append((const char*)data, len / 2);
      return false;
    }
    if ((int64_t)len > capacity_ - usedExcept("") ) return false;
    n.data.append((const char*)data, len);
    return true;
  }
  bool rename(const std::string& from, const std::string& to) override {
    if (!mutate("rename " + from + " " + to)) return false;
    if (failRenames_) return false;
    const auto it = nodes_.find(key(from));
    if (it == nodes_.end() || nodes_.count(key(to)) || !parentExists(to)) return false;
    Node n = it->second;
    n.path = to;
    nodes_.erase(it);
    nodes_[key(to)] = n;
    return true;
  }
  bool removeFile(const std::string& path) override {
    if (!mutate("remove " + path)) return false;
    if (failRemoves_) return false;
    const auto it = nodes_.find(key(path));
    if (it == nodes_.end() || it->second.isDir) return false;
    nodes_.erase(it);
    return true;
  }
  bool makeDirectory(const std::string& path) override {
    if (!mutate("mkdir " + path)) return false;
    if (nodes_.count(key(path)) || !parentExists(path)) return false;
    nodes_[key(path)] = {path, "", true};
    return true;
  }
  bool listDirectory(const std::string& path, std::vector<FileEntry>& out) override {
    out.clear();
    if (!usable()) return false;
    std::string base = path;
    if (base.back() != '/') base += '/';
    if (base != "/" && !isDirectory(base.substr(0, base.size() - 1))) return false;
    const std::string lowerBase = key(base);
    for (const auto& kv : nodes_) {
      if (kv.first.compare(0, lowerBase.size(), lowerBase) != 0) continue;
      const std::string rest = kv.second.path.substr(base.size());
      if (rest.empty() || rest.find('/') != std::string::npos) continue;
      out.push_back({rest, kv.second.isDir ? 0 : kv.second.data.size(), kv.second.isDir});
    }
    return true;
  }

  // Scripting.
  void removeCard() { present_ = false; mounted_ = false; }
  void insertCard() { present_ = true; }
  void mountsOnlyAtOrBelow(uint32_t hz) { maxClock_ = hz; }
  void refuseMount(bool refuse) { refuseMount_ = refuse; }
  void setCapacity(int64_t bytes) { capacity_ = bytes; }
  void failNextWrite() { failNext_ = true; }
  void failAllWrites(bool fail) { failAll_ = fail; }
  void failRenames(bool fail) { failRenames_ = fail; }
  void failRemoves(bool fail) { failRemoves_ = fail; }
  void failReads(bool fail) { failReads_ = fail; }
  void failReadsAt(uint64_t offset, int times) { readFailures_[offset] = times; }
  void loseCardAfter(int operations) { budget_ = operations; }
  void powerBack() {
    dead_ = false;
    budget_ = -1;
  }
  // A file put there without any checks, for the start of a test.
  void put(const std::string& path, const std::string& data) {
    const size_t slash = path.rfind('/');
    if (slash != 0 && slash != std::string::npos) nodes_[key(path.substr(0, slash))] = {path.substr(0, slash), "", true};
    nodes_[key(path)] = {path, data, false};
  }

  // Reading back.
  const std::vector<uint32_t>& mountAttempts() const { return attempts_; }
  const std::vector<std::string>& operations() const { return ops_; }
  int readCount() const { return reads_; }
  bool has(const std::string& path) const { return nodes_.count(key(path)) > 0; }
  std::string file(const std::string& path) const {
    const auto it = nodes_.find(key(path));
    return it == nodes_.end() ? std::string() : it->second.data;
  }
  std::vector<std::string> paths() const {
    std::vector<std::string> out;
    for (const auto& kv : nodes_) out.push_back(kv.second.path);
    return out;
  }

private:
  struct Node {
    std::string path;
    std::string data;
    bool isDir = false;
  };
  static std::string key(std::string s) {
    for (char& c : s)
      if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    return s;
  }
  bool usable() const { return mounted_ && present_ && !dead_; }
  // Counts a mutating call; false when the card is out of reach or has lost power by now.
  bool mutate(const std::string& what) {
    if (!usable()) return false;
    if (budget_ == 0) {
      dead_ = true;
      return false;
    }
    if (budget_ > 0) --budget_;
    ops_.push_back(what);
    return true;
  }
  bool parentExists(const std::string& path) const {
    const size_t slash = path.rfind('/');
    if (slash == 0 || slash == std::string::npos) return true;
    const auto it = nodes_.find(key(path.substr(0, slash)));
    return it != nodes_.end() && it->second.isDir;
  }
  int64_t usedExcept(const std::string& skip) const {
    int64_t used = 0;
    for (const auto& kv : nodes_)
      if (kv.first != skip) used += (int64_t)kv.second.data.size();
    return used;
  }

  std::map<std::string, Node> nodes_;
  bool present_ = true;
  bool mounted_ = false;
  bool stuck_ = false;
  bool refuseMount_ = false;
  uint32_t maxClock_ = 0xFFFFFFFFu;
  int64_t capacity_ = 1000000000;
  bool failNext_ = false;
  bool failAll_ = false;
  bool failRenames_ = false;
  bool failRemoves_ = false;
  bool failReads_ = false;
  std::map<uint64_t, int> readFailures_;
  int budget_ = -1;
  bool dead_ = false;
  int reads_ = 0;
  std::vector<uint32_t> attempts_;
  std::vector<std::string> ops_;
};

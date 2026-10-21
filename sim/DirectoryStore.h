#pragma once

// A ConfigStore over a directory of the computer: what the simulator's card is. It keeps the promises
// of the interface (hal/ConfigStore.h) that matter on a real card, because the web page exercises
// them: every path is checked with SafePath before anything is touched, names are compared without
// regard to case (the card is FAT, the computer's disk may not be), a write goes through a scratch
// file and replaces the old file only when it is complete, and scratch files are never visible.
//
// A directory that is read-only makes every write fail and change nothing; a write that fails halfway
// because the disk is full cannot be provoked here, and the scratch file is removed when it happens.

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include <app/SafePath.h>
#include <hal/ConfigStore.h>

namespace sim {

class DirectoryStore : public ConfigStore {
public:
  explicit DirectoryStore(std::filesystem::path root) : root_(std::move(root)) {}

  bool read(const std::string& path, std::string& out, size_t maxBytes) override {
    out.clear();
    std::filesystem::path file;
    if (!existingFile(path, file)) return false;
    std::ifstream in(file, std::ios::binary);
    if (!in) return false;
    out.resize(maxBytes);
    in.read(&out[0], static_cast<std::streamsize>(maxBytes));
    out.resize(static_cast<size_t>(in.gcount()));
    return true;
  }

  bool readTail(const std::string& path, std::string& out, size_t maxBytes) override {
    out.clear();
    std::filesystem::path file;
    if (!existingFile(path, file)) return false;
    std::error_code ec;
    const uintmax_t size = std::filesystem::file_size(file, ec);
    if (ec) return false;
    std::ifstream in(file, std::ios::binary);
    if (!in) return false;
    const uintmax_t skip = size > maxBytes ? size - maxBytes : 0;
    in.seekg(static_cast<std::streamoff>(skip));
    out.resize(static_cast<size_t>(size - skip));
    in.read(&out[0], static_cast<std::streamsize>(out.size()));
    out.resize(static_cast<size_t>(in.gcount()));
    return true;
  }

  bool readAt(const std::string& path, uint64_t offset, std::string& out, size_t maxBytes) override {
    out.clear();
    std::filesystem::path file;
    if (!existingFile(path, file)) return false;
    std::error_code ec;
    const uintmax_t size = std::filesystem::file_size(file, ec);
    if (ec) return false;
    if (offset >= size) return true;
    std::ifstream in(file, std::ios::binary);
    if (!in) return false;
    in.seekg(static_cast<std::streamoff>(offset));
    out.resize(static_cast<size_t>(std::min<uintmax_t>(size - offset, maxBytes)));
    in.read(&out[0], static_cast<std::streamsize>(out.size()));
    out.resize(static_cast<size_t>(in.gcount()));
    return true;
  }

  int64_t fileSize(const std::string& path) override {
    std::filesystem::path file;
    if (!existingFile(path, file)) return -1;
    std::error_code ec;
    const uintmax_t size = std::filesystem::file_size(file, ec);
    return ec ? -1 : static_cast<int64_t>(size);
  }

  bool write(const std::string& path, std::string_view data) override {
    if (!safepath::valid(path.c_str())) return false;
    std::filesystem::path target;
    if (!resolve(path, target, /*createParent=*/true)) return false;
    std::error_code ec;
    if (std::filesystem::is_directory(target, ec)) return false;
    const std::filesystem::path scratch = target.string() + safepath::kTempSuffix;
    {
      std::ofstream out(scratch, std::ios::binary | std::ios::trunc);
      if (!out) return false;
      out.write(data.data(), static_cast<std::streamsize>(data.size()));
      out.flush();
      if (!out) {
        out.close();
        std::filesystem::remove(scratch, ec);
        return false;
      }
    }
    std::filesystem::rename(scratch, target, ec);
    if (ec) {
      std::filesystem::remove(scratch, ec);
      return false;
    }
    return true;
  }

  bool remove(const std::string& path) override {
    std::filesystem::path file;
    if (!existingFile(path, file)) return false;
    std::error_code ec;
    return std::filesystem::remove(file, ec) && !ec;
  }

  bool list(const std::string& dir, std::vector<FileEntry>& out) override {
    out.clear();
    if (!safepath::valid(dir.c_str()) && dir != "/") return false;
    std::filesystem::path folder;
    if (!resolve(dir, folder, false)) return false;
    std::error_code ec;
    if (!std::filesystem::is_directory(folder, ec)) return false;
    for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
      const std::string name = entry.path().filename().string();
      if (safepath::endsWith(name.c_str(), safepath::kTempSuffix)) continue;
      FileEntry e;
      e.name = name;
      e.isDir = entry.is_directory(ec);
      e.size = e.isDir ? 0 : static_cast<size_t>(entry.file_size(ec));
      out.push_back(e);
    }
    return true;
  }

  const std::filesystem::path& root() const { return root_; }

private:
  static bool sameName(const std::string& a, const std::string& b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
             return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
           });
  }

  // The path on the disk for a card path, each name spelled as the disk spells it when it exists.
  // With `createParent` the last directory may be missing and is made (one level only, as the
  // interface says); any other missing directory fails.
  bool resolve(const std::string& path, std::filesystem::path& out, bool createParent) {
    if (path != "/" && !safepath::valid(path.c_str())) return false;
    std::vector<std::string> names;
    for (size_t at = 1; at < path.size();) {
      const size_t slash = path.find('/', at);
      names.push_back(path.substr(at, slash == std::string::npos ? std::string::npos : slash - at));
      at = slash == std::string::npos ? path.size() : slash + 1;
    }
    std::filesystem::path current = root_;
    std::error_code ec;
    for (size_t i = 0; i < names.size(); ++i) {
      std::filesystem::path next = current / names[i];
      bool found = false;
      if (std::filesystem::is_directory(current, ec)) {
        for (const auto& entry : std::filesystem::directory_iterator(current, ec)) {
          if (sameName(entry.path().filename().string(), names[i])) {
            next = entry.path();
            found = true;
            break;
          }
        }
      }
      if (!found && i + 2 == names.size() && createParent) {
        if (!std::filesystem::create_directory(next, ec) || ec) return false;
        found = true;
      }
      if (!found && i + 1 < names.size()) return false;
      current = next;
    }
    out = current;
    return true;
  }

  bool existingFile(const std::string& path, std::filesystem::path& out) {
    if (!safepath::valid(path.c_str())) return false;
    if (!resolve(path, out, false)) return false;
    std::error_code ec;
    return std::filesystem::is_regular_file(out, ec);
  }

  std::filesystem::path root_;
};

}  // namespace sim

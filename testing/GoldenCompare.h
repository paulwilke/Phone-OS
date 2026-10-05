#pragma once

// The part of the golden files that needs no test framework: reading, comparing and writing the
// ASCII art of a picture (see Golden.h for what a golden file is). The simulator compares its
// screens with it too, so that one comparison and one format serve both.

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "FrameBuffer.h"

namespace golden {

struct Result {
  bool ok = false;
  bool written = false;  // the file was (re)written because UPDATE_GOLDEN was set
  std::string message;   // what to show when !ok
};

inline std::string defaultDirectory() { return "test/golden"; }
inline bool updateRequested() { return std::getenv("UPDATE_GOLDEN") != nullptr; }

inline std::string readFile(const std::string& path, bool* found) {
  std::ifstream in(path, std::ios::binary);
  *found = (bool)in;
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

// The rows that differ, numbered, expected above actual; at most `limit` rows.
inline std::string diff(const std::string& expected, const std::string& actual, size_t limit = 6) {
  auto split = [](const std::string& s) {
    std::vector<std::string> rows;
    std::string row;
    for (char c : s) {
      if (c == '\n') {
        rows.push_back(row);
        row.clear();
      } else {
        row += c;
      }
    }
    if (!row.empty()) rows.push_back(row);
    return rows;
  };
  const auto e = split(expected), a = split(actual);
  std::ostringstream out;
  if (e.size() != a.size() || (!e.empty() && !a.empty() && e[0].size() != a[0].size())) {
    out << "size differs: expected " << (e.empty() ? 0 : e[0].size()) << "x" << e.size() << ", actual "
        << (a.empty() ? 0 : a[0].size()) << "x" << a.size() << "\n";
  }
  size_t shown = 0, differing = 0;
  for (size_t i = 0; i < std::max(e.size(), a.size()); ++i) {
    const std::string er = i < e.size() ? e[i] : "<missing row>";
    const std::string ar = i < a.size() ? a[i] : "<missing row>";
    if (er == ar) continue;
    ++differing;
    if (shown < limit) {
      out << "row " << i << "\n  expected " << er << "\n  actual   " << ar << "\n";
      ++shown;
    }
  }
  if (differing > shown) out << "... and " << (differing - shown) << " more differing row(s)\n";
  return out.str();
}

inline Result compare(const FrameBuffer& fb, const std::string& name, const ui::Rect& region,
                      const std::string& directory, bool update) {
  Result r;
  const std::string path = directory + "/" + name + ".txt";
  const std::string actual = fb.toAscii(region);
  if (update) {
    std::filesystem::create_directories(directory);
    std::ofstream out(path, std::ios::binary);
    out << actual;
    r.ok = r.written = (bool)out;
    if (!r.ok) r.message = "cannot write " + path;
    return r;
  }
  bool found = false;
  const std::string expected = readFile(path, &found);
  if (!found) {
    r.message = "golden file " + path + " does not exist; run the test with UPDATE_GOLDEN=1 to create it, then review it";
    return r;
  }
  if (expected == actual) {
    r.ok = true;
    return r;
  }
  r.message = "golden file " + path + " differs (run with UPDATE_GOLDEN=1 to accept the new picture after reviewing it):\n" +
              diff(expected, actual);
  return r;
}

}  // namespace golden

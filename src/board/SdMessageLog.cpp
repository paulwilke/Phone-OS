#include "SdMessageLog.h"

#include <string>

namespace board {

bool SdMessageLog::append(const char* line) {
  if (!card_.ready()) return false;
  SdVolume& v = card_.volume();
  const int64_t free = v.freeBytes();
  if (free >= 0 && free < kLogReserveBytes) return false;
  std::string data(line ? line : "");
  for (char& c : data)
    if (c == '\n' || c == '\r') c = '|';  // one message is one line, whatever it was given
  data += '\n';
  // After a failed write the file may end in a piece of a line: a break first keeps it from joining this one.
  if (lastFailed_) data.insert(data.begin(), '\n');
  lastFailed_ = !v.appendFile(kMessageLogPath, reinterpret_cast<const uint8_t*>(data.data()), data.size());
  if (lastFailed_) {
    card_.operationFailed();
    return false;
  }
  card_.operationSucceeded();
  return true;
}

bool SdMessageLog::readAll(Visitor& visitor) {
  if (!card_.ready()) return false;
  SdVolume& v = card_.volume();
  const int64_t size = v.fileSize(kMessageLogPath);
  if (size < 0) return false;  // no history yet
  std::string line;
  uint8_t block[512];
  uint64_t offset = 0;
  auto emit = [&]() {
    if (!line.empty()) visitor.line(line.c_str());
    line.clear();
  };
  while (offset < (uint64_t)size) {
    const int n = v.readAt(kMessageLogPath, offset, block, sizeof(block));
    if (n <= 0) {
      emit();
      card_.operationFailed();
      return false;
    }
    for (int i = 0; i < n; ++i) {
      const char c = (char)block[i];
      if (c == '\n' || c == '\r') emit();
      else if (line.size() < kMessageLineMax) line += c;
    }
    offset += (uint64_t)n;
  }
  emit();
  card_.operationSucceeded();
  return true;
}

}  // namespace board

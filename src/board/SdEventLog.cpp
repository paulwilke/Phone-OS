#include "SdEventLog.h"

#include <cstdio>

namespace board {

std::string SdEventLog::stamp(const std::string& text) {
  const uint32_t epoch = clock_.epochUtc();
  char head[32];
  if (epoch == 0) {
    std::snprintf(head, sizeof(head), "- --:-- ");
  } else {
    const int64_t local = (int64_t)epoch + (int64_t)clock_.tzOffsetMinutes() * 60;
    const int64_t second = ((local % 86400) + 86400) % 86400;
    std::snprintf(head, sizeof(head), "%u %02d:%02d ", (unsigned)epoch, (int)(second / 3600), (int)(second / 60 % 60));
  }
  return head + text;
}

void SdEventLog::log(std::string_view line) {
  std::string text(line);
  for (char& c : text)
    if (c == '\n' || c == '\r') c = '|';
  if (mirror_) mirror_->log(text);
  if (pending_.size() >= kLogPendingMax) {
    pending_.pop_front();
    ++dropped_;
  }
  pending_.push_back(stamp(text));
  flush();
}

bool SdEventLog::writeLine(const std::string& stamped) {
  SdVolume& v = card_.volume();
  std::string data = stamped + "\n";
  if (lastFailed_) data.insert(data.begin(), '\n');  // a piece of the last line must not join this one
  lastFailed_ = !v.appendFile(kSystemLogPath, reinterpret_cast<const uint8_t*>(data.data()), data.size());
  if (lastFailed_) {
    card_.operationFailed();
    return false;
  }
  card_.operationSucceeded();
  return true;
}

void SdEventLog::flush() {
  if (pending_.empty() || !card_.ready()) return;
  SdVolume& v = card_.volume();
  const int64_t free = v.freeBytes();
  if (free >= 0 && free < kLogReserveBytes) return;  // the lines wait: the configuration needs the last room
  if (v.fileSize(kSystemLogPath) >= kLogRotateBytes) {
    v.removeFile(kSystemLogOldPath);
    v.rename(kSystemLogPath, kSystemLogOldPath);
  }
  if (dropped_ > 0) {
    if (!writeLine(stamp("Log: " + std::to_string(dropped_) + " lines were lost while the card could not be written"))) return;
    dropped_ = 0;
  }
  while (!pending_.empty()) {
    if (!writeLine(pending_.front())) return;
    pending_.pop_front();
  }
}

}  // namespace board

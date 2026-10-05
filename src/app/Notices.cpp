#include "Notices.h"

void Notices::set(NoticeKind kind, const std::string& peer, int minute) {
  notice_.kind = kind;
  notice_.peer = peer;
  notice_.minute = minute;
  until_ = clock_.uptimeSeconds() + kSeconds;
}

const Notice& Notices::current() {
  if (clock_.uptimeSeconds() >= until_) notice_ = Notice();
  return notice_;
}

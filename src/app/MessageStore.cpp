#include "MessageStore.h"

#include <cstring>
#include <string>

#include "Utf8.h"

namespace {

char sourceChar(MsgSource s) { return s == MsgSource::Sms ? 's' : s == MsgSource::Radio ? 'm' : 'y'; }

bool sourceFrom(char c, MsgSource* out) {
  switch (c) {
    case 's': *out = MsgSource::Sms; return true;
    case 'm': *out = MsgSource::Radio; return true;
    case 'y': *out = MsgSource::System; return true;
  }
  return false;
}

// A text or peer on one line: a tab would shift the fields, a line break would
// split the message. The replacements lose the original character, which is the
// price of an append-only line format.
std::string oneLine(const char* text) {
  std::string s(text);
  for (char& c : s) {
    if (c == '\t') c = ' ';
    else if (c == '\n' || c == '\r') c = '|';
  }
  return s;
}

// Digits only, at least one, and it fits: strtoul would accept "abc" as 0, a sign,
// or a number that wraps around.
bool parseNumber(const char* begin, const char* end, uint32_t* out) {
  if (begin == end) return false;
  uint64_t v = 0;
  for (const char* p = begin; p < end; ++p) {
    if (*p < '0' || *p > '9') return false;
    v = v * 10 + (uint64_t)(*p - '0');
    if (v > 0xFFFFFFFFu) return false;
  }
  *out = (uint32_t)v;
  return true;
}

// Splits at tabs. The last field takes the rest of the line, so a text keeps any
// character except the tab that cannot be in it.
const char* nextField(const char* p, const char** end) {
  const char* tab = strchr(p, '\t');
  *end = tab ? tab : p + strlen(p);
  return tab ? tab + 1 : *end;
}

}  // namespace

void MessageStore::clear() {
  start_ = count_ = 0;
  nextSeq_ = 1;
  readUpTo_ = 0;
  damaged_ = 0;
}

void MessageStore::push(const Message& msg) {
  if (count_ < kCapacity) {
    items_[(start_ + count_) % kCapacity] = msg;
    ++count_;
  } else {
    items_[start_] = msg;
    start_ = (start_ + 1) % kCapacity;
  }
}

Message* MessageStore::find(uint32_t seq) {
  for (size_t i = 0; i < count_; ++i) {
    if (slot(i).seq == seq) return &slot(i);
  }
  return nullptr;
}

void MessageStore::write(const char* line) {
  if (log_ && !log_->append(line)) ++writeErrors_;
}

uint32_t MessageStore::add(const Message& input) {
  Message msg = input;
  msg.seq = nextSeq_++;
  msg.read = msg.dir == MsgDir::Out || msg.seq <= readUpTo_;
  push(msg);

  if (log_) {
    const char dir = msg.dir == MsgDir::In ? 'i' : (msg.pending ? 'q' : 'o');
    std::string line = "M\t" + std::to_string(msg.seq) + "\t" + std::to_string(msg.time) + "\t";
    line += sourceChar(msg.source);
    line += '\t';
    line += dir;
    line += '\t' + oneLine(msg.peer) + '\t' + oneLine(msg.text);
    write(line.c_str());
  }
  return msg.seq;
}

size_t MessageStore::unread() const {
  size_t n = 0;
  for (size_t i = 0; i < count_; ++i) {
    if (!at(i).read) ++n;
  }
  return n;
}

void MessageStore::markAllRead() {
  uint32_t last = readUpTo_;
  for (size_t i = 0; i < count_; ++i) {
    Message& m = slot(i);
    m.read = true;
    if (m.seq > last) last = m.seq;
  }
  if (last == readUpTo_) return;
  readUpTo_ = last;
  write(("R\t" + std::to_string(readUpTo_)).c_str());
}

bool MessageStore::markSent(uint32_t seq, MsgSource source) {
  Message* m = find(seq);
  if (!m || !m->pending) return false;
  m->pending = false;
  m->source = source;
  write(("S\t" + std::to_string(seq) + "\t" + sourceChar(source)).c_str());
  return true;
}

bool MessageStore::markDelivered(uint32_t seq, bool confirmed) {
  Message* m = find(seq);
  if (!m || m->dir != MsgDir::Out) return false;
  m->delivery = confirmed ? MsgDelivery::Confirmed : MsgDelivery::Unconfirmed;
  write(("D\t" + std::to_string(seq) + "\t" + (confirmed ? '1' : '0')).c_str());
  return true;
}

bool MessageStore::removeFromList(uint32_t seq) {
  for (size_t i = 0; i < count_; ++i) {
    if (slot(i).seq != seq) continue;
    for (size_t j = i + 1; j < count_; ++j) slot(j - 1) = slot(j);
    --count_;
    return true;
  }
  return false;
}

bool MessageStore::remove(uint32_t seq) {
  if (!removeFromList(seq)) return false;
  write(("X\t" + std::to_string(seq)).c_str());
  return true;
}

bool MessageStore::parseLine(const char* line) {
  if (line[1] != '\t') return false;
  const char* end;
  const char* p = line + 2;

  uint32_t seq = 0;
  switch (line[0]) {
    case 'R': {
      if (!parseNumber(p, p + strlen(p), &seq)) return false;
      if (seq > readUpTo_) readUpTo_ = seq;
      return true;
    }
    case 'X': {
      if (!parseNumber(p, p + strlen(p), &seq)) return false;
      removeFromList(seq);
      return true;
    }
    case 'S': {
      p = nextField(p, &end);
      if (!parseNumber(line + 2, end, &seq) || p[0] == '\0' || p[1] != '\0') return false;
      MsgSource source;
      if (!sourceFrom(p[0], &source)) return false;
      if (Message* m = find(seq)) {
        m->pending = false;
        m->source = source;
      }
      return true;
    }
    case 'D': {
      p = nextField(p, &end);
      if (!parseNumber(line + 2, end, &seq) || (p[0] != '0' && p[0] != '1') || p[1] != '\0') return false;
      if (Message* m = find(seq)) m->delivery = p[0] == '1' ? MsgDelivery::Confirmed : MsgDelivery::Unconfirmed;
      return true;
    }
    case 'M':
      break;
    default:
      return false;
  }

  Message m;
  const char* fieldEnd;
  const char* next = nextField(p, &fieldEnd);
  if (!parseNumber(p, fieldEnd, &m.seq) || m.seq == 0) return false;
  p = next;
  next = nextField(p, &fieldEnd);
  if (!parseNumber(p, fieldEnd, &m.time)) return false;
  p = next;
  next = nextField(p, &fieldEnd);
  if (fieldEnd - p != 1 || !sourceFrom(p[0], &m.source)) return false;
  p = next;
  next = nextField(p, &fieldEnd);
  if (fieldEnd - p != 1 || (p[0] != 'i' && p[0] != 'o' && p[0] != 'q')) return false;
  m.dir = p[0] == 'i' ? MsgDir::In : MsgDir::Out;
  m.pending = p[0] == 'q';
  p = next;
  // The peer and the text must both be there, even if the text is empty: a line cut
  // off by a power loss usually lacks the last tab.
  if (!strchr(p, '\t')) return false;
  next = nextField(p, &fieldEnd);
  utf8::copy(m.peer, sizeof(m.peer), p, fieldEnd);
  utf8::copy(m.text, sizeof(m.text), next, next + strlen(next));

  // A number that is already in memory would make every later mark ambiguous.
  if (find(m.seq)) return false;
  if (m.seq >= nextSeq_) nextSeq_ = m.seq + 1;
  push(m);
  return true;
}

bool MessageStore::restore() {
  clear();
  if (!log_) return false;

  struct Reader : MessageLog::Visitor {
    MessageStore& store;
    explicit Reader(MessageStore& s) : store(s) {}
    void line(const char* text) override {
      // Files edited or copied on Windows end their lines with \r\n.
      std::string l(text);
      while (!l.empty() && l.back() == '\r') l.pop_back();
      if (l.empty()) return;
      if (!store.parseLine(l.c_str())) ++store.damaged_;
    }
  } reader(*this);

  if (!log_->readAll(reader)) return false;

  // The read mark is applied at the end: its line can come before or after the
  // messages it covers, depending on when it was written.
  for (size_t i = 0; i < count_; ++i) {
    Message& m = slot(i);
    m.read = m.dir == MsgDir::Out || m.seq <= readUpTo_;
  }
  return damaged_ == 0;
}

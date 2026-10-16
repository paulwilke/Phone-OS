#pragma once

// The message history: SMS, radio and system entries in one list. The newest
// kCapacity entries are kept in memory; the complete history is a log file (the SD
// card on the device, an in-memory log in tests) behind the MessageLog interface.
//
// The file is only ever appended to, never rewritten. That keeps the code simple
// and survives a power loss in the middle of a write: at worst the last line is
// damaged. Every change after the message itself is a line of its own:
//
//   M <seq> <time> <source s|m|y> <direction i|o|q> <peer> <text>   a message (q = outgoing, waiting)
//   R <seq>             everything up to seq is read
//   S <seq> <source>    a waiting message was sent, and over which channel
//   D <seq> <1|0>       the radio confirmed delivery (1) or did not (0)
//   X <seq>             the message is deleted: its M line stays, reading skips it
//
// Fields are separated by tabs. Source letters: s = SMS, m = radio (mesh), y =
// system. Tabs and line breaks in a text or a peer are replaced so that a message
// is always exactly one line.

#include <cstddef>
#include <cstdint>

#include "../hal/MessageLog.h"

enum class MsgSource : uint8_t { Sms, Radio, System };

// Only for outgoing radio messages: the radio network confirms an arrival. SMS
// gets no answer from the network and stays Unknown.
enum class MsgDelivery : uint8_t { Unknown, Confirmed, Unconfirmed };
enum class MsgDir : uint8_t { In, Out };

struct Message {
  uint32_t seq = 0;   // running number, assigned by the store, never reused
  uint32_t time = 0;  // Unix time (UTC), 0 = unknown
  MsgSource source = MsgSource::Sms;
  MsgDir dir = MsgDir::In;
  bool read = false;
  bool pending = false;  // outgoing and not sent yet (in the queue)
  MsgDelivery delivery = MsgDelivery::Unknown;
  // A phone number, or the name of a radio contact (those are longer: up to 31
  // bytes). Empty for device notices.
  char peer[32] = {};
  char text[161] = {};
};


class MessageStore {
public:
  static constexpr size_t kCapacity = 50;

  void attach(MessageLog* log) { log_ = log; }

  // Rebuilds the list from the log; the newest kCapacity messages stay. Returns
  // false when there is no log, it cannot be read, or lines were damaged — the
  // readable rest is kept in every case (damagedLines() counts the others).
  //
  // A read that stops in the middle of the file keeps what it read, with the read
  // mark and the deletions it saw applied, and then nothing is written to the log
  // until the next restore() reads it to the end: the numbers in the unread rest
  // are unknown, and a new message could get one of them (decision D-37).
  bool restore();

  // Appends a message: assigns the number, writes the log line, returns the
  // number. A failed write does not lose the message in memory; it is counted in
  // writeErrors().
  uint32_t add(const Message& msg);

  size_t count() const { return count_; }
  // 0 is the oldest. `i` must be below count().
  const Message& at(size_t i) const { return items_[(start_ + i) % kCapacity]; }

  // Incoming messages not yet read, among those in memory.
  size_t unread() const;
  void markAllRead();

  // A waiting outgoing message went out over `source`. False when there is no such
  // waiting message.
  bool markSent(uint32_t seq, MsgSource source);

  // The radio network confirmed or did not confirm an outgoing message. False when
  // there is no such outgoing message.
  bool markDelivered(uint32_t seq, bool confirmed);

  // Deletes a message, read or not. Only the messages in memory can be deleted. The
  // file is not rewritten; a deletion line is appended.
  bool remove(uint32_t seq);

  // Lines that could not be used during the last restore().
  size_t damagedLines() const { return damaged_; }
  // Lines the last restore() was handed, damaged ones included.
  size_t linesRead() const { return linesRead_; }
  // The last restore() could not read the log to the end; until the next one that
  // does, every write is refused and counted in writeErrors().
  bool readStoppedEarly() const { return readStoppedEarly_; }
  // Log lines that could not be written since start.
  size_t writeErrors() const { return writeErrors_; }

private:
  Message& slot(size_t i) { return items_[(start_ + i) % kCapacity]; }
  void push(const Message& msg);
  bool removeFromList(uint32_t seq);
  Message* find(uint32_t seq);
  bool parseLine(const char* line);
  void write(const char* line);
  void clear();

  MessageLog* log_ = nullptr;
  Message items_[kCapacity];
  size_t start_ = 0, count_ = 0;
  uint32_t nextSeq_ = 1;
  uint32_t readUpTo_ = 0;  // every message with seq <= readUpTo_ is read
  size_t damaged_ = 0;
  size_t linesRead_ = 0;
  bool readStoppedEarly_ = false;
  size_t writeErrors_ = 0;
};

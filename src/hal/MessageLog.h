#pragma once

// Capability: the message history's file. The store appends one line for every change and reads all lines back
// when it starts; whether the SD card, a file of the simulator or a vector in a test stands behind it, it does
// not know. The file is only ever appended to: that survives a power loss in the middle of a write (at worst
// the last line is damaged) and keeps the code simple. What the lines mean is the store's business
// (src/app/MessageStore.h).
//
//   * append() takes one line without its line break and returns false when it could not be written (no
//     card, a full card): the store counts the failure and keeps the message on the screen;
//   * readAll() hands every line to the visitor, oldest first, without line breaks; empty lines are skipped.
//     It says whether it read to the end (Complete), found no history (Missing: no file yet, or no card), or
//     stopped in the middle of a file that is there (StoppedEarly; the lines read before were handed over).
//     The store must tell the last two apart: after an empty history it numbers from 1, after a read that
//     stopped early the numbers in the rest of the file are unknown.

class MessageLog {
public:
  virtual ~MessageLog() = default;
  virtual bool append(const char* line) = 0;  // without the line break
  class Visitor {
  public:
    virtual ~Visitor() = default;
    virtual void line(const char* text) = 0;
  };
  enum class Read { Complete, Missing, StoppedEarly };
  virtual Read readAll(Visitor& visitor) = 0;
};

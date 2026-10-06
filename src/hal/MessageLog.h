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
//     False when there is no file yet or it cannot be read; the lines read before a failure were handed over.

class MessageLog {
public:
  virtual ~MessageLog() = default;
  virtual bool append(const char* line) = 0;  // without the line break
  class Visitor {
  public:
    virtual ~Visitor() = default;
    virtual void line(const char* text) = 0;
  };
  virtual bool readAll(Visitor& visitor) = 0;
};

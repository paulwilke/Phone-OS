#pragma once

// The SD card stack on an in-memory volume, and the rigs that run the ConfigStore, EventLog and MessageLog
// contracts against the real classes on top of it. The card is mounted at construction.

#include <string>
#include <vector>

#include <board/SdCard.h>
#include <board/SdConfigStore.h>
#include <board/SdEventLog.h>
#include <board/SdMessageLog.h>
#include <fakes/FakeClock.h>
#include <fakes/FakeDelay.h>
#include <fakes/FakeEventLog.h>
#include <fakes/FakeGpio.h>
#include <fakes/FakeMessageLog.h>
#include <fakes/FakeSdVolume.h>

struct SdStack {
  FakeSdVolume volume;
  FakeDelay delay;
  FakeGpio gpio{&delay};
  FakeEventLog bootLog;  // what the card says about itself
  board::SdCard card{volume, gpio, delay};
  explicit SdStack(bool mount = true) {
    if (mount) card.begin(bootLog);
  }
};

struct SdConfigStoreRig {
  SdStack stack;
  board::SdConfigStore impl{stack.card};
  ConfigStore& store() { return impl; }
  bool failNextWrite() {
    stack.volume.failNextWrite();
    return true;
  }
  bool failAllWrites(bool fail) {
    stack.volume.failAllWrites(fail);
    return true;
  }
};

struct SdEventLogRig {
  SdStack stack;
  FakeClock clock;
  board::SdEventLog impl{stack.card, clock};
  EventLog& log() { return impl; }
  // The lines of the file without their stamp ("<epoch> <HH:MM> " or "- --:-- ").
  std::vector<std::string> records() {
    std::vector<std::string> out;
    const std::string text = stack.volume.file(board::kSystemLogPath);
    size_t at = 0;
    while (at < text.size()) {
      size_t end = text.find('\n', at);
      if (end == std::string::npos) end = text.size();
      const std::string line = text.substr(at, end - at);
      const size_t first = line.find(' ');
      const size_t second = line.find(' ', first + 1);
      out.push_back(line.substr(second + 1));
      at = end + 1;
    }
    return out;
  }
};

struct SdMessageLogRig {
  SdStack stack;
  board::SdMessageLog impl{stack.card};
  MessageLog& log() { return impl; }
  bool failNextAppend() {
    stack.volume.failNextWrite();
    return true;
  }
  bool failAllAppends(bool fail) {
    stack.volume.failAllWrites(fail);
    return true;
  }
};

struct FakeMessageLogRig {
  FakeMessageLog impl;
  MessageLog& log() { return impl; }
  bool failNextAppend() {
    impl.failNextAppend();
    return true;
  }
  bool failAllAppends(bool fail) {
    impl.failAllAppends(fail);
    return true;
  }
};

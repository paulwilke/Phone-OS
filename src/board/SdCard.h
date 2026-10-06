#pragma once

// The SD card as the rest of the device sees it: present and readable, or not. The device starts without a
// card and says so (the configuration falls back to its defaults, the history stays in memory, the log goes to
// the console); it never crashes on one, and a card that is pushed in later is found.
//
// Mounting asks the card at a descending bus clock, 4 MHz, 1 MHz, 400 kHz: the card hangs on the bus that
// the display and the radio share, and a card that a long line does not allow 4 MHz answers at a lower one.
// Between two attempts the volume is released and the chip selects are held high, because a library that
// is not released keeps its failure state and asks the card nothing the second time (an intact FAT32 card
// was "not found" for that reason on 2026-09-30). All three chip selects of the shared bus (display, card,
// radio) are held high first so that none of the other two listens while the card is asked.
//
// A card that fails three operations in a row is taken for pulled out; poll() asks again every 30 seconds
// while there is none.

#include <cstdint>

#include "../hal/EventLog.h"
#include "Delay.h"
#include "Gpio.h"
#include "SdVolume.h"

namespace board {

constexpr uint32_t kSdClocks[] = {4000000, 1000000, 400000};
constexpr int kSdFailuresUntilGone = 3;
constexpr uint32_t kSdRetryMs = 30000;
// The logs stop taking room below this much free space: a configuration write needs the last of it.
constexpr int64_t kLogReserveBytes = 32 * 1024;

class SdCard {
public:
  SdCard(SdVolume& volume, Gpio& gpio, Delay& delay) : volume_(volume), gpio_(gpio), delay_(delay) {}

  // Asks the card; true when it is mounted. Says what happened in the log, one line.
  bool begin(EventLog& log);
  // From the main loop: asks again every 30 seconds while the card is missing, and forgets a card that was
  // taken out.
  void poll(uint32_t nowMs, EventLog& log);

  bool ready() const { return ready_; }
  SdVolume& volume() { return volume_; }

  // The stores report what happened to their calls: three failures in a row and the card counts as gone.
  void operationFailed();
  void operationSucceeded() { failures_ = 0; }

private:
  SdVolume& volume_;
  Gpio& gpio_;
  Delay& delay_;
  bool ready_ = false;
  int failures_ = 0;
  uint32_t lastTryMs_ = 0;
};

}  // namespace board

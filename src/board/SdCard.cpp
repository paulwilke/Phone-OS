#include "SdCard.h"

#include <string>

#include "PinMap.h"

namespace board {

bool SdCard::begin(EventLog& log) {
  ready_ = false;
  failures_ = 0;
  for (uint8_t cs : {pin::kSdCs, pin::kDisplayCs, pin::kRadioCs}) {
    gpio_.setMode(cs, Gpio::Mode::Output);
    gpio_.write(cs, true);
  }
  delay_.delayMs(100);  // a card needs a few clocks of quiet after it was powered
  for (size_t i = 0; i < sizeof(kSdClocks) / sizeof(kSdClocks[0]); ++i) {
    if (i > 0) {
      volume_.unmount();
      gpio_.write(pin::kSdCs, true);
      delay_.delayMs(250);
    }
    if (volume_.mount(kSdClocks[i])) {
      ready_ = true;
      log.log(i == 0 ? "SD card: ready"
                     : "SD card: ready, found only at " + std::to_string(kSdClocks[i] / 1000) + " kHz");
      return true;
    }
  }
  volume_.unmount();
  log.log("SD card: not found or not readable (FAT32 with an MBR partition table is read, a GPT card is not); "
          "the device runs on its defaults");
  return false;
}

void SdCard::poll(uint32_t nowMs, EventLog& log) {
  if (ready_ && !volume_.mounted()) {
    ready_ = false;
    log.log("SD card: gone");
    lastTryMs_ = nowMs;
  }
  if (!ready_ && nowMs - lastTryMs_ >= kSdRetryMs) {
    lastTryMs_ = nowMs;
    begin(log);
  }
}

void SdCard::operationFailed() {
  if (!ready_) return;
  if (++failures_ >= kSdFailuresUntilGone) {
    ready_ = false;
    volume_.unmount();
    failures_ = 0;
  }
}

}  // namespace board

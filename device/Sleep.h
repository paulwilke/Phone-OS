#pragma once

// Light sleep for the time the power logic allows. The processor wakes on the timer, on a key or touch
// (their interrupt lines change), and on the modem's ring line (an incoming call or text). **Nothing sleeps
// while a cable is attached**: the USB bridge of the ESP32-S3 is switched off in light sleep and the console
// disappears; that rule is the power policy's (docs/core/power-policy.md), this function only carries a decision out.
//
// A line wakes on a level, not an edge (that is what light sleep offers), and a line that already has the level
// ends the sleep at once: in the night of 2026-10-14 the keyboard's interrupt and the ring line read low before
// every one of 398,000 sleeps, and none lasted. So each line is armed with the level it does not have now: a
// line that idles high wakes on low, one that idles low wakes on high, whatever its polarity (decision D-34).

#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

#include <board/LoopStats.h>
#include <board/PinMap.h>

// What one sleep reports for the loop line (decision D-33): the wake lines that were low before it, which end a
// sleep at once, and what ended it.
struct SleepReport {
  uint8_t lowBefore = 0;   // board::LoopStats::lineBit(...) of every wake line that read low
  board::LoopStats::WakeCause cause = board::LoopStats::WakeCause::Other;
};

inline SleepReport lightSleep(uint32_t milliseconds) {
  using board::LoopStats;
  SleepReport report;
  const struct { uint8_t pin; LoopStats::WakeLine line; } lines[] = {
      {board::pin::kKeyboardInterrupt, LoopStats::WakeLine::Keyboard},
      {board::pin::kTouchInterrupt, LoopStats::WakeLine::Touch},
      {board::pin::kModemRing, LoopStats::WakeLine::Ring},
  };
  for (const auto& l : lines) {
    const bool low = digitalRead(l.pin) == LOW;
    if (low) report.lowBefore |= LoopStats::lineBit(l.line);
    gpio_wakeup_enable((gpio_num_t)l.pin, low ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL);
  }
  esp_sleep_enable_gpio_wakeup();
  esp_sleep_enable_timer_wakeup((uint64_t)milliseconds * 1000ULL);
  Serial.flush();
  esp_light_sleep_start();
  switch (esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_TIMER: report.cause = LoopStats::WakeCause::Timer; break;
    case ESP_SLEEP_WAKEUP_GPIO: report.cause = LoopStats::WakeCause::Line; break;
    default: report.cause = LoopStats::WakeCause::Other; break;
  }
  return report;
}

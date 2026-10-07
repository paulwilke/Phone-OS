#pragma once

// Light sleep for the time the power logic allows. The processor wakes on the timer, on a key or touch
// (their interrupt lines go low), and on the modem's ring line (an incoming call or text). **Nothing sleeps
// while a cable is attached**: the USB bridge of the ESP32-S3 is switched off in light sleep and the console
// disappears; that rule is the power policy's (docs/power-policy.md), this function only carries a decision out.

#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

#include <board/PinMap.h>

inline void lightSleep(uint32_t milliseconds) {
  for (uint8_t pin : {board::pin::kKeyboardInterrupt, board::pin::kTouchInterrupt, board::pin::kModemRing})
    gpio_wakeup_enable((gpio_num_t)pin, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  esp_sleep_enable_timer_wakeup((uint64_t)milliseconds * 1000ULL);
  Serial.flush();
  esp_light_sleep_start();
}

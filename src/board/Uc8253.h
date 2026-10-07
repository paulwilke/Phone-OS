#pragma once

// The command codes of the UC8253 e-ink controller, as the controller's command set documents them. Codes
// only: **no value that is sent after one is in this file**. The values (panel setting, power and booster
// settings, data interval, resolution, the waveform tables, the format of a partial window) belong to the
// panel and come from its datasheet and the panel vendor's published demo, never from a library; they are the
// content of a PanelProfile (board/PanelProfile.h). A wrong power or booster value can damage the panel, so
// none is guessed here.

#include <cstdint>

namespace board {
namespace uc8253 {

constexpr uint8_t kPanelSetting = 0x00;
constexpr uint8_t kPowerSetting = 0x01;
constexpr uint8_t kPowerOff = 0x02;
constexpr uint8_t kPowerOn = 0x04;
constexpr uint8_t kBoosterSoftStart = 0x06;
constexpr uint8_t kDeepSleep = 0x07;
constexpr uint8_t kDataTransmission1 = 0x10;  // the picture the panel shows now (the "old" data)
constexpr uint8_t kDataStop = 0x11;
constexpr uint8_t kDisplayRefresh = 0x12;
constexpr uint8_t kDataTransmission2 = 0x13;  // the picture to show (the "new" data)
constexpr uint8_t kDataIntervalSetting = 0x50;
constexpr uint8_t kResolutionSetting = 0x61;
constexpr uint8_t kPartialWindow = 0x90;
constexpr uint8_t kPartialIn = 0x91;
constexpr uint8_t kPartialOut = 0x92;

}  // namespace uc8253
}  // namespace board

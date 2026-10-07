#pragma once

// The command codes of the UC8253 e-ink controller, as the controller's command set documents them. Codes
// only: **no value that is sent after one is in this file**. The values (panel setting, data interval,
// temperature override, the format of a partial window) belong to the panel and are the content of a
// PanelProfile (board/PanelProfile.h; the GDEQ031T10's is board/GDEQ031T10Profile.h).

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
constexpr uint8_t kCascadeSetting = 0xE0;     // bit 1 (TSFIX): the temperature comes from kForceTemperature
constexpr uint8_t kForceTemperature = 0xE5;   // the temperature the waveform is chosen for, in whole degrees

}  // namespace uc8253
}  // namespace board

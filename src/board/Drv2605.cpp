#include "Drv2605.h"

#include "PinMap.h"

namespace board {

namespace {
constexpr uint8_t kMode = 0x01;
constexpr uint8_t kRealTimeInput = 0x02;
constexpr uint8_t kLibrary = 0x03;
constexpr uint8_t kWaveform1 = 0x04;
constexpr uint8_t kWaveform2 = 0x05;
constexpr uint8_t kGo = 0x0C;
constexpr uint8_t kOverdrive = 0x0D;
constexpr uint8_t kSustainPositive = 0x0E;
constexpr uint8_t kSustainNegative = 0x0F;
constexpr uint8_t kBrake = 0x10;
constexpr uint8_t kAudioMax = 0x13;
constexpr uint8_t kFeedback = 0x1A;
constexpr uint8_t kControl3 = 0x1D;
}  // namespace

bool Drv2605::begin() {
  const uint8_t a = i2c::kHaptic;
  if (!bus_.probe(a)) return false;
  uint8_t feedback = 0, control3 = 0;
  if (!bus_.readRegister(a, kFeedback, feedback) || !bus_.readRegister(a, kControl3, control3)) return false;
  return bus_.writeRegister(a, kMode, 0x00) &&            // out of standby, internal trigger
         bus_.writeRegister(a, kRealTimeInput, 0x00) &&   // no real-time playback
         bus_.writeRegister(a, kWaveform1, 1) &&          // the default effect: a strong click
         bus_.writeRegister(a, kWaveform2, 0) &&          // end of the sequence
         bus_.writeRegister(a, kOverdrive, 0) && bus_.writeRegister(a, kSustainPositive, 0) &&
         bus_.writeRegister(a, kSustainNegative, 0) && bus_.writeRegister(a, kBrake, 0) &&
         bus_.writeRegister(a, kAudioMax, 0x64) &&
         bus_.writeRegister(a, kFeedback, (uint8_t)(feedback & 0x7F)) &&  // N_ERM_LRA = 0: an ERM motor
         bus_.writeRegister(a, kControl3, (uint8_t)(control3 | 0x20)) &&  // ERM_OPEN_LOOP
         bus_.writeRegister(a, kLibrary, 1) &&                            // ERM library 1
         bus_.writeRegister(a, kMode, 0x00);
}

bool Drv2605::fire(uint8_t effect) {
  const uint8_t a = i2c::kHaptic;
  return bus_.writeRegister(a, kWaveform1, effect) && bus_.writeRegister(a, kWaveform2, 0) &&
         bus_.writeRegister(a, kGo, 1);
}

}  // namespace board

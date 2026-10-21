#pragma once

// Pins, bus addresses and expander legs of the LilyGO T-Deck Pro MAX (board HD-V3-250911). One place, so a
// number is never written twice. The values come from the code of the manufacturer's and of Meck's
// reference firmware, never from their prose (docs/hardware/hardware-lessons.md), and the first four groups
// were confirmed on the device.

#include <cstdint>

namespace board {

// GPIO numbers of the ESP32-S3. A pin the board has no GPIO for is not listed.
namespace pin {
constexpr uint8_t kI2cSda = 13;
constexpr uint8_t kI2cScl = 14;

// One SPI bus carries the display, the SD card and the radio, each with its own chip select. Exactly
// one SPI object may drive it; a second one on the same pins makes the peripherals fight.
constexpr uint8_t kSpiSck = 36;
constexpr uint8_t kSpiMosi = 33;
constexpr uint8_t kSpiMiso = 47;
constexpr uint8_t kDisplayCs = 34;
constexpr uint8_t kSdCs = 48;
constexpr uint8_t kRadioCs = 3;

constexpr uint8_t kDisplayDc = 35;
constexpr uint8_t kDisplayReset = 9;
constexpr uint8_t kDisplayBusy = 37;
constexpr uint8_t kFrontlight = 41;

constexpr uint8_t kKeyboardInterrupt = 15;
constexpr uint8_t kKeyboardBacklight = 42;
constexpr uint8_t kTouchInterrupt = 12;
constexpr uint8_t kBootButton = 0;

// The schematic names the modem's lines from the modem's side: 7682_RXD is the modem's input. The
// processor's UART wants the pins from its own side, so the two views cross. Both are named here, and
// the host test pins the crossing, because the lines were once passed on literally.
constexpr uint8_t kModemRxd = 10;  // the modem's input, driven by the processor
constexpr uint8_t kModemTxd = 11;  // the modem's output, read by the processor
constexpr uint8_t kModemUartRx = kModemTxd;  // what the processor passes to its UART as receive pin
constexpr uint8_t kModemUartTx = kModemRxd;
constexpr uint8_t kModemRing = 7;
constexpr uint8_t kModemDtr = 8;

// Named from the processor's side already.
constexpr uint8_t kGnssUartRx = 2;
constexpr uint8_t kGnssUartTx = 16;
constexpr uint8_t kGnssPps = 1;

// The motion sensor's interrupt. The sensor has no reset line. Not a free pin although the line is polled.
constexpr uint8_t kMotionInterrupt = 21;

constexpr uint8_t kRadioReset = 4;
constexpr uint8_t kRadioDio1 = 5;
constexpr uint8_t kRadioBusy = 6;

}  // namespace pin

// I2C addresses (7 bit).
namespace i2c {
constexpr uint8_t kAudioCodec = 0x18;  // not used: call audio runs through the modem
constexpr uint8_t kTouch = 0x1A;
constexpr uint8_t kExpander = 0x20;
constexpr uint8_t kMotion = 0x28;
constexpr uint8_t kKeyboard = 0x34;
constexpr uint8_t kFuelGauge = 0x55;
constexpr uint8_t kHaptic = 0x5A;
// The charger changed during the production series: newer boards carry an SY6970, older ones a BQ25896
// with the same registers on the next address. A driver that knows one address takes the bus for broken.
constexpr uint8_t kChargerSy6970 = 0x6A;
constexpr uint8_t kChargerBq25896 = 0x6B;
}  // namespace i2c

// The legs of the XL9555 expander, numbered in sequence: P00..P07 are 0..7 and P10..P17 are 8..15. The
// manufacturer's names read like decimals, and P10 is leg 8, not 10. The expander's interrupt line is
// not connected, so its inputs can only be polled.
namespace leg {
constexpr uint8_t kModemPower = 0;       // high: supply of the A7682E
constexpr uint8_t kRadioPower = 1;       // high: supply of the SX1262
constexpr uint8_t kGnssPower = 2;        // high: supply of the satellite receiver
constexpr uint8_t kMotionRail = 3;       // high: the 1.8 V rail of the motion sensor
constexpr uint8_t kRadioAntenna = 4;     // high: internal antenna, low: external
constexpr uint8_t kHapticPower = 5;      // high: supply of the vibration driver
constexpr uint8_t kAmplifier = 6;        // high: speaker amplifier on (the schematic misspells it SHUTDOWM)
constexpr uint8_t kTouchReset = 7;       // low: reset, active low
constexpr uint8_t kModemPowerKey = 8;    // the modem's power key line
constexpr uint8_t kKeyboardReset = 9;    // low: reset, active low
constexpr uint8_t kAudioSelect = 10;     // high: sound from the modem, low: from the codec
constexpr uint8_t kCount = 16;
}  // namespace leg

// The expander's registers.
namespace xl9555 {
constexpr uint8_t kInput0 = 0x00;
constexpr uint8_t kInput1 = 0x01;
constexpr uint8_t kOutput0 = 0x02;
constexpr uint8_t kOutput1 = 0x03;
constexpr uint8_t kConfig0 = 0x06;  // 0 = output, 1 = input
constexpr uint8_t kConfig1 = 0x07;

// The state the legs are put into at start (read back on the device): radio, receiver and the 1.8 V rail
// on, internal antenna, modem off, motor and amplifier off, resets released, sound from the codec.
constexpr uint8_t kBootPort0 = 0x9E;
constexpr uint8_t kBootPort1 = 0x02;
}  // namespace xl9555

}  // namespace board

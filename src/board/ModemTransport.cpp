#include "ModemTransport.h"

#include <string>

#include "PinMap.h"

namespace board {

void ModemTransport::begin() {
  gpio_.setMode(pin::kModemDtr, Gpio::Mode::Output);
  gpio_.write(pin::kModemDtr, false);
}

void ModemTransport::write(const char* data, size_t len) {
  // A port that takes less than it was given is waited for: a command cut short is a command that fails
  // in an unpredictable way.
  size_t done = 0;
  int stalled = 0;
  while (done < len && stalled < 3) {
    const size_t n = uart_.write(reinterpret_cast<const uint8_t*>(data) + done, len - done);
    if (n == 0) {
      ++stalled;
      delay_.delayMs(1);
    } else {
      stalled = 0;
      done += n;
    }
  }
  if (done < len) log_.log("Modem: the serial port took " + std::to_string(done) + " of " + std::to_string(len) + " bytes");
}

void ModemTransport::expanderLeg(uint8_t leg, bool high, const char* what) {
  if (!expander_.set(leg, high)) log_.log(std::string("Modem: the expander refused to switch ") + what);
}

void ModemTransport::power(bool on) { expanderLeg(leg::kModemPower, on, on ? "the supply on" : "the supply off"); }

void ModemTransport::pwrkeyPulse() {
  expanderLeg(leg::kModemPowerKey, true, "the power key high");
  delay_.delayMs(kPowerKeyHighMs);
  expanderLeg(leg::kModemPowerKey, false, "the power key low");
  delay_.delayMs(kPowerKeyLowMs);
  expanderLeg(leg::kModemPowerKey, true, "the power key high again");
}

void ModemTransport::setAudioToModem(bool on) {
  if (on) {
    expanderLeg(leg::kAudioSelect, true, "the sound to the modem");
    expanderLeg(leg::kAmplifier, true, "the amplifier on");
  } else {
    expanderLeg(leg::kAmplifier, false, "the amplifier off");
    expanderLeg(leg::kAudioSelect, false, "the sound to the codec");
  }
}

void ModemTransport::setDtr(bool high) { gpio_.write(pin::kModemDtr, high); }

}  // namespace board

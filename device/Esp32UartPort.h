#pragma once

// A serial port of the processor behind the UartPort seam. The pins are given from the processor's side (the
// receive pin is the one the other chip transmits on); board/PinMap.h names them that way for both uses.

#include <Arduino.h>

#include <atomic>

#include <board/UartPort.h>

class Esp32UartPort : public UartPort {
public:
  // `rxBuffer` is what the receive queue holds between two passes of the loop. The core's default of 256 bytes
  // is 22 ms of the modem at 115200 baud, and a pass that refreshes the e-ink panel takes hundreds: the bytes
  // in between were lost, and a text read from the SIM came in broken (decision D-41).
  Esp32UartPort(HardwareSerial& serial, uint32_t baud, uint8_t rxPin, uint8_t txPin, size_t rxBuffer = 256)
      : serial_(serial), baud_(baud), rx_(rxPin), tx_(txPin), rxBuffer_(rxBuffer) {}

  void begin() {
    serial_.setRxBufferSize(rxBuffer_);   // before begin(): the core allocates the queue there
    serial_.onReceiveError([this](hardwareSerial_error_t e) {
      if (e == UART_BUFFER_FULL_ERROR || e == UART_FIFO_OVF_ERROR) overflows_.fetch_add(1);
    });
    serial_.begin(baud_, SERIAL_8N1, rx_, tx_);
  }

  // How often the receive queue or the chip's own buffer overflowed since the last call, so bytes were lost.
  // Counted in the core's UART task, read in the loop.
  uint32_t takeOverflows() { return overflows_.exchange(0); }

  size_t write(const uint8_t* data, size_t len) override { return serial_.write(data, len); }
  int read() override { return serial_.available() ? serial_.read() : -1; }

private:
  HardwareSerial& serial_;
  uint32_t baud_;
  uint8_t rx_;
  uint8_t tx_;
  size_t rxBuffer_;
  std::atomic<uint32_t> overflows_{0};
};

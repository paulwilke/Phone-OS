# The device: the firmware's platform code

Code: `device/` (the firmware's counterpart of `sim/`), the testable board logic in `src/board/`, the scripted buses in `testing/fakes/` (`FakeI2cBus`, `FakeGpio`, `FakeUartPort`, `FakeDelay`); tests in `test/test_pin_map/`, `test/test_board_fakes/`, `test/test_power_up/`. Hardware lessons that the code must honour: [hardware-lessons.md](hardware-lessons.md).

## Where what lives

| Place | Holds | May include |
|---|---|---|
| `src/board/` | everything about the board's chips that can run on a laptop: the pin map, the seams to the buses and pins, register decoding, start-up sequences | `src/board/`, `src/hal/`, `src/drivers/` |
| `device/` | only the implementations of the seams on the processor's peripherals and `main.cpp` | `src/` and the Arduino and ESP32 headers |
| `testing/fakes/` | scripted implementations of the seams | the seams |

`src/` never includes `device/`, and no file under `src/` names Arduino or an ESP32 header; `device/` never includes `sim/` or `testing/`; nothing includes MeshCore. The wiring of the whole firmware is in [device-console.md](device-console.md). `scripts/check_layers.py` checks all of it, and `test/test_check_layers/test_check.py` runs the checker on small trees that break each rule, so a rule that stops firing is noticed.

The rule behind the split: **a bug found on the device gets a host test.** The hardware is reached only through four small seams, so what is above them is tested against a scripted chip.

| Seam | Header | Device implementation | Fake |
|---|---|---|---|
| I2C bus | `src/board/I2cBus.h` | `device/Esp32I2cBus.h` | `FakeI2cBus`: each chip is a file of byte registers; an address nobody added does not acknowledge; refused writes; a log with the time of every transaction |
| serial port | `src/board/UartPort.h` | with the drivers that use it | `FakeUartPort` |
| processor pins | `src/board/Gpio.h` | `device/Esp32Gpio.h` | `FakeGpio` |
| waiting | `src/board/Delay.h` | `device/Esp32Delay.h` | `FakeDelay` (adds up instead of waiting) |

## The pin map

`src/board/PinMap.h` is the one place for GPIO numbers, I2C addresses and expander legs. Three traps are pinned by `test_pin_map`: the modem's UART lines are named from the modem's side in the schematic and cross on the processor's side; the expander's legs are numbered in sequence (the manufacturer's `P10` is leg 8, the audio switch `P12` is leg 10); the charger is at 0x6A on newer boards and at 0x6B on older ones.

## The start-up order

`board::powerUp()` (`src/board/PowerUp.cpp`) brings the board up: the expander first (all legs outputs, then the boot state `0x9E`/`0x02`, in which the modem is off and the resets are released), the front light off, the touch reset held low for 20 ms and given 50 ms to settle, the keyboard reset the same, the boot button as an input and the SPI's MISO with a pull-up. Without an answering expander nothing behind it is touched, the firmware says so, and the processor's own pins are still set.

## Building

```sh
pio run -e device      # the firmware, compile-only; CI does the same
```

The board is PlatformIO's own `esp32-s3-devkitc-1`, set in `platformio.ini` to what the T-Deck Pro MAX has (ESP32-S3, 16 MB QIO flash, 8 MB QSPI PSRAM, USB serial on the processor's own port); the partition table is the core's default. **A build is not a test, and a test on the host is not a test on the device:** everything about behaviour on the device stays "not verified" until the owner's checklist ([bring-up.md](bring-up.md)) says otherwise. Flashing is the owner's decision.

## The console and the serial port

The console is the processor's own USB port. **Opening the serial port from a computer resets the ESP32**, which is why a tool that reads the logs restarts the device, and the firmware never waits for a computer to open the port: it writes whether or not anybody listens.

## What is not here

The LoRa radio has no device implementation: `RadioLink` has none, the firmware starts without a radio and the feature is off.

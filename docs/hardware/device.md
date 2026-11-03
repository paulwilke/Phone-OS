# The device: the firmware's platform code

Code: `device/` (the firmware's counterpart of `sim/`), the testable board logic in `src/board/`, the scripted buses in `testing/fakes/` (`FakeI2cBus`, `FakeGpio`, `FakeUartPort`, `FakeDelay`); tests in `test/test_pin_map/`, `test/test_board_fakes/`, `test/test_power_up/`. Hardware lessons that the code must honour: [hardware-lessons.md](hardware-lessons.md).

## Where what lives

| Place | Holds | May include |
|---|---|---|
| `src/board/` | everything about the board's chips that can run on a laptop: the pin map, the seams to the buses and pins, register decoding, start-up sequences | `src/board/`, `src/hal/`, `src/drivers/` |
| `device/` | only the implementations of the seams on the processor's peripherals and `main.cpp` | `src/` and the Arduino and ESP32 headers |
| `testing/fakes/` | scripted implementations of the seams | the seams |

`src/` never includes `device/`, and no file under `src/` names Arduino or an ESP32 header; `device/` never includes `sim/` or `testing/`; nothing includes MeshCore except `device/MeshCoreRadio.cpp`, the one adapter (D-59). The wiring of the whole firmware is in [device-console.md](device-console.md). `scripts/check_layers.py` checks all of it, and `test/test_check_layers/test_check.py` runs the checker on small trees that break each rule, so a rule that stops firing is noticed.

Two small helpers of `device/` are not seams: `device/SerialLog.h` writes a line of the event log to the USB console (the card's log, `SdEventLog`, hands it every line, and a start line that comes before the card goes to it directly), and `device/Esp32Heap.h` gives the free heap and the largest free block for log lines that look for a squeeze (the loading of the web catalogs, the Wi-Fi's link detail).

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

`board::powerUp()` (`src/board/PowerUp.cpp`) brings the board up: the expander first (all legs outputs, then the boot state `0x9E`/`0x02`, in which the modem is off and the resets are released), the front light off, the touch reset held low for 20 ms and given 50 ms to settle, the keyboard reset the same, the boot button as an input, the SPI's MISO with a pull-up, and the three wake lines (the keyboard's interrupt, the touch chip's, the modem's ring line) with the processor's pull-ups, so that an open-drain line idles high (decision D-34: without them the keyboard's and the ring line read low before every sleep of a night, and no sleep lasted). Without an answering expander nothing behind it is touched, the firmware says so, and the processor's own pins are still set.

## Building

```sh
pio run -e device      # the firmware, compile-only; CI does the same
```

The board is PlatformIO's own `esp32-s3-devkitc-1`, set in `platformio.ini` to what the T-Deck Pro MAX has (ESP32-S3, 16 MB QIO flash, 8 MB QSPI PSRAM, USB serial on the processor's own port); the partition table is the core's default. **A build is not a test, and a test on the host is not a test on the device:** everything about behaviour on the device stays "not verified" until the owner's checklist ([bring-up.md](bring-up.md)) says otherwise. Flashing is the owner's decision.

## The console and the serial port

The console is the processor's own USB port. **Opening the serial port from a computer resets the ESP32**, which is why a tool that reads the logs restarts the device, and the firmware never waits for a computer to open the port: it writes whether or not anybody listens.

## What is not here

## The LoRa radio

`RadioLink` has one device implementation, `device/MeshCoreRadio.{h,cpp}`, on MeshCore as it is (D-59): a chat node that never repeats a packet (`allowPacketForward` is false), with MeshCore's own cipher, contacts and airtime limit. The `device` environment fetches MeshCore at one pinned commit through `lib_deps` (with RadioLib, Crypto, RTClib, Melopero RV3028 and CayenneLPP, which its `library.json` asks for); nothing of it is stored here, the other environments do not fetch it, and `scripts/check_layers.py` fails on an include of it anywhere but in `MeshCoreRadio.cpp`. The chip is the SX1262 on the shared SPI bus (`boardSpi()`, chip select, reset, DIO1 and BUSY from `src/board/PinMap.h`), at MeshCore's EU narrow setting of 869.618 MHz, 62.5 kHz, SF 8, coding rate 4/5, 22 dBm, with an airtime factor of 20 (D-61: MeshCore's budget can spend two hours' share in one, so at most 9.6 % of any hour on air, under the 10 % of the sub-band). The setting is for Europe only (the 869.4 to 869.65 MHz sub-band); the device is used with the antenna it came with. MeshCore has a board variant for the T-Deck Pro and none for the Pro MAX; the chip's values (DIO2 as antenna switch, TCXO 2.4 V on DIO3, current limit 140 mA, boosted receive gain) are those of Meck's variant for the MAX, whose pins are the ones in `src/board/PinMap.h`.

Contacts are the chat nodes heard in advertisements (repeaters and rooms are not listed); a sent message gets one report in the order of sending, confirmed by the ACK or unconfirmed when MeshCore's window runs out. **Start and stop.** `device/main.cpp` creates the adapter only when the application asks for the radio (`takeRadioStartRequest()`: the SOS without a mobile network, D-62, or the radio page where the profile allows it) and destroys it on `takeRadioStopRequest()`; nothing runs in between that is not asked for. Starting it: the radio rail (expander leg `kRadioPower`, high is the SX1262's supply) and the antenna switch (leg `kRadioAntenna`, high is the internal antenna) of `src/board/PinMap.h` are set, the stored identity is read, `begin()` starts the chip, and the node advertises itself once and then every ten minutes while it runs (MeshCore delivers a message only from a node it knows, so a stranger can write only after the advertisement). The legs are those named in `PinMap.h` (its comments give their meaning; the boot state `0x9E`, read back on the device, already has both set). `loop()` runs at every pass of the main loop. The chip's power follows the attachment: `PowerCoordination` keeps the rail while a radio is attached and cuts it once it is detached. If the chip does not answer, the object is dropped, the rail is cut and the application ends the emergency (`radioStartFailed()`), so that save mode does not stay on without a radio.

**The identity** (the key pair, 96 bytes) is kept as hex in `/radio-identity.txt` in the root of the card, plain text like everything on it (D-55), and read at every start; without the file a new one is made and stored. The contacts are **not** kept: the adapter offers no way to store them, so after a restart the nodes are heard again from their advertisements. The file holds a private key: a card that is handed on carries it.

**Unverified on the device:** that the chip answers on this rail, that the sleep of the processor (light sleep up to two seconds) loses no packet, the advertisement, the message, the acknowledgement.

**Compiled, host-tested for the application side, not verified on the device:** `pio run -e device` builds the adapter and the start from `device/main.cpp`; the application's emergency is covered by `test_emergency_radio` and the scenario `radio-emergency`; whether the chip answers, whether an advertisement is heard, whether a message arrives and is confirmed have not been tried.

# The device's input and feedback: keyboard, touch, vibration, motion

Code: `src/board/` (`Tca8418`, `KeyboardDecoder`, `Cst66xx`, `Drv2605`, `BoardHaptic`, `BoardMotion`, `AccelerometerHub`), the device's part in `device/Esp32Bhi260Hub.h`; tests in `test/test_tca8418/`, `test_keyboard_decoder/`, `test_cst66xx/`, `test_board_haptic/`, `test_board_motion/` and, for the contracts, `test/test_hardware_contracts/` (`testing/BoardInputRigs.h`). The scripted chips are `FakeI2cBus` (registers, FIFOs, command replies) and `FakeAccelerometerHub`.

Everything that decides what a byte means is in `src/` and tested on the host with the values and sequences of the notes; the device only implements the I2C seam and the sensor hub. **Compiled and tested on the host is not verified on the device**: the first start prints every decoded input to the console (`[input] keyboard text U+0061`) so the owner's checklist ([device.md](device.md)) can check each key.

## Keyboard

`Tca8418` starts the matrix controller (scanner off, empty the FIFO the earlier session left, matrix of 8 rows by 10 columns, scanner on) and hands out key events; the register sequence is the one proven on the device, except register `0x29`: it is `DEBOUNCE_DIS_1`, a set bit switches the debounce of that row off, and the earlier code wrote `0x03`, which switched it off for twenty of the 35 keys (a bouncing key then types its letter twice). It is `0x00` here, the chip's default; whether the keys still bounce under longer typing is not verified.

`KeyboardDecoder` turns scan codes into `ui::Input`:

| Key | Gives |
|---|---|
| letters, Space | text; capital after Shift (a tap arms the next key, held it stays; both Shifts together do nothing) |
| Alt or Sym, then a key | the symbol or digit printed on that key (`W` is 1, `Q` is #, `P` is @, the mic key is 0, the $ key is $); the layer works for one key |
| Enter | `Key::Select` |
| Backspace | `Key::Back` (a text field deletes with it, an empty one goes back) |
| $ key | `Key::Up` |
| mic key | `Key::Down` |

The device has no arrow keys, and `W` and `S` cannot scroll as in the earlier code: a letter is text here, and the screens do not guess whether a text field has the focus. The two keys with no other use in this product, the $ key and the mic key, scroll instead; touch (the soft keys, taps on rows) reaches every function as well. The earlier code's chords for the backlights (both Shifts, Alt+B) are not carried over: nothing in the interface uses them.

## The three keys under the screen, and touch

They are **keys of the touch chip**, not of the keyboard. The chip is a CST66xx; the wiki and the pin list say CST328, and a driver for the wrong family reads machine code. `Cst66xxTouch` asks the chip for its identity (up to five tries, taking it out of its boot loader between them) and refuses to read a family it does not speak; the resolution comes from the chip (240 by 320, the display's own coordinate system, nothing converted or mirrored). A report is nine bytes at `0xD0070000`: the checksum, the kind, the number of fingers and keys, and one message of five bytes. Keys stand before fingers; a finger's coordinates are `x = b4 + (b7 & 0x0F) << 8` and `y = b5 + (b7 & 0xF0) << 4`; the event nibble of `b8` says whether the finger is down. The chip is read with the address ended by a stop and a separate read 200 µs later (a repeated start returns garbage), and its normal mode is set with the manufacturer's command sequence (without it the fingers come but the keys do not).

`TouchDecoder` makes **one touch per contact**, when the finger lands (a moving or resting finger is no second tap), and one hardware key per press on the way down: left `Key::Home`, middle `Key::Inbox`, right `Key::Compose` (confirmed on the device on 2026-10-01). A key needs a correct checksum, because an invented key press jumps to another screen and a swallowed one costs a second press; a finger is used as the device has proven it works. Coordinates outside the panel are clamped. Only the one finger and no gestures are evaluated. A chip of another family is reported and the touch is off; the cst3xx family of the earlier code, which no device of this series has, is not carried over.

## Vibration

`Drv2605` runs the sequence of the chip's own library (an ERM motor in open loop, effect library 1). `BoardHaptic` plays the patterns of `HapticPattern` through it and passes the `Haptic` contract. The motor's supply (expander leg 5) is switched on with the first pulse and **off two seconds after the last**; the earlier code left it on after the first use, and the driver draws current while it idles. A motor that does not answer loses its pulses, the pattern runs out and the supply goes off again. The effect numbers are the data sheet's; the rhythms have been felt only as far as "the motor runs" (2026-10-01).

## Motion

`BoardMotion` switches the sensor's 1.8 V rail (expander leg 3, on in the boot state, so it is switched off at start), waits 30 ms, and starts the hub; `stop()` stops it and cuts the rail, because the firmware lives in the chip's RAM. The chip is a small computer of its own: starting loads its firmware and takes about 11 seconds. The hub's implementation (`device/Esp32Bhi260Hub.h`) uses SensorLib; **the Bosch firmware image inside it is not part of this repository and its redistribution terms are not verified** ([THIRD_PARTY.md](../THIRD_PARTY.md)).

**The samples are handed on unchanged.** Lying flat the device reads about −0.95 on z; the motion page's mapping from the sensor's axes to the picture ([screen-motion.md](screen-motion.md)) was checked with these values, so a "correction" of the sign would flip every direction. `test_board_motion` passes the values of that page's table through `boot::SensorMotionSource` and checks they arrive as they were.

## Not verified on the device

Every key and the layer symbols against the printed keys, the bounce of the keys, the position of a touch against the picture, the three keys, the felt rhythm of the patterns and the supply cut after two seconds (the driver must start again after it), and the whole motion start (rail, firmware, stream).

#pragma once

// Everything that is the panel's and not the controller's: the data of its datasheet and of the vendor's demo.
// It is data, handed to the driver; the driver contains no number of it. A profile that is not there means the
// panel is not driven: the firmware says "the display is not configured" instead of sending values nobody
// has checked to a part that can be damaged by them.

#include <cstddef>
#include <cstdint>
#include <vector>

#include "../ui/Display.h"

namespace board {

// One command with its data bytes.
struct PanelCommand {
  uint8_t command;
  std::vector<uint8_t> data;
};

struct PanelProfile {
  int width = 0;    // the panel's memory, in pixels
  int height = 0;
  // After the hardware reset: the commands that bring the controller up (panel setting, power, booster,
  // resolution, data interval, waveform tables) and power it on.
  std::vector<PanelCommand> start;
  // Sent before the data of a full refresh and of a partial one (for example the waveform of that kind).
  std::vector<PanelCommand> beforeFull;
  std::vector<PanelCommand> beforePartial;
  // Is a black pixel a clear bit in the data the controller takes?
  bool blackIsZero = false;
  // The data bytes of the partial window command for this rectangle (x and width in multiples of 8); empty
  // output means this window cannot be addressed.
  std::vector<uint8_t> (*encodeWindow)(const ui::Rect& window) = nullptr;
  // The commands that put the controller to sleep after a refresh (power off, deep sleep with its code).
  std::vector<PanelCommand> sleep;
  uint32_t resetLowMs = 0;
  uint32_t resetHighMs = 0;
  uint32_t busyTimeoutMs = 0;
  // Does the controller report busy with a high level?
  bool busyIsHigh = false;
};

}  // namespace board

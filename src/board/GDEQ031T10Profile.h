#pragma once

// The panel's values for the display of the T-Deck Pro Max: Good Display GDEQ031T10 (240 x 320, 1 bit) on a
// UltraChip UC8253. Where each value comes from is written at the value (GDEQ031T10Profile.cpp) and in
// docs/hardware/device-display.md.

#include "PanelProfile.h"

namespace board {

const PanelProfile& gdeq031t10Profile();

}  // namespace board

#pragma once

// The picture of the device itself: the project's name, the board and the version, centred, with an optional line
// under them. At start it is drawn before anything else, as 3.x did (owner, 2026-10-08: it was missing in 4.x); it
// has no catalog words, because it comes before the catalogs are read. Before the device switches off it is drawn
// with the line "Switched off": e-ink keeps its last picture without power, and a device that went off showing a page
// looks like one that still runs.

#include <string>

#include "ui/Display.h"

namespace ui {

// `name` is the project's ("Phone OS"), `board` the device's ("T-Deck Pro MAX"), `version` the firmware's;
// `line` goes under them. The name and the line are wrapped.
void drawSplash(Display& display, const std::string& name, const std::string& board, const std::string& version,
                const std::string& line = std::string());

}  // namespace ui

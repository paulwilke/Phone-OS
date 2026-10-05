#pragma once

// What the status bar shows, taken from the core's state. The bar itself (StatusBar.h) knows only
// its own small model; this is the translation from the core, so that every screen draws the same
// bar and a change of what the bar shows is made once.
//
// The signal strength is not on the bar. The bar changes rarely on purpose, because every change is
// a redraw of the e-ink panel, and the strength changes with every step the child takes; it is on
// the network and status pages, which are opened on purpose.

#include "app/AppCore.h"
#include "ui/StatusBar.h"

namespace ui {

StatusModel statusModelOf(const AppCore::State& state);

}  // namespace ui

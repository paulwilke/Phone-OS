#pragma once

// The line of words at the top of a page of the options (settings, Wi-Fi, network, status,
// language) and the rule under it. It wraps when the word is long in a language, so that nothing is
// cut; the page lays out what is left below `pageTitleHeight`.

#include <string>

#include "ui/Display.h"

namespace ui {

// How much room the title and its rule take in `area`.
int pageTitleHeight(const Rect& area, const std::string& title);

// Draws it at the top of `area` and returns the height it took.
int drawPageTitle(Display& display, const Rect& area, const std::string& title);

}  // namespace ui

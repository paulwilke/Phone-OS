#pragma once

// Where the standard parts of a screen go on the 240 x 320 display, from the top: the status
// bar, the alert bar, the content, the soft keys. The alert bar keeps its place when empty:
// a bar that appears and disappears shoves the content down and up, which on e-ink is two
// full redraws and a finger that hits another tile than the one it aimed at.

#include "ui/Display.h"

namespace ui {

constexpr int kStatusBarHeight = 22;
constexpr int kAlertBarHeight = 20;
constexpr int kSoftKeysHeight = 28;
constexpr int kContentGap = 2;  // between the content and the soft keys

struct ScreenLayout {
  Rect status;
  Rect alert;
  Rect content;
  Rect softKeys;

  static ScreenLayout standard(int width = kScreenWidth, int height = kScreenHeight) {
    ScreenLayout l;
    l.status = {0, 0, width, kStatusBarHeight};
    l.alert = {0, kStatusBarHeight, width, kAlertBarHeight};
    l.softKeys = {0, height - kSoftKeysHeight, width, kSoftKeysHeight};
    const int top = kStatusBarHeight + kAlertBarHeight + kContentGap;
    l.content = {0, top, width, l.softKeys.y - kContentGap - top};
    return l;
  }
};

}  // namespace ui

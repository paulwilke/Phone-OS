#pragma once

// Motion: the acceleration sensor to look at — a spirit level, and a count of how often the device was shaken.
// The ball rolls the way the device is tilted; flat on a table it sits in the middle. It moves in steps of a
// field (0.15 g, seven fields from side to side, about 25 degrees at the edge) and not smoothly: every
// change of the picture costs the e-ink panel a refresh, and a pixel that trembled would redraw all the time.
//
// **The sensor runs only while the page is open** (it is a small computer of its own and costs current), and
// starting it takes about 11 seconds and blocks. So the page first draws "Sensor is starting", and only on
// the next tick, when that frame is on the panel, starts it. It stops again whenever the page is left by any
// way (onLeave), the lock included.
//
// **The mapping from the sensor to the screen.** The sensor sits turned 90 degrees against the screen, so its
// y axis does what the screen's horizontal does, and its x axis the vertical. The ball rolls to the lower
// side: with the sensor's values as the device reports them, the ball is at `-cell(y)` across and
// `-cell(x)` up. This is the mapping that was found wrong on the first device and corrected on the device
// (checked by hand on 2026-10-03), and the tests pin it value by value (see screen-motion.md).
//
// Enter sets the shake counter to zero. Without a sensor the page says so.

#include "app/ShakeDetector.h"
#include "ui/AppScreen.h"
#include "ui/MotionSource.h"

namespace ui {

class MotionScreen : public AppScreen {
public:
  MotionScreen(ScreenContext context, MotionSource& source) : AppScreen(context), source_(source) {}

  void onEnter() override;
  void onLeave() override;
  void onTick() override;

  // The ball's field, -3 .. +3 from the middle: x positive = right, y positive = up on the screen.
  int ballX() const { return -cell(last_.y); }
  int ballY() const { return -cell(last_.x); }
  uint32_t shakes() const { return shake_.count(); }

  // A field is 0.15 g.
  static int cell(float g);

protected:
  void draw(Display& display, const Rect& area) override;
  SoftKeys keys() override;
  bool onInput(const Input& input, const Rect& area) override;

private:
  enum class Phase { Idle, NoSensor, Starting, Failed, Running };

  MotionSource& source_;
  ShakeDetector shake_;
  Acceleration last_;
  Phase phase_ = Phase::Idle;
  bool startingShown_ = false;  // the frame that says "starting" has been drawn
};

}  // namespace ui

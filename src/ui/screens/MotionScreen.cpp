#include "ui/screens/MotionScreen.h"

#include <algorithm>
#include <cmath>
#include <string_view>

#include "ui/Fonts.h"
#include "ui/Formats.h"
#include "ui/PageTitle.h"
#include "ui/Shapes.h"
#include "ui/Text.h"

namespace ui {

namespace {
constexpr float kFieldG = 0.15f;
constexpr int kFields = 7;
constexpr int kMaxCell = 3;
}  // namespace

int MotionScreen::cell(float g) {
  const int c = static_cast<int>(std::lround(g / kFieldG));
  return std::max(-kMaxCell, std::min(kMaxCell, c));
}

void MotionScreen::onEnter() {
  shake_.reset();
  last_ = Acceleration();
  startingShown_ = false;
  phase_ = source_.available() ? Phase::Starting : Phase::NoSensor;
}

void MotionScreen::onLeave() {
  if (source_.running()) source_.stop();
  phase_ = Phase::Idle;
}

void MotionScreen::onTick() {
  if (phase_ == Phase::Starting && startingShown_) {
    phase_ = source_.start() ? Phase::Running : Phase::Failed;
    return;
  }
  if (phase_ != Phase::Running) return;
  Acceleration sample;
  if (source_.read(sample) && sample.valid) {
    last_ = sample;
    shake_.feed(sample.x, sample.y, sample.z);   // every sample counts, however seldom the picture is drawn
  }
}

void MotionScreen::draw(Display& display, const Rect& area) {
  const int header = drawPageTitle(display, area, t().get("motion.title"));
  const Font& body = font(FontSize::Text);
  const Font& small = font(FontSize::Small);
  const Rect below{area.x, area.y + header, area.w, area.h - header};

  const bool measuring = phase_ == Phase::Running && last_.valid;
  if (!measuring) {
    const char* key = phase_ == Phase::NoSensor ? "motion.no-sensor" : phase_ == Phase::Failed ? "motion.failed" : "motion.starting";
    const std::string words = t().get(key);                             // the lines are views into it
    int y = below.y + 6;
    for (std::string_view line : text::wrap(body, words, below.w - 12)) {
      text::draw(display, body, below.x + 6, y, line);
      y += body.lineHeight();
    }
    if (phase_ == Phase::Starting) startingShown_ = true;               // from now on the next tick may start it
    return;
  }

  // The spirit level: a square of seven by seven fields with a cross, and the ball in its field.
  const int room = below.h - 2 * body.lineHeight() - 8;
  int side = std::min(room, below.w - 20);
  side -= side % (2 * kFields);                                         // seven fields that divide evenly
  const int field = side / kFields;
  const int x0 = below.x + (below.w - side) / 2;
  const int y0 = below.y + 4;
  display.fillRect({x0, y0, side, 1}, true);
  display.fillRect({x0, y0 + side - 1, side, 1}, true);
  display.fillRect({x0, y0, 1, side}, true);
  display.fillRect({x0 + side - 1, y0, 1, side}, true);
  display.fillRect({x0 + side / 2, y0 + 2, 1, side - 4}, true);
  display.fillRect({x0 + 2, y0 + side / 2, side - 4, 1}, true);
  const int cx = x0 + side / 2 + ballX() * field;
  const int cy = y0 + side / 2 - ballY() * field;
  shapes::fillRoundRect(display, {cx - field / 2, cy - field / 2, field, field}, field / 2, true);

  int y = y0 + side + 6;
  text::drawCentered(display, body, below, y, t().get("motion.shakes", {{"n", static_cast<int>(shake_.count())}}));
  y += body.lineHeight() + 2;
  const float magnitude = std::sqrt(last_.x * last_.x + last_.y * last_.y + last_.z * last_.z);
  const std::string g = format::tenths(magnitude);
  text::drawCentered(display, small, below, y, t().get("motion.force", {{"g", g.c_str()}}));
}

SoftKeys MotionScreen::keys() {
  SoftKeys keys;
  keys.back = t().get("key.back");
  if (phase_ == Phase::Running && last_.valid) keys.action = t().get("motion.reset");
  return keys;
}

bool MotionScreen::onInput(const Input& input, const Rect&) {
  if (input.isKey(Key::Back)) {
    nav().back();                      // the page's onLeave stops the sensor
    return true;
  }
  if (input.isKey(Key::Select) && phase_ == Phase::Running && last_.valid) {
    shake_.reset();
    return true;
  }
  return false;
}

}  // namespace ui

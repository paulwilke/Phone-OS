#include "ui/screens/SosScreen.h"

#include <algorithm>
#include <string>
#include <string_view>

#include "ui/Fonts.h"
#include "ui/Shapes.h"
#include "ui/Text.h"
#include "ui/Wording.h"

namespace ui {

namespace {

constexpr int kGap = 8;

// A paragraph of fixed words, wrapped and centred; the y below it. The words are a parameter so that
// the lines, which are views into them, live as long as they are drawn.
int paragraph(Display& display, const Font& font, const Rect& area, int y, const std::string& words, bool black = true) {
  for (std::string_view line : text::wrap(font, words, area.w - 16)) {
    text::drawCentered(display, font, area, y, line, black);
    y += font.lineHeight();
  }
  return y;
}

int paragraphHeight(const Font& font, const Rect& area, const std::string& words) {
  return static_cast<int>(text::wrap(font, words, area.w - 16).size()) * font.lineHeight();
}

}  // namespace

bool SosScreen::preventsAutoLock() const {
  const AppCore::State s = context().app.state();
  return s.sosPhase != SosFlow::Phase::Idle || s.emergencyRunning;
}

SosScreen::View SosScreen::view() const {
  if (state().sosPhase == SosFlow::Phase::Choosing) return View::Choice;
  if (state().sosPhase == SosFlow::Phase::QuietConfirm) return View::Quiet;
  // A reply to a parent's query is not the child's business; only an emergency or a pick-up is shown.
  const bool ours = state().pingKind != SosPing::Kind::Query;
  const bool known = state().emergencyRunning || state().pingStage == SosPing::Stage::Done || state().pingStage == SosPing::Stage::Failed;
  return ours && known ? View::Progress : View::Idle;
}

int SosScreen::titleBottom(const Rect& area) const { return area.y + 2 + font(FontSize::Large).lineHeight() + 4; }

Rect SosScreen::choiceRect(const Rect& area, size_t index) {
  // Two digits of seconds: the countdown line takes as many lines as it will ever need.
  const std::string countdown = t().get("sos.countdown", {{"seconds", 99}});
  const int top = titleBottom(area) + paragraphHeight(font(FontSize::Text), area, countdown) + kGap;
  const int height = (area.bottom() - top - 2 * kGap) / 3;
  return {area.x + 8, top + static_cast<int>(index) * (height + kGap), area.w - 16, height};
}

std::string SosScreen::quietWords() {
  const std::string until = wording::clockTime(state().schoolTimeEndMinute);
  return t().get("sos.quiet-locked", {{"time", until.c_str()}});
}

std::string SosScreen::quietHint() {
  return t().get("sos.quiet-hint", {{"n", SosFlow::kOverrideTaps}, {"seconds", static_cast<int>(SosFlow::kTapWindowMs / 1000)}});
}

// The big button under the two paragraphs, as high as the room allows.
Rect SosScreen::quietButton(const Rect& area) {
  const int y = titleBottom(area) + paragraphHeight(font(FontSize::Text), area, quietWords()) + 6 +
                paragraphHeight(font(FontSize::Small), area, quietHint());
  const int bottom = area.bottom() - 4;
  const int height = std::min(110, std::max(40, bottom - y - 8));
  return {area.x + 24, y + 8 + (bottom - y - 8 - height) / 2, area.w - 48, height};
}

void SosScreen::draw(Display& display, const Rect& area) {
  const Font& large = font(FontSize::Large);
  text::drawCentered(display, large, area, area.y + 2, t().get("home.sos"));
  const int top = titleBottom(area);
  switch (view()) {
    case View::Choice: drawChoice(display, area, top); break;
    case View::Quiet: drawQuiet(display, area, top); break;
    case View::Progress: drawProgress(display, area, top); break;
    case View::Idle: paragraph(display, font(FontSize::Text), area, top + 20, t().get("sos.idle")); break;
  }
}

void SosScreen::drawChoice(Display& display, const Rect& area, int top) {
  const Font& body = font(FontSize::Text);
  const std::string countdown = t().get("sos.countdown", {{"seconds", static_cast<int>(state().sosSecondsLeft)}});
  paragraph(display, body, area, top, countdown);
  const char* keysOf[3] = {"sos.emergency", "sos.pickup", "sos.cancel"};
  for (size_t i = 0; i < 3; ++i) {
    const Rect box = choiceRect(area, i);
    const bool focused = i == selection_;
    if (focused) shapes::fillRoundRect(display, box, 8, true);
    else shapes::outlineRoundRect(display, box, 8, true);
    // The number is the key that chooses it. The word is fixed and wraps instead of being cut.
    const std::string label = std::to_string(i + 1) + " " + t().get(keysOf[i]);
    const auto lines = text::wrap(body, label, box.w - 16);
    int y = box.y + (box.h - static_cast<int>(lines.size()) * body.lineHeight()) / 2;
    for (std::string_view line : lines) {
      text::drawCentered(display, body, box, y, line, !focused);
      y += body.lineHeight();
    }
  }
}

void SosScreen::drawQuiet(Display& display, const Rect& area, int top) {
  int y = paragraph(display, font(FontSize::Text), area, top, quietWords());
  paragraph(display, font(FontSize::Small), area, y + 6, quietHint());
  // The button: large, with the number of taps that are still needed.
  const Rect button = quietButton(area);
  shapes::fillRoundRect(display, button, 12, true);
  const Font& large = font(FontSize::Large);
  text::drawCentered(display, large, button, button.y + (button.h - large.lineHeight()) / 2, std::to_string(state().sosTapsLeft), false);
}

void SosScreen::drawProgress(Display& display, const Rect& area, int top) {
  const Font& body = font(FontSize::Text);
  const Font& small = font(FontSize::Small);
  const bool failed = state().pingStage == SosPing::Stage::Failed;
  const bool done = state().pingStage == SosPing::Stage::Done;

  // What was chosen, so that it is plain which of the two is under way.
  int y = paragraph(display, small, area, top, t().get(state().pingKind == SosPing::Kind::Pickup ? "sos.pickup" : "sos.emergency"));
  y += 6;
  // Three bars: filled is done, and the last fills only when the text is really out. The core counts a
  // failed ping as three of three; drawn so, a text that never left would look like a success.
  const int filled = failed ? std::min(state().pingProgress, 2) : state().pingProgress;
  const int barWidth = (area.w - 24 - 2 * kGap) / 3;
  for (int i = 0; i < 3; ++i) {
    const Rect bar{area.x + 12 + i * (barWidth + kGap), y, barWidth, 26};
    if (i < filled) shapes::fillRoundRect(display, bar, 6, true);
    else shapes::outlineRoundRect(display, bar, 6, true);
  }
  y += 26 + 10;
  const char* stepKey = failed ? "sos.failed" : state().pingProgress >= 3 ? "sos.step-sent" : state().pingProgress == 2 ? "sos.step-found" : "sos.step-search";
  y = paragraph(display, body, area, y, t().get(stepKey));
  y += 6;

  std::string detail;
  if (failed) {
    detail = t().get("sos.failed-detail");
  } else if (done) {
    detail = state().pingHasFix ? t().get("sos.sent-accuracy", {{"meters", static_cast<int>(state().pingAccuracyMeters)}}) : t().get("sos.sent-no-fix");
  } else if (state().pingHasFix) {
    detail = t().get("sos.refining", {{"seconds", static_cast<int>(state().pingElapsedSeconds)}, {"meters", static_cast<int>(state().pingAccuracyMeters)}});
  } else {
    detail = t().get("sos.searching", {{"seconds", static_cast<int>(state().pingElapsedSeconds)}});
  }
  y = paragraph(display, small, area, y, detail);
  y += 4;
  if (done) {
    const std::string name = wording::peerName(state().pingPeer, context().app.profile().contacts, t());
    paragraph(display, small, area, y, t().get(state().queued > 0 ? "sos.sent-waiting" : "sos.sent-to", {{"name", name.c_str()}}));
  }
}

SoftKeys SosScreen::keys() {
  SoftKeys keys;
  keys.back = t().get("key.back");
  switch (view()) {
    case View::Choice:
      keys.scroll = true;
      keys.action = t().get("key.choose");
      break;
    case View::Quiet: keys.action = t().get("sos.tap"); break;
    case View::Progress:
      if (state().pingStage == SosPing::Stage::Failed) keys.action = t().get("sos.again");
      break;
    case View::Idle: break;
  }
  return keys;
}

bool SosScreen::choose(size_t index) {
  if (index == 0) app().chooseSos(SosPing::Kind::Emergency);
  else if (index == 1) app().chooseSos(SosPing::Kind::Pickup);
  else nav().back();   // leaving releases the search (onLeave)
  return true;
}

std::vector<Rect> SosScreen::targets(const Rect& area) {
  switch (view()) {
    case View::Choice: return {choiceRect(area, 0), choiceRect(area, 1), choiceRect(area, 2)};
    case View::Quiet: return {quietButton(area)};
    case View::Progress:
    case View::Idle: break;
  }
  return {};
}

bool SosScreen::onInput(const Input& input, const Rect& area) {
  switch (view()) {
    case View::Choice:
      if (input.kind == Input::Kind::Touch) {
        for (size_t i = 0; i < 3; ++i)
          if (choiceRect(area, i).contains(input.x, input.y)) return choose(i);
        return false;
      }
      if (input.kind == Input::Kind::Text) {
        if (input.character >= '1' && input.character <= '3') return choose(input.character - '1');
        return false;
      }
      if (input.isKey(Key::Up)) {
        selection_ = selection_ ? selection_ - 1 : 2;
        return true;
      }
      if (input.isKey(Key::Down)) {
        selection_ = (selection_ + 1) % 3;
        return true;
      }
      if (input.isKey(Key::Select)) return choose(selection_);
      if (input.isKey(Key::Back)) return choose(2);
      return false;
    case View::Quiet:
      if (input.isKey(Key::Select)) {
        app().tapSos();
        return true;
      }
      if (input.kind == Input::Kind::Touch) {
        if (!quietButton(area).contains(input.x, input.y)) return false;
        app().tapSos();
        return true;
      }
      if (input.isKey(Key::Back)) {
        nav().back();
        return true;
      }
      return false;
    case View::Progress:
      if (input.isKey(Key::Select) && state().pingStage == SosPing::Stage::Failed) return app().beginSos();
      if (input.isKey(Key::Back)) {
        nav().back();
        return true;
      }
      return false;
    case View::Idle:
      if (input.isKey(Key::Back)) {
        nav().back();
        return true;
      }
      return false;
  }
  return false;
}

}  // namespace ui

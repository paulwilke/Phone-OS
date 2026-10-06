#include "ui/screens/SelfTestScreen.h"

#include <algorithm>
#include <cstdio>
#include <string_view>
#include <vector>

#include "ui/Fonts.h"
#include "ui/Formats.h"
#include "ui/Shapes.h"
#include "ui/Text.h"

namespace ui {

namespace {

constexpr HapticPattern kPatterns[] = {HapticPattern::Click, HapticPattern::Ring, HapticPattern::Message,
                                       HapticPattern::Sent, HapticPattern::Failed};
constexpr size_t kPatternCount = sizeof(kPatterns) / sizeof(kPatterns[0]);
const char* const kPatternWords[] = {"selftest.pattern-click", "selftest.pattern-ring", "selftest.pattern-message",
                                     "selftest.pattern-sent", "selftest.pattern-failed"};
constexpr int kHeaderGap = 3;

// A code point as UTF-8, for the key that was typed.
std::string utf8(uint32_t c) {
  std::string out;
  if (c < 0x80) {
    out += static_cast<char>(c);
  } else if (c < 0x800) {
    out += static_cast<char>(0xC0 | (c >> 6));
    out += static_cast<char>(0x80 | (c & 0x3F));
  } else if (c < 0x10000) {
    out += static_cast<char>(0xE0 | (c >> 12));
    out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (c & 0x3F));
  } else {
    out += static_cast<char>(0xF0 | (c >> 18));
    out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (c & 0x3F));
  }
  return out;
}

}  // namespace

SelfTestScreen::SelfTestScreen(ScreenContext context) : AppScreen(context), grid_(2, 4) {}

void SelfTestScreen::onEnter() {
  mode_ = Mode::Menu;
  result_ = Result::None;
  pattern_ = 0;
  gpsTest_ = false;
  lastKey_.clear();
  keyPressed_ = false;
  tapX_ = tapY_ = -1;
  grid_.resetSelection();
}

void SelfTestScreen::onLeave() {
  if (gpsTest_) app().setTestPageOpen(false);
  gpsTest_ = false;
}

void SelfTestScreen::onTick() {
  if ((mode_ == Mode::Keys || mode_ == Mode::Touch) && uptime() - lastInput_ >= kIdleSeconds) mode_ = Mode::Menu;
}

// The tiles carry the words of the moment; set before drawing and before input, so that input works even on
// the first frame.
void SelfTestScreen::refreshTiles() {
  std::vector<Tile> tiles;
  for (const char* name : {"selftest.vibration", "selftest.light", "selftest.display", "selftest.keys", "selftest.touch",
                           "selftest.gps", "selftest.motion", "selftest.network"}) {
    Tile tile;
    tile.label = t().get(name);
    tiles.push_back(std::move(tile));
  }
  grid_.setPages({std::move(tiles)});
}

// The room of the header and of the result is the largest any state of them takes in this language, so that
// the grid stays where it is when a check reports and the words wrap longer.
SelfTestScreen::Reserved SelfTestScreen::reserved(const Rect& area) {
  const Font& small = font(FontSize::Small);
  const auto lines = [&](const std::string& words) { return static_cast<int>(text::wrap(small, words, area.w - 8).size()); };
  int headLines = 0;
  for (bool light : {false, true})
    for (bool gps : {false, true}) headLines = std::max(headLines, lines(headerWords(light, gps, 100)));
  int resultLines = 2;
  for (size_t pattern = 0; pattern < kPatternCount; ++pattern) resultLines = std::max(resultLines, lines(resultWords(Result::Vibration, pattern, false, GpsState::Off)));
  for (bool light : {false, true}) resultLines = std::max(resultLines, lines(resultWords(Result::Light, 0, light, GpsState::Off)));
  for (GpsState gps : {GpsState::Off, GpsState::Searching, GpsState::Fix}) resultLines = std::max(resultLines, lines(resultWords(Result::Gps, 0, false, gps)));
  return {headLines, resultLines};
}

Rect SelfTestScreen::gridArea(const Rect& area) {
  const Reserved room = reserved(area);
  const int top = (room.headLines + room.resultLines) * font(FontSize::Small).lineHeight() + 2 * kHeaderGap + 1;
  return {area.x, area.y + top, area.w, area.h - top};
}

std::string SelfTestScreen::headerWords(bool light, bool gps, int percent) {
  const std::string lightWord = t().get(light ? "selftest.on" : "selftest.off");
  const std::string gpsWord = t().get(gps ? "selftest.on" : "selftest.off");
  return t().get("selftest.status", {{"light", lightWord.c_str()}, {"gps", gpsWord.c_str()}}) + "\n" +
         t().get("selftest.battery", {{"percent", percent}});
}

// The two lines of a result, as one text with a line break.
std::string SelfTestScreen::resultWords(Result result, size_t pattern, bool lightOn, GpsState gps) {
  switch (result) {
    case Result::None: return std::string("\n");
    case Result::Vibration: {
      const std::string name = t().get(kPatternWords[pattern]);
      return t().get("selftest.vibration-result", {{"pattern", name.c_str()}}) + "\n" + t().get("selftest.felt");
    }
    case Result::Light: return t().get(lightOn ? "selftest.light-on" : "selftest.light-off") + "\n" + t().get("selftest.brighter");
    case Result::Gps:
      switch (gps) {
        case GpsState::Off: return t().get("selftest.gps-off") + "\n";
        case GpsState::Searching: return t().get("selftest.gps-searching") + "\n" + t().get("selftest.gps-no-fix");
        case GpsState::Fix: {
          GnssFix fix;
          uint32_t age = 0;
          app().lastKnownFix(fix, &age);                                // the numbers of the fix, or the widest of them
          const std::string hdop = format::tenths(fix.valid ? fix.hdop : 99.9f);
          return t().get("selftest.gps-fix", {{"sats", static_cast<int>(fix.sats)}}) + "\n" +
                 t().get("selftest.gps-accuracy", {{"meters", static_cast<int>(SosPing::accuracyMeters(fix.valid ? fix.hdop : 99.9f))}, {"hdop", hdop.c_str()}});
        }
      }
  }
  return std::string("\n");
}

// What the last check says now.
std::string SelfTestScreen::currentResult() {
  GpsState gps = GpsState::Off;
  if (gpsTest_) {
    GnssFix fix;
    uint32_t age = 0;
    const bool live = state().positioningActive && app().lastKnownFix(fix, &age) && fix.valid && age <= 5;
    gps = live ? GpsState::Fix : GpsState::Searching;
  }
  return resultWords(result_, played_, state().frontlight, gps);
}

std::string SelfTestScreen::currentHeader() {
  return headerWords(state().frontlight, state().positioningActive, state().batteryPercent);
}

void SelfTestScreen::draw(Display& display, const Rect& area) {
  switch (mode_) {
    case Mode::Menu: drawMenu(display, area); break;
    case Mode::Pattern: drawPattern(display, area); break;
    case Mode::Keys:
    case Mode::Touch: drawInput(display, area); break;
  }
}

void SelfTestScreen::drawMenu(Display& display, const Rect& area) {
  const Font& small = font(FontSize::Small);
  const Reserved room = reserved(area);
  const std::string head = currentHeader();                             // the lines are views into these
  const std::string result = currentResult();
  int y = area.y;
  for (std::string_view line : text::wrap(small, head, area.w - 8)) {
    text::draw(display, small, area.x + 4, y, line);
    y += small.lineHeight();
  }
  // The rule and the result lie where the reserved header ends, wherever the header's words end.
  y = area.y + room.headLines * small.lineHeight() + kHeaderGap / 2;
  display.fillRect({area.x, y, area.w, 1}, true);
  y += kHeaderGap;
  for (std::string_view line : text::wrap(small, result, area.w - 8)) {
    text::draw(display, small, area.x + 4, y, line);
    y += small.lineHeight();
  }

  refreshTiles();
  grid_.draw(display, gridArea(area));
}

// Stripes of changing width above and a chequerboard below: does the panel show evenly?
void SelfTestScreen::drawPattern(Display& display, const Rect& area) {
  const int half = area.h / 2;
  for (int x = 0, w = 2; x < area.w; x += w * 2, w = w < 16 ? w + 2 : 2)
    display.fillRect({area.x + x, area.y, std::min(w, area.w - x), half}, true);
  const int cell = 12;
  for (int yy = half; yy < area.h; yy += cell) {
    for (int xx = ((yy - half) / cell) % 2 ? cell : 0; xx < area.w; xx += 2 * cell)
      display.fillRect({area.x + xx, area.y + yy, std::min(cell, area.w - xx), std::min(cell, area.h - yy)}, true);
  }
}

void SelfTestScreen::drawInput(Display& display, const Rect& area) {
  const Font& small = font(FontSize::Small);
  const Font& body = font(FontSize::Text);
  const bool keysMode = mode_ == Mode::Keys;
  const std::string title = t().get(keysMode ? "selftest.keys-title" : "selftest.touch-title");
  int y = area.y;
  for (std::string_view line : text::wrap(small, title, area.w - 8)) {
    text::draw(display, small, area.x + 4, y, line);
    y += small.lineHeight();
  }
  y += kHeaderGap / 2;
  display.fillRect({area.x, y, area.w, 1}, true);
  y += 6;

  if (keysMode) {
    const std::string name = keyPressed_ ? lastKey_ : t().get("selftest.no-key");
    for (std::string_view line : text::wrap(body, name, area.w - 16)) {
      text::draw(display, body, area.x + 8, y, line);
      y += body.lineHeight();
    }
  } else {
    const std::string tap = tapX_ >= 0 ? t().get("selftest.tap", {{"x", tapX_}, {"y", tapY_}}) : t().get("selftest.no-tap");
    text::drawCentered(display, body, area, y, tap);
    if (tapX_ >= 0) {                                                  // a cross on the spot, to see axes and distance
      display.fillRect({tapX_ - 8, tapY_, 17, 2}, true);
      display.fillRect({tapX_, tapY_ - 8, 2, 17}, true);
    }
  }
  const uint32_t waited = std::min(uptime() - lastInput_, kIdleSeconds);
  const std::string ends = t().get("selftest.ends-in", {{"n", static_cast<int>(kIdleSeconds - waited)}});
  const auto endLines = text::wrap(small, ends, area.w - 8);          // the lines are views into `ends`
  int ey = area.bottom() - static_cast<int>(endLines.size()) * small.lineHeight();
  for (std::string_view line : endLines) {
    text::draw(display, small, area.x + 4, ey, line);
    ey += small.lineHeight();
  }
}

SoftKeys SelfTestScreen::keys() {
  SoftKeys keys;
  switch (mode_) {
    case Mode::Menu:
      keys.back = t().get("key.back");
      keys.scroll = true;
      keys.action = t().get("selftest.run");
      break;
    case Mode::Pattern:
    case Mode::Touch: keys.back = t().get("selftest.done"); break;
    case Mode::Keys: break;   // every key is a key to test, Backspace too: no soft key, the test ends by itself
  }
  return keys;
}

std::string SelfTestScreen::keyName(const Input& input) {
  if (input.kind == Input::Kind::Key) {
    switch (input.key) {
      case Key::Up: return t().get("selftest.key-up");
      case Key::Down: return t().get("selftest.key-down");
      case Key::Select: return t().get("selftest.key-enter");
      case Key::Back: return t().get("selftest.key-back");
      case Key::Home: return t().get("selftest.key-home");
      case Key::Inbox: return t().get("selftest.key-inbox");
      case Key::Compose: return t().get("selftest.key-compose");
    }
  }
  if (input.character == ' ') return t().get("selftest.key-space");
  if (input.character >= 0x20 && input.character != 0x7F) {
    const std::string character = utf8(input.character);
    return t().get("selftest.key-char", {{"char", character.c_str()}});
  }
  char code[12];
  std::snprintf(code, sizeof(code), "0x%02X", static_cast<unsigned>(input.character));
  return t().get("selftest.key-code", {{"code", code}});
}

void SelfTestScreen::run(Test test) {
  switch (test) {
    case Test::Vibration:
      played_ = pattern_;
      app().playTestPattern(kPatterns[pattern_]);
      pattern_ = (pattern_ + 1) % kPatternCount;
      result_ = Result::Vibration;
      break;
    case Test::Light:
      app().toggleFrontlight();
      result_ = Result::Light;
      break;
    case Test::Display: mode_ = Mode::Pattern; break;
    case Test::Keys:
      mode_ = Mode::Keys;
      lastInput_ = uptime();
      keyPressed_ = false;
      break;
    case Test::Touch:
      mode_ = Mode::Touch;
      lastInput_ = uptime();
      tapX_ = tapY_ = -1;
      break;
    case Test::Gps:
      gpsTest_ = !gpsTest_;
      app().setTestPageOpen(gpsTest_);
      result_ = Result::Gps;
      break;
    case Test::Motion: nav().open(screen::kMotion); break;
    case Test::Network: nav().open(screen::kNet); break;
  }
}

bool SelfTestScreen::onInput(const Input& input, const Rect& area) {
  switch (mode_) {
    case Mode::Keys:
      lastKey_ = keyName(input);
      keyPressed_ = true;
      lastInput_ = uptime();
      return true;
    case Mode::Touch:
      if (input.isKey(Key::Back) || input.isKey(Key::Select)) {
        mode_ = Mode::Menu;
        return true;
      }
      if (input.kind == Input::Kind::Touch) {
        tapX_ = input.x;
        tapY_ = input.y;
        lastInput_ = uptime();
        return true;
      }
      return false;
    case Mode::Pattern:
      if (input.isKey(Key::Back) || input.isKey(Key::Select) || input.kind == Input::Kind::Touch) {
        mode_ = Mode::Menu;
        return true;
      }
      return false;
    case Mode::Menu: break;
  }
  if (input.isKey(Key::Back)) {
    nav().back();
    return true;
  }
  refreshTiles();
  bool chosen = false;
  const bool used = grid_.handle(input, gridArea(area), &chosen);
  if (chosen) run(static_cast<Test>(grid_.selected()));
  return used;
}

}  // namespace ui

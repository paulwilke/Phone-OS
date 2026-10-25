#include "ui/screens/NumberScreen.h"

#include "ui/Fonts.h"
#include "ui/Shapes.h"
#include "ui/Symbols.h"
#include "ui/Text.h"

namespace ui {

namespace {

constexpr int kGap = 4;
constexpr int kFieldPadding = 4;

const char* const kKeys[NumberScreen::kColumns * NumberScreen::kRows] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "+", "0", symbol::kBackspace,
};

bool isDigit(char c) { return c >= '0' && c <= '9'; }

}  // namespace

const char* NumberScreen::keyLabel(size_t index) { return index < kColumns * kRows ? kKeys[index] : ""; }

void NumberScreen::clear() {
  number_.clear();
  tooShort_ = false;
}

size_t NumberScreen::digits() const {
  size_t n = 0;
  for (char c : number_) n += isDigit(c) ? 1 : 0;
  return n;
}

bool NumberScreen::append(char c) {
  tooShort_ = false;
  // The plus belongs in front of a number and nowhere else; a digit beyond what a number can have
  // is ignored, not cut into a different number.
  if (c == '+') {
    if (!number_.empty()) return false;
  } else if (!isDigit(c) || digits() >= phone::kMaxDigits) {
    return false;
  }
  number_ += c;
  return true;
}

void NumberScreen::deleteLast() {
  tooShort_ = false;
  if (!number_.empty()) number_.pop_back();
}

bool NumberScreen::accept() {
  if (digits() < kMinDigits) return false;
  // Writing to any recipient is the profile's decision; without it this page leads nowhere.
  if (!context().app.profile().freeText) return false;
  char canonical[phone::kNormalizedSize] = {};
  const char* country = context().app.profile().contacts.countryCode();
  session().recipient = phone::normalize(number_.c_str(), country, canonical, sizeof canonical) ? canonical : number_;
  session().viaRadio = false;
  nav().open(screen::kCompose);
  return true;
}

Rect NumberScreen::fieldRect(const Rect& area) const {
  const int title = font(FontSize::Small).lineHeight() + 2;
  return {area.x + 2, area.y + title, area.w - 4, font(FontSize::Tile).lineHeight() + 6};
}

std::vector<Rect> NumberScreen::keyRects(const Rect& area) const {
  const Rect field = fieldRect(area);
  const int top = field.bottom() + 2 + font(FontSize::Small).lineHeight() + 2;  // the note under the field has its line
  const int cellWidth = (area.w - kGap * (static_cast<int>(kColumns) + 1)) / static_cast<int>(kColumns);
  const int cellHeight = (area.bottom() - top - kGap * (static_cast<int>(kRows) + 1)) / static_cast<int>(kRows);
  std::vector<Rect> rects;
  for (size_t i = 0; i < kColumns * kRows; ++i) {
    const int col = static_cast<int>(i % kColumns), row = static_cast<int>(i / kColumns);
    rects.push_back({area.x + kGap + col * (cellWidth + kGap), top + kGap + row * (cellHeight + kGap), cellWidth, cellHeight});
  }
  return rects;
}

void NumberScreen::draw(Display& display, const Rect& area) {
  const Font& small = font(FontSize::Small);
  const Font& big = font(FontSize::Tile);

  text::draw(display, small, area.x + 4, area.y, t().get("number.title"));

  // The number, as large as it fits; an empty field stays empty (a placeholder "+" would look as if
  // it were typed, and the child would type the prefix a second time).
  const Rect field = fieldRect(area);
  shapes::outlineRoundRect(display, field, 6);
  if (!number_.empty()) {
    const int room = field.w - 2 * kFieldPadding - 4;
    const Font* use = &big;
    for (FontSize size : {FontSize::Tile, FontSize::Text, FontSize::Small}) {
      use = &font(size);
      if (text::width(*use, number_) <= room) break;
    }
    text::draw(display, *use, field.x + kFieldPadding + 2, field.y + (field.h - use->lineHeight()) / 2, number_);
  }
  // The note stands where the mistake happened, not in the notice line, which goes by itself.
  if (tooShort_) text::draw(display, small, area.x + 8, field.bottom() + 2, t().get("number.too-short"));

  const std::vector<Rect> rects = keyRects(area);
  for (size_t i = 0; i < rects.size(); ++i) {
    const Rect& key = rects[i];
    if (key.h < 8) break;
    shapes::outlineRoundRect(display, key, 8);
    display.fillRect({key.x + 9, key.bottom(), key.w - 17, 1}, true);   // the shadow that makes a button a button
    display.fillRect({key.right(), key.y + 9, 1, key.h - 17}, true);
    text::drawCentered(display, big, key, key.y + (key.h - big.lineHeight()) / 2, kKeys[i]);
  }
}

SoftKeys NumberScreen::keys() {
  SoftKeys keys;
  keys.back = t().get(number_.empty() ? "key.back" : "key.delete");
  keys.action = t().get("key.next");
  return keys;
}

std::vector<Rect> NumberScreen::targets(const Rect& area) { return keyRects(area); }

bool NumberScreen::onInput(const Input& input, const Rect& area) {
  if (input.kind == Input::Kind::Text) {
    const uint32_t c = input.character;
    if (c < 0x80) append(static_cast<char>(c));
    return true;
  }
  if (input.kind == Input::Kind::Touch) {
    const std::vector<Rect> rects = keyRects(area);
    for (size_t i = 0; i < rects.size(); ++i) {
      if (!rects[i].contains(input.x, input.y)) continue;
      if (i == kColumns * kRows - 1) deleteLast();
      else append(kKeys[i][0]);
      return true;
    }
    return false;
  }
  if (input.isKey(Key::Select)) {
    tooShort_ = digits() < kMinDigits;
    if (!tooShort_) accept();
    return true;
  }
  if (input.isKey(Key::Back)) {
    // Delete, and with nothing left to delete go back: one rule, the same as writing a message.
    if (number_.empty()) nav().back();
    else deleteLast();
    return true;
  }
  return false;
}

}  // namespace ui

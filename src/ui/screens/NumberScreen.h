#pragma once

// A number typed in: for the child who has to write to somebody who is in no list — a relative on
// holiday, a neighbour, the hospital. It is the way to a *message*, and only where the profile lets
// the child choose any recipient (`Profile::freeText`); it never starts a call. Calls go to the
// numbers the profile allows (`Profile::mayCall`), and that is a security limit of the parents, not of
// this page.
//
// Twelve large keys on the screen, as on every phone, because on the keyboard the digits are on the
// second layer, two keys each, and nobody reading a number out to a child has the patience for that.
// The keyboard works too: every digit, and the plus sign as the first character.
//
//   Next ↵   goes on to write the message, to the number as the book writes it
//   Back ⌫   deletes the last character; with an empty field it goes back
//
// **A number is never shortened.** The field takes at most the 15 digits a number can have
// (`phone::kMaxDigits`) and ignores a sixteenth instead of writing a different number than was
// typed; the font gets smaller when the digits do not fit in the large one. Fewer than three digits
// are a slip of the finger, not a recipient: the page says "Still too short" under the field, where
// the mistake happened, and stays.
//
// The number is made canonical (`+49…`) before it is handed on, so that the answer lands in the same
// conversation as one from the address book.

#include <string>
#include <vector>

#include "ui/AppScreen.h"

namespace ui {

class NumberScreen : public AppScreen {
public:
  using AppScreen::AppScreen;

  static constexpr size_t kMinDigits = 3;
  static constexpr size_t kColumns = 3;
  static constexpr size_t kRows = 4;

  // The twelve keys in reading order; the last one deletes.
  static const char* keyLabel(size_t index);

  // Opened afresh each time.
  void onEnter() override { clear(); }

  const std::string& number() const { return number_; }

protected:
  void draw(Display& display, const Rect& area) override;
  SoftKeys keys() override;
  bool onInput(const Input& input, const Rect& area) override;
  std::vector<Rect> targets(const Rect& area) override;

private:
  void clear();
  bool append(char c);
  void deleteLast();
  bool accept();
  size_t digits() const;
  // Where the keys are, for drawing and for touch.
  std::vector<Rect> keyRects(const Rect& area) const;
  Rect fieldRect(const Rect& area) const;

  std::string number_;
  bool tooShort_ = false;
};

}  // namespace ui

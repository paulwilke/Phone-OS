#pragma once

// The radio page: the long-range radio that carries messages where the mobile network does not reach.
// The page always says the word "radio", never the name of the software underneath.
//
// A head line says in one sentence how the radio is and, when it runs, how well it hears. Under it the
// controls the state offers, as buttons, and — when the radio runs — the contacts it knows, each with its
// way (direct or via detours) and when it was last heard:
//
//   not allowed          nothing to press; the profile does not allow the radio
//   off                  "Switch radio on" (the application starts it and attaches it)
//   running              "Radio off (restart)"
//
// Enter on a contact writes to it over the radio, but only where the profile lets the child choose any
// recipient (`freeText`): the contact is left in the session and the compose page opens. Without that
// switch the contacts are shown and nothing writes to them.
//
// Names of contacts are shortened with "..."; everything else is a fixed word and wraps inside its row, so
// a row is as high as its words need.

#include <string>
#include <vector>

#include "ui/AppScreen.h"

namespace ui {

class RadioScreen : public AppScreen {
public:
  using AppScreen::AppScreen;

  void onEnter() override { selected_ = 0; first_ = 0; }

protected:
  void draw(Display& display, const Rect& area) override;
  SoftKeys keys() override;
  bool onInput(const Input& input, const Rect& area) override;

private:
  enum class Control { Start, Stop };
  struct Row {
    bool isControl;
    Control control;
    std::string label, sub, name;  // `name` of a contact, to write to
  };

  std::string head();
  void rebuild();
  Rect rowsArea(const Rect& area);
  // The room for the words of a row, from the area the page was given.
  void measure(const Rect& area);
  int rowHeight(const Row& row) const;
  // How many rows from `first` fit in `room`.
  size_t fitting(size_t first, int room) const;
  void keepInView(const Rect& area);
  bool writable() const;
  bool activate(const Row& row);

  std::vector<Row> rows_;
  size_t selected_ = 0;
  size_t first_ = 0;
  int wordsWidth_ = 0;
};

}  // namespace ui

#pragma once

// To whom to write: the page between the "Text" tile and the page that writes the message. It lists
// the people the child may write to — the contacts and the numbers a call action reaches, the call
// targets first in the parents' order — then "Other" for the rest, and "Cancel". Choosing leaves
// the number in the session (`Session::recipient`) and opens the compose page.
//
// Where the profile lets the child choose **any** recipient (`Profile::freeText`), the list behind
// "Other" ends with "New number", which opens the number page. Without that switch there is no way
// to a number that is in no list. Radio contacts are chosen on the radio page, which also leaves them
// in the session.
//
// Layout and rules of the list are those of the phone page ([screen-phone.md](screen-phone.md)): names
// are shortened with "...", a number never is, the page opens afresh each time.

#include <string>
#include <vector>

#include "ui/AppScreen.h"
#include "ui/ListView.h"

namespace ui {

class RecipientScreen : public AppScreen {
public:
  using AppScreen::AppScreen;

  void onEnter() override;

protected:
  void draw(Display& display, const Rect& area) override;
  SoftKeys keys() override;
  bool onInput(const Input& input, const Rect& area) override;
  std::vector<Rect> targets(const Rect& area) override;

private:
  struct Row {
    enum class Kind { Contact, Other, NewNumber, Cancel };
    Kind kind;
    std::string label;
    std::string phone;
    bool isNumber;
  };

  void rebuild();
  Rect listArea(const Rect& area);
  void activate(const Row& row);

  std::vector<Row> rows_;
  ListView list_;
  bool other_ = false;
};

}  // namespace ui

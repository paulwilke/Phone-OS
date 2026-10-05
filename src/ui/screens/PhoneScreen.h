#pragma once

// Whom to call: the one place where a call starts. Choosing a name dials; nothing else on the
// device does, and a press in a pocket never calls, because the home tile only opens this page.
//
// **Only numbers the child may call are on it** — the whitelist rule is the profile's
// (`Profile::mayCall`): a contact, or a number a call action reaches — so the page lists exactly
// those two kinds and nothing is typed here. A call to a number that is not allowed would be
// refused by the core anyway; the page does not offer it.
//
// The short list is who the parents arranged as call targets, in their order, then "Other", which
// opens the rest of the contacts, and "Cancel". Without any call target the short list is all the
// contacts. A name is shortened with "..." when it is too long for the row; a number (a call target
// who is in no contact book is shown by it) never is.
//
// Dialling is the core's decision: when it refuses — school time, a number that is not allowed,
// cellular off, no network — it says why in the notice line and the page stays; when it accepts,
// the call page opens.

#include <string>
#include <vector>

#include "ui/AppScreen.h"
#include "ui/ListView.h"

namespace ui {

class PhoneScreen : public AppScreen {
public:
  using AppScreen::AppScreen;

  // Opened afresh each time: the short list, the first row.
  void onEnter() override;

protected:
  void draw(Display& display, const Rect& area) override;
  SoftKeys keys() override;
  bool onInput(const Input& input, const Rect& area) override;

private:
  struct Row {
    enum class Kind { Contact, Other, Cancel };
    Kind kind;
    std::string label;
    std::string phone;
    bool isNumber;  // the label is the number itself
  };

  void rebuild();
  Rect listArea(const Rect& area);
  void activate(const Row& row);

  std::vector<Row> rows_;
  ListView list_;
  bool other_ = false;  // the rest of the contacts instead of the call targets
};

}  // namespace ui

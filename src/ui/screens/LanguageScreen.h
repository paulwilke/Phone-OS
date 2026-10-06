#pragma once

// The language of the screen: every language the translator has, by the name it calls itself
// ("Deutsch", "Français"), so that a child who cannot read the present language still finds theirs. That
// includes the languages parents have put on the card (`lang/user/`): the translator lists them like
// the built-in ones.
//
// Choosing applies at once: the page asks the core to switch (`AppCore::setInterfaceLanguage`), and
// the next frame, this one included, is in the new language. The core keeps the choice in the profile.
// A language the core refuses stays as it was. The page does not close itself: whoever chose can see
// the result, and Back leaves.
//
// The language in use stands under the title ("Now: Deutsch"), because the focus moves away from it.
// The list opens with the focus on it. The order is the translator's (by language code).

#include <string>
#include <vector>

#include "ui/AppScreen.h"
#include "ui/ListView.h"

namespace ui {

class LanguageScreen : public AppScreen {
public:
  using AppScreen::AppScreen;

  void onEnter() override;

protected:
  void draw(Display& display, const Rect& area) override;
  SoftKeys keys() override;
  bool onInput(const Input& input, const Rect& area) override;

private:
  void rebuild();
  Rect listArea(const Rect& area);
  std::string now();

  std::vector<std::string> codes_;
  ListView list_;
  bool focusOnCurrent_ = true;
};

}  // namespace ui

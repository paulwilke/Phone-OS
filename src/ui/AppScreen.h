#pragma once

// The base of every screen of the device: the frame around the content — status bar, notice line,
// soft keys — and the translation of touches, so that a concrete screen writes only what is its
// own. A screen draws its content in the rectangle it is given and says what the soft keys are;
// it handles keys, and a tap on the content arrives as a touch. A tap on a soft key never reaches
// the screen as a touch: it arrives as the key it stands for, so a screen has one input logic for
// keys and soft-key taps.
//
// Screens read the core through `state()`, a snapshot taken once per frame and once per input, so
// that what a screen decides on and what it draws are the same numbers. They call the core's entry
// points through `app()`, and open other screens through `nav()`. They never keep words: every text
// is asked of the catalogs when it is drawn (`t()`), so that a change of language is complete at
// the next frame.

#include <string>

#include "app/AppCore.h"
#include "i18n/Translator.h"
#include "ui/AppStatus.h"
#include "ui/Layout.h"
#include "ui/Screen.h"
#include "ui/ScreenIds.h"
#include "ui/ScreenStack.h"
#include "ui/SoftKeys.h"

namespace ui {

// What every screen is given by the application that wires them up.
struct ScreenContext {
  AppCore& app;
  i18n::Translator& translator;
  Navigation& navigation;
};

class AppScreen : public Screen {
public:
  explicit AppScreen(ScreenContext context) : context_(context) {}

  // Draws the frame, then the content. The whole screen is painted, so what an earlier frame left
  // behind never shows through.
  void render(Display& display) final;

  // A tap on a soft key is turned into its key; everything else goes to onInput().
  bool handle(const Input& input) final;

  // The soft keys as they were drawn last, for tests and for the touch translation.
  const SoftKeys& softKeys() const { return keys_; }
  // The notice line as it was drawn last.
  const std::string& noticeLine() const { return notice_; }

protected:
  // Draws the content inside `area`, which is what the frame leaves.
  virtual void draw(Display& display, const Rect& area) = 0;
  // The soft keys of the screen in its present state.
  virtual SoftKeys keys() = 0;
  // A key, or a touch on the content (`area` is where the content was drawn last).
  virtual bool onInput(const Input& input, const Rect& area) = 0;

  AppCore& app() { return context_.app; }
  i18n::Translator& t() { return context_.translator; }
  Navigation& nav() { return context_.navigation; }
  const AppCore::State& state() const { return state_; }

private:
  ScreenContext context_;
  AppCore::State state_;
  ScreenLayout layout_ = ScreenLayout::standard();
  SoftKeys keys_;
  std::string notice_;
};

}  // namespace ui

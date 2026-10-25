#pragma once

// The self-test page: eight things a person with the device in hand can check, as a grid of buttons; Enter
// (or a tap) runs the one in focus. Above the grid are the state now (light, receiver, battery) and the
// result of the last check, two lines high even when empty so that the grid never moves.
//
//   vibration  plays the five patterns in turn (click, ring, message, sent, failed): "Felt it?"
//   light      switches the front light: "Brighter?"
//   display    stripes and a chequerboard over the whole content area, to see whether the panel shows
//              evenly or leaves ghosts, dead lines, gradients; a tap or Back ends it
//   keys       every key is named as it is pressed — the arrows, Enter, Backspace, the three keys under the
//              screen, letters. No soft key here: Backspace is one of the keys to test, so the test ends by
//              itself after a few seconds without input
//   touch      the place of every tap as numbers and a cross; Back (soft key) ends it, and so do a few
//              seconds without a tap
//   GPS        holds the receiver (the test page's reason) and shows how it is doing, live
//   motion     opens the motion page
//   network    opens the network page
//
// What the page holds it gives back whenever it is left (`onLeave`): the receiver. The words of a result are
// worded when drawn, so a result stays right after a change of language.

#include <string>

#include "ui/AppScreen.h"
#include "ui/TileGrid.h"

namespace ui {

class SelfTestScreen : public AppScreen {
public:
  explicit SelfTestScreen(ScreenContext context);

  void onEnter() override;
  void onLeave() override;
  void onTick() override;
  // The key test wants every key, the three under the screen too.
  bool wantsHardwareKeys() const override { return mode_ == Mode::Keys; }

  // How long a key or touch test waits for the next input before it ends.
  static constexpr uint32_t kIdleSeconds = 6;

protected:
  void draw(Display& display, const Rect& area) override;
  SoftKeys keys() override;
  bool onInput(const Input& input, const Rect& area) override;
  std::vector<Rect> targets(const Rect& area) override;

private:
  enum class Mode { Menu, Pattern, Keys, Touch };
  enum class Test { Vibration, Light, Display, Keys, Touch, Gps, Motion, Network };
  enum class Result { None, Vibration, Light, Gps };

  void drawMenu(Display& display, const Rect& area);
  void drawPattern(Display& display, const Rect& area);
  void drawInput(Display& display, const Rect& area);
  void run(Test test);
  // How many lines the header and the result may take: the most any state of them needs.
  struct Reserved {
    int headLines, resultLines;
  };
  Reserved reserved(const Rect& area);
  void refreshTiles();
  Rect gridArea(const Rect& area);
  enum class GpsState { Off, Searching, Fix };
  std::string headerWords(bool light, bool gps, int percent);
  std::string resultWords(Result result, size_t pattern, bool lightOn, GpsState gps);
  std::string currentHeader();
  std::string currentResult();
  std::string keyName(const Input& input);
  uint32_t uptime() { return app().state().uptimeSeconds; }

  Mode mode_ = Mode::Menu;
  Result result_ = Result::None;
  size_t pattern_ = 0;      // the next pattern to play
  size_t played_ = 0;       // the one played last
  bool gpsTest_ = false;
  std::string lastKey_;
  bool keyPressed_ = false;
  int tapX_ = -1, tapY_ = -1;
  uint32_t lastInput_ = 0;
  TileGrid grid_;
};

}  // namespace ui

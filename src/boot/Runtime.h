#pragma once

// The one composition of the device: the application core, the screens, the screen stack and the
// web service, wired to whatever capabilities the host hands over. The firmware's main loop and the
// simulator's both create their capabilities, build a Runtime, and loop on step(); everything
// between the capabilities and the display is this class and is therefore the same code on the
// device, on the desk and in CI.
//
// Building a Runtime **is** booting: the user catalogs are read from /lang/, the configuration from
// /config/, the interface language is chosen, the core is started and the stack stands on the home
// screen. Every problem on the way is a diagnostic (and a line in the event log), never a refusal
// to start: a card with a typo in it must still give a device that rings.
//
// What the host still does, because only hardware can: sleeps the processor as step() says, starts
// the radio when core().takeRadioStartRequest() says so, restarts when restartRequested() says so,
// switches off when core().powerOffRequested() says so, and tells the Runtime what was pressed.
// The shipped catalogs are the host's too (embedded on the device, read from lang/ in the
// simulator): the translator it passes holds them already, and the Runtime adds the user's.
//
// The screens that exist today are the ones registered; a tile for a screen that does not exist
// yet opens nothing (the stack ignores a name it does not know).

#include <cstdint>
#include <string>
#include <vector>

#include "../app/AppCore.h"
#include "../config/ConfigLoader.h"
#include "../config/Diagnostic.h"
#include "../hal/ConfigStore.h"
#include "../hal/MotionSensor.h"
#include "../ui/Display.h"
#include "../ui/Input.h"
#include "../ui/ScreenStack.h"
#include "../ui/Session.h"
#include "../ui/screens/CallScreen.h"
#include "../ui/screens/ClockScreen.h"
#include "../ui/screens/ComposeScreen.h"
#include "../ui/screens/HomeScreen.h"
#include "../ui/screens/LockScreen.h"
#include "../ui/screens/MessageListScreen.h"
#include "../ui/screens/NumberScreen.h"
#include "../ui/screens/PhoneScreen.h"
#include "../ui/screens/RecipientScreen.h"
#include "../web/WebAssets.h"
#include "../web/WebService.h"

namespace boot {

// What the host provides, as references to what must exist and pointers to what a device may lack.
struct Capabilities {
  Clock& clock;
  Power& power;
  Modem& modem;
  GnssReceiver& gnss;
  Haptic& haptic;
  EventLog& log;
  ConfigStore& store;
  Wifi& wifi;
  TcpServer& tcp;
  WebAssets& assets;
  ui::Display& display;
  WifiController::Random random;
  const char* firmware;
  RadioLink* radio = nullptr;      // without it the core has no radio
  MotionSensor* motion = nullptr;  // its presence puts the motion tile on the home screen
};

class Runtime {
public:
  // Seconds without input after which the device locks.
  static constexpr uint32_t kAutoLockSeconds = 120;
  // The picture is redrawn at most this often when nothing was pressed. It is redrawn from the
  // state every time, and the refresh logic (ui/RefreshPolicy.h) decides whether the panel works.
  static constexpr uint32_t kFrameIntervalMs = 1000;

  // `translator` holds the shipped catalogs. `nowMs` is the host's millisecond counter.
  Runtime(const Capabilities& capabilities, i18n::Translator& translator, uint32_t nowMs);
  ~Runtime();
  Runtime(const Runtime&) = delete;
  Runtime& operator=(const Runtime&) = delete;

  // One pass of the main loop: the core, the web service, the stack, and a frame when one is due.
  // Returns the core's decision on sleeping, which the host carries out.
  PowerDecision step(uint32_t nowMs);

  // The user pressed or touched something. The frame it causes is drawn at once.
  void input(const ui::Input& input, uint32_t nowMs);

  // Chooses the interface language, as the language page or the configuration would; the very
  // next frame is in it. False for a code no catalog has (nothing changes). Lasts until the next
  // start: writing the configuration back is the configuration's job.
  bool setLanguage(const std::string& code);

  // Draws a frame now, whether one is due or not (a screenshot of what the state says).
  void render();

  // What went wrong while booting, in the loader's own words.
  const std::vector<config::Diagnostic>& diagnostics() const { return diagnostics_; }

  // The web interface or the core asked for a restart: the host starts again from the top.
  bool restartRequested() const { return core_.restartRequested() || web_.restartRequested(); }

  AppCore& core() { return core_; }
  ui::ScreenStack& stack() { return stack_; }
  WebService& web() { return web_; }
  i18n::Translator& translator() { return translator_; }

private:
  // Reads /lang/ and /config/ and selects the language; fills `diagnostics`.
  static config::ConfigLoader::Result boot(const Capabilities& capabilities, i18n::Translator& translator,
                                           std::vector<config::Diagnostic>& diagnostics);
  void followCalls(uint32_t nowMs);

  Capabilities caps_;
  i18n::Translator& translator_;
  std::vector<config::Diagnostic> diagnostics_;
  config::ConfigLoader::Result loaded_;
  void loadUserWebCatalogs();
  AppCore core_;
  ui::Session session_;
  ui::ScreenStack stack_;
  ui::HomeScreen home_;
  ui::LockScreen lock_;
  ui::ClockScreen clock_;
  ui::PhoneScreen phone_;
  ui::NumberScreen number_;
  ui::CallScreen call_;
  ui::MessageListScreen messages_;
  ui::ComposeScreen compose_;
  ui::RecipientScreen recipient_;
  WebService web_;

  uint32_t lastFrameMs_ = 0;
  bool frameDue_ = true;
  // The call page is on top because a call came in, not because the child opened it: only then
  // does the host take it away again when the call is over.
  bool callInterrupt_ = false;
};

}  // namespace boot

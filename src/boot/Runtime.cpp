#include "Runtime.h"

namespace boot {

namespace {

std::string describe(const config::Diagnostic& diagnostic) {
  std::string line = std::string("config: ") + config::diagnosticName(diagnostic.code);
  for (const std::string& arg : diagnostic.args) line += " " + arg;
  return line;
}

}  // namespace

config::ConfigLoader::Result Runtime::boot(const Capabilities& capabilities, i18n::Translator& translator,
                                           std::vector<config::Diagnostic>& diagnostics) {
  // The user's catalogs first, so that a language they bring can be the one the configuration asks for.
  config::ConfigLoader::loadUserCatalogs(capabilities.store, translator, diagnostics);
  config::ConfigLoader::Result loaded = config::ConfigLoader(capabilities.store).load();
  diagnostics.insert(diagnostics.end(), loaded.diagnostics.begin(), loaded.diagnostics.end());

  // A code nobody has leaves the translator on English, which is what it starts with.
  if (config::ConfigLoader::checkLanguages(loaded.profile, translator, diagnostics))
    translator.select(loaded.profile.interfaceLanguage);
  return loaded;
}

Runtime::Runtime(const Capabilities& capabilities, i18n::Translator& translator, uint32_t nowMs)
    : caps_(capabilities),
      translator_(translator),
      loaded_(boot(capabilities, translator, diagnostics_)),
      core_(capabilities.clock, capabilities.power, capabilities.modem, capabilities.gnss, capabilities.haptic,
            capabilities.log, translator, loaded_.profile),
      stack_(ui::screen::kHome),
      home_(ui::ScreenContext{core_, translator, stack_, session_, capabilities.motion != nullptr}),
      lock_(ui::ScreenContext{core_, translator, stack_, session_}),
      clock_(ui::ScreenContext{core_, translator, stack_, session_}),
      phone_(ui::ScreenContext{core_, translator, stack_, session_}),
      number_(ui::ScreenContext{core_, translator, stack_, session_}),
      call_(ui::ScreenContext{core_, translator, stack_, session_}),
      messages_(ui::ScreenContext{core_, translator, stack_, session_}),
      compose_(ui::ScreenContext{core_, translator, stack_, session_}),
      recipient_(ui::ScreenContext{core_, translator, stack_, session_}),
      web_(capabilities.tcp, capabilities.store, capabilities.assets, core_, translator),
      lastFrameMs_(nowMs) {
  core_.setFirmware(capabilities.firmware);
  if (capabilities.radio) core_.setRadio(capabilities.radio);
  core_.setWifi(capabilities.wifi, capabilities.random);
  core_.start();
  for (const config::Diagnostic& diagnostic : diagnostics_) core_.log(describe(diagnostic).c_str());

  stack_.add(ui::screen::kHome, home_);
  stack_.add(ui::screen::kLock, lock_);
  stack_.add(ui::screen::kClock, clock_);
  stack_.add(ui::screen::kPhone, phone_);
  stack_.add(ui::screen::kNumber, number_);
  stack_.add(ui::screen::kCall, call_);
  stack_.add(ui::screen::kMessages, messages_);
  stack_.add(ui::screen::kCompose, compose_);
  stack_.add(ui::screen::kRecipient, recipient_);
  stack_.setLockScreen(ui::screen::kLock);
  stack_.setAutoLockSeconds(kAutoLockSeconds);
  // The three keys under the screen act the same on every screen, like the home key of a phone.
  stack_.setHardwareKey(ui::Key::Home, [](ui::Navigation& nav) { nav.home(); });
  stack_.setHardwareKey(ui::Key::Inbox, [](ui::Navigation& nav) { nav.open(ui::screen::kMessages); });
  stack_.setHardwareKey(ui::Key::Compose, [](ui::Navigation& nav) { nav.open(ui::screen::kRecipient); });
  stack_.begin(nowMs / 1000);
}

PowerDecision Runtime::step(uint32_t nowMs) {
  const bool due = frameDue_ || nowMs - lastFrameMs_ >= kFrameIntervalMs;
  const uint32_t untilFrame = due ? 0 : kFrameIntervalMs - (nowMs - lastFrameMs_);
  // The panel keeps its picture without current, so "the display is on" means somebody may be looking:
  // not while the device is locked, which is how it goes into the pocket.
  const PowerDecision decision = core_.poll(!stack_.locked(), untilFrame);
  web_.poll(nowMs / 1000);
  followCalls(nowMs);
  stack_.poll(nowMs / 1000);
  if (due) {
    render();
    lastFrameMs_ = nowMs;
  }
  return decision;
}

void Runtime::input(const ui::Input& in, uint32_t nowMs) {
  core_.noteInput();
  stack_.handle(in, nowMs / 1000);
  frameDue_ = true;
}

bool Runtime::setLanguage(const std::string& code) {
  if (!translator_.select(code)) return false;
  frameDue_ = true;
  return true;
}

void Runtime::render() {
  caps_.display.beginFrame();
  stack_.render(caps_.display);
  caps_.display.endFrame();
  frameDue_ = false;
}

// A call that comes in takes the screen, even from the lock; the page is taken away again when the
// call is over. A call the child started is on the screen because the phone page opened it, and
// stays until the child has read how it ended (ui/screens/CallScreen.h).
void Runtime::followCalls(uint32_t nowMs) {
  const ModemState call = core_.state().callState;
  if (call == ModemState::RingingIn && !callInterrupt_ && stack_.visibleId() != ui::screen::kCall) {
    stack_.interrupt(ui::screen::kCall, nowMs / 1000);
    callInterrupt_ = true;
    frameDue_ = true;
  } else if (callInterrupt_ && call != ModemState::RingingIn && call != ModemState::InCall) {
    stack_.endInterrupt(ui::screen::kCall);
    callInterrupt_ = false;
    frameDue_ = true;
  }
}

}  // namespace boot

#include "ui/screens/NetScreen.h"

#include "ui/PageTitle.h"
#include "ui/Wording.h"

namespace ui {

std::string NetScreen::words() {
  const AppCore::State& s = state();
  std::string text;
  const auto line = [&](const std::string& words) {
    if (!text.empty()) text += '\n';
    text += words;
  };
  const auto onOff = [&](bool on) { return t().get(on ? "option.on" : "option.off"); };

  const char* simState = nullptr;
  const char* hint = nullptr;
  switch (s.sim) {
    case SimStatus::Missing: simState = "net.sim-missing"; hint = "net.hint-missing"; break;
    case SimStatus::PinRequired: simState = "net.sim-pin"; hint = "net.hint-pin"; break;
    case SimStatus::PinWrong: simState = "net.sim-wrong-pin"; hint = "net.hint-wrong-pin"; break;
    case SimStatus::PukRequired: simState = "net.sim-locked"; hint = "net.hint-locked"; break;
    case SimStatus::Unknown:
    case SimStatus::Ready: break;
  }
  if (simState) {
    const std::string problem = t().get(simState);
    line(t().get("net.sim", {{"state", problem.c_str()}}));
  }
  const std::string network = wording::networkLabel(s.network, t());
  line(t().get("net.operator", {{"name", network.c_str()}}));
  const bool connected = s.network.kind == Connectivity::NetworkLabel::Kind::Operator;
  // The IMS is what telephony needs besides the network, so an adult can see which of the two is missing.
  // Where the modem has no verdict, or the network allows SMS only, there is nothing to say about it.
  if (connected && (s.voice == VoiceStatus::Ready || s.voice == VoiceStatus::ImsMissing)) {
    const std::string ims = t().get(s.voice == VoiceStatus::Ready ? "net.ims-yes" : "net.ims-no");
    line(t().get("net.ims", {{"state", ims.c_str()}}));
  }
  const char* linkKey = !connected                          ? "net.not-connected"
                        : s.voice == VoiceStatus::SmsOnly   ? "net.sms-only"
                        : s.voice == VoiceStatus::ImsMissing ? "net.telephony-not-ready"
                                                             : "net.connected";
  const std::string link = t().get(linkKey);
  line(t().get("net.state", {{"state", link.c_str()}}));
  line(t().get("net.signal", {{"bars", s.signalBars}}));
  const std::string cellular = onOff(s.cellularEnabled);
  line(t().get("net.cellular", {{"state", cellular.c_str()}}));
  const std::string modem = t().get(s.modemAsleep ? "net.modem-asleep" : "net.modem-awake");
  line(t().get("net.modem", {{"state", modem.c_str()}}));
  const std::string time = t().get(s.wall.known ? "net.time-known" : "net.time-unknown");
  line(t().get("net.time", {{"state", time.c_str()}}));
  line(t().get("net.waiting", {{"n", static_cast<int>(s.queued)}}));

  // "SMS only" needs the words: it is the network's rule and no fault of the device.
  if (hint) {
    line(std::string());
    line(t().get(hint));
  } else if (connected && s.voice == VoiceStatus::SmsOnly) {
    line(std::string());
    line(t().get("net.hint-sms-only"));
  } else if (connected && s.voice == VoiceStatus::ImsMissing) {
    line(std::string());
    line(t().get("net.hint-ims-missing"));
  }
  return text;
}

Rect NetScreen::textArea(const Rect& area) {
  const int header = pageTitleHeight(area, t().get("net.title"));
  return {area.x, area.y + header, area.w, area.h - header};
}

void NetScreen::draw(Display& display, const Rect& area) {
  drawPageTitle(display, area, t().get("net.title"));
  const std::string now = words();
  if (now != text_) {
    text_ = now;
    block_.setText(now);
  }
  block_.draw(display, textArea(area));
}

SoftKeys NetScreen::keys() {
  SoftKeys keys;
  keys.back = t().get("key.back");
  const Rect area = ScreenLayout::standard().content;
  const Rect text = textArea(area);
  keys.scroll = block_.lineCount(text) > block_.visibleLines(text);
  return keys;
}

bool NetScreen::onInput(const Input& input, const Rect& area) {
  if (input.isKey(Key::Back)) {
    nav().back();
    return true;
  }
  return block_.handle(input, textArea(area));
}

}  // namespace ui

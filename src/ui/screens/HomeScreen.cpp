#include "ui/screens/HomeScreen.h"

#include <string>

#include "ui/Symbols.h"

namespace ui {

HomeScreen::HomeScreen(ScreenContext context) : AppScreen(context) {}

namespace {

bool somebodyToReach(const Profile& profile) { return profile.contacts.count() > 0 || !profile.callTargets.empty(); }

}  // namespace

std::vector<std::vector<HomeScreen::Kind>> HomeScreen::pages() const {
  const Profile& profile = context().app.profile();
  std::vector<Kind> child, device;
  if (somebodyToReach(profile)) {
    child.push_back(Kind::Phone);
    child.push_back(Kind::Text);
  }
  if (!profile.sosRecipient(std::string()).empty()) child.push_back(Kind::Sos);
  child.push_back(Kind::Inbox);
  if (profile.radioAllowed) child.push_back(Kind::Radio);
  child.push_back(Kind::Light);   // the child's: it lights the way in the dark (owner, 2026-10-08)

  // The device's page; the parents may switch each tile off ([home] in settings.toml).
  const HomeTiles& shown = profile.home;
  if (shown.network) device.push_back(Kind::Network);
  if (shown.gps) device.push_back(Kind::Gps);
  if (shown.clock) device.push_back(Kind::Clock);
  if (shown.settings) device.push_back(Kind::Settings);
  if (shown.wifi && profile.parentTools && context().app.wifi() != nullptr) device.push_back(Kind::Wifi);
  if (shown.motion && context().motionSensor) device.push_back(Kind::Motion);
  if (shown.tests) device.push_back(Kind::Tests);
  if (shown.status && profile.parentTools) device.push_back(Kind::Status);
  if (device.empty()) return {child};   // no second page and no dots for it
  return {child, device};
}

Tile HomeScreen::tileOf(Kind kind) {
  Tile tile;
  switch (kind) {
    case Kind::Phone: tile.label = t().get("home.phone"); tile.symbol = symbol::kTelephone; break;
    case Kind::Text: tile.label = t().get("home.text"); tile.symbol = symbol::kAeroplane; break;
    case Kind::Sos: tile.label = t().get("home.sos"); tile.symbol = symbol::kPin; break;
    case Kind::Inbox:
      tile.label = t().get("home.inbox");
      tile.symbol = symbol::kBubble;
      // A number only when there is something new, so that it stands out when it is there.
      if (state().unread > 0) tile.badge = std::string(symbol::kEnvelope) + std::to_string(state().unread);
      break;
    case Kind::Radio: tile.label = t().get("home.radio"); tile.symbol = symbol::kRadio; break;
    case Kind::Network: tile.label = t().get("home.network"); break;
    case Kind::Gps: tile.label = t().get("home.gps"); break;
    case Kind::Clock: tile.label = t().get("home.clock"); break;
    case Kind::Settings: tile.label = t().get("home.settings"); break;
    case Kind::Wifi: tile.label = t().get("home.wifi"); break;
    case Kind::Motion: tile.label = t().get("home.motion"); break;
    case Kind::Tests: tile.label = t().get("home.tests"); break;
    case Kind::Status: tile.label = t().get("home.status"); break;
    case Kind::Light: tile.label = t().get("home.light"); break;
  }
  return tile;
}

// The tiles are rebuilt at every frame and every input, because the badge follows the core. The
// grid keeps the selection while the tiles are set again.
void HomeScreen::rebuild() {
  std::vector<std::vector<Tile>> tiles;
  order_.clear();
  for (const std::vector<Kind>& page : pages()) {
    std::vector<Tile> row;
    for (Kind kind : page) {
      row.push_back(tileOf(kind));
      order_.push_back(kind);
    }
    tiles.push_back(std::move(row));
  }
  grid_.setPages(std::move(tiles));
}

HomeScreen::Kind HomeScreen::selected() {
  rebuild();
  return order_[grid_.selected()];
}

void HomeScreen::draw(Display& display, const Rect& area) {
  rebuild();
  grid_.draw(display, area);
}

// No arrows: the tiles are touched, the page dots turn the page, and the keys Up and Down still move the
// selection (owner, 2026-10-08: the arrows only took room). The one button is Lock, in the place of Back, because
// Back locks the device here and a key nobody can see is a key a child does not find. The word never changes
// with the selection, so the bar costs no redraw.
SoftKeys HomeScreen::keys() {
  SoftKeys keys;
  keys.back = t().get("key.lock");
  return keys;
}

bool HomeScreen::onInput(const Input& input, const Rect& area) {
  if (input.isKey(Key::Back)) {
    nav().lock();
    return true;
  }
  rebuild();
  bool chosen = false;
  const bool used = grid_.handle(input, area, &chosen);
  if (chosen) activate(order_[grid_.selected()]);
  return used;
}

void HomeScreen::activate(Kind kind) {
  switch (kind) {
    case Kind::Phone:
      // School time: says until when, and stays here. The core words it.
      if (app().refuseInSchoolTime()) return;
      nav().open(screen::kPhone);
      return;
    case Kind::Text: nav().open(screen::kRecipient); return;
    case Kind::Sos:
      if (app().beginSos()) nav().open(screen::kSos);
      return;
    case Kind::Inbox: nav().open(screen::kMessages); return;
    case Kind::Radio: nav().open(screen::kRadio); return;
    case Kind::Network: nav().open(screen::kNet); return;
    case Kind::Gps: nav().open(screen::kGnss); return;
    case Kind::Clock: nav().open(screen::kClock); return;
    case Kind::Settings: nav().open(screen::kSettings); return;
    case Kind::Wifi: nav().open(screen::kWifi); return;
    case Kind::Motion: nav().open(screen::kMotion); return;
    case Kind::Tests: nav().open(screen::kSelfTest); return;
    case Kind::Status: nav().open(screen::kStatus); return;
    case Kind::Light: app().toggleFrontlight(); return;
  }
}

}  // namespace ui

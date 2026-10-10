// Host tests for the home screen: which tiles exist, their order, what each tile does, the badge,
// school time, the soft keys, touch, and the layout in every language.
// allow-non-english: the catalogs under test contain German and French.

#include <unity.h>

#include <DeviceRig.h>
#include <Shows.h>
#include <app/SystemEntry.h>
#include <ui/Fonts.h>
#include <ui/Text.h>
#include <board/Cst66xx.h>
#include <ui/screens/HomeScreen.h>

#include <optional>

using namespace people;
using namespace ui;
using Kind = HomeScreen::Kind;

void setUp() {}
void tearDown() {}

namespace {

std::vector<Kind> flat(const std::vector<std::vector<Kind>>& pages) {
  std::vector<Kind> all;
  for (const auto& page : pages) all.insert(all.end(), page.begin(), page.end());
  return all;
}

bool has(const std::vector<Kind>& kinds, Kind kind) {
  for (Kind k : kinds)
    if (k == kind) return true;
  return false;
}

void press(HomeScreen& home, Key key, int times = 1) {
  for (int i = 0; i < times; ++i) home.handle(Input::pressed(key));
}

// A grid with the same pages, to ask where a tile is.
Rect tileRect(const HomeScreen& home, size_t index) {
  TileGrid grid(3, 3);
  std::vector<std::vector<Tile>> pages;
  for (const auto& page : home.pages()) pages.emplace_back(page.size());
  grid.setPages(pages);
  return grid.tileRect(ScreenLayout::standard().content, index);
}

Profile everything() {
  return ProfileBuilder().childName("Alex").radio(true).parentTools(true).build();
}

Profile inSchool() { return ProfileBuilder().childName("Alex").schoolTime("tue", "14:00", "15:00").build(); }

}  // namespace

// ---- which tiles exist -----------------------------------------------------------------------

// The light is the child's (owner, 2026-10-08): it is on the first page, behind the inbox.
void test_a_child_device_has_five_tiles_in_front_and_the_tools_behind() {
  DeviceRig rig;
  HomeScreen home(rig.context());
  const auto pages = home.pages();
  TEST_ASSERT_EQUAL(2, pages.size());
  const std::vector<Kind> front = {Kind::Phone, Kind::Text, Kind::Sos, Kind::Inbox, Kind::Light};
  const std::vector<Kind> back = {Kind::Network, Kind::Gps, Kind::Clock, Kind::Settings, Kind::Tests};
  TEST_ASSERT_TRUE(pages[0] == front);
  TEST_ASSERT_TRUE(pages[1] == back);
}

void test_the_radio_tile_is_in_front_and_only_when_the_profile_allows_the_radio() {
  DeviceRig without;
  TEST_ASSERT_FALSE(has(flat(HomeScreen(without.context()).pages()), Kind::Radio));
  DeviceRig with(ProfileBuilder().radio(true).build());
  const auto pages = HomeScreen(with.context()).pages();
  TEST_ASSERT_TRUE(pages[0][pages[0].size() - 2] == Kind::Radio);   // behind the inbox, before the light, on the first page
  TEST_ASSERT_FALSE(has(pages[1], Kind::Radio));
}

void test_status_and_wifi_are_the_parents_tools() {
  DeviceRig child;
  const auto kinds = flat(HomeScreen(child.context()).pages());
  TEST_ASSERT_FALSE(has(kinds, Kind::Status));
  TEST_ASSERT_FALSE(has(kinds, Kind::Wifi));

  DeviceRig parents(ProfileBuilder().parentTools(true).build());
  const auto shown = flat(HomeScreen(parents.context()).pages());
  TEST_ASSERT_TRUE(has(shown, Kind::Status));
  TEST_ASSERT_TRUE(has(shown, Kind::Wifi));
}

void test_the_wifi_tile_needs_a_wifi_to_switch() {
  DeviceRig noWifi(ProfileBuilder().parentTools(true).build(), false);
  const auto kinds = flat(HomeScreen(noWifi.context()).pages());
  TEST_ASSERT_FALSE(has(kinds, Kind::Wifi));
  TEST_ASSERT_TRUE(has(kinds, Kind::Status));                // the status page does not
}

void test_the_motion_tile_needs_a_motion_sensor() {
  DeviceRig rig;
  TEST_ASSERT_FALSE(has(flat(HomeScreen(rig.context()).pages()), Kind::Motion));
  ScreenContext withSensor = rig.context();
  withSensor.motionSensor = true;
  TEST_ASSERT_TRUE(has(flat(HomeScreen(withSensor).pages()), Kind::Motion));
}

void test_phone_text_and_sos_need_somebody_to_reach() {
  DeviceRig nobody(ProfileBuilder().noContacts().build());
  const auto kinds = flat(HomeScreen(nobody.context()).pages());
  TEST_ASSERT_FALSE(has(kinds, Kind::Phone));
  TEST_ASSERT_FALSE(has(kinds, Kind::Text));
  TEST_ASSERT_FALSE(has(kinds, Kind::Sos));                  // nobody to send the position to
  TEST_ASSERT_TRUE(has(kinds, Kind::Inbox));                 // the history is there either way

  // A call target that is in no contact book is still somebody: the call actions may reach it.
  DeviceRig target(ProfileBuilder().noContacts().callTargets({kParentOne}).build());
  const auto reachable = flat(HomeScreen(target.context()).pages());
  TEST_ASSERT_TRUE(has(reachable, Kind::Phone));
  TEST_ASSERT_TRUE(has(reachable, Kind::Text));
  TEST_ASSERT_TRUE(has(reachable, Kind::Sos));

  // Contacts but no call action: the child may phone and write, and SOS has nobody to reach.
  DeviceRig book(ProfileBuilder().callTargets({}).build());
  const auto contacts = flat(HomeScreen(book.context()).pages());
  TEST_ASSERT_TRUE(has(contacts, Kind::Phone));
  TEST_ASSERT_FALSE(has(contacts, Kind::Sos));
}

// The parents choose which tiles of the second page the child sees ([home] in settings.toml, owner, 2026-10-08);
// the first page is the child's and stays. Wi-Fi and Status need the parent tools besides.
void test_the_parents_switch_tiles_of_the_second_page_off() {
  Profile p = everything();
  p.home.gps = false;
  p.home.tests = false;
  p.home.wifi = false;
  DeviceRig rig(p);
  const auto pages = HomeScreen(rig.context()).pages();
  TEST_ASSERT_FALSE(has(pages[1], Kind::Gps));
  TEST_ASSERT_FALSE(has(pages[1], Kind::Tests));
  TEST_ASSERT_FALSE(has(pages[1], Kind::Wifi));
  TEST_ASSERT_TRUE(has(pages[1], Kind::Network) && has(pages[1], Kind::Status) && has(pages[1], Kind::Settings));
  TEST_ASSERT_TRUE(has(pages[0], Kind::Light));              // the child's page is not switched here

  Profile child = ProfileBuilder().build();
  child.home.status = true;                                  // on, but without the parent tools still not shown
  DeviceRig noTools(child);
  TEST_ASSERT_FALSE(has(flat(HomeScreen(noTools.context()).pages()), Kind::Status));

  Profile none = everything();
  none.home = HomeTiles{false, false, false, false, false, false, false, false};
  DeviceRig empty(none);
  HomeScreen home(empty.context());
  TEST_ASSERT_EQUAL(1, home.pages().size());                 // all off: one page, and no dots
  empty.poll();
  home.render(empty.frame);
  const Rect content = ScreenLayout::standard().content;
  TEST_ASSERT_EQUAL(0, empty.frame.blackPixels(Rect{content.x, content.bottom() - 18, content.w, 18}));
}

// A contact ticked "SOS" is somebody to reach: the SOS tile is there without any call button.
void test_a_contact_ticked_for_sos_gives_the_sos_tile() {
  Profile p = ProfileBuilder().callTargets({}).build();
  DeviceRig without(p);
  TEST_ASSERT_FALSE(has(flat(HomeScreen(without.context()).pages()), Kind::Sos));
  p.sosContacts = {kParentOne};
  DeviceRig with(p);
  TEST_ASSERT_TRUE(has(flat(HomeScreen(with.context()).pages()), Kind::Sos));
}

void test_the_tools_always_fit_on_the_second_page() {
  // Every tool at once: eight, and a page holds nine. A tenth would be unreachable.
  DeviceRig rig(everything(), true);
  ScreenContext all = rig.context();
  all.motionSensor = true;
  const auto pages = HomeScreen(all).pages();
  TEST_ASSERT_EQUAL(8, pages[1].size());
  TEST_ASSERT_TRUE(pages[1].size() <= 9);
  TEST_ASSERT_TRUE(pages[0].size() <= 9);
}

// ---- moving the selection ---------------------------------------------------------------------

void test_down_goes_through_both_pages_and_wraps_and_the_page_follows() {
  DeviceRig rig;
  HomeScreen home(rig.context());
  rig.poll();
  home.render(rig.frame);
  TEST_ASSERT_TRUE(Kind::Phone == home.selected());
  TEST_ASSERT_EQUAL(0, home.page());
  press(home, Key::Down, 4);
  TEST_ASSERT_TRUE(Kind::Light == home.selected());
  TEST_ASSERT_EQUAL(0, home.page());
  press(home, Key::Down);                                    // the first tool: the second page
  TEST_ASSERT_TRUE(Kind::Network == home.selected());
  TEST_ASSERT_EQUAL(1, home.page());
  press(home, Key::Down, 4);
  TEST_ASSERT_TRUE(Kind::Tests == home.selected());
  press(home, Key::Down);                                    // and round to the start
  TEST_ASSERT_TRUE(Kind::Phone == home.selected());
  TEST_ASSERT_EQUAL(0, home.page());
  press(home, Key::Up);                                      // backwards to the last
  TEST_ASSERT_TRUE(Kind::Tests == home.selected());
}

void test_the_selection_is_kept_when_the_screen_is_left_and_come_back_to() {
  DeviceRig rig;
  HomeScreen home(rig.context());
  press(home, Key::Down, 5);                                 // the network tile, on the second page
  home.onLeave();
  home.onEnter();
  TEST_ASSERT_TRUE(Kind::Network == home.selected());
  TEST_ASSERT_EQUAL(1, home.page());
}

// ---- what a tile does -------------------------------------------------------------------------

void test_no_tile_appears_twice() {
  DeviceRig rig(everything());
  ScreenContext all = rig.context();
  all.motionSensor = true;
  const std::vector<Kind> kinds = flat(HomeScreen(all).pages());
  for (size_t i = 0; i < kinds.size(); ++i)
    for (size_t j = i + 1; j < kinds.size(); ++j) TEST_ASSERT_FALSE(kinds[i] == kinds[j]);
  TEST_ASSERT_EQUAL(14, kinds.size());                          // every tile there is
}

void test_every_tile_opens_its_screen() {
  DeviceRig rig(everything());
  ScreenContext all = rig.context();
  all.motionSensor = true;
  HomeScreen home(all);
  const struct { Kind kind; ScreenId id; } table[] = {
      {Kind::Phone, screen::kPhone},      {Kind::Text, screen::kRecipient}, {Kind::Sos, screen::kSos},
      {Kind::Inbox, screen::kMessages},   {Kind::Radio, screen::kRadio},    {Kind::Network, screen::kNet},
      {Kind::Gps, screen::kGnss},         {Kind::Clock, screen::kClock},    {Kind::Settings, screen::kSettings},
      {Kind::Wifi, screen::kWifi},        {Kind::Motion, screen::kMotion},  {Kind::Tests, screen::kSelfTest},
      {Kind::Status, screen::kStatus}};
  const std::vector<Kind> order = flat(home.pages());
  for (const auto& row : table) {
    rig.nav.clear();
    // Walk to the tile with the keys, as a child does.
    for (size_t i = 0; i < order.size() && home.selected() != row.kind; ++i) press(home, Key::Down);
    TEST_ASSERT_TRUE(home.selected() == row.kind);
    TEST_ASSERT_TRUE(home.handle(Input::pressed(Key::Select)));
    TEST_ASSERT_TRUE_MESSAGE(rig.nav.opened(row.id), (std::to_string(static_cast<int>(row.kind)) + ": " + rig.nav.last()).c_str());
  }
}

void test_the_phone_tile_opens_the_phone_page_without_dialling() {
  DeviceRig rig;
  HomeScreen home(rig.context());
  home.handle(Input::pressed(Key::Select));
  TEST_ASSERT_TRUE(rig.nav.opened(screen::kPhone));
  TEST_ASSERT_FALSE(rig.core.state().callActive);                // nothing is dialled by a tile
}

void test_the_text_tile_asks_for_the_recipient_first() {
  DeviceRig rig;
  HomeScreen home(rig.context());
  press(home, Key::Down);
  home.handle(Input::pressed(Key::Select));
  TEST_ASSERT_TRUE(rig.nav.opened(screen::kRecipient));
}

void test_the_light_tile_switches_the_front_light_and_says_so() {
  DeviceRig rig;
  HomeScreen home(rig.context());
  press(home, Key::Down, 4);                                 // the light is the last tile of the first page
  TEST_ASSERT_TRUE(Kind::Light == home.selected());
  TEST_ASSERT_FALSE(rig.core.state().frontlight);
  home.handle(Input::pressed(Key::Select));
  TEST_ASSERT_TRUE(rig.core.state().frontlight);
  TEST_ASSERT_TRUE(rig.core.state().notice.kind == NoticeKind::LightOn);
  TEST_ASSERT_EQUAL(0, rig.nav.calls.size());                // it stays on the home screen
  home.handle(Input::pressed(Key::Select));
  TEST_ASSERT_FALSE(rig.core.state().frontlight);
  TEST_ASSERT_TRUE(rig.core.state().notice.kind == NoticeKind::LightOff);
}

void test_the_sos_tile_begins_the_emergency_and_opens_its_page() {
  DeviceRig rig;
  HomeScreen home(rig.context());
  press(home, Key::Down, 2);
  TEST_ASSERT_TRUE(Kind::Sos == home.selected());
  home.handle(Input::pressed(Key::Select));
  TEST_ASSERT_TRUE(rig.nav.opened(screen::kSos));
  TEST_ASSERT_TRUE(rig.core.state().sosPhase == SosFlow::Phase::Choosing);
  TEST_ASSERT_EQUAL(0, rig.modem.sent().size());             // pressing is not sending
}

void test_the_back_key_locks_and_a_typed_character_is_not_used() {
  DeviceRig rig;
  HomeScreen home(rig.context());
  TEST_ASSERT_TRUE(home.handle(Input::pressed(Key::Back)));
  TEST_ASSERT_EQUAL_STRING("lock", rig.nav.last().c_str());
  TEST_ASSERT_FALSE(home.handle(Input::text('a')));
  TEST_ASSERT_TRUE(home.selected() == Kind::Phone);
}

// ---- school time ------------------------------------------------------------------------------

void test_in_school_time_the_phone_tile_says_until_when_and_stays_home() {
  DeviceRig rig(inSchool());
  HomeScreen home(rig.context());
  rig.poll();
  TEST_ASSERT_TRUE(home.handle(Input::pressed(Key::Select)));
  TEST_ASSERT_EQUAL(0, rig.nav.calls.size());
  TEST_ASSERT_TRUE(rig.core.state().notice.kind == NoticeKind::SchoolTimeUntil);
  TEST_ASSERT_EQUAL(15 * 60, rig.core.state().notice.minute);
  home.render(rig.frame);
  TEST_ASSERT_EQUAL_STRING("School time until 15:00", home.noticeLine().c_str());
  rig.language("de");
  home.render(rig.frame);
  TEST_ASSERT_EQUAL_STRING("Schulzeit bis 15:00", home.noticeLine().c_str());
  TEST_ASSERT_FALSE(rig.core.state().callActive);
}

void test_in_school_time_the_other_tiles_work_and_the_sos_opens_with_its_warning() {
  DeviceRig rig(inSchool());
  HomeScreen home(rig.context());
  rig.poll();
  press(home, Key::Down);                                    // text: not blocked in school time
  home.handle(Input::pressed(Key::Select));
  TEST_ASSERT_TRUE(rig.nav.opened(screen::kRecipient));
  rig.nav.clear();
  press(home, Key::Down);                                    // SOS
  home.handle(Input::pressed(Key::Select));
  TEST_ASSERT_TRUE(rig.nav.opened(screen::kSos));
  TEST_ASSERT_TRUE(rig.core.state().sosPhase == SosFlow::Phase::QuietConfirm);
  TEST_ASSERT_EQUAL(SosFlow::kOverrideTaps, rig.core.state().sosTapsLeft);
}

void test_after_school_time_the_phone_tile_opens_again() {
  DeviceRig rig(inSchool());
  HomeScreen home(rig.context());
  rig.clock.setEpochUtc(rig::kTuesdayAfternoon + 40 * 60);  // 15:12
  rig.poll();
  home.handle(Input::pressed(Key::Select));
  TEST_ASSERT_TRUE(rig.nav.opened(screen::kPhone));
}

// ---- the badge --------------------------------------------------------------------------------

void test_the_inbox_tile_shows_how_many_are_unread_and_nothing_when_none() {
  DeviceRig rig;
  HomeScreen home(rig.context());
  rig.poll();
  home.render(rig.frame);
  const Rect inbox = tileRect(home, 3);
  FrameBuffer none;
  home.render(none);
  rig.modem.injectSms(kParentOne, "School is out", 1);
  rig.modem.injectSms(kParentOne, "Are you coming?", 2);
  rig.poll();
  FrameBuffer two;
  home.render(two);
  // The badge sits in the top right corner of the tile: ink there now, none before.
  const Rect corner{inbox.right() - 26, inbox.y + 2, 22, 13};
  TEST_ASSERT_EQUAL(0, none.blackPixels(corner));
  TEST_ASSERT_TRUE(two.blackPixels(corner) > 0);
  // And it counts: three unread draw more ink than two.
  rig.modem.injectSms(kParentOne, "Hello?", 3);
  rig.poll();
  FrameBuffer three;
  home.render(three);
  TEST_ASSERT_FALSE(two.toAscii(corner) == three.toAscii(corner));
}

void test_system_entries_count_as_unread_on_the_tile() {
  DeviceRig rig;
  HomeScreen home(rig.context());
  const Rect inbox = tileRect(home, 3);
  rig.poll();
  FrameBuffer none;
  home.render(none);
  rig.modem.injectIncomingCall(kParentOne);                  // a call nobody answers is a missed call
  rig.poll();
  rig.modem.remoteHangup();
  rig.poll();
  TEST_ASSERT_EQUAL(1, rig.core.state().unread);
  FrameBuffer one;
  home.render(one);
  const Rect corner{inbox.right() - 26, inbox.y + 2, 22, 13};
  TEST_ASSERT_TRUE(one.blackPixels(corner) > 0);
}

// ---- soft keys and touch ----------------------------------------------------------------------

// No arrows on the home screen (owner, 2026-10-08: they only took room); the tiles are touched and the dots turn
// the page. The device stays usable without touch: Down reaches every tile of both pages once and comes round,
// Select opens the tile.
void test_the_soft_keys_have_no_arrows_but_a_lock_button_and_the_keys_still_reach_every_tile() {
  DeviceRig rig;
  HomeScreen home(rig.context());
  rig.poll();
  home.render(rig.frame);
  const SoftKeys keys = home.softKeys();
  TEST_ASSERT_FALSE(keys.scroll);
  TEST_ASSERT_TRUE(keys.action.empty());
  TEST_ASSERT_EQUAL_STRING("Lock", keys.back.c_str());
  std::vector<Kind> seen;
  bool secondPage = false;
  do {
    for (Kind k : seen) TEST_ASSERT_FALSE(k == home.selected());     // every tile once
    seen.push_back(home.selected());
    secondPage = secondPage || home.page() == 1;
    home.handle(Input::pressed(Key::Down));
  } while (home.selected() != seen.front() && seen.size() < 30);
  TEST_ASSERT_TRUE(secondPage);
  TEST_ASSERT_TRUE(seen.size() > 9);
  home.handle(Input::pressed(Key::Down));                             // the second tile: Text
  home.handle(Input::pressed(Key::Select));
  TEST_ASSERT_TRUE(rig.nav.opened(screen::kRecipient));
}

namespace {
// The nine bytes the touch chip sends for a finger at (x, y), or for no finger (Cst66xx.h).
board::cst66xx::Report chipReport(int x, int y, bool down) {
  uint8_t buf[9] = {};
  buf[2] = 0xFF;
  buf[3] = 0x01;
  buf[4] = (uint8_t)(x & 0xFF);
  buf[5] = (uint8_t)(y & 0xFF);
  buf[6] = 40;
  buf[7] = (uint8_t)(((x >> 8) & 0x0F) | (((y >> 8) & 0x0F) << 4));
  buf[8] = down ? 0x60 : 0x00;
  const uint16_t sum = board::cst66xx::checksum(&buf[4], 5);
  buf[0] = (uint8_t)(sum & 0xFF);
  buf[1] = (uint8_t)(sum >> 8);
  return board::cst66xx::parseReport(buf, sizeof buf);
}

// A finger lands at (x, y) and goes up again: the input the device's own translation makes of it.
Input fingerAt(board::TouchDecoder& decoder, int x, int y) {
  Input in;
  TEST_ASSERT_TRUE(decoder.decode(chipReport(x, y, true), in));
  Input none;
  decoder.decode(chipReport(x, y, false), none);
  return in;
}
}  // namespace

// The page dots of 4.0 were six pixels side by side in the middle, twelve pixels of target each (owner,
// 2026-10-08: hard to hit). Now the strip under the tiles is cut in two: its left half is the first page, its
// right half the second, as tall as before. Checked through the touch chip's own coordinates, up to the edges.
void test_the_page_dots_are_wide_targets_that_turn_the_page_through_the_touch_chip() {
  DeviceRig rig;
  HomeScreen home(rig.context());
  rig.poll();
  home.render(rig.frame);
  const Rect content = ScreenLayout::standard().content;
  TileGrid grid(3, 3);
  grid.setPages({std::vector<Tile>(4), std::vector<Tile>(6)});
  TEST_ASSERT_TRUE(grid.dotTarget(content, 0) == (Rect{content.x, content.bottom() - 18, content.w / 2, 18}));
  TEST_ASSERT_TRUE(grid.dotTarget(content, 1) == (Rect{content.x + content.w / 2, content.bottom() - 18, content.w - content.w / 2, 18}));

  board::TouchDecoder decoder;
  const int y = content.bottom() - 9;
  // the right half near the middle, its right edge, and a reading beyond the panel (clamped by the decoder)
  for (int x : {content.w / 2 + 1, content.w - 1, content.w + 20}) {
    TEST_ASSERT_TRUE(home.handle(fingerAt(decoder, x, y)));
    TEST_ASSERT_EQUAL(1, home.page());
    TEST_ASSERT_TRUE(Kind::Network == home.selected());                // the first tile of that page
    TEST_ASSERT_EQUAL(0, rig.nav.calls.size());                        // a dot is not a tile
    TEST_ASSERT_TRUE(home.handle(fingerAt(decoder, 1, y)));            // the left edge: the first page
    TEST_ASSERT_EQUAL(0, home.page());
  }
  // the top and bottom rows of the strip still count; a finger just above it is on the tiles' side
  TEST_ASSERT_TRUE(home.handle(fingerAt(decoder, 200, content.bottom() - 18)));
  TEST_ASSERT_EQUAL(1, home.page());
  home.handle(fingerAt(decoder, 10, content.bottom() - 1));
  TEST_ASSERT_EQUAL(0, home.page());
}

void test_a_tap_on_a_tile_opens_it_and_a_tap_beside_the_tiles_does_nothing() {
  DeviceRig rig;
  HomeScreen home(rig.context());
  rig.poll();
  home.render(rig.frame);
  const Rect phone = tileRect(home, 0);                        // a tap on the phone tile opens the page and dials nothing
  TEST_ASSERT_TRUE(home.handle(Input::touch(phone.x + 5, phone.y + 5)));
  TEST_ASSERT_TRUE(rig.nav.opened(screen::kPhone));
  TEST_ASSERT_FALSE(rig.core.state().callActive);
  rig.nav.clear();
  const Rect text = tileRect(home, 1);
  TEST_ASSERT_TRUE(home.handle(Input::touch(text.x + 5, text.y + 5)));
  TEST_ASSERT_TRUE(rig.nav.opened(screen::kRecipient));
  TEST_ASSERT_TRUE(Kind::Text == home.selected());
  rig.nav.clear();
  const Rect empty = tileRect(home, 3);                      // below the four tiles of the page are free slots
  TEST_ASSERT_FALSE(home.handle(Input::touch(empty.x + 5, empty.bottom() + 20)));
  TEST_ASSERT_EQUAL(0, rig.nav.calls.size());
}

void test_a_tap_on_a_tile_of_the_other_page_is_not_possible_and_the_dots_switch_pages() {
  DeviceRig rig;
  HomeScreen home(rig.context());
  rig.poll();
  home.render(rig.frame);
  const Rect content = ScreenLayout::standard().content;
  TileGrid grid(3, 3);
  grid.setPages({std::vector<Tile>(4), std::vector<Tile>(6)});
  const Rect second = grid.dotTarget(content, 1);
  TEST_ASSERT_TRUE(home.handle(Input::touch(second.x + 3, second.y + 3)));
  TEST_ASSERT_EQUAL(1, home.page());
  TEST_ASSERT_TRUE(Kind::Network == home.selected());
  TEST_ASSERT_EQUAL(0, rig.nav.calls.size());                // a dot is not a tile
  const Rect first = grid.dotTarget(content, 0);
  home.handle(Input::touch(first.x + 3, first.y + 3));
  TEST_ASSERT_EQUAL(0, home.page());
}

// ---- the layout in every language -------------------------------------------------------------

void test_every_label_fits_its_tile_in_the_shipped_languages() {
  TileGrid grid(3, 3);
  grid.setPages({std::vector<Tile>(9)});
  const int inner = grid.tileRect(ScreenLayout::standard().content, 0).w - 6;
  TEST_ASSERT_EQUAL(68, inner);
  const char* keys[] = {"home.phone", "home.text", "home.sos", "home.inbox", "home.radio", "home.network", "home.gps",
                        "home.clock", "home.settings", "home.wifi", "home.motion", "home.tests", "home.status", "home.light"};
  std::string tooWide;
  languages::forEach([&](const std::string& language, i18n::Translator& t) {
    if (language == i18n::kPseudoCode) return;   // longer than any real language on purpose: exempt
    for (const char* key : keys) {
      const int width = text::width(font(FontSize::Small), t.get(key));
      if (width > inner) tooWide += language + " " + key + " " + t.get(key) + " (" + std::to_string(width) + " px)\n";
    }
  });
  TEST_ASSERT_TRUE_MESSAGE(tooWide.empty(), ("wider than " + std::to_string(inner) + " px:\n" + tooWide).c_str());
}

// The Lock button is there in every language, with its whole word (the symbol goes first when the slot is short).
void test_the_lock_button_is_there_in_every_language_with_its_whole_word() {
  const struct { const char* code; const char* word; } words[] = {{"en", "Lock"}, {"de", "Sperren"}, {"fr", "Verrouiller"}};
  for (const auto& w : words) {
    DeviceRig rig;
    rig.language(w.code);
    HomeScreen home(rig.context());
    rig.poll();
    home.render(rig.frame);
    const SoftKeys keys = home.softKeys();
    TEST_ASSERT_EQUAL_STRING_MESSAGE(w.word, keys.back.c_str(), w.code);
    const Rect slot = softKeySlots(keys, ScreenLayout::standard().softKeys).back;
    TEST_ASSERT_FALSE_MESSAGE(slot.empty(), w.code);
    TEST_ASSERT_TRUE_MESSAGE(text::width(font(FontSize::Small), keys.back) <= slot.w - 6, w.code);
    TEST_ASSERT_TRUE_MESSAGE(shows::text(rig.frame, w.word, FontSize::Small, ScreenLayout::standard().softKeys), w.code);
  }
}

void test_the_tiles_say_their_words_in_the_language_of_the_moment() {
  const struct { const char* code; const char* front[5]; const char* tools[5]; } words[] = {
      {"en", {"Phone", "Text", "SOS", "Inbox", "Light"}, {"Network", "GPS", "Clock", "Settings", "Tests"}},
      {"de", {"Telefon", "Text", "SOS", "Eingang", "Licht"}, {"Netz", "GPS", "Uhr", "Option", "Tests"}},
      {"fr", {"Appels", "\xC3\x89" "crire", "SOS", "Courrier", "Lumi\xC3\xA8re"}, {"R\xC3\xA9seau", "GPS", "Heure", "Options", "Tests"}}};
  for (const auto& w : words) {
    DeviceRig rig;
    rig.language(w.code);
    HomeScreen home(rig.context());
    rig.poll();
    home.render(rig.frame);
    const Rect content = ScreenLayout::standard().content;
    for (const char* label : w.front) {
      // The selected tile is inverted; the others are black on white.
      const bool shown = shows::text(rig.frame, label, FontSize::Small, content) ||
                         shows::text(rig.frame, label, FontSize::Small, content, true);
      TEST_ASSERT_TRUE_MESSAGE(shown, (std::string(w.code) + " " + label).c_str());
    }
    press(home, Key::Down, 5);
    home.render(rig.frame);
    for (const char* label : w.tools) {
      const bool shown = shows::text(rig.frame, label, FontSize::Small, content) ||
                         shows::text(rig.frame, label, FontSize::Small, content, true);
      TEST_ASSERT_TRUE_MESSAGE(shown, (std::string(w.code) + " " + label).c_str());
    }
    // And none of the first page is on the second.
    TEST_ASSERT_FALSE(shows::text(rig.frame, w.front[3], FontSize::Small, content));
  }
}

void test_both_pages_are_drawn_inside_the_screen_in_every_language() {
  // Labels of the shipped languages fit their tile (the test above); the pseudo-language, which is
  // exempt from that, must still stay on the display.
  languages::forEach([](const std::string& language, i18n::Translator&) {
    DeviceRig rig(everything());
    ScreenContext all = rig.context();
    all.motionSensor = true;
    rig.language(language);
    HomeScreen home(all);
    rig.modem.injectSms(kParentOne, "Hello", 1);
    rig.poll();
    for (int page = 0; page < 2; ++page) {
      FrameBuffer fb;
      home.render(fb);
      TEST_ASSERT_FALSE_MESSAGE(fb.clipped(), (language + ":\n" + fb.describeClips()).c_str());
      press(home, Key::Down, 5);
    }
  });
}

void test_the_home_pictures_in_english_and_german() {
  for (const char* language : {"en", "de"}) {
    DeviceRig rig(everything());
    ScreenContext all = rig.context();
    all.motionSensor = true;
    rig.language(language);
    HomeScreen home(all);
    rig.modem.injectSms(kParentOne, "Hello", 1);
    rig.modem.injectSms(kParentOne, "Hello again", 2);
    rig.poll();
    home.render(rig.frame);
    golden::expect(rig.frame, std::string("home_front_") + language);
    press(home, Key::Down, 5 + 2);                           // five tiles in front, then the second page, the third tool
    home.render(rig.frame);
    golden::expect(rig.frame, std::string("home_tools_") + language);
  }
}

void test_a_notice_does_not_move_the_tiles() {
  DeviceRig rig(inSchool());
  HomeScreen home(rig.context());
  rig.poll();
  home.render(rig.frame);
  const Rect content = ScreenLayout::standard().content;
  const std::string quiet = rig.frame.toAscii(content);
  home.handle(Input::pressed(Key::Select));                  // the phone tile says "School time until 15:00"
  home.render(rig.frame);
  TEST_ASSERT_TRUE(rig.frame.blackPixels(ScreenLayout::standard().alert) > 0);
  TEST_ASSERT_TRUE(quiet == rig.frame.toAscii(content));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_a_child_device_has_five_tiles_in_front_and_the_tools_behind);
  RUN_TEST(test_the_radio_tile_is_in_front_and_only_when_the_profile_allows_the_radio);
  RUN_TEST(test_status_and_wifi_are_the_parents_tools);
  RUN_TEST(test_the_wifi_tile_needs_a_wifi_to_switch);
  RUN_TEST(test_the_motion_tile_needs_a_motion_sensor);
  RUN_TEST(test_phone_text_and_sos_need_somebody_to_reach);
  RUN_TEST(test_the_parents_switch_tiles_of_the_second_page_off);
  RUN_TEST(test_a_contact_ticked_for_sos_gives_the_sos_tile);
  RUN_TEST(test_the_tools_always_fit_on_the_second_page);
  RUN_TEST(test_down_goes_through_both_pages_and_wraps_and_the_page_follows);
  RUN_TEST(test_the_selection_is_kept_when_the_screen_is_left_and_come_back_to);
  RUN_TEST(test_no_tile_appears_twice);
  RUN_TEST(test_every_tile_opens_its_screen);
  RUN_TEST(test_the_phone_tile_opens_the_phone_page_without_dialling);
  RUN_TEST(test_the_text_tile_asks_for_the_recipient_first);
  RUN_TEST(test_the_light_tile_switches_the_front_light_and_says_so);
  RUN_TEST(test_the_sos_tile_begins_the_emergency_and_opens_its_page);
  RUN_TEST(test_the_back_key_locks_and_a_typed_character_is_not_used);
  RUN_TEST(test_in_school_time_the_phone_tile_says_until_when_and_stays_home);
  RUN_TEST(test_in_school_time_the_other_tiles_work_and_the_sos_opens_with_its_warning);
  RUN_TEST(test_after_school_time_the_phone_tile_opens_again);
  RUN_TEST(test_the_inbox_tile_shows_how_many_are_unread_and_nothing_when_none);
  RUN_TEST(test_system_entries_count_as_unread_on_the_tile);
  RUN_TEST(test_the_soft_keys_have_no_arrows_but_a_lock_button_and_the_keys_still_reach_every_tile);
  RUN_TEST(test_the_page_dots_are_wide_targets_that_turn_the_page_through_the_touch_chip);
  RUN_TEST(test_a_tap_on_a_tile_opens_it_and_a_tap_beside_the_tiles_does_nothing);
  RUN_TEST(test_a_tap_on_a_tile_of_the_other_page_is_not_possible_and_the_dots_switch_pages);
  RUN_TEST(test_every_label_fits_its_tile_in_the_shipped_languages);
  RUN_TEST(test_the_lock_button_is_there_in_every_language_with_its_whole_word);
  RUN_TEST(test_the_tiles_say_their_words_in_the_language_of_the_moment);
  RUN_TEST(test_both_pages_are_drawn_inside_the_screen_in_every_language);
  RUN_TEST(test_the_home_pictures_in_english_and_german);
  RUN_TEST(test_a_notice_does_not_move_the_tiles);
  return UNITY_END();
}

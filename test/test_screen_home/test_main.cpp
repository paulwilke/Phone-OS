// Host tests for the home screen: which tiles exist, their order, what each tile does, the badge,
// school time, the soft keys, touch, and the layout in every language.
// allow-non-english: the catalogs under test contain German and French.

#include <unity.h>

#include <DeviceRig.h>
#include <Shows.h>
#include <app/SystemEntry.h>
#include <ui/Fonts.h>
#include <ui/Text.h>
#include <ui/screens/HomeScreen.h>

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

void test_a_child_device_has_four_tiles_in_front_and_the_tools_behind() {
  DeviceRig rig;
  HomeScreen home(rig.context());
  const auto pages = home.pages();
  TEST_ASSERT_EQUAL(2, pages.size());
  const std::vector<Kind> front = {Kind::Phone, Kind::Text, Kind::Sos, Kind::Inbox};
  const std::vector<Kind> back = {Kind::Network, Kind::Gps, Kind::Clock, Kind::Settings, Kind::Tests, Kind::Light};
  TEST_ASSERT_TRUE(pages[0] == front);
  TEST_ASSERT_TRUE(pages[1] == back);
}

void test_the_radio_tile_is_in_front_and_only_when_the_profile_allows_the_radio() {
  DeviceRig without;
  TEST_ASSERT_FALSE(has(flat(HomeScreen(without.context()).pages()), Kind::Radio));
  DeviceRig with(ProfileBuilder().radio(true).build());
  const auto pages = HomeScreen(with.context()).pages();
  TEST_ASSERT_TRUE(pages[0].back() == Kind::Radio);          // behind the inbox, on the first page
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

void test_the_tools_always_fit_on_the_second_page() {
  // Every tool at once: nine, and a page holds nine. A tenth would be unreachable.
  DeviceRig rig(everything(), true);
  ScreenContext all = rig.context();
  all.motionSensor = true;
  const auto pages = HomeScreen(all).pages();
  TEST_ASSERT_EQUAL(9, pages[1].size());
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
  press(home, Key::Down, 3);
  TEST_ASSERT_TRUE(Kind::Inbox == home.selected());
  TEST_ASSERT_EQUAL(0, home.page());
  press(home, Key::Down);                                    // the first tool: the second page
  TEST_ASSERT_TRUE(Kind::Network == home.selected());
  TEST_ASSERT_EQUAL(1, home.page());
  press(home, Key::Down, 5);
  TEST_ASSERT_TRUE(Kind::Light == home.selected());
  press(home, Key::Down);                                    // and round to the start
  TEST_ASSERT_TRUE(Kind::Phone == home.selected());
  TEST_ASSERT_EQUAL(0, home.page());
  press(home, Key::Up);                                      // backwards to the last
  TEST_ASSERT_TRUE(Kind::Light == home.selected());
}

void test_the_selection_is_kept_when_the_screen_is_left_and_come_back_to() {
  DeviceRig rig;
  HomeScreen home(rig.context());
  press(home, Key::Down, 4);                                 // the network tile, on the second page
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
  press(home, Key::Down, 9);                                 // 4 + 5 tools: the light is the last tile
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

void test_the_soft_keys_show_the_arrows_only() {
  DeviceRig rig;
  HomeScreen home(rig.context());
  rig.poll();
  home.render(rig.frame);
  TEST_ASSERT_TRUE(home.softKeys().scroll);
  TEST_ASSERT_TRUE(home.softKeys().back.empty());
  TEST_ASSERT_TRUE(home.softKeys().action.empty());
  const SoftKeySlots slots = softKeySlots(home.softKeys(), ScreenLayout::standard().softKeys);
  TEST_ASSERT_FALSE(slots.up.empty());
  TEST_ASSERT_FALSE(slots.down.empty());
  TEST_ASSERT_TRUE(slots.back.empty());
  TEST_ASSERT_TRUE(slots.action.empty());
}

void test_a_tap_on_the_arrows_moves_the_selection() {
  DeviceRig rig;
  HomeScreen home(rig.context());
  rig.poll();
  home.render(rig.frame);
  const SoftKeySlots slots = softKeySlots(home.softKeys(), ScreenLayout::standard().softKeys);
  home.handle(Input::touch(slots.down.x + 2, slots.down.y + 2));
  TEST_ASSERT_TRUE(Kind::Text == home.selected());
  home.handle(Input::touch(slots.up.x + 2, slots.up.y + 2));
  TEST_ASSERT_TRUE(Kind::Phone == home.selected());
  TEST_ASSERT_EQUAL(0, rig.nav.calls.size());
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

void test_the_tiles_say_their_words_in_the_language_of_the_moment() {
  const struct { const char* code; const char* front[4]; const char* tools[6]; } words[] = {
      {"en", {"Phone", "Text", "SOS", "Inbox"}, {"Network", "GPS", "Clock", "Settings", "Tests", "Light"}},
      {"de", {"Telefon", "Text", "SOS", "Eingang"}, {"Netz", "GPS", "Uhr", "Option", "Tests", "Licht"}},
      {"fr", {"Appels", "\xC3\x89" "crire", "SOS", "Courrier"}, {"R\xC3\xA9seau", "GPS", "Heure", "Options", "Tests", "Lumi\xC3\xA8re"}}};
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
    press(home, Key::Down, 4);
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
  RUN_TEST(test_a_child_device_has_four_tiles_in_front_and_the_tools_behind);
  RUN_TEST(test_the_radio_tile_is_in_front_and_only_when_the_profile_allows_the_radio);
  RUN_TEST(test_status_and_wifi_are_the_parents_tools);
  RUN_TEST(test_the_wifi_tile_needs_a_wifi_to_switch);
  RUN_TEST(test_the_motion_tile_needs_a_motion_sensor);
  RUN_TEST(test_phone_text_and_sos_need_somebody_to_reach);
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
  RUN_TEST(test_the_soft_keys_show_the_arrows_only);
  RUN_TEST(test_a_tap_on_the_arrows_moves_the_selection);
  RUN_TEST(test_a_tap_on_a_tile_opens_it_and_a_tap_beside_the_tiles_does_nothing);
  RUN_TEST(test_a_tap_on_a_tile_of_the_other_page_is_not_possible_and_the_dots_switch_pages);
  RUN_TEST(test_every_label_fits_its_tile_in_the_shipped_languages);
  RUN_TEST(test_the_tiles_say_their_words_in_the_language_of_the_moment);
  RUN_TEST(test_both_pages_are_drawn_inside_the_screen_in_every_language);
  RUN_TEST(test_the_home_pictures_in_english_and_german);
  RUN_TEST(test_a_notice_does_not_move_the_tiles);
  return UNITY_END();
}

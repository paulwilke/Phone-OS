// Frame budgets: how often the e-ink panel may be refreshed in a scenario. A screen that draws
// something different every second (a clock with seconds, a blinking cursor, a number that
// ticks) makes the panel flash and wastes the battery; each scenario here states the refreshes
// it is allowed and fails, with the history, when the interface asks for more.

#include <unity.h>

#include <Languages.h>
#include <RefreshRig.h>
#include <ui/Layout.h>
#include <ui/ListView.h>
#include <ui/ScreenStack.h>
#include <ui/SoftKeys.h>
#include <ui/StatusBar.h>
#include <ui/Symbols.h>
#include <ui/TextBlock.h>
#include <ui/TileGrid.h>

using namespace ui;
using i18n::Translator;

void setUp() {}
void tearDown() {}

namespace {

constexpr uint32_t kSecond = 1000, kMinute = 60 * kSecond, kHour = 60 * kMinute;

// A home screen as the framework builds it: status bar, alert bar, tiles, soft keys.
struct HomeScreen {
  explicit HomeScreen(Translator& translator) : t(translator), grid(2, 3) {
    status.link = StatusModel::Link::Operator;
    status.operatorName = "Example Mobile";
    status.batteryPercent = 68;
    status.dateKnown = true;
    status.month = 9;
    status.day = 30;
    status.weekday = 2;
    std::vector<Tile> tiles = {{t.get("home.phone"), symbol::kTelephone, ""},
                               {t.get("home.text"), symbol::kAeroplane, ""},
                               {t.get("home.inbox"), symbol::kBubble, ""},
                               {t.get("home.sos"), symbol::kPin, ""}};
    grid.setPages({tiles});
    keys.back = t.get("key.back");
    keys.action = t.get("key.open");
    keys.scroll = true;
  }

  void draw(Display& d) {
    const ScreenLayout l = ScreenLayout::standard();
    drawStatusBar(d, l.status, status, t);
    drawAlertBar(d, l.alert, alert);
    grid.draw(d, l.content);
    drawSoftKeys(d, l.softKeys, keys);
  }

  bool press(Key key) {
    bool chosen;
    return grid.handle(Input::pressed(key), ScreenLayout::standard().content, &chosen);
  }

  Translator& t;
  StatusModel status;
  std::string alert;
  TileGrid grid;
  SoftKeys keys;
};

Translator english() {
  Translator t = languages::loaded();
  t.select("en");
  return t;
}

}  // namespace

// ---- idle -----------------------------------------------------------------------------------

void test_an_idle_hour_with_a_polled_status_costs_no_refresh() {
  Translator t = english();
  HomeScreen home(t);
  RefreshRig rig;
  rig.frame(0, [&](Display& d) { home.draw(d); });
  rig.expect("boot", 0, 1);
  rig.mark();
  // The screen is redrawn every second, the battery runs down inside one bar (68 % .. 61 % are
  // four bars), the network is polled: none of it changes the picture.
  for (uint32_t s = 1; s <= 3600; ++s) {
    home.status.batteryPercent = 68 - static_cast<int>(s / 500);
    rig.frame(s * kSecond, [&](Display& d) { home.draw(d); });
  }
  TEST_ASSERT_EQUAL(61, home.status.batteryPercent);
  rig.expect("an idle hour", 0, 0);
  TEST_ASSERT_EQUAL(3600, rig.controller.stats().skipped);
}

void test_nine_idle_hours_cost_one_full_refresh_per_four_hours() {
  Translator t = english();
  HomeScreen home(t);
  RefreshRig rig;
  rig.frame(0, [&](Display& d) { home.draw(d); });
  rig.mark();
  for (uint32_t m = 1; m <= 9 * 60; ++m) rig.frame(m * kMinute, [&](Display& d) { home.draw(d); });
  rig.expect("nine idle hours", 0, 2);   // at 4 h and at 8 h
  TEST_ASSERT_EQUAL(4 * kHour, rig.panel.events[1].nowMs);
  TEST_ASSERT_EQUAL(8 * kHour, rig.panel.events[2].nowMs);
}

// ---- one event, one refresh ---------------------------------------------------------------------

void test_each_move_of_the_selection_is_one_partial_refresh_and_the_tenth_change_is_full() {
  Translator t = english();
  HomeScreen home(t);
  RefreshRig rig;
  rig.frame(0, [&](Display& d) { home.draw(d); });
  rig.mark();
  for (int i = 1; i <= 9; ++i) {
    home.press(Key::Down);
    rig.frame(i * kSecond, [&](Display& d) { home.draw(d); });
    rig.expect("moves", i, 0);
  }
  home.press(Key::Down);
  rig.frame(10 * kSecond, [&](Display& d) { home.draw(d); });
  rig.expect("ten moves", 9, 1);   // the ghosts of nine partial refreshes are cleared
}

void test_a_key_that_changes_nothing_costs_no_refresh() {
  Translator t = english();
  HomeScreen home(t);
  home.grid.setPages({{Tile{"Only", nullptr, ""}}});   // one tile: Up and Down stay where they are
  RefreshRig rig;
  rig.frame(0, [&](Display& d) { home.draw(d); });
  rig.mark();
  for (Key k : {Key::Up, Key::Down, Key::Up}) {
    home.press(k);
    rig.frame(kSecond, [&](Display& d) { home.draw(d); });
  }
  rig.expect("keys on a single tile", 0, 0);

  ListView list;
  list.setItems({{"Parent One", true}, {"Parent Two", true}});
  const Rect area = ScreenLayout::standard().content;
  RefreshRig listRig;
  listRig.frame(0, [&](Display& d) { list.draw(d, area); });
  listRig.mark();
  bool chosen;
  list.handle(Input::pressed(Key::Up), area, &chosen);   // already on the first row
  listRig.frame(kSecond, [&](Display& d) { list.draw(d, area); });
  listRig.expect("up on the first row", 0, 0);
  list.handle(Input::pressed(Key::Down), area, &chosen);
  listRig.frame(2 * kSecond, [&](Display& d) { list.draw(d, area); });
  listRig.expect("down", 1, 0);
  list.handle(Input::pressed(Key::Down), area, &chosen);  // already on the last row
  listRig.frame(3 * kSecond, [&](Display& d) { list.draw(d, area); });
  listRig.expect("down on the last row", 1, 0);

  TextBlock block;
  block.setText("A short text");
  RefreshRig blockRig;
  blockRig.frame(0, [&](Display& d) { block.draw(d, area); });
  blockRig.mark();
  block.handle(Input::pressed(Key::Down), area);   // nothing to scroll
  blockRig.frame(kSecond, [&](Display& d) { block.draw(d, area); });
  blockRig.expect("scrolling a short text", 0, 0);
}

void test_what_the_status_bar_shows_changes_the_panel_only_when_it_changes_the_picture() {
  Translator t = english();
  HomeScreen home(t);
  RefreshRig rig;
  rig.frame(0, [&](Display& d) { home.draw(d); });
  rig.mark();
  const auto redraw = [&](uint32_t at) { rig.frame(at, [&](Display& d) { home.draw(d); }); };

  home.status.batteryPercent = 61;                  // same four bars
  redraw(1 * kSecond);
  rig.expect("battery inside a bar", 0, 0);
  home.status.batteryPercent = 60;                  // the fourth bar goes
  redraw(2 * kSecond);
  rig.expect("battery crosses a bar", 1, 0);

  home.status.locating = true;                      // the GPS note appears ...
  redraw(3 * kSecond);
  home.status.locating = false;                     // ... and goes
  redraw(4 * kSecond);
  rig.expect("GPS on and off", 3, 0);

  home.status.day = 1;                              // midnight
  home.status.month = 10;
  home.status.weekday = 3;
  redraw(5 * kSecond);
  rig.expect("a new day", 4, 0);

  home.status.operatorName = "Example Mobile";      // polled again, same name
  redraw(6 * kSecond);
  rig.expect("the network polled again", 4, 0);
}

void test_an_alert_costs_a_refresh_when_it_appears_and_one_when_it_goes() {
  Translator t = english();
  HomeScreen home(t);
  RefreshRig rig;
  rig.frame(0, [&](Display& d) { home.draw(d); });
  rig.mark();
  home.alert = "Parent One is calling";
  for (uint32_t s = 1; s <= 5; ++s) rig.frame(s * kSecond, [&](Display& d) { home.draw(d); });   // shown for five seconds
  home.alert = "";
  rig.frame(6 * kSecond, [&](Display& d) { home.draw(d); });
  rig.expect("an alert", 2, 0);
}

// ---- the stack -----------------------------------------------------------------------------------

namespace {

// A screen that draws a home picture or a list, for the stack scenarios.
class ScenarioScreen : public Screen {
public:
  explicit ScenarioScreen(std::function<void(Display&)> draw) : draw_(std::move(draw)) {}
  bool handle(const Input&) override { return false; }
  void render(Display& d) override { draw_(d); }
  void onEnter() override { ++entered; }
  int entered = 0;

private:
  std::function<void(Display&)> draw_;
};

}  // namespace

void test_navigating_costs_one_refresh_per_screen_change_and_locking_stops_the_flicker() {
  Translator t = english();
  HomeScreen home(t);
  ListView list;
  list.setItems({{"Parent One", true}, {"Parent Two", true}});
  TextBlock lock(FontSize::Large);
  lock.setText("Locked");

  ScenarioScreen homeScreen([&](Display& d) { home.draw(d); });
  ScenarioScreen listScreen([&](Display& d) {
    drawStatusBar(d, ScreenLayout::standard().status, home.status, t);
    list.draw(d, ScreenLayout::standard().content);
  });
  ScenarioScreen lockScreen([&](Display& d) { lock.draw(d, ScreenLayout::standard().content); });
  enum { Home = 1, List, Lock };
  ScreenStack stack(Home);
  stack.add(Home, homeScreen);
  stack.add(List, listScreen);
  stack.add(Lock, lockScreen);
  stack.setLockScreen(Lock);
  stack.setAutoLockSeconds(120);
  stack.begin(0);

  RefreshRig rig;
  const auto frame = [&](uint32_t seconds) {
    stack.poll(seconds);
    rig.frame(seconds * kSecond, [&](Display& d) { stack.render(d); });
  };
  frame(0);
  rig.mark();
  stack.open(List);
  frame(1);
  rig.expect("open the list", 1, 0);
  stack.back();
  frame(2);
  rig.expect("back", 2, 0);
  for (uint32_t s = 3; s <= 60; ++s) frame(s);
  rig.expect("a minute of looking at the home screen", 2, 0);

  for (uint32_t s = 61; s <= 3 * 3600; ++s) frame(s);   // the device locks after 120 s idle; three hours pass
  TEST_ASSERT_TRUE(stack.locked());
  rig.expect("locked for three hours", 3, 0);            // the lock screen is one more change, then nothing
}

// ---- determinism --------------------------------------------------------------------------------------

void test_drawing_the_same_state_twice_gives_the_same_picture_in_every_language() {
  languages::forEach([](const std::string& language, Translator& t) {
    HomeScreen home(t);
    home.alert = t.get("call.no-answer", {{"name", "Parent One"}});
    FrameBuffer a, b;
    home.draw(a);
    home.draw(b);
    TEST_ASSERT_TRUE_MESSAGE(a == b, language.c_str());

    ListView list;
    list.setItems({{t.get("messages.none"), false}, {"Parent One", true}, {"Parent Two", true}});
    FrameBuffer c, d;
    const Rect area = ScreenLayout::standard().content;
    list.draw(c, area);
    list.draw(d, area);
    TEST_ASSERT_TRUE_MESSAGE(c == d, language.c_str());

    TextBlock block;
    block.setText(t.get("school.until", {{"time", "14:30"}}));
    FrameBuffer e, f;
    block.draw(e, area);
    block.draw(f, area);
    TEST_ASSERT_TRUE_MESSAGE(e == f, language.c_str());
  });
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_an_idle_hour_with_a_polled_status_costs_no_refresh);
  RUN_TEST(test_nine_idle_hours_cost_one_full_refresh_per_four_hours);
  RUN_TEST(test_each_move_of_the_selection_is_one_partial_refresh_and_the_tenth_change_is_full);
  RUN_TEST(test_a_key_that_changes_nothing_costs_no_refresh);
  RUN_TEST(test_what_the_status_bar_shows_changes_the_panel_only_when_it_changes_the_picture);
  RUN_TEST(test_an_alert_costs_a_refresh_when_it_appears_and_one_when_it_goes);
  RUN_TEST(test_navigating_costs_one_refresh_per_screen_change_and_locking_stops_the_flicker);
  RUN_TEST(test_drawing_the_same_state_twice_gives_the_same_picture_in_every_language);
  return UNITY_END();
}

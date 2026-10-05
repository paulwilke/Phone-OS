// Host tests for the lock screen: unlocking takes two presses of Enter and nothing else does, what
// the screen says without a clock, the front light, and the stack around it.
// allow-non-english: the catalogs under test contain German and French.

#include <unity.h>

#include <DeviceRig.h>
#include <RefreshRig.h>
#include <Shows.h>
#include <ui/Text.h>
#include <ui/screens/HomeScreen.h>
#include <ui/screens/LockScreen.h>

using namespace people;
using namespace ui;

void setUp() {}
void tearDown() {}

namespace {

bool enter(LockScreen& lock) { return lock.handle(Input::pressed(Key::Select)); }

const Rect kContent = ScreenLayout::standard().content;

bool says(const FrameBuffer& fb, const std::string& words, FontSize size = FontSize::Text) {
  return shows::text(fb, words, size, kContent);
}

// A sentence the screen wraps over lines: every line of it is there, each centred on its own.
bool saysWrapped(const FrameBuffer& fb, const std::string& words) {
  const std::vector<std::string_view> lines = text::wrap(font(FontSize::Text), words, kContent.w - 24);
  for (std::string_view line : lines)
    if (!says(fb, std::string(line))) return false;
  return !lines.empty();
}

}  // namespace

// ---- unlocking --------------------------------------------------------------------------------

void test_two_enters_in_a_row_unlock_and_one_does_not() {
  DeviceRig rig;
  LockScreen lock(rig.context());
  TEST_ASSERT_TRUE(enter(lock));
  TEST_ASSERT_TRUE(lock.armed());
  TEST_ASSERT_EQUAL(0, rig.nav.calls.size());
  TEST_ASSERT_TRUE(enter(lock));
  TEST_ASSERT_EQUAL(1, rig.nav.calls.size());
  TEST_ASSERT_EQUAL_STRING("unlock", rig.nav.last().c_str());
  TEST_ASSERT_FALSE(lock.armed());
  enter(lock);                                               // a third one is the first of the next pair
  TEST_ASSERT_EQUAL(1, rig.nav.calls.size());
}

void test_any_other_key_between_the_two_starts_again() {
  const Input others[] = {Input::pressed(Key::Up),     Input::pressed(Key::Down),   Input::pressed(Key::Back),
                          Input::pressed(Key::Home),   Input::pressed(Key::Inbox),  Input::pressed(Key::Compose),
                          Input::text('a'),            Input::text(0x20AC)};
  for (const Input& other : others) {
    DeviceRig rig;
    LockScreen lock(rig.context());
    enter(lock);
    TEST_ASSERT_TRUE(lock.handle(other));                    // used up: nothing gets through to the device
    TEST_ASSERT_FALSE(lock.armed());
    enter(lock);                                             // this is a first Enter again
    TEST_ASSERT_EQUAL(0, rig.nav.calls.size());
    enter(lock);
    TEST_ASSERT_EQUAL_STRING("unlock", rig.nav.last().c_str());
  }
}

void test_taps_never_unlock_and_do_not_disturb_a_first_enter() {
  DeviceRig rig;
  LockScreen lock(rig.context());
  lock.render(rig.frame);
  for (int i = 0; i < 6; ++i) lock.handle(Input::touch(120, 150));
  lock.handle(Input::touch(10, 310));                        // where a soft key would be
  lock.handle(Input::touch(230, 310));
  TEST_ASSERT_EQUAL(0, rig.nav.calls.size());
  enter(lock);                                               // after many taps this is only the first Enter
  TEST_ASSERT_EQUAL(0, rig.nav.calls.size());
  lock.handle(Input::touch(120, 150));                       // a tap between the two does not break the pair
  enter(lock);
  TEST_ASSERT_EQUAL_STRING("unlock", rig.nav.last().c_str());
}

void test_there_are_no_soft_keys_because_a_tap_on_them_would_do_nothing() {
  DeviceRig rig;
  LockScreen lock(rig.context());
  rig.poll();
  lock.render(rig.frame);
  TEST_ASSERT_TRUE(lock.softKeys().back.empty());
  TEST_ASSERT_TRUE(lock.softKeys().action.empty());
  TEST_ASSERT_FALSE(lock.softKeys().scroll);
}

void test_entering_the_screen_starts_the_pair_again() {
  DeviceRig rig;
  LockScreen lock(rig.context());
  enter(lock);
  lock.onLeave();
  lock.onEnter();                                            // locked again: the first Enter of an earlier lock is forgotten
  TEST_ASSERT_FALSE(lock.armed());
  enter(lock);
  TEST_ASSERT_EQUAL(0, rig.nav.calls.size());
}

// ---- the front light ----------------------------------------------------------------------------

void test_locking_switches_the_front_light_off() {
  DeviceRig rig;
  LockScreen lock(rig.context());
  rig.core.toggleFrontlight();
  TEST_ASSERT_TRUE(rig.core.state().frontlight);
  lock.onEnter();
  TEST_ASSERT_FALSE(rig.core.state().frontlight);
  lock.onEnter();                                            // and a light that is off stays off
  TEST_ASSERT_FALSE(rig.core.state().frontlight);
}

// ---- what it says -----------------------------------------------------------------------------

void test_it_says_what_the_device_is_connected_to_and_how() {
  DeviceRig rig;
  LockScreen lock(rig.context());
  rig.poll();
  lock.render(rig.frame);
  TEST_ASSERT_TRUE(says(rig.frame, "Example", FontSize::Tile));
  TEST_ASSERT_TRUE(says(rig.frame, "Connected"));

  rig.modem.setNetwork("Example Mobile", 4, false);          // the network allows SMS only
  rig.poll();
  lock.render(rig.frame);
  TEST_ASSERT_TRUE(says(rig.frame, "Example", FontSize::Tile));
  TEST_ASSERT_TRUE(says(rig.frame, "SMS only"));
  TEST_ASSERT_FALSE(says(rig.frame, "Connected"));

  rig.modem.setNetwork("", 0);
  rig.poll();
  lock.render(rig.frame);
  TEST_ASSERT_TRUE(says(rig.frame, "No network", FontSize::Tile));
  TEST_ASSERT_TRUE(says(rig.frame, "Not connected"));
}

void test_it_says_it_in_german_and_french() {
  DeviceRig rig;
  LockScreen lock(rig.context());
  rig.poll();
  rig.language("de");
  lock.render(rig.frame);
  TEST_ASSERT_TRUE(says(rig.frame, "Verbunden"));
  TEST_ASSERT_TRUE(saysWrapped(rig.frame, "Zum Entsperren zweimal \xE2\x86\xB5 dr\xC3\xBC" "cken"));
  rig.language("fr");
  lock.render(rig.frame);
  TEST_ASSERT_TRUE(says(rig.frame, "Connect\xC3\xA9"));
}

void test_the_hint_names_the_key_and_changes_after_the_first_press() {
  DeviceRig rig;
  LockScreen lock(rig.context());
  rig.poll();
  lock.render(rig.frame);
  const std::string first = rig.frame.toAscii(kContent);
  TEST_ASSERT_TRUE(saysWrapped(rig.frame, "Press \xE2\x86\xB5 twice to unlock"));
  enter(lock);
  lock.render(rig.frame);
  TEST_ASSERT_TRUE(saysWrapped(rig.frame, "Press \xE2\x86\xB5 once more"));
  TEST_ASSERT_FALSE(saysWrapped(rig.frame, "Press \xE2\x86\xB5 twice to unlock"));
  TEST_ASSERT_FALSE(first == rig.frame.toAscii(kContent));
}

void test_it_says_how_many_are_unread_in_the_right_number() {
  DeviceRig rig;
  LockScreen lock(rig.context());
  rig.poll();
  lock.render(rig.frame);
  TEST_ASSERT_FALSE(says(rig.frame, "\xE2\x9C\x89 1 new message"));
  rig.modem.injectSms(kParentOne, "School is out", 1);
  rig.poll();
  lock.render(rig.frame);
  TEST_ASSERT_TRUE(says(rig.frame, "\xE2\x9C\x89 1 new message"));
  rig.modem.injectSms(kParentOne, "Are you coming?", 2);
  rig.poll();
  lock.render(rig.frame);
  TEST_ASSERT_TRUE(says(rig.frame, "\xE2\x9C\x89 2 new messages"));
  rig.language("fr");                                        // French counts 0 and 1 as one
  lock.render(rig.frame);
  TEST_ASSERT_TRUE(says(rig.frame, "\xE2\x9C\x89 2 nouveaux messages"));
}

void test_there_is_no_clock_so_a_minute_going_by_changes_nothing() {
  DeviceRig rig;
  LockScreen lock(rig.context());
  rig.run(10);                                               // the start-up fix is over and the bar is as it stays
  lock.render(rig.frame);
  const std::string before = rig.frame.toAscii();
  rig.clock.advanceSeconds(120);                             // 14:34
  rig.poll();
  FrameBuffer later;
  lock.render(later);
  TEST_ASSERT_TRUE(before == later.toAscii());
  TEST_ASSERT_FALSE(says(later, "14:34", FontSize::Large));
}

// ---- the stack around it --------------------------------------------------------------------------

void test_unlocking_goes_to_the_home_screen_and_the_sensor_keys_do_nothing_meanwhile() {
  DeviceRig rig;
  ScreenStack stack(screen::kHome);
  const ScreenContext context{rig.core, rig.translator, stack};
  HomeScreen home(context);
  LockScreen lock(context);
  stack.add(screen::kHome, home);
  stack.add(screen::kLock, lock);
  stack.setLockScreen(screen::kLock);
  int hardwareKeyActions = 0;
  for (Key key : {Key::Home, Key::Inbox, Key::Compose}) stack.setHardwareKey(key, [&](Navigation&) { ++hardwareKeyActions; });
  stack.begin(0);
  stack.lock();
  TEST_ASSERT_TRUE(stack.locked());
  TEST_ASSERT_EQUAL(screen::kLock, stack.visibleId());

  for (int i = 0; i < 3; ++i)
    for (Key key : {Key::Home, Key::Inbox, Key::Compose}) stack.handle(Input::pressed(key), 1);
  stack.handle(Input::touch(120, 150), 1);
  TEST_ASSERT_TRUE(stack.locked());
  TEST_ASSERT_EQUAL(0, hardwareKeyActions);

  stack.handle(Input::pressed(Key::Select), 2);
  TEST_ASSERT_TRUE(stack.locked());
  stack.handle(Input::pressed(Key::Select), 2);
  TEST_ASSERT_FALSE(stack.locked());
  TEST_ASSERT_EQUAL(screen::kHome, stack.visibleId());
}

void test_a_screen_that_was_left_by_the_lock_comes_back_pair_free() {
  DeviceRig rig;
  ScreenStack stack(screen::kHome);
  const ScreenContext context{rig.core, rig.translator, stack};
  HomeScreen home(context);
  LockScreen lock(context);
  stack.add(screen::kHome, home);
  stack.add(screen::kLock, lock);
  stack.setLockScreen(screen::kLock);
  stack.begin(0);
  stack.lock();
  stack.handle(Input::pressed(Key::Select), 1);              // a first Enter
  TEST_ASSERT_TRUE(lock.armed());
  stack.unlock();                                            // unlocked some other way (an incoming call, the host)
  stack.lock();
  TEST_ASSERT_FALSE(lock.armed());
}

// ---- what it costs the panel ----------------------------------------------------------------------

void test_an_hour_on_the_lock_screen_costs_no_refresh_after_the_first_picture() {
  DeviceRig rig;
  ScreenStack stack(screen::kHome);
  const ScreenContext context{rig.core, rig.translator, stack};
  HomeScreen home(context);
  LockScreen lock(context);
  stack.add(screen::kHome, home);
  stack.add(screen::kLock, lock);
  stack.setLockScreen(screen::kLock);
  stack.begin(0);
  stack.lock();
  RefreshRig panel;
  uint32_t ms = 0;
  rig.run(10);                                               // the start-up fix is over: the "GPS" on the bar is gone
  panel.frame(ms, [&](Display& d) { stack.render(d); });
  panel.mark();
  // A frame every 20 seconds for an hour, as the lock asks for; the core is polled meanwhile.
  for (ms = 20000; ms <= 3600 * 1000; ms += 20000) {
    rig.clock.advanceSeconds(20);
    rig.poll();
    panel.frame(ms, [&](Display& d) { stack.render(d); });
  }
  panel.expect("an hour on the lock screen", 0, 0);
}

void test_two_minutes_on_the_home_screen_cost_the_lock_and_nothing_else() {
  DeviceRig rig;
  ScreenStack stack(screen::kHome);
  const ScreenContext context{rig.core, rig.translator, stack};
  HomeScreen home(context);
  LockScreen lock(context);
  stack.add(screen::kHome, home);
  stack.add(screen::kLock, lock);
  stack.setLockScreen(screen::kLock);
  stack.setAutoLockSeconds(120);
  stack.begin(0);
  RefreshRig panel;
  rig.run(10);                                               // the start-up fix is over: the "GPS" on the bar is gone
  panel.frame(0, [&](Display& d) { stack.render(d); });
  panel.mark();
  for (uint32_t second = 1; second <= 130; ++second) {
    rig.clock.advanceSeconds(1);
    rig.poll();
    stack.poll(second);
    panel.frame(second * 1000, [&](Display& d) { stack.render(d); });
  }
  TEST_ASSERT_TRUE(stack.locked());
  panel.expect("two minutes on the home screen until it locks", 1, 0);   // the lock itself, and nothing before it
}

// ---- layout -----------------------------------------------------------------------------------

void test_the_lock_is_drawn_inside_the_screen_in_every_language() {
  languages::forEach([](const std::string& language, i18n::Translator&) {
    DeviceRig rig;
    rig.language(language);
    LockScreen lock(rig.context());
    rig.modem.injectSms(kParentOne, "Hello", 1);
    rig.poll();
    for (int state = 0; state < 3; ++state) {
      FrameBuffer fb;
      lock.render(fb);
      TEST_ASSERT_FALSE_MESSAGE(fb.clipped(), (language + ":\n" + fb.describeClips()).c_str());
      if (state == 0) rig.modem.setNetwork("", 0);
      if (state == 1) enter(lock);
      rig.poll();
    }
  });
}

void test_the_longest_operator_label_the_core_hands_over_fits_the_line() {
  DeviceRig rig;
  LockScreen lock(rig.context());
  rig.modem.setNetwork("Internationalmobile Communications of Example Country", 4);
  rig.poll();
  const std::string label = rig.core.state().network.name;
  TEST_ASSERT_EQUAL(10, label.size());                       // the core cuts it to ten characters
  lock.render(rig.frame);
  TEST_ASSERT_FALSE(rig.frame.clipped());
  TEST_ASSERT_TRUE(says(rig.frame, label, FontSize::Tile));
  TEST_ASSERT_TRUE(text::width(font(FontSize::Tile), "WWWWWWWWWW") < kContent.w - 8);   // even the widest ten letters
}

void test_the_lock_pictures_in_english_and_german() {
  for (const char* language : {"en", "de"}) {
    DeviceRig rig;
    rig.language(language);
    LockScreen lock(rig.context());
    rig.modem.injectSms(kParentOne, "Hello", 1);
    rig.modem.injectSms(kParentOne, "Hello again", 2);
    rig.poll();
    lock.render(rig.frame);
    golden::expect(rig.frame, std::string("lock_") + language);
    enter(lock);
    lock.render(rig.frame);
    golden::expect(rig.frame, std::string("lock_armed_") + language);
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_two_enters_in_a_row_unlock_and_one_does_not);
  RUN_TEST(test_any_other_key_between_the_two_starts_again);
  RUN_TEST(test_taps_never_unlock_and_do_not_disturb_a_first_enter);
  RUN_TEST(test_there_are_no_soft_keys_because_a_tap_on_them_would_do_nothing);
  RUN_TEST(test_entering_the_screen_starts_the_pair_again);
  RUN_TEST(test_locking_switches_the_front_light_off);
  RUN_TEST(test_it_says_what_the_device_is_connected_to_and_how);
  RUN_TEST(test_it_says_it_in_german_and_french);
  RUN_TEST(test_the_hint_names_the_key_and_changes_after_the_first_press);
  RUN_TEST(test_it_says_how_many_are_unread_in_the_right_number);
  RUN_TEST(test_there_is_no_clock_so_a_minute_going_by_changes_nothing);
  RUN_TEST(test_unlocking_goes_to_the_home_screen_and_the_sensor_keys_do_nothing_meanwhile);
  RUN_TEST(test_a_screen_that_was_left_by_the_lock_comes_back_pair_free);
  RUN_TEST(test_an_hour_on_the_lock_screen_costs_no_refresh_after_the_first_picture);
  RUN_TEST(test_two_minutes_on_the_home_screen_cost_the_lock_and_nothing_else);
  RUN_TEST(test_the_lock_is_drawn_inside_the_screen_in_every_language);
  RUN_TEST(test_the_longest_operator_label_the_core_hands_over_fits_the_line);
  RUN_TEST(test_the_lock_pictures_in_english_and_german);
  return UNITY_END();
}

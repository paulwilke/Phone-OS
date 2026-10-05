// Host tests for the screen stack: navigation, and the matrix of every way a screen can
// become visible or stop being visible, with the onEnter/onLeave calls each one must cause.

#include <unity.h>

#include <random>
#include <string>
#include <vector>

#include <ui/ScreenStack.h>

using namespace ui;

void setUp() {}
void tearDown() {}

namespace {

enum Id : ScreenId { Home = 1, Messages, Compose, Recipients, Settings, Call, Lock };

const char* name(ScreenId id) {
  switch (id) {
    case Home: return "home";
    case Messages: return "messages";
    case Compose: return "compose";
    case Recipients: return "recipients";
    case Settings: return "settings";
    case Call: return "call";
    case Lock: return "lock";
  }
  return "?";
}

// One log shared by all test screens, so that the order of calls across screens is visible.
struct Log {
  std::vector<std::string> events;
  std::string text() const {
    std::string out;
    for (size_t i = 0; i < events.size(); ++i) out += (i ? " " : "") + events[i];
    return out;
  }
  void clear() { events.clear(); }
};

class TestScreen : public Screen {
public:
  TestScreen(ScreenId id, Log& log) : id_(id), log_(log) {}
  void onEnter() override {
    log_.events.push_back(std::string("+") + name(id_));
    ++entered;
    TEST_ASSERT_FALSE_MESSAGE(visible, "onEnter while already visible");
    visible = true;
    if (onEnterHook) onEnterHook();
  }
  void onLeave() override {
    log_.events.push_back(std::string("-") + name(id_));
    TEST_ASSERT_TRUE_MESSAGE(visible, "onLeave while not visible");
    visible = false;
    if (onLeaveHook) onLeaveHook();
  }
  bool handle(const Input& input) override {
    handled.push_back(input);
    return consumes;
  }
  void render(Display&) override { ++rendered; }
  void onTick() override {
    ++ticked;
    if (onTickHook) onTickHook();
  }
  bool wantsHardwareKeys() const override { return rawKeys; }
  bool preventsAutoLock() const override { return keepAwake; }

  bool visible = false;
  bool consumes = true;
  bool rawKeys = false;
  bool keepAwake = false;
  int entered = 0, rendered = 0, ticked = 0;
  std::vector<Input> handled;
  std::function<void()> onEnterHook, onLeaveHook, onTickHook;

private:
  ScreenId id_;
  Log& log_;
};

struct Rig {
  Log log;
  TestScreen home{Home, log}, messages{Messages, log}, compose{Compose, log}, recipients{Recipients, log},
      settings{Settings, log}, call{Call, log}, lock{Lock, log};
  ScreenStack stack{Home};

  Rig() {
    stack.add(Home, home);
    stack.add(Messages, messages);
    stack.add(Compose, compose);
    stack.add(Recipients, recipients);
    stack.add(Settings, settings);
    stack.add(Call, call);
    stack.add(Lock, lock);
    stack.setLockScreen(Lock);
  }
  void start() {
    stack.begin(0);
    log.clear();
  }
  std::string path() const {
    std::string out;
    for (size_t i = 0; i < stack.depth(); ++i) out += (i ? ">" : "") + std::string(name(stack.pathAt(i)));
    return out;
  }
};

#define ASSERT_LOG(rig, expected) TEST_ASSERT_EQUAL_STRING(expected, (rig).log.text().c_str())

}  // namespace

// ---- starting -----------------------------------------------------------------------------

void test_begin_enters_the_root_screen() {
  Rig r;
  r.stack.begin(0);
  ASSERT_LOG(r, "+home");
  TEST_ASSERT_EQUAL(Home, r.stack.visibleId());
  TEST_ASSERT_FALSE(r.stack.locked());
}

void test_begin_again_leaves_the_visible_screen_first() {
  Rig r;
  r.start();
  r.stack.open(Messages);
  r.log.clear();
  r.stack.begin(10);
  ASSERT_LOG(r, "-messages +home");
  TEST_ASSERT_EQUAL_STRING("home", r.path().c_str());
}

void test_nothing_happens_before_begin() {
  Rig r;
  r.stack.open(Messages);
  TEST_ASSERT_FALSE(r.stack.handle(Input::pressed(Key::Select), 0));
  ASSERT_LOG(r, "");
}

// ---- the matrix: every way the visible screen changes ---------------------------------------

void test_open_a_new_screen() {
  Rig r;
  r.start();
  r.stack.open(Messages);
  ASSERT_LOG(r, "-home +messages");
  TEST_ASSERT_EQUAL_STRING("home>messages", r.path().c_str());
}

void test_open_the_visible_screen_again_changes_nothing() {
  Rig r;
  r.start();
  r.stack.open(Messages);
  r.log.clear();
  r.stack.open(Messages);
  ASSERT_LOG(r, "");
  TEST_ASSERT_EQUAL(2, r.stack.depth());
}

void test_open_a_screen_deeper_in_the_path_goes_back_to_it() {
  Rig r;
  r.start();
  r.stack.open(Messages);
  r.stack.open(Compose);
  r.stack.open(Recipients);
  r.log.clear();
  r.stack.open(Messages);
  ASSERT_LOG(r, "-recipients +messages");  // only the visible one leaves; compose was not visible
  TEST_ASSERT_EQUAL_STRING("home>messages", r.path().c_str());
}

void test_open_the_root_goes_back_to_it() {
  Rig r;
  r.start();
  r.stack.open(Messages);
  r.stack.open(Compose);
  r.log.clear();
  r.stack.open(Home);
  ASSERT_LOG(r, "-compose +home");
  TEST_ASSERT_EQUAL_STRING("home", r.path().c_str());
}

void test_open_with_a_full_path_replaces_the_newest_screen() {
  Rig r;
  r.start();
  r.stack.open(Messages);
  r.stack.open(Compose);
  r.stack.open(Recipients);
  TEST_ASSERT_EQUAL(ScreenStack::kMaxDepth, r.stack.depth());
  r.log.clear();
  r.stack.open(Settings);
  ASSERT_LOG(r, "-recipients +settings");
  TEST_ASSERT_EQUAL_STRING("home>messages>compose>settings", r.path().c_str());
}

void test_back_returns_to_the_screen_below() {
  Rig r;
  r.start();
  r.stack.open(Messages);
  r.log.clear();
  r.stack.back();
  ASSERT_LOG(r, "-messages +home");
}

void test_back_at_the_root_does_nothing() {
  Rig r;
  r.start();
  r.stack.back();
  ASSERT_LOG(r, "");
  TEST_ASSERT_EQUAL(1, r.stack.depth());
}

void test_home_from_deep_in_the_path_leaves_only_the_visible_screen() {
  Rig r;
  r.start();
  r.stack.open(Messages);
  r.stack.open(Compose);
  r.log.clear();
  r.stack.home();
  ASSERT_LOG(r, "-compose +home");
  TEST_ASSERT_EQUAL(1, r.stack.depth());
}

void test_home_at_the_root_does_nothing() {
  Rig r;
  r.start();
  r.stack.home();
  ASSERT_LOG(r, "");
}

void test_lock_leaves_the_visible_screen_and_enters_the_lock_screen() {
  Rig r;
  r.start();
  r.stack.open(Messages);
  r.log.clear();
  r.stack.lock();
  ASSERT_LOG(r, "-messages +lock");
  TEST_ASSERT_TRUE(r.stack.locked());
  TEST_ASSERT_EQUAL(Lock, r.stack.visibleId());
  r.log.clear();
  r.stack.lock();  // already locked
  ASSERT_LOG(r, "");
}

void test_unlock_goes_to_the_root_screen() {
  Rig r;
  r.start();
  r.stack.open(Messages);
  r.stack.lock();
  r.log.clear();
  r.stack.unlock();
  ASSERT_LOG(r, "-lock +home");
  TEST_ASSERT_EQUAL(1, r.stack.depth());
  r.log.clear();
  r.stack.unlock();  // not locked
  ASSERT_LOG(r, "");
}

void test_unlock_enters_the_root_screen_even_if_it_was_the_one_locked_away() {
  Rig r;
  r.start();
  r.stack.lock();
  r.log.clear();
  r.stack.unlock();
  ASSERT_LOG(r, "-lock +home");  // home left when locking, so it enters again
}

void test_interrupt_from_a_screen_keeps_the_path_under_it() {
  Rig r;
  r.start();
  r.stack.open(Messages);
  r.log.clear();
  r.stack.interrupt(Call, 5);
  ASSERT_LOG(r, "-messages +call");
  TEST_ASSERT_EQUAL_STRING("home>messages>call", r.path().c_str());
}

void test_interrupt_from_the_lock_screen_unlocks_and_leaves_the_lock_screen() {
  Rig r;
  r.start();
  r.stack.open(Messages);
  r.stack.lock();
  r.log.clear();
  r.stack.interrupt(Call, 5);
  ASSERT_LOG(r, "-lock +call");
  TEST_ASSERT_FALSE(r.stack.locked());
  TEST_ASSERT_EQUAL_STRING("home>messages>call", r.path().c_str());
}

void test_end_of_an_interrupt_goes_back_only_while_it_is_still_visible() {
  Rig r;
  r.start();
  r.stack.interrupt(Call, 1);
  r.log.clear();
  r.stack.endInterrupt(Call);
  ASSERT_LOG(r, "-call +home");

  // The user already went home: ending the call has nothing to undo.
  r.stack.interrupt(Call, 2);
  r.stack.home();
  r.log.clear();
  r.stack.endInterrupt(Call);
  ASSERT_LOG(r, "");

  // The device locked meanwhile: the lock screen stays.
  r.stack.interrupt(Call, 3);
  r.stack.lock();
  r.log.clear();
  r.stack.endInterrupt(Call);
  ASSERT_LOG(r, "");
  TEST_ASSERT_TRUE(r.stack.locked());
}

void test_navigation_is_ignored_while_locked_except_interrupt_and_unlock() {
  Rig r;
  r.start();
  r.stack.lock();
  r.log.clear();
  r.stack.open(Messages);
  r.stack.back();
  r.stack.home();
  ASSERT_LOG(r, "");
  TEST_ASSERT_EQUAL(Lock, r.stack.visibleId());
  TEST_ASSERT_EQUAL_STRING("home", r.path().c_str());   // and the path under the lock screen did not change either
}

void test_an_unknown_screen_is_not_opened() {
  Rig r;
  r.start();
  r.stack.open(99);
  r.stack.interrupt(99, 0);
  ASSERT_LOG(r, "");
  TEST_ASSERT_EQUAL(1, r.stack.depth());
}

void test_lock_without_a_lock_screen_does_nothing() {
  Log log;
  TestScreen home(Home, log);
  ScreenStack stack(Home);
  stack.add(Home, home);
  stack.begin(0);
  stack.lock();
  TEST_ASSERT_FALSE(stack.locked());
}

void test_the_open_filter_refuses_screens_but_not_the_root() {
  Rig r;
  r.start();
  r.stack.setOpenFilter([](ScreenId id) { return id != Compose; });
  r.stack.open(Compose);
  ASSERT_LOG(r, "");
  r.stack.open(Messages);
  r.stack.open(Home);  // the root is always allowed
  ASSERT_LOG(r, "-home +messages -messages +home");
  r.log.clear();
  r.stack.interrupt(Compose, 0);  // interrupts are not subject to the filter
  ASSERT_LOG(r, "-home +compose");
}

// ---- invariants over arbitrary sequences ------------------------------------------------------

void test_exactly_the_visible_screen_is_entered_after_any_sequence_of_operations() {
  std::mt19937 random(12345);
  for (int run = 0; run < 200; ++run) {
    Rig r;
    r.stack.setAutoLockSeconds(7);
    r.stack.begin(0);
    uint32_t now = 0;
    TestScreen* all[] = {&r.home, &r.messages, &r.compose, &r.recipients, &r.settings, &r.call, &r.lock};
    for (int step = 0; step < 60; ++step) {
      now += random() % 5;
      const ScreenId id = (ScreenId)(Home + random() % 6);
      switch (random() % 9) {
        case 0: r.stack.open(id); break;
        case 1: r.stack.back(); break;
        case 2: r.stack.home(); break;
        case 3: r.stack.lock(); break;
        case 4: r.stack.unlock(); break;
        case 5: r.stack.interrupt(id, now); break;
        case 6: r.stack.endInterrupt(id); break;
        case 7: r.stack.poll(now); break;
        case 8: r.stack.handle(Input::pressed(random() % 2 ? Key::Home : Key::Select), now); break;
      }
      int visibleCount = 0;
      for (TestScreen* s : all) visibleCount += s->visible ? 1 : 0;
      TEST_ASSERT_EQUAL_MESSAGE(1, visibleCount, "not exactly one screen entered");
      TestScreen* shown = all[r.stack.visibleId() - Home];
      TEST_ASSERT_TRUE_MESSAGE(shown->visible, "the visible screen is not the entered one");
      TEST_ASSERT_TRUE(r.stack.depth() >= 1 && r.stack.depth() <= ScreenStack::kMaxDepth);
    }
  }
}

// ---- input ---------------------------------------------------------------------------------------

void test_input_goes_to_the_visible_screen_only() {
  Rig r;
  r.start();
  r.stack.open(Messages);
  r.stack.handle(Input::pressed(Key::Down), 1);
  TEST_ASSERT_EQUAL(0, r.home.handled.size());
  TEST_ASSERT_EQUAL(1, r.messages.handled.size());
  r.stack.lock();
  r.stack.handle(Input::touch(3, 4), 2);
  TEST_ASSERT_EQUAL(1, r.messages.handled.size());
  TEST_ASSERT_EQUAL(1, r.lock.handled.size());
  TEST_ASSERT_EQUAL(3, r.lock.handled[0].x);
}

void test_handle_returns_whether_the_screen_used_the_input() {
  Rig r;
  r.start();
  r.home.consumes = false;
  TEST_ASSERT_FALSE(r.stack.handle(Input::pressed(Key::Up), 0));
  r.home.consumes = true;
  TEST_ASSERT_TRUE(r.stack.handle(Input::pressed(Key::Up), 0));
}

void test_a_hardware_key_runs_its_action_instead_of_reaching_the_screen() {
  Rig r;
  r.start();
  r.stack.setHardwareKey(Key::Inbox, [](Navigation& nav) { nav.open(Messages); });
  r.stack.setHardwareKey(Key::Home, [](Navigation& nav) { nav.home(); });
  TEST_ASSERT_TRUE(r.stack.handle(Input::pressed(Key::Inbox), 0));
  ASSERT_LOG(r, "-home +messages");
  TEST_ASSERT_EQUAL(0, r.messages.handled.size());
  r.log.clear();
  TEST_ASSERT_TRUE(r.stack.handle(Input::pressed(Key::Home), 0));
  ASSERT_LOG(r, "-messages +home");
}

void test_a_hardware_key_without_an_action_reaches_the_screen() {
  Rig r;
  r.start();
  r.stack.setHardwareKey(Key::Home, [](Navigation& nav) { nav.home(); });
  r.stack.handle(Input::pressed(Key::Compose), 0);
  TEST_ASSERT_EQUAL(1, r.home.handled.size());
}

void test_a_screen_that_wants_the_hardware_keys_gets_them() {
  Rig r;
  r.start();
  r.stack.setHardwareKey(Key::Inbox, [](Navigation& nav) { nav.open(Messages); });
  r.home.rawKeys = true;
  r.stack.handle(Input::pressed(Key::Inbox), 0);
  TEST_ASSERT_EQUAL(1, r.home.handled.size());
  TEST_ASSERT_EQUAL(Home, r.stack.visibleId());
}

void test_the_hardware_keys_do_nothing_while_locked() {
  Rig r;
  r.start();
  r.stack.setHardwareKey(Key::Inbox, [](Navigation& nav) { nav.open(Messages); });
  r.stack.lock();
  r.log.clear();
  r.stack.handle(Input::pressed(Key::Inbox), 0);
  ASSERT_LOG(r, "");
  TEST_ASSERT_EQUAL(1, r.lock.handled.size());  // the lock screen sees it and may ignore it
  TEST_ASSERT_EQUAL(Lock, r.stack.visibleId());
}

void test_an_action_with_several_steps_is_a_transition_per_step() {
  Rig r;
  r.start();
  r.stack.open(Messages);
  r.log.clear();
  r.stack.setHardwareKey(Key::Compose, [](Navigation& nav) {
    nav.home();
    nav.open(Compose);
  });
  r.stack.handle(Input::pressed(Key::Compose), 0);
  ASSERT_LOG(r, "-messages +home -home +compose");
  TEST_ASSERT_EQUAL_STRING("home>compose", r.path().c_str());
}

void test_render_draws_the_visible_screen_only() {
  Rig r;
  r.start();
  r.stack.open(Messages);
  struct Nothing : Display {
    int width() const override { return 1; }
    int height() const override { return 1; }
    void beginFrame() override {}
    void endFrame() override {}
    void setPixel(int, int, bool) override {}
    void fillRect(const Rect&, bool) override {}
    void outlineRect(const Rect&, bool) override {}
    void drawBitmap(int, int, const uint8_t*, int, int, bool) override {}
  } display;
  r.stack.render(display);
  TEST_ASSERT_EQUAL(0, r.home.rendered);
  TEST_ASSERT_EQUAL(1, r.messages.rendered);
  r.stack.lock();
  r.stack.render(display);
  TEST_ASSERT_EQUAL(1, r.lock.rendered);
}

// ---- re-entrancy -----------------------------------------------------------------------------------

void test_navigating_from_inside_onenter_is_ignored_and_the_path_stays_consistent() {
  Rig r;
  r.start();
  r.messages.onEnterHook = [&] { r.stack.open(Settings); };
  r.stack.open(Messages);
  ASSERT_LOG(r, "-home +messages");
  TEST_ASSERT_EQUAL_STRING("home>messages", r.path().c_str());
  TEST_ASSERT_TRUE(r.messages.visible);
}

void test_navigating_from_inside_onleave_is_ignored() {
  Rig r;
  r.start();
  r.home.onLeaveHook = [&] { r.stack.lock(); };
  r.stack.open(Messages);
  ASSERT_LOG(r, "-home +messages");
  TEST_ASSERT_FALSE(r.stack.locked());
}

// ---- auto-lock ----------------------------------------------------------------------------------------

void test_the_device_locks_after_the_idle_time() {
  Rig r;
  r.stack.setAutoLockSeconds(120);
  r.stack.begin(1000);
  r.log.clear();
  r.stack.poll(1119);
  TEST_ASSERT_FALSE(r.stack.locked());
  r.stack.poll(1120);
  TEST_ASSERT_TRUE(r.stack.locked());
  ASSERT_LOG(r, "-home +lock");
}

void test_input_restarts_the_idle_time() {
  Rig r;
  r.stack.setAutoLockSeconds(120);
  r.stack.begin(0);
  r.stack.handle(Input::pressed(Key::Down), 100);
  r.stack.poll(219);
  TEST_ASSERT_FALSE(r.stack.locked());
  r.stack.poll(220);
  TEST_ASSERT_TRUE(r.stack.locked());
}

void test_poll_ticks_the_visible_screen_only() {
  Rig r;
  r.start();
  r.stack.poll(1);
  r.stack.poll(2);
  TEST_ASSERT_EQUAL(2, r.home.ticked);
  r.stack.open(Messages);
  r.stack.poll(3);
  TEST_ASSERT_EQUAL(2, r.home.ticked);                               // not while something else is shown
  TEST_ASSERT_EQUAL(1, r.messages.ticked);
}

void test_poll_ticks_the_lock_screen_while_locked_and_nothing_before_begin() {
  Rig r;
  r.stack.poll(1);
  TEST_ASSERT_EQUAL(0, r.home.ticked);
  r.start();
  r.stack.lock();
  r.stack.poll(2);
  TEST_ASSERT_EQUAL(1, r.lock.ticked);
  TEST_ASSERT_EQUAL(0, r.home.ticked);
}

void test_the_idle_check_after_a_tick_asks_the_screen_that_is_visible_now() {
  Rig r;
  r.stack.setAutoLockSeconds(10);
  r.stack.begin(0);
  r.stack.open(Messages);
  r.home.keepAwake = true;                                           // the screen the tick goes back to
  r.log.clear();
  r.messages.onTickHook = [&] { r.stack.back(); };
  r.stack.poll(100);                                                 // idle for long
  TEST_ASSERT_FALSE(r.stack.locked());
  ASSERT_LOG(r, "-messages +home");
}

void test_a_screen_can_keep_the_device_awake() {
  Rig r;
  r.stack.setAutoLockSeconds(10);
  r.stack.begin(0);
  r.stack.interrupt(Call, 0);
  r.call.keepAwake = true;
  r.stack.poll(1000);
  TEST_ASSERT_FALSE(r.stack.locked());
  r.call.keepAwake = false;
  r.stack.poll(1000);
  TEST_ASSERT_TRUE(r.stack.locked());
}

void test_auto_lock_off_and_the_clock_wrapping_around() {
  Rig r;
  r.stack.begin(0);
  r.stack.poll(1000000);
  TEST_ASSERT_FALSE(r.stack.locked());  // 0 = off
  Rig w;
  w.stack.setAutoLockSeconds(100);
  w.stack.begin(0xFFFFFFF0u);
  w.stack.poll(50);  // 66 seconds later, across the wrap
  TEST_ASSERT_FALSE(w.stack.locked());
  w.stack.poll(90);  // 106 seconds later
  TEST_ASSERT_TRUE(w.stack.locked());
}

void test_a_locked_device_does_not_lock_again() {
  Rig r;
  r.stack.setAutoLockSeconds(5);
  r.stack.begin(0);
  r.stack.poll(10);
  r.log.clear();
  r.stack.poll(20);
  ASSERT_LOG(r, "");
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_begin_enters_the_root_screen);
  RUN_TEST(test_begin_again_leaves_the_visible_screen_first);
  RUN_TEST(test_nothing_happens_before_begin);
  RUN_TEST(test_open_a_new_screen);
  RUN_TEST(test_open_the_visible_screen_again_changes_nothing);
  RUN_TEST(test_open_a_screen_deeper_in_the_path_goes_back_to_it);
  RUN_TEST(test_open_the_root_goes_back_to_it);
  RUN_TEST(test_open_with_a_full_path_replaces_the_newest_screen);
  RUN_TEST(test_back_returns_to_the_screen_below);
  RUN_TEST(test_back_at_the_root_does_nothing);
  RUN_TEST(test_home_from_deep_in_the_path_leaves_only_the_visible_screen);
  RUN_TEST(test_home_at_the_root_does_nothing);
  RUN_TEST(test_lock_leaves_the_visible_screen_and_enters_the_lock_screen);
  RUN_TEST(test_unlock_goes_to_the_root_screen);
  RUN_TEST(test_unlock_enters_the_root_screen_even_if_it_was_the_one_locked_away);
  RUN_TEST(test_interrupt_from_a_screen_keeps_the_path_under_it);
  RUN_TEST(test_interrupt_from_the_lock_screen_unlocks_and_leaves_the_lock_screen);
  RUN_TEST(test_end_of_an_interrupt_goes_back_only_while_it_is_still_visible);
  RUN_TEST(test_navigation_is_ignored_while_locked_except_interrupt_and_unlock);
  RUN_TEST(test_an_unknown_screen_is_not_opened);
  RUN_TEST(test_lock_without_a_lock_screen_does_nothing);
  RUN_TEST(test_the_open_filter_refuses_screens_but_not_the_root);
  RUN_TEST(test_exactly_the_visible_screen_is_entered_after_any_sequence_of_operations);
  RUN_TEST(test_input_goes_to_the_visible_screen_only);
  RUN_TEST(test_handle_returns_whether_the_screen_used_the_input);
  RUN_TEST(test_a_hardware_key_runs_its_action_instead_of_reaching_the_screen);
  RUN_TEST(test_a_hardware_key_without_an_action_reaches_the_screen);
  RUN_TEST(test_a_screen_that_wants_the_hardware_keys_gets_them);
  RUN_TEST(test_the_hardware_keys_do_nothing_while_locked);
  RUN_TEST(test_an_action_with_several_steps_is_a_transition_per_step);
  RUN_TEST(test_render_draws_the_visible_screen_only);
  RUN_TEST(test_navigating_from_inside_onenter_is_ignored_and_the_path_stays_consistent);
  RUN_TEST(test_navigating_from_inside_onleave_is_ignored);
  RUN_TEST(test_the_device_locks_after_the_idle_time);
  RUN_TEST(test_input_restarts_the_idle_time);
  RUN_TEST(test_poll_ticks_the_visible_screen_only);
  RUN_TEST(test_poll_ticks_the_lock_screen_while_locked_and_nothing_before_begin);
  RUN_TEST(test_the_idle_check_after_a_tick_asks_the_screen_that_is_visible_now);
  RUN_TEST(test_a_screen_can_keep_the_device_awake);
  RUN_TEST(test_auto_lock_off_and_the_clock_wrapping_around);
  RUN_TEST(test_a_locked_device_does_not_lock_again);
  return UNITY_END();
}

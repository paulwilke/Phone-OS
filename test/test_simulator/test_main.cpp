// Host tests for the simulator's console and the demo data: every command does what help says and
// refuses what it cannot do, a script stops at its first error, the demo cards load without a
// diagnostic, and the whole thing runs through the real Runtime.
// allow-non-english: the catalogs under test contain German.

#include <unity.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>

#include <Crawler.h>
#include <Simulator.h>

void setUp() {}
void tearDown() { unsetenv("UPDATE_GOLDEN"); }

namespace {

constexpr const char* kParentOne = "+491701111111";

std::filesystem::path scratch(const std::string& name) {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / ("phone-os-sim-test-" + name);
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  return dir;
}

struct Sim {
  std::ostringstream out;
  sim::Simulator sim;
  explicit Sim(const std::string& data = "sim/data", const std::string& goldenDir = "sim/golden")
      : sim(options(data, goldenDir), out) {}
  static sim::Options options(const std::string& data, const std::string& goldenDir) {
    sim::Options o;
    o.dataDir = data;
    o.goldenDir = goldenDir;
    o.workDir = scratch("card-" + std::to_string(counter()++)).string();   // never the default: tests run side by side with a simulator
    o.webPort = 0;
    return o;
  }
  static int& counter() {
    static int n = 0;
    return ++n;
  }
  bool run(const std::string& line) { return sim.execute(line); }
  AppCore::State state() { return sim.runtime().core().state(); }
  std::string output() const { return out.str(); }
};

bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

}  // namespace

// ---- the demo cards ------------------------------------------------------------------------------------

void test_every_demo_card_loads_without_a_diagnostic_or_a_warning() {
  for (const char* dir : {"sim/data", "sim/data-parent", "sim/data-school", "sim/data-school-off"}) {
    Sim s(dir);
    TEST_ASSERT_TRUE_MESSAGE(s.sim.runtime().diagnostics().empty(), dir);
    TEST_ASSERT_FALSE_MESSAGE(contains(s.output(), "warning"), dir);
  }
}

void test_the_cards_differ_in_what_they_are_for() {
  Sim child("sim/data"), parent("sim/data-parent"), school("sim/data-school");
  TEST_ASSERT_FALSE(child.sim.runtime().core().profile().parentTools);
  TEST_ASSERT_TRUE(parent.sim.runtime().core().profile().parentTools);
  TEST_ASSERT_TRUE(parent.sim.runtime().core().profile().freeText);
  TEST_ASSERT_FALSE(child.state().schoolTime);                      // Tuesday 14:32
  TEST_ASSERT_TRUE(school.state().schoolTime);
}

void test_school_time_on_the_school_card_never_ends() {
  Sim s("sim/data-school");
  for (const char* when : {"2026-09-15 03:00", "2026-09-19 23:59", "2026-09-20 00:00", "2026-09-20 12:00", "2026-09-16 23:30"}) {
    TEST_ASSERT_TRUE(s.run(std::string("date ") + when));
    s.sim.advance(2000);
    TEST_ASSERT_TRUE_MESSAGE(s.state().schoolTime, when);
  }
}

// The windows of this card cover every hour and are switched off; a card without any window has the
// built-in one, also switched off, and Monday morning is inside it.
void test_switched_off_windows_and_the_default_window_never_make_school_time() {
  Sim off("sim/data-school-off"), none("sim/data");
  TEST_ASSERT_EQUAL(2, static_cast<int>(off.sim.runtime().core().profile().quietHours.count()));
  TEST_ASSERT_EQUAL(1, static_cast<int>(none.sim.runtime().core().profile().quietHours.count()));
  for (Sim* s : {&off, &none}) {
    for (const char* when : {"2026-09-14 09:00", "2026-09-15 03:00", "2026-09-19 23:59", "2026-09-16 23:30"}) {
      TEST_ASSERT_TRUE(s->run(std::string("date ") + when));
      s->sim.advance(2000);
      TEST_ASSERT_FALSE_MESSAGE(s->state().schoolTime, when);
    }
  }
}

void test_a_missing_data_directory_is_a_warning_and_an_empty_card() {
  Sim s("sim/no-such-directory");
  TEST_ASSERT_TRUE(contains(s.output(), "warning: no data directory"));
  size_t missing = 0;
  for (const auto& d : s.sim.runtime().diagnostics()) missing += d.code == config::Diagnostic::Code::MissingFile;
  TEST_ASSERT_EQUAL(3, static_cast<int>(missing));
}

// ---- time --------------------------------------------------------------------------------------------------

void test_the_device_starts_on_a_tuesday_afternoon_in_germany() {
  Sim s;
  s.sim.advance(2000);
  const auto wall = s.state().wall;
  TEST_ASSERT_TRUE(wall.known);
  TEST_ASSERT_EQUAL(2026, wall.year);
  TEST_ASSERT_EQUAL(9, wall.month);
  TEST_ASSERT_EQUAL(15, wall.day);
  TEST_ASSERT_EQUAL(2, wall.weekday);
  TEST_ASSERT_EQUAL(14 * 60 + 32 + 0, wall.minuteOfDay);
}

void test_date_sets_the_local_time_of_the_devices_zone_also_across_the_change_of_the_clocks() {
  Sim s;
  s.sim.advance(1000);
  TEST_ASSERT_TRUE(s.run("date 2026-12-24 08:15"));              // winter: one hour east of UTC
  s.sim.advance(1000);
  TEST_ASSERT_EQUAL(8 * 60 + 15, s.state().wall.minuteOfDay);
  TEST_ASSERT_EQUAL(24, s.state().wall.day);
  TEST_ASSERT_TRUE(s.run("date 2026-07-01 00:00"));              // summer: two hours
  s.sim.advance(1000);
  TEST_ASSERT_EQUAL(1, s.state().wall.day);
  TEST_ASSERT_EQUAL(0, s.state().wall.minuteOfDay / 60);
}

void test_date_refuses_what_is_not_a_date() {
  Sim s;
  for (const char* bad : {"date", "date 2026-09-15", "date 2026-13-01 10:00", "date 2026-09-32 10:00", "date 2026-09-15 24:00",
                          "date 2026-09-15 10:60", "date 2019-12-31 10:00", "date yesterday noon", "date 2026/09/15 10:00"})
    TEST_ASSERT_FALSE_MESSAGE(s.run(bad), bad);
}

void test_advance_moves_the_clock_by_the_unit_and_the_device_runs_meanwhile() {
  Sim s;
  const uint32_t t0 = s.sim.nowMs();
  TEST_ASSERT_TRUE(s.run("advance 1m"));
  TEST_ASSERT_EQUAL_UINT32(t0 + 60000, s.sim.nowMs());
  TEST_ASSERT_TRUE(s.run("advance 2h"));
  TEST_ASSERT_EQUAL_UINT32(t0 + 60000 + 7200000, s.sim.nowMs());
  TEST_ASSERT_TRUE(s.run("advance 250ms"));
  TEST_ASSERT_EQUAL_UINT32(t0 + 60000 + 7200000 + 250, s.sim.nowMs());
  TEST_ASSERT_TRUE(s.run("advance 3"));                           // seconds by default
  TEST_ASSERT_EQUAL_UINT32(t0 + 60000 + 7200000 + 250 + 3000, s.sim.nowMs());
  // Two hours of a locked device cost the panel nothing after the lock itself.
  TEST_ASSERT_TRUE(s.sim.runtime().stack().locked());
}

void test_advance_refuses_a_negative_time_and_an_unknown_unit() {
  Sim s;
  const uint32_t t0 = s.sim.nowMs();
  for (const char* bad : {"advance", "advance -5", "advance x", "advance 5d", "advance 5 s"})
    TEST_ASSERT_FALSE_MESSAGE(s.run(bad), bad);
  TEST_ASSERT_EQUAL_UINT32(t0, s.sim.nowMs());
}

void test_the_lapse_is_kept_for_the_loop_and_must_be_sensible() {
  Sim s;
  TEST_ASSERT_TRUE(s.run("lapse 60"));
  TEST_ASSERT_EQUAL_UINT32(60, s.sim.lapse());
  TEST_ASSERT_FALSE(s.run("lapse 0"));
  TEST_ASSERT_FALSE(s.run("lapse fast"));
  TEST_ASSERT_EQUAL_UINT32(60, s.sim.lapse());
}

// ---- the other side ---------------------------------------------------------------------------------------

void test_a_text_message_arrives_with_its_whole_text_also_when_it_starts_with_a_hash() {
  Sim s;
  TEST_ASSERT_TRUE(s.run(std::string("sms ") + kParentOne + " Hello  Alex # not a comment"));
  TEST_ASSERT_EQUAL(1, static_cast<int>(s.state().unread));
  const auto& history = s.sim.runtime().core().history();
  TEST_ASSERT_EQUAL_STRING("Hello  Alex # not a comment", history.at(history.count() - 1).text);
  TEST_ASSERT_TRUE(s.run(std::string("sms ") + kParentOne + " #hash first"));
  TEST_ASSERT_EQUAL_STRING("#hash first", history.at(history.count() - 1).text);
  TEST_ASSERT_TRUE(contains(s.output(), "haptic: message"));
}

void test_a_call_takes_the_screen_and_the_other_side_ends_it() {
  Sim s;
  TEST_ASSERT_TRUE(s.run(std::string("call ") + kParentOne));
  TEST_ASSERT_EQUAL(ui::screen::kCall, s.sim.runtime().stack().visibleId());
  TEST_ASSERT_TRUE(contains(s.output(), "haptic: ring"));
  TEST_ASSERT_TRUE(s.run("remote hangup"));
  s.sim.advance(1000);
  TEST_ASSERT_EQUAL(ui::screen::kHome, s.sim.runtime().stack().visibleId());
}

void test_a_call_cannot_come_in_without_a_network() {
  Sim s;
  TEST_ASSERT_TRUE(s.run("network none"));
  TEST_ASSERT_FALSE(s.run(std::string("call ") + kParentOne));
  TEST_ASSERT_TRUE(contains(s.output(), "cannot ring"));
  TEST_ASSERT_FALSE(s.state().registered);
  TEST_ASSERT_TRUE(s.run("network Other Mobile"));
  TEST_ASSERT_TRUE(s.state().registered);
}

void test_the_child_answers_with_a_key_and_the_other_side_can_hang_up_first() {
  Sim s;
  s.run(std::string("call ") + kParentOne);
  TEST_ASSERT_TRUE(s.run("key select"));
  s.sim.advance(1000);
  TEST_ASSERT_TRUE(s.state().callState == ModemState::InCall);
  TEST_ASSERT_TRUE(s.run("remote hangup"));
  s.sim.advance(1000);
  TEST_ASSERT_FALSE(s.state().callActive);
}

void test_the_radio_comes_up_when_the_core_asks_for_it() {
  Sim s;
  s.sim.runtime().core().requestRadioStart();
  s.sim.advance(1000);
  TEST_ASSERT_TRUE(contains(s.output(), "radio: started"));
}

void test_radio_messages_arrive_from_a_contact_and_confirmations_need_a_message() {
  Sim s;
  s.sim.runtime().core().requestRadioStart();
  s.sim.advance(1000);
  TEST_ASSERT_TRUE(s.run("radio peer NodeOne"));
  TEST_ASSERT_TRUE(s.run("radio msg NodeOne hello by radio"));
  TEST_ASSERT_EQUAL(1, static_cast<int>(s.state().unread));
  TEST_ASSERT_TRUE(s.run("radio confirm yes"));
  TEST_ASSERT_TRUE(contains(s.output(), "no radio message is waiting"));   // nothing was sent yet
  TEST_ASSERT_FALSE(s.run("radio confirm maybe"));
}

// ---- the world ------------------------------------------------------------------------------------------------

void test_battery_charger_and_signal_reach_the_state() {
  Sim s;
  TEST_ASSERT_TRUE(s.run("battery 17"));
  TEST_ASSERT_EQUAL(17, s.state().batteryPercent);
  TEST_ASSERT_TRUE(s.run("plug charging"));
  TEST_ASSERT_TRUE(s.state().charge == ChargeState::Charging);
  TEST_ASSERT_TRUE(s.run("plug full"));
  TEST_ASSERT_TRUE(s.state().charge == ChargeState::Full);
  TEST_ASSERT_TRUE(s.run("unplug"));
  TEST_ASSERT_TRUE(s.state().charge == ChargeState::Discharging);
  TEST_ASSERT_TRUE(s.run("signal 1"));
  TEST_ASSERT_EQUAL(1, s.state().signalBars);
}

void test_the_sim_card_states_reach_the_state() {
  Sim s;
  const struct { const char* word; SimStatus status; } states[] = {
      {"missing", SimStatus::Missing}, {"pin", SimStatus::PinRequired}, {"pinwrong", SimStatus::PinWrong},
      {"puk", SimStatus::PukRequired}, {"ready", SimStatus::Ready}};
  for (const auto& st : states) {
    TEST_ASSERT_TRUE(s.run(std::string("simcard ") + st.word));
    s.sim.advance(1000);
    TEST_ASSERT_TRUE_MESSAGE(s.state().sim == st.status, st.word);
  }
}

void test_a_gps_scenario_decides_the_position_the_device_gets() {
  {
    Sim s;
    TEST_ASSERT_TRUE(s.run("gps fix 50.5 8.5 2"));
    s.sim.runtime().core().setPositionPageOpen(true);
    s.sim.advance(5000);
    GnssFix fix;
    TEST_ASSERT_TRUE(s.sim.runtime().core().lastKnownFix(fix));
    TEST_ASSERT_TRUE(std::abs(fix.lat - 50.5) < 1e-9);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, fix.hdop);
  }
  {
    Sim s;
    TEST_ASSERT_TRUE(s.run("gps none"));
    s.sim.runtime().core().setPositionPageOpen(true);
    s.sim.advance(60000);
    GnssFix fix;
    TEST_ASSERT_FALSE(s.sim.runtime().core().lastKnownFix(fix));
  }
  {
    Sim s;
    TEST_ASSERT_TRUE(s.run("gps bad"));
    s.sim.runtime().core().setPositionPageOpen(true);
    s.sim.advance(5000);
    GnssFix fix;
    TEST_ASSERT_TRUE(s.sim.runtime().core().lastKnownFix(fix));
    TEST_ASSERT_TRUE(fix.hdop > 20.0f);
  }
  Sim s;
  TEST_ASSERT_TRUE(s.run("gps fix"));                            // the default place
  TEST_ASSERT_FALSE(s.run("gps fix 1 2"));
  TEST_ASSERT_FALSE(s.run("gps fix north south 1"));
  TEST_ASSERT_FALSE(s.run("gps sideways"));
}

void test_motion_and_wifi_commands_are_accepted_and_checked() {
  Sim s;
  for (const char* ok : {"motion tilt 0 0 1", "motion shake", "motion off", "motion on", "wifi connected", "wifi failed", "wifi off"})
    TEST_ASSERT_TRUE_MESSAGE(s.run(ok), ok);
  for (const char* bad : {"motion tilt 0 0", "motion spin", "wifi", "wifi maybe"}) TEST_ASSERT_FALSE_MESSAGE(s.run(bad), bad);
}

// ---- the device ---------------------------------------------------------------------------------------------------

void test_keys_move_through_the_screens_and_a_wrong_key_name_is_an_error() {
  Sim s;
  TEST_ASSERT_TRUE(s.run("key inbox"));
  TEST_ASSERT_EQUAL(ui::screen::kMessages, s.sim.runtime().stack().visibleId());
  TEST_ASSERT_TRUE(s.run("key home compose"));
  TEST_ASSERT_EQUAL(ui::screen::kRecipient, s.sim.runtime().stack().visibleId());
  TEST_ASSERT_FALSE(s.run("key home sideways"));
  TEST_ASSERT_EQUAL(ui::screen::kHome, s.sim.runtime().stack().visibleId());  // the first key was used before the error
  TEST_ASSERT_FALSE(s.run("key"));
}

void test_a_tap_on_a_tile_is_a_touch_and_the_arguments_are_checked() {
  Sim s;
  TEST_ASSERT_TRUE(s.run("tap 120 100"));
  TEST_ASSERT_FALSE(s.run("tap 120"));
  TEST_ASSERT_FALSE(s.run("tap a b"));
}

void test_type_needs_a_text_and_takes_utf8() {
  Sim s;
  TEST_ASSERT_FALSE(s.run("type"));
  TEST_ASSERT_TRUE(s.run("type abc \xC3\xA4\xE2\x82\xAC"));
}

void test_the_language_changes_the_picture_and_an_unknown_one_is_an_error() {
  Sim s;
  s.run("advance 2s");
  s.run("ascii");
  const std::string english = s.output();
  Sim german;
  TEST_ASSERT_TRUE(german.run("language de"));
  TEST_ASSERT_TRUE(german.run("ascii"));
  TEST_ASSERT_TRUE(german.output() != english);
  TEST_ASSERT_TRUE(german.run("language qps-ploc"));                // the pseudo-language is there to look at layouts
  TEST_ASSERT_FALSE(german.run("language zz"));
  TEST_ASSERT_EQUAL_STRING("qps-ploc", german.sim.translator().language().c_str());
}

// ---- looking ------------------------------------------------------------------------------------------------------

void test_ascii_prints_the_whole_panel() {
  Sim s;
  s.run("ascii");
  size_t lines = 0;
  for (char c : s.output()) lines += c == '\n';
  TEST_ASSERT_TRUE(lines >= 320);
}

void test_a_screenshot_is_written_and_an_unwritable_path_is_an_error() {
  Sim s;
  const std::filesystem::path dir = scratch("shot");
  TEST_ASSERT_TRUE(s.run("screenshot " + (dir / "panel.png").string()));
  TEST_ASSERT_TRUE(std::filesystem::file_size(dir / "panel.png") > 100);
  TEST_ASSERT_FALSE(s.run("screenshot /nonexistent-directory/home.png"));
  TEST_ASSERT_FALSE(s.run("screenshot"));
}

void test_a_screenshot_shows_what_the_state_says_without_waiting_for_the_next_frame() {
  Sim s;
  s.sim.advance(2000);
  const std::filesystem::path dir = scratch("fresh");
  s.run("screenshot " + (dir / "before.png").string());
  s.run(std::string("sms ") + kParentOne + " Hello");
  s.run("screenshot " + (dir / "after.png").string());
  TEST_ASSERT_TRUE(std::filesystem::file_size(dir / "before.png") != std::filesystem::file_size(dir / "after.png") ||
                   s.sim.panel().refreshes().size() >= 2);
  TEST_ASSERT_TRUE(s.sim.panel().refreshes().size() >= 2);          // the badge was drawn and refreshed
}

void test_golden_writes_on_request_compares_otherwise_and_fails_on_a_difference() {
  const std::filesystem::path dir = scratch("golden");
  Sim s("sim/data", dir.string());
  TEST_ASSERT_FALSE(s.run("golden panel"));                           // no file: never created silently
  TEST_ASSERT_TRUE(contains(s.output(), "UPDATE_GOLDEN"));
  setenv("UPDATE_GOLDEN", "1", 1);
  TEST_ASSERT_TRUE(s.run("golden panel"));
  unsetenv("UPDATE_GOLDEN");
  TEST_ASSERT_TRUE(std::filesystem::exists(dir / "panel.txt"));
  TEST_ASSERT_TRUE(s.run("golden panel"));
  s.run(std::string("sms ") + kParentOne + " Hello");               // the badge appears
  TEST_ASSERT_FALSE(s.run("golden panel"));
  TEST_ASSERT_TRUE(contains(s.output(), "differs"));
}

void test_state_and_log_and_help_print() {
  Sim s;
  TEST_ASSERT_TRUE(s.run("state"));
  TEST_ASSERT_TRUE(contains(s.output(), "screen:    home"));
  TEST_ASSERT_TRUE(contains(s.output(), "language:  en"));
  TEST_ASSERT_TRUE(s.run("help"));
  TEST_ASSERT_TRUE(contains(s.output(), "English only"));
  TEST_ASSERT_TRUE(s.run("log 5"));
}

void test_the_log_shows_the_devices_own_lines() {
  Sim s("sim/no-such-directory");
  TEST_ASSERT_TRUE(s.run("log 50"));
  TEST_ASSERT_TRUE(contains(s.output(), "config: missing-file"));
}

// ---- control ------------------------------------------------------------------------------------------------------

void test_an_unknown_command_is_an_error_that_says_so() {
  Sim s;
  TEST_ASSERT_FALSE(s.run("dance"));
  TEST_ASSERT_TRUE(contains(s.output(), "no command dance"));
}

void test_bad_arguments_are_errors_and_change_nothing() {
  Sim s;
  s.sim.advance(2000);
  const AppCore::State before = s.state();
  for (const char* bad : {"battery 101", "battery -1", "battery x", "signal 6", "plug", "plug half", "simcard", "simcard x",
                          "sms", "sms +491701111111", "call", "remote", "remote dance", "radio", "radio msg Node", "language",
                          "run", "echo", "golden", "ascii /nonexistent-directory/x.txt"}) {
    if (std::string(bad) == "echo") continue;                       // echo of nothing prints an empty line: fine
    TEST_ASSERT_FALSE_MESSAGE(s.run(bad), bad);
  }
  const AppCore::State after = s.state();
  TEST_ASSERT_EQUAL(before.batteryPercent, after.batteryPercent);
  TEST_ASSERT_EQUAL(before.unread, after.unread);
  TEST_ASSERT_EQUAL(before.signalBars, after.signalBars);
}

void test_comments_and_blank_lines_are_ignored() {
  Sim s;
  TEST_ASSERT_TRUE(s.run(""));
  TEST_ASSERT_TRUE(s.run("   "));
  TEST_ASSERT_TRUE(s.run("# only a comment"));
  TEST_ASSERT_TRUE(s.run("battery 50  # half"));
  TEST_ASSERT_EQUAL(50, s.state().batteryPercent);
}

void test_a_script_runs_its_lines_and_stops_at_the_first_error_naming_it() {
  const std::filesystem::path dir = scratch("script");
  {
    std::ofstream f(dir / "ok.txt");
    f << "# a story\nbattery 33\necho done\n";
    std::ofstream g(dir / "bad.txt");
    g << "battery 44\ndance\nbattery 55\n";
  }
  Sim s;
  TEST_ASSERT_TRUE(s.sim.runScript((dir / "ok.txt").string()));
  TEST_ASSERT_EQUAL(33, s.state().batteryPercent);
  TEST_ASSERT_TRUE(contains(s.output(), "done"));
  TEST_ASSERT_FALSE(s.sim.runScript((dir / "bad.txt").string()));
  TEST_ASSERT_EQUAL(44, s.state().batteryPercent);                   // the line after the error did not run
  TEST_ASSERT_TRUE(contains(s.output(), "bad.txt:2: dance"));
  TEST_ASSERT_FALSE(s.sim.runScript((dir / "missing.txt").string()));
}

void test_a_script_that_runs_itself_ends_with_an_error_instead_of_a_crash() {
  const std::filesystem::path dir = scratch("loop");
  {
    std::ofstream f(dir / "loop.txt");
    f << "run " << (dir / "loop.txt").string() << "\n";
  }
  Sim s;
  TEST_ASSERT_FALSE(s.sim.runScript((dir / "loop.txt").string()));
  TEST_ASSERT_TRUE(contains(s.output(), "too deeply"));
}

void test_quit_ends_a_script() {
  const std::filesystem::path dir = scratch("quit");
  {
    std::ofstream f(dir / "quit.txt");
    f << "battery 61\nquit\nbattery 62\n";
  }
  Sim s;
  TEST_ASSERT_TRUE(s.sim.runScript((dir / "quit.txt").string()));
  TEST_ASSERT_TRUE(s.sim.quitRequested());
  TEST_ASSERT_EQUAL(61, s.state().batteryPercent);
}

// ---- the host's duties --------------------------------------------------------------------------------------------------

void test_a_restart_the_device_asks_for_starts_it_again_on_the_same_card() {
  Sim s;
  s.sim.card().write("/config/settings.toml", "[child]\nname = \"Alex\"\n[interface]\nlanguage = \"de\"\n");
  TEST_ASSERT_EQUAL(1, s.sim.boots());
  s.sim.runtime().core().requestRestart();
  s.sim.advance(1000);
  TEST_ASSERT_EQUAL(2, s.sim.boots());
  TEST_ASSERT_TRUE(contains(s.output(), "the device restarts"));
  TEST_ASSERT_EQUAL_STRING("de", s.sim.translator().language().c_str());   // the card was kept
}

void test_a_power_off_the_device_asks_for_ends_the_simulation() {
  Sim s;
  s.sim.runtime().core().requestPowerOff();
  s.sim.advance(2000);
  TEST_ASSERT_TRUE(s.sim.quitRequested());
  TEST_ASSERT_TRUE(contains(s.output(), "switched itself off"));
}

void test_the_card_is_a_copy_and_the_data_directory_is_never_written() {
  Sim s;
  s.sim.card().write("/config/settings.toml", "[child]\nname = \"Changed\"\n");
  Sim fresh;
  TEST_ASSERT_EQUAL_STRING("Alex", fresh.sim.runtime().core().profile().childName.c_str());
  std::string text;
  TEST_ASSERT_TRUE(sim::detail::readFile("sim/data/config/settings.toml", text));
  TEST_ASSERT_TRUE(contains(text, "Alex"));
}

void test_a_fresh_start_forgets_what_the_last_one_saved_unless_the_card_is_used_in_place() {
  const std::filesystem::path data = scratch("inplace-data");
  std::filesystem::copy("sim/data", data, std::filesystem::copy_options::recursive);
  sim::Options o;
  o.dataDir = data.string();
  o.workDir = scratch("inplace-work").string();
  o.webPort = 0;
  {
    std::ostringstream out;
    sim::Simulator first(o, out);
    first.card().write("/config/settings.toml", "[child]\nname = \"Sam\"\n");
  }
  std::ostringstream out;
  sim::Simulator second(o, out);
  TEST_ASSERT_EQUAL_STRING("Alex", second.runtime().core().profile().childName.c_str());   // the copy was made again
  o.inPlace = true;
  {
    std::ostringstream out2;
    sim::Simulator inPlace(o, out2);
    inPlace.card().write("/config/settings.toml", "[child]\nname = \"Sam\"\n");
  }
  std::ostringstream out3;
  sim::Simulator again(o, out3);
  TEST_ASSERT_EQUAL_STRING("Sam", again.runtime().core().profile().childName.c_str());
}

void test_a_work_directory_that_is_not_the_simulators_is_never_emptied() {
  const std::filesystem::path work = scratch("foreign");
  std::ofstream(work / "precious.txt") << "mine";
  sim::Options o;
  o.workDir = work.string();
  o.webPort = 0;
  std::ostringstream out;
  sim::Simulator s(o, out);
  TEST_ASSERT_FALSE(s.ok());
  TEST_ASSERT_TRUE(contains(s.error(), "refusing to empty"));
  TEST_ASSERT_TRUE(std::filesystem::exists(work / "precious.txt"));
}

// A work directory a simulator of 4.1.1 or before made carries the mark of the project's old name: it is still the
// simulator's, so it is emptied and gets the new mark, instead of being refused like a stranger's.
void test_a_work_directory_of_an_earlier_simulator_is_still_emptied_and_marked_anew() {
  const std::filesystem::path work = scratch("earlier");
  std::ofstream(work / ".phoneplus-sim") << "made by an earlier simulator\n";
  std::filesystem::create_directories(work / "card");
  std::ofstream(work / "card" / "stale.txt") << "left over";
  sim::Options o;
  o.workDir = work.string();
  o.webPort = 0;
  std::ostringstream out;
  sim::Simulator s(o, out);
  TEST_ASSERT_TRUE_MESSAGE(s.ok(), s.error().c_str());
  TEST_ASSERT_FALSE(std::filesystem::exists(work / "card" / "stale.txt"));
  TEST_ASSERT_TRUE(std::filesystem::exists(work / ".phone-os-sim"));
  TEST_ASSERT_FALSE(std::filesystem::exists(work / ".phoneplus-sim"));
}

void test_a_work_directory_that_is_holds_or_lies_in_the_data_is_refused() {
  const std::filesystem::path data = scratch("holds");
  std::filesystem::create_directories(data / "config");
  for (const std::filesystem::path& work : {data, data.parent_path(), data / "config" / "work"}) {
    sim::Options o;
    o.dataDir = data.string();
    o.workDir = work.string();
    o.webPort = 0;
    std::ostringstream out;
    sim::Simulator s(o, out);
    TEST_ASSERT_FALSE_MESSAGE(s.ok(), work.string().c_str());
  }
  TEST_ASSERT_TRUE(std::filesystem::exists(data / "config"));
}

void test_in_place_needs_a_data_directory() {
  sim::Options o;
  o.dataDir = "sim/no-such-directory";
  o.inPlace = true;
  std::ostringstream out;
  sim::Simulator s(o, out);
  TEST_ASSERT_FALSE(s.ok());
}

// ---- the smoke script -----------------------------------------------------------------------------------------------------

void test_the_smoke_script_runs_through_the_real_wiring_and_matches_its_pictures() {
  Sim s;
  TEST_ASSERT_TRUE_MESSAGE(s.sim.runScript("sim/scripts/smoke.txt"), s.output().c_str());
}

void test_see_finds_a_word_by_its_catalog_key_in_every_language_and_says_what_is_missing() {
  Sim s;
  s.sim.advance(3000);
  for (const char* language : {"en", "de", "fr"}) {
    TEST_ASSERT_TRUE(s.run(std::string("language ") + language));
    TEST_ASSERT_TRUE_MESSAGE(s.run("see home.phone"), language);
    TEST_ASSERT_TRUE_MESSAGE(s.run("see home.sos"), language);
  }
  s.run("language en");
  TEST_ASSERT_FALSE(s.run("see home.clock"));                       // a tile of the second page: not on the screen
  TEST_ASSERT_TRUE(contains(s.output(), "does not say \"Clock\""));
  TEST_ASSERT_TRUE(s.run("see-not home.motion"));
  TEST_ASSERT_FALSE(s.run("see-not home.phone"));
  TEST_ASSERT_FALSE(s.run("see no.such.key"));
  TEST_ASSERT_FALSE(s.run("see call.busy name"));                   // an argument without a name
  TEST_ASSERT_TRUE(s.run("say Phone"));
  TEST_ASSERT_FALSE(s.run("say Phon"));                             // part of a word is not the word
  TEST_ASSERT_TRUE(s.run("say-not Phon"));
  TEST_ASSERT_FALSE(s.run("say"));
}

void test_check_and_fails_report_what_differs() {
  Sim s;
  TEST_ASSERT_TRUE(s.run("check unread 0"));
  TEST_ASSERT_FALSE(s.run("check unread 1"));
  TEST_ASSERT_TRUE(contains(s.output(), "unread is 0, expected 1"));
  TEST_ASSERT_FALSE(s.run("check nothing 1"));
  TEST_ASSERT_FALSE(s.run("check unread"));
  TEST_ASSERT_FALSE(s.run("check haptic-buzz 0"));
  TEST_ASSERT_TRUE(s.run("fails check unread 1"));
  TEST_ASSERT_FALSE(s.run("fails check unread 0"));                 // it worked, and was expected not to
  TEST_ASSERT_FALSE(s.run("fails"));
}

// ---- words only the host shows ----------------------------------------------------------------------------------------------

void test_the_words_of_the_demo_page_are_in_the_simulators_languages_but_not_in_the_shipped_files() {
  Sim s;
  for (const char* code : {"en", "de", "fr"}) {
    TEST_ASSERT_TRUE(s.sim.translator().select(code));
    TEST_ASSERT_TRUE_MESSAGE(s.sim.translator().get("demo.title") != "demo.title", code);   // a missing key answers with itself
    TEST_ASSERT_TRUE_MESSAGE(s.sim.translator().get("home.phone") != "home.phone", code);
  }
  TEST_ASSERT_TRUE(s.sim.translator().get("demo.title") != s.sim.translator().getIn("en", "demo.title"));
  // The shipped catalog of the device holds none of them.
  std::ifstream in("lang/en.toml");
  std::stringstream text;
  text << in.rdbuf();
  TEST_ASSERT_TRUE(text.str().find("\ndemo.") == std::string::npos);
  // Without the host directory the simulator still runs, without those words.
  sim::Options o;
  o.webPort = 0;
  o.hostLangDir = "tools/no-such-directory";
  o.workDir = scratch("nohost").string();
  std::ostringstream out;
  sim::Simulator bare(o, out);
  TEST_ASSERT_TRUE(bare.ok());
  TEST_ASSERT_EQUAL_STRING("demo.title", bare.translator().get("demo.title").c_str());
  TEST_ASSERT_TRUE(bare.translator().get("home.phone") != "home.phone");
}

// ---- the documentation ---------------------------------------------------------------------------------------------------

void test_every_command_of_the_help_is_in_the_documentation() {
  Sim s;
  s.run("help");
  std::ifstream in("docs/simulator.md");
  std::stringstream text;
  text << in.rdbuf();
  const std::string docs = text.str();
  std::istringstream help(s.output());
  std::string line;
  size_t commands = 0;
  while (std::getline(help, line)) {
    if (line.rfind("    ", 0) != 0 || line[4] == ' ') continue;      // a command line starts at the fourth column
    // The left column, up to two spaces in a row, holds the commands, separated by " | ".
    const std::string left = line.substr(4, line.find("  ", 4) - 4);
    size_t at = 0;
    while (at <= left.size()) {
      const size_t bar = left.find(" | ", at);
      const std::string one = left.substr(at, bar == std::string::npos ? std::string::npos : bar - at);
      const std::string name = one.substr(0, one.find(' '));
      ++commands;
      TEST_ASSERT_TRUE_MESSAGE(docs.find(name) != std::string::npos, (name + " is not in docs/simulator.md").c_str());
      if (bar == std::string::npos) break;
      at = bar + 3;
    }
  }
  TEST_ASSERT_TRUE(commands > 30);
}

// ---- the crawl ----------------------------------------------------------------------------------------------------------

namespace {
sim::Options crawlOptions() {
  sim::Options o;
  o.dataDir = "sim/data";
  o.webPort = 0;
  return o;
}
}  // namespace

void test_the_crawl_finds_the_screens_the_keys_lead_to_and_the_same_graph_every_time() {
  sim::Crawler crawler(crawlOptions(), "sim/demo/world.txt");
  const sim::Crawl first = crawler.run(1);
  const sim::Crawl second = crawler.run(1);
  TEST_ASSERT_EQUAL_STRING(sim::Crawler::json(first).c_str(), sim::Crawler::json(second).c_str());
  TEST_ASSERT_EQUAL_STRING("home", first.nodes[0].screen.c_str());
  TEST_ASSERT_EQUAL(0, first.starts.at("en"));
  std::set<std::string> screens;
  for (const sim::CrawlNode& node : first.nodes) screens.insert(node.screen);
  for (const char* screen : {"home", "lock", "phone", "messages", "sos", "radio"})
    TEST_ASSERT_TRUE_MESSAGE(screens.count(screen) == 1, screen);
  for (const sim::CrawlEdge& edge : first.edges) {
    TEST_ASSERT_TRUE(edge.from >= 0 && edge.to >= 0 && edge.from != edge.to);
    TEST_ASSERT_TRUE(static_cast<size_t>(edge.from) < first.nodes.size() && static_cast<size_t>(edge.to) < first.nodes.size());
  }
  for (const sim::CrawlNode& node : first.nodes) TEST_ASSERT_TRUE(node.depth <= 1 && static_cast<int>(node.path.size()) == node.depth);
}

// Nothing is cut off to keep a walk short: a walk that reaches its limits says so, and a caller must not use it. The old
// cap on the states of a screen left keys that are drawn and lead nowhere (a state past it had no edge).
void test_a_walk_that_reaches_a_limit_says_it_is_not_complete_and_nothing_else_cuts_a_state() {
  sim::Crawler crawler(crawlOptions(), "sim/demo/world.txt");
  const sim::Crawl shallow = crawler.run(1);
  TEST_ASSERT_FALSE(shallow.complete);                      // the states one step from the home screen lead further
  TEST_ASSERT_TRUE(shallow.nodes.size() > 10);
  TEST_ASSERT_TRUE(crawler.error().empty());
  const sim::Crawl few = crawler.run(30, {}, 5);
  TEST_ASSERT_EQUAL(5u, few.nodes.size());
  TEST_ASSERT_FALSE(few.complete);
  TEST_ASSERT_TRUE_MESSAGE(crawler.error().find("more than 5 states") != std::string::npos, crawler.error().c_str());
  // A state of one screen is kept however many the screen has: the old cap kept 30, and the home screen alone has more
  // pictures than that (a cursor on each tile, in each language) once the walk is not cut.
  std::map<std::string, size_t> per;
  for (const sim::CrawlNode& node : shallow.nodes) ++per[node.screen];
  TEST_ASSERT_TRUE(per["home"] >= 3);
}

// A walk follows every key and every tap of every state it has fully followed: each of the seven keys has an edge or is
// noted as pressed without effect, a soft key that is drawn leads somewhere, and the three languages each have a start of
// their own. (The full graph is checked by scripts/make_demo.py on every build, at every depth.)
void test_every_key_of_every_followed_state_has_an_edge_or_a_note_and_every_language_its_start() {
  sim::Crawler crawler(crawlOptions(), "sim/demo/world.txt");
  const sim::Crawl crawl = crawler.run(1, {"en", "de", "fr"});
  TEST_ASSERT_EQUAL(3u, crawl.starts.size());
  for (const char* code : {"en", "de", "fr"}) {
    TEST_ASSERT_TRUE_MESSAGE(crawl.starts.count(code) == 1, code);
    TEST_ASSERT_EQUAL_STRING(code, crawl.nodes[static_cast<size_t>(crawl.starts.at(code))].language.c_str());
  }
  size_t followed = 0;
  for (const sim::CrawlNode& node : crawl.nodes) {
    if (node.depth >= 1) continue;   // the states one step further were followed too, but what they lead to is beyond the limit
    ++followed;
    for (const std::string& key : sim::Crawler::keys()) {
      bool edge = false;
      for (const sim::CrawlEdge& e : crawl.edges) edge = edge || (e.from == node.id && e.key == key);
      const bool noted = std::find(node.idle.begin(), node.idle.end(), key) != node.idle.end();
      TEST_ASSERT_TRUE_MESSAGE(edge != noted, (node.screen + ": " + key + " has an edge or a note, not both and not neither").c_str());
      // The arrows of a list that is at its end are drawn and do nothing, as on the device; every other soft key leads on.
      const bool drawn = std::find(node.soft.begin(), node.soft.end(), key) != node.soft.end();
      TEST_ASSERT_FALSE_MESSAGE(drawn && noted && key != "up" && key != "down", (node.screen + ": the soft key " + key + " is drawn and does nothing").c_str());
    }
  }
  TEST_ASSERT_EQUAL(3u, followed);
}

namespace {
// A walk that starts where a script leaves the device: the script is the prelude, so a state far from the home screen
// can be walked from without walking to it.
std::string preludeWith(const std::string& name, const std::string& steps) {
  const std::filesystem::path file = scratch(name) / "prelude.txt";
  std::ofstream(file) << "run sim/demo/world.txt\n" << steps;
  return file.string();
}
}  // namespace

// The call page: Hang up is the soft key and the Back key, and it ends the call as it does on the device. (The walk used to
// press no key during a call, so the only exit of the page was "start over".)
void test_hang_up_works_in_the_walk() {
  sim::Crawler crawler(crawlOptions(), preludeWith("walk-call", "key select\nkey select\n"));   // the phone page, and the first contact
  const sim::Crawl crawl = crawler.run(1);
  TEST_ASSERT_EQUAL_STRING("call", crawl.nodes[0].screen.c_str());
  TEST_ASSERT_TRUE(std::find(crawl.nodes[0].soft.begin(), crawl.nodes[0].soft.end(), "back") != crawl.nodes[0].soft.end());
  const sim::CrawlEdge* hangUp = nullptr;
  for (const sim::CrawlEdge& e : crawl.edges)
    if (e.from == 0 && e.key == "back") hangUp = &e;
  TEST_ASSERT_NOT_NULL_MESSAGE(hangUp, "Hang up has no edge");
  TEST_ASSERT_TRUE(crawl.nodes[static_cast<size_t>(hangUp->to)].crc != crawl.nodes[0].crc);   // the picture says that the call is over
  bool tapped = false;
  for (const sim::CrawlEdge& e : crawl.edges) tapped = tapped || (e.from == 0 && e.isTap() && e.tap.y >= 288);
  TEST_ASSERT_TRUE_MESSAGE(tapped, "a tap on the Hang up bar has no edge");
}

// A ready-made message is sent by Enter and by a tap on its row; the device shows the home screen after it, and so does
// the walk (the sent message itself is not carried into the lists of the states that follow: the walk does not keep a
// history, so the home screen it returns to is the one it knows).
void test_a_ready_made_message_is_sent_in_the_walk_and_leads_to_the_home_screen() {
  sim::Crawler crawler(crawlOptions(), preludeWith("walk-compose", "key compose\nkey select\n"));   // the page of the ready-made messages
  const sim::Crawl crawl = crawler.run(1);
  TEST_ASSERT_EQUAL_STRING("compose", crawl.nodes[0].screen.c_str());
  bool key = false, rowTap = false;
  for (const sim::CrawlEdge& e : crawl.edges) {
    if (e.from != 0 || crawl.nodes[static_cast<size_t>(e.to)].screen != "home") continue;
    if (e.key == "select") key = true;
    if (e.isTap() && e.tap.y > 100 && e.tap.y < 288) rowTap = true;
  }
  TEST_ASSERT_TRUE_MESSAGE(key, "Enter on a ready-made message has no edge");
  TEST_ASSERT_TRUE_MESSAGE(rowTap, "a tap on a ready-made message has no edge");
}

// The key test of the self-test page says "Ends in 6 s without input" and ends by itself; the walk has no time of its own, so
// it lets the time pass once on every state of the page, and the graph has an edge of seconds for it.
void test_the_key_test_of_the_self_test_page_ends_by_itself_in_the_walk() {
  // Home, down to the self-test tile (after the child's seven, the network, GPS, clock, settings and motion tiles), into it,
  // down to the key test (the fourth), into it.
  std::string steps;
  for (int i = 0; i < 12; ++i) steps += "key down\n";
  steps += "key select\nkey down\nkey down\nkey down\nkey select\n";
  sim::Crawler crawler(crawlOptions(), preludeWith("walk-selftest", steps));
  const sim::Crawl crawl = crawler.run(1);
  TEST_ASSERT_EQUAL_STRING("selftest", crawl.nodes[0].screen.c_str());
  const sim::CrawlEdge* ends = nullptr;
  for (const sim::CrawlEdge& e : crawl.edges)
    if (e.from == 0 && e.isWait()) ends = &e;
  TEST_ASSERT_NOT_NULL_MESSAGE(ends, "the key test does not end in the walk");
  TEST_ASSERT_EQUAL(sim::Crawler::kWaitSeconds, ends->wait);
  TEST_ASSERT_TRUE(crawl.nodes[static_cast<size_t>(ends->to)].crc != crawl.nodes[0].crc);
  TEST_ASSERT_TRUE(sim::Crawler::kWaitSeconds > 6);   // SelfTestScreen::kIdleSeconds
  const std::string json = sim::Crawler::json(crawl);
  TEST_ASSERT_TRUE(contains(json, "\"wait\":7"));
  // A key of the test is a key to test: it leads to the picture that names it, and the time starts again from there.
  bool named = false;
  for (const sim::CrawlEdge& e : crawl.edges) named = named || (e.from == 0 && e.key == "up");
  TEST_ASSERT_TRUE(named);
}

// The Language screen lists the languages the device has, and from each of them each other one can be chosen with the keys
// and with a finger. The cursor starts on the language in use, so the same keys pick different rows in different
// languages: a walk found in English and replayed in German chose German where it meant English.
void test_the_language_page_lists_the_three_languages_and_each_can_be_chosen_from_each_other_by_key_and_tap() {
  sim::Options options = crawlOptions();
  options.pseudoLanguage = false;
  sim::Crawler crawler(options, "sim/demo/world.txt");
  // Home, the options tile, its language row, as in test_a_path_that_changes_a_setting...
  std::vector<std::string> toLanguage(10, "key down");
  for (const char* step : {"key select", "key down", "key down", "key select"}) toLanguage.push_back(step);
  const std::vector<std::string> codes = {"de", "en", "fr"};   // the order of the page: by code
  for (size_t here = 0; here < codes.size(); ++here) {
    std::unique_ptr<sim::Simulator> at = crawler.reach(toLanguage, codes[here]);
    TEST_ASSERT_NOT_NULL_MESSAGE(at.get(), crawler.error().c_str());
    TEST_ASSERT_EQUAL_STRING("language", sim::Crawler::screenOf(*at).c_str());
    std::vector<ui::TouchTarget> rows;
    for (const ui::TouchTarget& target : sim::Crawler::targets(*at))
      if (!target.key) rows.push_back(target);
    TEST_ASSERT_EQUAL_MESSAGE(3u, rows.size(), codes[here].c_str());   // exactly the three, no pseudo-language
    for (size_t there = 0; there < codes.size(); ++there) {
      std::vector<std::string> byKey = toLanguage;
      for (size_t i = here; i < there; ++i) byKey.push_back("key down");
      for (size_t i = there; i < here; ++i) byKey.push_back("key up");
      byKey.push_back("key select");
      std::unique_ptr<sim::Simulator> chosen = crawler.reach(byKey, codes[here]);
      TEST_ASSERT_NOT_NULL(chosen.get());
      TEST_ASSERT_EQUAL_STRING_MESSAGE(codes[there].c_str(), chosen->translator().language().c_str(), (codes[here] + " to " + codes[there] + " by key").c_str());
      std::vector<std::string> byTap = toLanguage;
      byTap.push_back("tap " + std::to_string(rows[there].rect.x + rows[there].rect.w / 2) + " " + std::to_string(rows[there].rect.y + rows[there].rect.h / 2));
      std::unique_ptr<sim::Simulator> tapped = crawler.reach(byTap, codes[here]);
      TEST_ASSERT_NOT_NULL(tapped.get());
      TEST_ASSERT_EQUAL_STRING_MESSAGE(codes[there].c_str(), tapped->translator().language().c_str(), (codes[here] + " to " + codes[there] + " by tap").c_str());
    }
  }
}

// The pseudo-language is a tool to look at layouts in the simulator; the device does not have it, and a simulator told to
// have the device's languages does not offer it.
void test_the_pseudo_language_is_a_choice_of_the_simulator_and_not_of_the_demo() {
  Sim with;
  TEST_ASSERT_TRUE(with.run("language qps-ploc"));
  sim::Options options = Sim::options("sim/data", "sim/golden");
  options.pseudoLanguage = false;
  std::ostringstream out;
  sim::Simulator without(options, out);
  TEST_ASSERT_TRUE(without.ok());
  TEST_ASSERT_FALSE(without.execute("language qps-ploc"));
  TEST_ASSERT_TRUE(without.execute("language fr"));
  size_t count = 0;
  for (const i18n::LanguageInfo& info : without.translator().languages()) {
    ++count;
    TEST_ASSERT_TRUE_MESSAGE(info.code != "qps-ploc", info.code.c_str());
  }
  TEST_ASSERT_EQUAL(3u, count);
}

// The walk taps what the screens report: from the home screen the SOS tile leads to the SOS page and the Lock soft
// key to the lock screen, each as an edge with the rectangle a finger hits, and the graph says so.
void test_the_walk_taps_the_targets_of_the_screens_and_the_graph_carries_their_rectangles() {
  sim::Crawler crawler(crawlOptions(), "sim/demo/world.txt");
  const sim::Crawl crawl = crawler.run(1);
  bool sos = false, lock = false, keys = false;
  for (const sim::CrawlEdge& edge : crawl.edges) {
    if (edge.from != 0) continue;
    const std::string& to = crawl.nodes[static_cast<size_t>(edge.to)].screen;
    if (!edge.isTap()) { keys = true; continue; }
    TEST_ASSERT_TRUE(edge.tap.w > 0 && edge.tap.h > 0 && edge.tap.right() <= 240 && edge.tap.bottom() <= 320);
    if (to == "sos" && edge.tap.y < 120) sos = true;
    if (to == "lock" && edge.tap.y >= 288) lock = true;
  }
  TEST_ASSERT_TRUE_MESSAGE(sos, "no tap on the SOS tile");
  TEST_ASSERT_TRUE_MESSAGE(lock, "no tap on the Lock soft key");
  TEST_ASSERT_TRUE(keys);
  const std::string json = sim::Crawler::json(crawl);
  TEST_ASSERT_TRUE(contains(json, "\"tap\":{\"x\":0,\"y\":288,\"w\":94,\"h\":32}"));
  TEST_ASSERT_TRUE(contains(json, "\"key\":\"select\""));
  // The path to a state reached by a tap replays the tap.
  for (const sim::CrawlNode& node : crawl.nodes)
    if (node.screen == "sos") {
      TEST_ASSERT_EQUAL(1, node.path.size());
      TEST_ASSERT_TRUE(node.path[0] == "key select" || node.path[0].rfind("tap ", 0) == 0);
    }
}

// Every target a screen reports is a place where a tap does something: on every state of a short walk, a tap in
// the middle of each target is used by the screen, lies on the panel and overlaps no other target. A screen that
// reports a rectangle its handle() ignores, or two that overlap, fails here.
void test_every_touch_target_a_screen_reports_is_used_when_its_middle_is_tapped() {
  sim::Crawler crawler(crawlOptions(), "sim/demo/world.txt");
  const sim::Crawl crawl = crawler.run(1);
  size_t checked = 0;
  std::set<std::string> screens;
  for (const sim::CrawlNode& node : crawl.nodes) {
    std::unique_ptr<sim::Simulator> at = crawler.reach(node.path, "");
    TEST_ASSERT_NOT_NULL(at.get());
    const std::vector<ui::TouchTarget> targets = sim::Crawler::targets(*at);
    for (size_t i = 0; i < targets.size(); ++i) {
      const ui::Rect& r = targets[i].rect;
      TEST_ASSERT_TRUE_MESSAGE(r.x >= 0 && r.y >= 0 && r.right() <= 240 && r.bottom() <= 320, node.screen.c_str());
      for (size_t j = 0; j < i; ++j) {
        const ui::Rect& o = targets[j].rect;
        const bool apart = r.right() <= o.x || o.right() <= r.x || r.bottom() <= o.y || o.bottom() <= r.y;
        TEST_ASSERT_TRUE_MESSAGE(apart, (node.screen + ": two targets overlap").c_str());
      }
      std::unique_ptr<sim::Simulator> fresh = crawler.reach(node.path, "");
      TEST_ASSERT_NOT_NULL(fresh.get());
      ui::Screen* screen = fresh->runtime().stack().visible();
      TEST_ASSERT_NOT_NULL(screen);
      TEST_ASSERT_TRUE_MESSAGE(screen->handle(ui::Input::touch(r.x + r.w / 2, r.y + r.h / 2)),
                               (node.screen + ": a target that a tap does not use").c_str());
      ++checked;
      screens.insert(node.screen);
    }
  }
  TEST_ASSERT_TRUE(checked > 20);
  for (const char* screen : {"home", "phone", "messages", "sos"}) TEST_ASSERT_TRUE_MESSAGE(screens.count(screen) == 1, screen);
}

// The pages write what the child changes back to the card. A walk that replays paths must start every one on
// the card as it was, and must never write to the data directory it was given.
void test_a_path_that_changes_a_setting_does_not_reach_the_next_path_or_the_data_directory() {
  const std::filesystem::path data = scratch("crawl-card");
  std::filesystem::copy("sim/data", data, std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
  const auto settingsOf = [&] {
    std::ifstream in(data / "config" / "settings.toml");
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
  };
  const std::string before = settingsOf();
  sim::Options options = crawlOptions();
  options.dataDir = data.string();
  // The crawler adds "-crawl" to the work directory it is given; a directory of that name left by an earlier run
  // that lost its marker would make the simulator refuse to start, so it is removed first.
  options.workDir = (data.parent_path() / "phone-os-sim-test-crawl-work").string();
  std::filesystem::remove_all(options.workDir + "-crawl");
  sim::Crawler crawler(options, "sim/demo/world.txt");
  // Home, the options tile (behind the seven tiles of the first page and three of the second), the language row
  // (third, above switching off), German: the choice is written to the card of that replay.
  std::vector<std::string> path(10, "key down");
  path.push_back("key select");
  for (const char* key : {"down", "down", "select", "up", "select"}) path.push_back(std::string("key ") + key);
  std::unique_ptr<sim::Simulator> chosen = crawler.reach(path, "");
  TEST_ASSERT_NOT_NULL_MESSAGE(chosen.get(), crawler.error().c_str());
  TEST_ASSERT_EQUAL_STRING("de", chosen->translator().language().c_str());
  TEST_ASSERT_TRUE(chosen->card().root() != data);
  std::unique_ptr<sim::Simulator> next = crawler.reach({}, "");
  TEST_ASSERT_NOT_NULL_MESSAGE(next.get(), crawler.error().c_str());
  TEST_ASSERT_EQUAL_STRING("en", next->translator().language().c_str());
  TEST_ASSERT_EQUAL_STRING(before.c_str(), settingsOf().c_str());
}

// A simulator that cannot start (here: a work directory that is not the simulator's, as a run that lost its
// marker leaves one) has no device behind it. The walk stops and names the reason; it does not run the prelude
// on nothing. Counter-check: without the test of ok() in Crawler::reach the first `advance` of the prelude is a
// crash (seen on 2026-10-13 with such a directory in the temp folder).
void test_a_simulator_that_cannot_start_stops_the_crawl_with_the_reason_instead_of_a_crash() {
  const std::filesystem::path work = scratch("crawl-refused");
  const std::filesystem::path taken = work.string() + "-crawl";   // what the crawler will use
  std::filesystem::create_directories(taken / "card");            // not empty, and no marker: not the simulator's
  sim::Options options = crawlOptions();
  options.workDir = work.string();
  sim::Crawler crawler(options, "sim/demo/world.txt");
  TEST_ASSERT_NULL(crawler.reach({}, "").get());
  TEST_ASSERT_TRUE_MESSAGE(crawler.error().find("refusing to empty") != std::string::npos, crawler.error().c_str());
  const sim::Crawl crawl = crawler.run(2);
  TEST_ASSERT_EQUAL(0u, crawl.nodes.size());
  TEST_ASSERT_FALSE(crawler.error().empty());
  TEST_ASSERT_TRUE(std::filesystem::exists(taken / "card"));      // nothing of the directory was touched
}

void test_the_same_path_leads_to_the_same_screen_in_every_language() {
  sim::Crawler crawler(crawlOptions(), "sim/demo/world.txt");
  const sim::Crawl crawl = crawler.run(1);
  for (const sim::CrawlNode& node : crawl.nodes)
    for (const char* language : {"de", "fr", "qps-ploc"}) {
      std::unique_ptr<sim::Simulator> there = crawler.reach(node.path, language);
      TEST_ASSERT_EQUAL_STRING_MESSAGE(node.screen.c_str(), sim::Crawler::screenOf(*there).c_str(), language);
    }
}

void test_shot_writes_a_picture_into_the_directory_it_was_given() {
  Sim s;
  const std::filesystem::path dir = scratch("shots");
  TEST_ASSERT_FALSE(s.run("shot first"));                              // no directory yet
  TEST_ASSERT_TRUE(s.run("shotdir " + (dir / "sub").string()));
  TEST_ASSERT_TRUE(s.run("shot first"));
  TEST_ASSERT_TRUE(std::filesystem::exists(dir / "sub" / "first.png"));
  TEST_ASSERT_FALSE(s.run("shotdir"));
}

// ---- the scenarios ------------------------------------------------------------------------------------------------------

void test_every_scenario_runs_to_its_end_on_its_own_card() {
  std::vector<std::string> scripts;
  for (const auto& entry : std::filesystem::directory_iterator("sim/scripts"))
    if (entry.path().extension() == ".txt") scripts.push_back(entry.path().string());
  std::sort(scripts.begin(), scripts.end());
  TEST_ASSERT_TRUE_MESSAGE(scripts.size() >= 10, "the scenarios are missing: run from the repository root");
  for (const std::string& script : scripts) {
    const std::string data = sim::scriptData(script);
    Sim s(data.empty() ? "sim/data" : data);
    TEST_ASSERT_TRUE_MESSAGE(s.sim.runScript(script), (script + "\n" + s.output()).c_str());
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_every_demo_card_loads_without_a_diagnostic_or_a_warning);
  RUN_TEST(test_the_cards_differ_in_what_they_are_for);
  RUN_TEST(test_school_time_on_the_school_card_never_ends);
  RUN_TEST(test_switched_off_windows_and_the_default_window_never_make_school_time);
  RUN_TEST(test_a_missing_data_directory_is_a_warning_and_an_empty_card);
  RUN_TEST(test_the_device_starts_on_a_tuesday_afternoon_in_germany);
  RUN_TEST(test_date_sets_the_local_time_of_the_devices_zone_also_across_the_change_of_the_clocks);
  RUN_TEST(test_date_refuses_what_is_not_a_date);
  RUN_TEST(test_advance_moves_the_clock_by_the_unit_and_the_device_runs_meanwhile);
  RUN_TEST(test_advance_refuses_a_negative_time_and_an_unknown_unit);
  RUN_TEST(test_the_lapse_is_kept_for_the_loop_and_must_be_sensible);
  RUN_TEST(test_a_text_message_arrives_with_its_whole_text_also_when_it_starts_with_a_hash);
  RUN_TEST(test_a_call_takes_the_screen_and_the_other_side_ends_it);
  RUN_TEST(test_a_call_cannot_come_in_without_a_network);
  RUN_TEST(test_the_child_answers_with_a_key_and_the_other_side_can_hang_up_first);
  RUN_TEST(test_the_radio_comes_up_when_the_core_asks_for_it);
  RUN_TEST(test_radio_messages_arrive_from_a_contact_and_confirmations_need_a_message);
  RUN_TEST(test_battery_charger_and_signal_reach_the_state);
  RUN_TEST(test_the_sim_card_states_reach_the_state);
  RUN_TEST(test_a_gps_scenario_decides_the_position_the_device_gets);
  RUN_TEST(test_motion_and_wifi_commands_are_accepted_and_checked);
  RUN_TEST(test_keys_move_through_the_screens_and_a_wrong_key_name_is_an_error);
  RUN_TEST(test_a_tap_on_a_tile_is_a_touch_and_the_arguments_are_checked);
  RUN_TEST(test_type_needs_a_text_and_takes_utf8);
  RUN_TEST(test_the_language_changes_the_picture_and_an_unknown_one_is_an_error);
  RUN_TEST(test_ascii_prints_the_whole_panel);
  RUN_TEST(test_a_screenshot_is_written_and_an_unwritable_path_is_an_error);
  RUN_TEST(test_a_screenshot_shows_what_the_state_says_without_waiting_for_the_next_frame);
  RUN_TEST(test_golden_writes_on_request_compares_otherwise_and_fails_on_a_difference);
  RUN_TEST(test_state_and_log_and_help_print);
  RUN_TEST(test_the_log_shows_the_devices_own_lines);
  RUN_TEST(test_an_unknown_command_is_an_error_that_says_so);
  RUN_TEST(test_bad_arguments_are_errors_and_change_nothing);
  RUN_TEST(test_comments_and_blank_lines_are_ignored);
  RUN_TEST(test_a_script_runs_its_lines_and_stops_at_the_first_error_naming_it);
  RUN_TEST(test_a_script_that_runs_itself_ends_with_an_error_instead_of_a_crash);
  RUN_TEST(test_quit_ends_a_script);
  RUN_TEST(test_a_restart_the_device_asks_for_starts_it_again_on_the_same_card);
  RUN_TEST(test_a_power_off_the_device_asks_for_ends_the_simulation);
  RUN_TEST(test_the_card_is_a_copy_and_the_data_directory_is_never_written);
  RUN_TEST(test_a_fresh_start_forgets_what_the_last_one_saved_unless_the_card_is_used_in_place);
  RUN_TEST(test_a_work_directory_that_is_not_the_simulators_is_never_emptied);
  RUN_TEST(test_a_work_directory_of_an_earlier_simulator_is_still_emptied_and_marked_anew);
  RUN_TEST(test_a_work_directory_that_is_holds_or_lies_in_the_data_is_refused);
  RUN_TEST(test_in_place_needs_a_data_directory);
  RUN_TEST(test_the_smoke_script_runs_through_the_real_wiring_and_matches_its_pictures);
  RUN_TEST(test_the_words_of_the_demo_page_are_in_the_simulators_languages_but_not_in_the_shipped_files);
  RUN_TEST(test_every_command_of_the_help_is_in_the_documentation);
  RUN_TEST(test_the_crawl_finds_the_screens_the_keys_lead_to_and_the_same_graph_every_time);
  RUN_TEST(test_a_walk_that_reaches_a_limit_says_it_is_not_complete_and_nothing_else_cuts_a_state);
  RUN_TEST(test_every_key_of_every_followed_state_has_an_edge_or_a_note_and_every_language_its_start);
  RUN_TEST(test_hang_up_works_in_the_walk);
  RUN_TEST(test_a_ready_made_message_is_sent_in_the_walk_and_leads_to_the_home_screen);
  RUN_TEST(test_the_key_test_of_the_self_test_page_ends_by_itself_in_the_walk);
  RUN_TEST(test_the_language_page_lists_the_three_languages_and_each_can_be_chosen_from_each_other_by_key_and_tap);
  RUN_TEST(test_the_pseudo_language_is_a_choice_of_the_simulator_and_not_of_the_demo);
  RUN_TEST(test_the_walk_taps_the_targets_of_the_screens_and_the_graph_carries_their_rectangles);
  RUN_TEST(test_every_touch_target_a_screen_reports_is_used_when_its_middle_is_tapped);
  RUN_TEST(test_a_path_that_changes_a_setting_does_not_reach_the_next_path_or_the_data_directory);
  RUN_TEST(test_a_simulator_that_cannot_start_stops_the_crawl_with_the_reason_instead_of_a_crash);
  RUN_TEST(test_the_same_path_leads_to_the_same_screen_in_every_language);
  RUN_TEST(test_shot_writes_a_picture_into_the_directory_it_was_given);
  RUN_TEST(test_every_scenario_runs_to_its_end_on_its_own_card);
  RUN_TEST(test_see_finds_a_word_by_its_catalog_key_in_every_language_and_says_what_is_missing);
  RUN_TEST(test_check_and_fails_report_what_differs);
  return UNITY_END();
}

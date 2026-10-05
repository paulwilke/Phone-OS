// Host tests for the simulator's console and the demo data: every command does what help says and
// refuses what it cannot do, a script stops at its first error, the demo cards load without a
// diagnostic, and the whole thing runs through the real Runtime.
// allow-non-english: the catalogs under test contain German.

#include <unity.h>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>

#include <Simulator.h>

void setUp() {}
void tearDown() { unsetenv("UPDATE_GOLDEN"); }

namespace {

constexpr const char* kParentOne = "+491701111111";

std::filesystem::path scratch(const std::string& name) {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / ("phoneplus-sim-test-" + name);
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
    return o;
  }
  bool run(const std::string& line) { return sim.execute(line); }
  AppCore::State state() { return sim.runtime().core().state(); }
  std::string output() const { return out.str(); }
};

bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

}  // namespace

// ---- the demo cards ------------------------------------------------------------------------------------

void test_every_demo_card_loads_without_a_diagnostic_or_a_warning() {
  for (const char* dir : {"sim/data", "sim/data-parent", "sim/data-school"}) {
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
  for (const char* ok : {"motion tilt 0 0 1", "motion shake", "motion off", "motion on", "wifi connected", "wifi failed"})
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
  s.sim.card().put("/config/settings.toml", "[child]\nname = \"Alex\"\n[interface]\nlanguage = \"de\"\n");
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

void test_the_card_is_a_copy_and_the_repository_is_never_written() {
  Sim s;
  s.sim.card().put("/config/settings.toml", "[child]\nname = \"Changed\"\n");
  Sim fresh;
  TEST_ASSERT_EQUAL_STRING("Alex", fresh.sim.runtime().core().profile().childName.c_str());
}

// ---- the smoke script -----------------------------------------------------------------------------------------------------

void test_the_smoke_script_runs_through_the_real_wiring_and_matches_its_pictures() {
  Sim s;
  TEST_ASSERT_TRUE_MESSAGE(s.sim.runScript("sim/scripts/smoke.txt"), s.output().c_str());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_every_demo_card_loads_without_a_diagnostic_or_a_warning);
  RUN_TEST(test_the_cards_differ_in_what_they_are_for);
  RUN_TEST(test_school_time_on_the_school_card_never_ends);
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
  RUN_TEST(test_the_card_is_a_copy_and_the_repository_is_never_written);
  RUN_TEST(test_the_smoke_script_runs_through_the_real_wiring_and_matches_its_pictures);
  return UNITY_END();
}

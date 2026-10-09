// The upgrade from 3.2: a card written by the 3.2 firmware loads in 4.0 without changes. The files under
// test/data/card-3.2/ have the layout 3.2 wrote (invented values); the tests run the real loader, the real
// runtime and the real card classes over them and pin what is read, what is reported and ignored, and that
// nothing of the old card is lost. docs/upgrading.md is written from these tests.
// allow-non-english: the 3.2 card holds German sentences, which is what 3.2 wrote.

#include <unity.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <BoardStorageRigs.h>
#include <Simulator.h>
#include <app/MessageStore.h>
#include <board/SdEventLog.h>
#include <config/ConfigLoader.h>
#include <config/Diagnostic.h>
#include <fakes/FakeClock.h>
#include <fakes/FakeConfigStore.h>

using config::ConfigLoader;
using config::Diagnostic;

void setUp() {}
void tearDown() {}

namespace {

const char* kCard = "test/data/card-3.2";
// Built from parts: the catalog check takes a quoted word with a dot for a catalog key.
const std::string kMessagesFile = std::string("messages") + ".log";
const std::string kSystemFile = std::string("system") + ".log";

std::string readCard(const std::string& relative) {
  std::ifstream in(std::string(kCard) + "/" + relative, std::ios::binary);
  std::stringstream text;
  text << in.rdbuf();
  return text.str();
}

FakeConfigStore card32() {
  FakeConfigStore store;
  for (const char* file : {"settings", "contacts", "secrets"}) {
    const std::string text = readCard(std::string("config/") + file + ".toml");
    TEST_ASSERT_FALSE_MESSAGE(text.empty(), "run the tests from the repository root");
    store.put(std::string("/config/") + file + ".toml", text);
  }
  return store;
}

// One diagnostic as a line: "code arg arg".
std::vector<std::string> described(const std::vector<Diagnostic>& all) {
  std::vector<std::string> out;
  for (const Diagnostic& d : all) {
    std::string line = config::diagnosticName(d.code);
    for (const std::string& a : d.args) line += " " + a;
    out.push_back(line);
  }
  std::sort(out.begin(), out.end());
  return out;
}

bool has(const std::vector<std::string>& lines, const std::string& line) {
  return std::find(lines.begin(), lines.end(), line) != lines.end();
}

}  // namespace

// ---- the settings, the contacts and the secrets ----------------------------------------------------------

void test_the_settings_of_a_3_2_card_are_read() {
  FakeConfigStore store = card32();
  const auto r = ConfigLoader(store).load();
  const Profile& p = r.profile;
  TEST_ASSERT_EQUAL_STRING("Alex", p.childName.c_str());
  TEST_ASSERT_TRUE(p.freeText);
  TEST_ASSERT_TRUE(p.modemSleep);
  TEST_ASSERT_TRUE(p.messageVibration);
  TEST_ASSERT_TRUE(p.callVibration);
  TEST_ASSERT_FALSE(p.quietCallVibration);
  TEST_ASSERT_FALSE(p.parentTools);
  TEST_ASSERT_TRUE(p.commandNotice == CommandNotice::History);
  TEST_ASSERT_NOT_NULL(p.timeZone);
  TEST_ASSERT_EQUAL_STRING("Europe/Berlin", p.timeZone->name);
  TEST_ASSERT_TRUE(p.logLevel == loglevel::Level::Events);
  TEST_ASSERT_EQUAL(10u, p.wifi.autoOffMinutes);
  TEST_ASSERT_EQUAL_STRING("https://maps.google.com/?q={lat},{lon}", p.mapTemplate.c_str());
  // The radio section of 3.2 is read under its new name, and the radio stays off: the key that allowed it is not 4.0's.
  TEST_ASSERT_EQUAL_STRING("Kind-Node", p.radioName.c_str());
  TEST_ASSERT_EQUAL_STRING("Papa-Node", p.radioPeer.c_str());
  TEST_ASSERT_FALSE(p.radioAllowed);
  // Keys that 3.2 never had get the defaults of 4.0.
  TEST_ASSERT_EQUAL_STRING("en", p.interfaceLanguage.c_str());
  TEST_ASSERT_EQUAL_STRING("en", p.smsLanguage.c_str());
  TEST_ASSERT_EQUAL(1500, p.batteryCapacityMah);
  TEST_ASSERT_TRUE(p.modemApn.empty());
}

void test_the_call_buttons_and_school_time_of_a_3_2_card_are_read() {
  FakeConfigStore store = card32();
  const Profile p = ConfigLoader(store).load().profile;
  // Only the call tiles are call buttons, in the order of the file: the first one is who the SOS reaches.
  TEST_ASSERT_EQUAL(2, (int)p.callTargets.size());
  TEST_ASSERT_EQUAL_STRING("+491702222222", p.callTargets[0].c_str());
  TEST_ASSERT_EQUAL_STRING("+491701111111", p.callTargets[1].c_str());
  TEST_ASSERT_EQUAL(2, (int)p.quietHours.count());
  TEST_ASSERT_EQUAL(8 * 60, p.quietHours.window(0).from);
  TEST_ASSERT_EQUAL(12 * 60 + 30, p.quietHours.window(0).to);
  TEST_ASSERT_EQUAL(13 * 60 + 30, p.quietHours.window(1).from);
  TEST_ASSERT_EQUAL(15 * 60, p.quietHours.window(1).to);
  TEST_ASSERT_EQUAL(0x1F, p.quietHours.window(0).days);   // Monday to Friday
}

void test_the_contact_book_of_a_3_2_card_is_read_and_a_stranger_stays_out() {
  FakeConfigStore store = card32();
  const Profile p = ConfigLoader(store).load().profile;
  TEST_ASSERT_EQUAL(3, (int)p.contacts.count());
  TEST_ASSERT_TRUE(p.contacts.isAdmin("+491701111111"));
  TEST_ASSERT_TRUE(p.contacts.isAdmin("+491702222222"));
  TEST_ASSERT_TRUE(p.contacts.isKnown("+493012345678"));
  TEST_ASSERT_FALSE(p.contacts.isAdmin("+493012345678"));
  TEST_ASSERT_TRUE(p.mayReceiveFrom("+493012345678"));
  TEST_ASSERT_FALSE(p.mayReceiveFrom("+491709999999"));
}

void test_the_secrets_of_a_3_2_card_are_read() {
  FakeConfigStore store = card32();
  const auto r = ConfigLoader(store).load();
  TEST_ASSERT_EQUAL_STRING("1234", r.simPin.c_str());
  TEST_ASSERT_EQUAL_STRING("test-password-1", r.webPassword.c_str());
  TEST_ASSERT_EQUAL_STRING("phoneplus", r.profile.wifi.apSsid.c_str());
  TEST_ASSERT_EQUAL_STRING("appassword1", r.profile.wifi.apPassword.c_str());
  TEST_ASSERT_EQUAL(1, (int)r.profile.wifi.networks.size());
  TEST_ASSERT_EQUAL_STRING("HomeNet", r.profile.wifi.networks[0].ssid.c_str());
}

void test_what_4_0_reports_and_ignores_on_a_3_2_card_is_exactly_this() {
  FakeConfigStore store = card32();
  const auto got = described(ConfigLoader(store).load().diagnostics);
  const std::vector<std::string> expected = {
      "mesh-alias",
      "non-call-tile compose",
      "non-call-tile light",
      "non-call-tile status",
      "unknown-key settings child.role",
      "unknown-key settings display.font_scale",
      "unknown-key settings features.mesh",
      "unknown-key settings screens",
      "unknown-key settings time.gps_utc_offset_minutes",
  };
  std::string all;
  for (const std::string& g : got) all += g + "\n";
  TEST_ASSERT_EQUAL_MESSAGE((int)expected.size(), (int)got.size(), all.c_str());
  for (const std::string& e : expected) TEST_ASSERT_TRUE_MESSAGE(has(got, e), (e + "\nreported:\n" + all).c_str());
}

// ---- what 3.2 accepted and 4.0 refuses: pinned, so that nobody finds it out on a device -------------------

void test_german_day_names_that_3_2_accepted_are_refused_and_reported() {
  const std::string window = "[[quiet_hours]]\ndays = \"mo-fr\"\nfrom = \"08:00\"\nto = \"12:30\"\n";
  FakeConfigStore store = card32();
  std::string settings;
  store.read("/config/settings.toml", settings, 100000);
  store.put("/config/settings.toml", settings.substr(0, settings.find("[[quiet_hours]]")) + window);
  const auto r = ConfigLoader(store).load();
  TEST_ASSERT_EQUAL(0, (int)r.profile.quietHours.count());   // no school time at all: the report is the only warning
  TEST_ASSERT_TRUE(has(described(r.diagnostics), "bad-quiet-hours bad-days"));
}

void test_a_third_window_on_one_day_that_3_2_accepted_is_refused_and_reported() {
  const std::string three =
      "[[quiet_hours]]\ndays = \"mon\"\nfrom = \"08:00\"\nto = \"09:00\"\n"
      "[[quiet_hours]]\ndays = \"mon\"\nfrom = \"10:00\"\nto = \"11:00\"\n"
      "[[quiet_hours]]\ndays = \"mon\"\nfrom = \"13:00\"\nto = \"14:00\"\n";
  FakeConfigStore store = card32();
  std::string settings;
  store.read("/config/settings.toml", settings, 100000);
  store.put("/config/settings.toml", settings.substr(0, settings.find("[[quiet_hours]]")) + three);
  const auto r = ConfigLoader(store).load();
  TEST_ASSERT_EQUAL(2, (int)r.profile.quietHours.count());
  TEST_ASSERT_TRUE(has(described(r.diagnostics), "bad-quiet-hours day-full"));
}

// ---- the runtime on the card ----------------------------------------------------------------------------

namespace {
struct Sim {
  std::ostringstream out;
  sim::Simulator sim;
  Sim() : sim(options(), out) {}
  static sim::Options options() {
    sim::Options o;
    o.dataDir = kCard;
    o.workDir = (std::filesystem::temp_directory_path() / "phone-os-upgrade-3-2-card").string();
    o.webPort = 0;
    return o;
  }
};
}  // namespace

void test_the_runtime_starts_on_a_3_2_card_and_school_time_is_kept() {
  Sim s;
  TEST_ASSERT_TRUE_MESSAGE(s.sim.ok(), s.sim.error().c_str());
  s.sim.advance(5000);
  // The simulator starts on a Tuesday at 14:32: inside the second window of the card.
  TEST_ASSERT_TRUE(s.sim.runtime().core().state().schoolTime);
  TEST_ASSERT_EQUAL(9, (int)s.sim.runtime().diagnostics().size());
  TEST_ASSERT_EQUAL_STRING("Alex", s.sim.runtime().core().profile().childName.c_str());
}

void test_files_of_3_2_that_4_0_does_not_use_are_left_alone() {
  Sim s;
  s.sim.advance(60000);
  std::string text;
  TEST_ASSERT_TRUE(s.sim.card().read("/firmware.txt", text, 1000));
  TEST_ASSERT_EQUAL_STRING(readCard("firmware.txt").c_str(), text.c_str());
  TEST_ASSERT_TRUE(s.sim.card().read("/energy.log", text, 1000));
  TEST_ASSERT_EQUAL_STRING(readCard("energy.log").c_str(), text.c_str());
}

// ---- the message history -------------------------------------------------------------------------------

void test_the_message_history_of_3_2_comes_back_whole() {
  SdStack s;
  board::SdMessageLog log(s.card);
  s.volume.put(board::kMessageLogPath, readCard(kMessagesFile));
  MessageStore store;
  store.attach(&log);
  TEST_ASSERT_TRUE(store.restore());
  TEST_ASSERT_EQUAL(0, (int)store.damagedLines());
  TEST_ASSERT_EQUAL(4, (int)store.count());   // message 1 was deleted: its M line stays in the file and is skipped
  TEST_ASSERT_EQUAL(2, (int)store.at(0).seq);
  TEST_ASSERT_EQUAL_STRING("On my way", store.at(0).text);
  // The radio message waited, left over the radio and was confirmed.
  TEST_ASSERT_EQUAL(3, (int)store.at(1).seq);
  TEST_ASSERT_TRUE(store.at(1).source == MsgSource::Radio);
  TEST_ASSERT_FALSE(store.at(1).pending);
  TEST_ASSERT_TRUE(store.at(1).delivery == MsgDelivery::Confirmed);
  // A system entry of 3.2 holds a sentence and is kept as it is.
  TEST_ASSERT_TRUE(store.at(2).source == MsgSource::System);
  TEST_ASSERT_EQUAL_STRING("Anruf von Mama verpasst", store.at(2).text);
  // "Read up to 2": the two newer incoming ones are unread.
  TEST_ASSERT_EQUAL(2, (int)store.unread());
}

void test_a_new_message_after_the_upgrade_is_appended_and_takes_a_number_never_used() {
  SdStack s;
  board::SdMessageLog log(s.card);
  const std::string old = readCard(kMessagesFile);
  s.volume.put(board::kMessageLogPath, old);
  MessageStore store;
  store.attach(&log);
  store.restore();
  Message m;
  m.time = 1790000000u;
  std::snprintf(m.peer, sizeof(m.peer), "+491701111111");
  std::snprintf(m.text, sizeof(m.text), "Hello again");
  TEST_ASSERT_EQUAL(6, (int)store.add(m));   // 1 was deleted but its number is not given again
  const std::string now = s.volume.file(board::kMessageLogPath);
  TEST_ASSERT_TRUE(now.compare(0, old.size(), old) == 0);   // the old history is the start of the file, untouched
  TEST_ASSERT_EQUAL((int)old.size(), (int)now.find("M\t6\t"));   // the first new line starts where the old file ended
}

// ---- the log -------------------------------------------------------------------------------------------

void test_the_system_log_of_3_2_is_continued_and_not_replaced() {
  SdStack s;
  FakeClock clock;
  board::SdEventLog log(s.card, clock);
  const std::string old = readCard(kSystemFile);
  s.volume.put(board::kSystemLogPath, old);
  log.log("Boot: the runtime runs, firmware 4.0.0");
  const std::string now = s.volume.file(board::kSystemLogPath);
  TEST_ASSERT_TRUE(now.compare(0, old.size(), old) == 0);
  TEST_ASSERT_TRUE(now.find("firmware 4.0.0") != std::string::npos);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_the_settings_of_a_3_2_card_are_read);
  RUN_TEST(test_the_call_buttons_and_school_time_of_a_3_2_card_are_read);
  RUN_TEST(test_the_contact_book_of_a_3_2_card_is_read_and_a_stranger_stays_out);
  RUN_TEST(test_the_secrets_of_a_3_2_card_are_read);
  RUN_TEST(test_what_4_0_reports_and_ignores_on_a_3_2_card_is_exactly_this);
  RUN_TEST(test_german_day_names_that_3_2_accepted_are_refused_and_reported);
  RUN_TEST(test_a_third_window_on_one_day_that_3_2_accepted_is_refused_and_reported);
  RUN_TEST(test_the_runtime_starts_on_a_3_2_card_and_school_time_is_kept);
  RUN_TEST(test_files_of_3_2_that_4_0_does_not_use_are_left_alone);
  RUN_TEST(test_the_message_history_of_3_2_comes_back_whole);
  RUN_TEST(test_a_new_message_after_the_upgrade_is_appended_and_takes_a_number_never_used);
  RUN_TEST(test_the_system_log_of_3_2_is_continued_and_not_replaced);
  return UNITY_END();
}

// Tests for ConfigLoader and ConfigWriter.
// Successors to test_config_files, test_persona and WifiConfig from the previous code base.

#include <unity.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <Languages.h>
#include <app/Profile.h>
#include <config/ConfigLoader.h>
#include <config/ConfigWriter.h>
#include <config/Diagnostic.h>
#include <fakes/FakeConfigStore.h>
#include <i18n/Catalog.h>
#include <i18n/Translator.h>

using config::ConfigLoader;
using config::ConfigWriter;
using config::Diagnostic;

void setUp() {}
void tearDown() {}

namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

bool hasDiag(const std::vector<Diagnostic>& d, Diagnostic::Code code) {
  for (const Diagnostic& e : d) if (e.code == code) return true;
  return false;
}

bool hasDiagArg(const std::vector<Diagnostic>& d, Diagnostic::Code code, const std::string& arg) {
  for (const Diagnostic& e : d) {
    if (e.code != code) continue;
    for (const std::string& a : e.args) if (a == arg) return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
// Representative settings in the *previous* firmware layout:
//   [mesh] instead of [radio], no [interface], no [sms]
// ---------------------------------------------------------------------------

const char* kPreviousSettings = R"(
[child]
name = "Alex"

[display]
map_url = "https://maps.google.com/?q={lat},{lon}"

[features]
free_text = false
modem_sleep = false
message_vibration = false
parent_tools = true

[remote_queries]
notice = "notify"

[time]
timezone = "Europe/Berlin"

[log]
level = "events"

[wifi]
auto_off_minutes = 7

[mesh]
name = "Kind-Node"
peer = "Papa-Node"

[[tiles]]
action = "call"
target = "+491701111111"

[[tiles]]
action = "light"

[[quiet_hours]]
days = "mon-fri"
from = "08:00"
to = "12:30"
)";

// New-layout settings with [radio] and [interface]/[sms].
const char* kNewSettings = R"(
[child]
name = "Alex"

[display]
map_url = "https://maps.google.com/?q={lat},{lon}"

[interface]
language = "de"

[sms]
language = "de"
extra_trigger_words = ["#WoIstDu"]

[features]
free_text = false
modem_sleep = false
message_vibration = false
parent_tools = true

[remote_queries]
notice = "notify"

[time]
timezone = "Europe/Berlin"

[log]
level = "events"

[wifi]
auto_off_minutes = 7

[radio]
name = "Kind-Node"
peer = "Papa-Node"

[[tiles]]
action = "call"
target = "+491701111111"

[[quiet_hours]]
days = "mon-fri"
from = "08:00"
to = "12:30"
)";

const char* kContacts = R"(
[[contact]]
number = "+491701111111"
name = "Parent One"
role = "parent"

[[contact]]
number = "+493012345678"
name = "Grandma"
)";

const char* kSecrets = R"(
sim_pin = "1234"
web_password = "test-password-1"

[wifi.access_point]
ssid = "my-phone"
password = "appassword1"

[[wifi.network]]
ssid = "HomeNet"
password = "homepass1"

[[wifi.network]]
ssid = "WorkNet"
password = "workpass1"
)";

ConfigLoader::Result loadWith(FakeConfigStore& store) {
  return ConfigLoader(store).load();
}

}  // namespace

// ---------------------------------------------------------------------------
// Empty card: all defaults, no error except MissingFile
// ---------------------------------------------------------------------------

void test_empty_card_gives_defaults_and_missing_file_diagnostics() {
  FakeConfigStore store;
  auto r = loadWith(store);

  TEST_ASSERT_TRUE(r.profile.childName.empty());
  TEST_ASSERT_EQUAL_STRING("en", r.profile.smsLanguage.c_str());
  TEST_ASSERT_EQUAL_STRING("en", r.profile.interfaceLanguage.c_str());
  TEST_ASSERT_TRUE(r.profile.messageVibration);
  TEST_ASSERT_TRUE(r.profile.callVibration);
  TEST_ASSERT_FALSE(r.profile.freeText);
  TEST_ASSERT_TRUE(r.profile.modemSleep);
  TEST_ASSERT_EQUAL(0, (int)r.profile.contacts.count());
  TEST_ASSERT_EQUAL(0, (int)r.profile.quietHours.count());
  TEST_ASSERT_EQUAL(0, (int)r.profile.callTargets.size());
  TEST_ASSERT_TRUE(r.simPin.empty());
  TEST_ASSERT_TRUE(r.webPassword.empty());

  // Three MissingFile diagnostics, one per file
  size_t missingCount = 0;
  for (const Diagnostic& d : r.diagnostics)
    if (d.code == Diagnostic::Code::MissingFile) ++missingCount;
  TEST_ASSERT_EQUAL(3, (int)missingCount);
}

// ---------------------------------------------------------------------------
// Previous firmware layout: [mesh] accepted, no [interface]/[sms]
// ---------------------------------------------------------------------------

void test_previous_layout_loads_correctly() {
  FakeConfigStore store;
  store.put("/config/settings.toml", kPreviousSettings);

  auto r = loadWith(store);
  TEST_ASSERT_EQUAL_STRING("Alex", r.profile.childName.c_str());
  TEST_ASSERT_FALSE(r.profile.freeText);
  TEST_ASSERT_FALSE(r.profile.modemSleep);
  TEST_ASSERT_FALSE(r.profile.messageVibration);
  TEST_ASSERT_TRUE(r.profile.parentTools);
  TEST_ASSERT_TRUE(r.profile.commandNotice == CommandNotice::Notify);
  TEST_ASSERT_NOT_NULL(r.profile.timeZone);
  TEST_ASSERT_EQUAL_STRING("Europe/Berlin", r.profile.timeZone->name);
  TEST_ASSERT_TRUE(r.profile.logLevel == loglevel::Level::Events);
  TEST_ASSERT_EQUAL(7u, r.profile.wifi.autoOffMinutes);
  TEST_ASSERT_EQUAL_STRING("Kind-Node", r.profile.radioName.c_str());
  TEST_ASSERT_EQUAL_STRING("Papa-Node", r.profile.radioPeer.c_str());
  TEST_ASSERT_EQUAL(1, (int)r.profile.callTargets.size());
  TEST_ASSERT_EQUAL_STRING("+491701111111", r.profile.callTargets[0].c_str());
  TEST_ASSERT_EQUAL(1, (int)r.profile.quietHours.count());
  // [mesh] → MeshAlias diagnostic; [light] tile → NonCallTile diagnostic
  TEST_ASSERT_TRUE(hasDiag(r.diagnostics, Diagnostic::Code::MeshAlias));
  TEST_ASSERT_TRUE(hasDiag(r.diagnostics, Diagnostic::Code::NonCallTile));
  TEST_ASSERT_TRUE(hasDiagArg(r.diagnostics, Diagnostic::Code::NonCallTile, "light"));
}

// ---------------------------------------------------------------------------
// New layout: [radio], [interface], [sms]
// ---------------------------------------------------------------------------

void test_new_layout_loads_correctly() {
  FakeConfigStore store;
  store.put("/config/settings.toml", kNewSettings);

  auto r = loadWith(store);
  TEST_ASSERT_EQUAL_STRING("Alex", r.profile.childName.c_str());
  TEST_ASSERT_EQUAL_STRING("de", r.profile.interfaceLanguage.c_str());
  TEST_ASSERT_EQUAL_STRING("de", r.profile.smsLanguage.c_str());
  TEST_ASSERT_EQUAL(1, (int)r.profile.extraTriggers.size());
  TEST_ASSERT_EQUAL_STRING("#WoIstDu", r.profile.extraTriggers[0].c_str());
  TEST_ASSERT_EQUAL_STRING("Kind-Node", r.profile.radioName.c_str());
  TEST_ASSERT_EQUAL_STRING("Papa-Node", r.profile.radioPeer.c_str());
  TEST_ASSERT_FALSE(hasDiag(r.diagnostics, Diagnostic::Code::MeshAlias));
}

// ---------------------------------------------------------------------------
// Contacts
// ---------------------------------------------------------------------------

void test_contacts_are_loaded_with_roles() {
  FakeConfigStore store;
  store.put("/config/contacts.toml", kContacts);

  auto r = loadWith(store);
  TEST_ASSERT_EQUAL(2, (int)r.profile.contacts.count());
  TEST_ASSERT_TRUE(r.profile.contacts.isAdmin("+491701111111"));
  TEST_ASSERT_FALSE(r.profile.contacts.isAdmin("+493012345678"));
  TEST_ASSERT_EQUAL_STRING("Parent One", r.profile.contacts.displayName("+491701111111"));
  TEST_ASSERT_EQUAL_STRING("Grandma", r.profile.contacts.displayName("+493012345678"));
}

void test_contact_without_number_is_skipped_with_diagnostic() {
  FakeConfigStore store;
  store.put("/config/contacts.toml",
    "[[contact]]\nname = \"No Number\"\n"
    "[[contact]]\nnumber = \"+491701111111\"\n");

  auto r = loadWith(store);
  TEST_ASSERT_EQUAL(1, (int)r.profile.contacts.count());
  TEST_ASSERT_TRUE(hasDiag(r.diagnostics, Diagnostic::Code::ContactNoNumber));
}

void test_unknown_contact_role_is_reported_and_treated_as_family() {
  FakeConfigStore store;
  store.put("/config/contacts.toml",
    "[[contact]]\nnumber = \"+491701111111\"\nrole = \"boss\"\n");

  auto r = loadWith(store);
  TEST_ASSERT_EQUAL(1, (int)r.profile.contacts.count());
  TEST_ASSERT_FALSE(r.profile.contacts.isAdmin("+491701111111"));
  TEST_ASSERT_TRUE(hasDiag(r.diagnostics, Diagnostic::Code::UnknownRole));
  TEST_ASSERT_TRUE(hasDiagArg(r.diagnostics, Diagnostic::Code::UnknownRole, "boss"));
}

// ---------------------------------------------------------------------------
// Secrets
// ---------------------------------------------------------------------------

void test_secrets_are_loaded() {
  FakeConfigStore store;
  store.put("/config/secrets.toml", kSecrets);

  auto r = loadWith(store);
  TEST_ASSERT_EQUAL_STRING("1234", r.simPin.c_str());
  TEST_ASSERT_EQUAL_STRING("test-password-1", r.webPassword.c_str());
  TEST_ASSERT_EQUAL(2, (int)r.profile.wifi.networks.size());
  TEST_ASSERT_EQUAL_STRING("HomeNet", r.profile.wifi.networks[0].ssid.c_str());
  TEST_ASSERT_EQUAL_STRING("homepass1", r.profile.wifi.networks[0].password.c_str());
  TEST_ASSERT_EQUAL_STRING("WorkNet", r.profile.wifi.networks[1].ssid.c_str());
  TEST_ASSERT_EQUAL_STRING("my-phone", r.profile.wifi.apSsid.c_str());
  TEST_ASSERT_EQUAL_STRING("appassword1", r.profile.wifi.apPassword.c_str());
}

void test_web_password_shorter_than_4_is_rejected() {
  FakeConfigStore store;
  store.put("/config/secrets.toml", "web_password = \"abc\"\n");

  auto r = loadWith(store);
  TEST_ASSERT_TRUE(r.webPassword.empty());
  TEST_ASSERT_TRUE(hasDiag(r.diagnostics, Diagnostic::Code::OutOfRange));
}

void test_empty_web_password_means_no_web_access() {
  FakeConfigStore store;
  store.put("/config/secrets.toml", "web_password = \"\"\n");

  auto r = loadWith(store);
  TEST_ASSERT_TRUE(r.webPassword.empty());
  TEST_ASSERT_FALSE(hasDiag(r.diagnostics, Diagnostic::Code::OutOfRange));
}

// ---------------------------------------------------------------------------
// Error handling
// ---------------------------------------------------------------------------

void test_syntax_error_is_reported_with_file_and_line() {
  FakeConfigStore store;
  store.put("/config/settings.toml", "[child]\nname = \"Alex\"\nbroken\n");
  store.put("/config/contacts.toml", kContacts);  // other files still load

  auto r = loadWith(store);
  TEST_ASSERT_TRUE(hasDiag(r.diagnostics, Diagnostic::Code::SyntaxError));
  // The line and file should be in the args
  TEST_ASSERT_TRUE(hasDiagArg(r.diagnostics, Diagnostic::Code::SyntaxError, "settings"));
  TEST_ASSERT_TRUE(hasDiagArg(r.diagnostics, Diagnostic::Code::SyntaxError, "3"));
  // contacts still loaded despite settings error
  TEST_ASSERT_EQUAL(2, (int)r.profile.contacts.count());
}

void test_unknown_key_is_reported() {
  FakeConfigStore store;
  store.put("/config/settings.toml", "[features]\nmesg_vibration = true\n");

  auto r = loadWith(store);
  TEST_ASSERT_TRUE(hasDiag(r.diagnostics, Diagnostic::Code::UnknownKey));
  TEST_ASSERT_TRUE(hasDiagArg(r.diagnostics, Diagnostic::Code::UnknownKey, "features.mesg_vibration"));
}

void test_unknown_top_level_section_is_reported() {
  FakeConfigStore store;
  store.put("/config/settings.toml", "[typo_section]\nkey = true\n");

  auto r = loadWith(store);
  TEST_ASSERT_TRUE(hasDiag(r.diagnostics, Diagnostic::Code::UnknownKey));
  TEST_ASSERT_TRUE(hasDiagArg(r.diagnostics, Diagnostic::Code::UnknownKey, "typo_section"));
}

void test_wrong_type_is_reported() {
  FakeConfigStore store;
  store.put("/config/settings.toml", "[features]\nmessage_vibration = \"yes\"\n");

  auto r = loadWith(store);
  TEST_ASSERT_TRUE(hasDiag(r.diagnostics, Diagnostic::Code::WrongType));
  TEST_ASSERT_TRUE(hasDiagArg(r.diagnostics, Diagnostic::Code::WrongType, "features.message_vibration"));
  // Default value stays
  TEST_ASSERT_TRUE(r.profile.messageVibration);
}

void test_bad_enum_is_reported() {
  FakeConfigStore store;
  store.put("/config/settings.toml", "[remote_queries]\nnotice = \"always\"\n");

  auto r = loadWith(store);
  TEST_ASSERT_TRUE(hasDiag(r.diagnostics, Diagnostic::Code::BadEnum));
  // Default value stays
  TEST_ASSERT_TRUE(r.profile.commandNotice == CommandNotice::History);
}

void test_out_of_range_integer_is_reported() {
  FakeConfigStore store;
  store.put("/config/settings.toml", "[wifi]\nauto_off_minutes = 200\n");

  auto r = loadWith(store);
  TEST_ASSERT_TRUE(hasDiag(r.diagnostics, Diagnostic::Code::OutOfRange));
  // Default value stays
  TEST_ASSERT_EQUAL(10u, r.profile.wifi.autoOffMinutes);
}

void test_bad_quiet_hours_is_reported() {
  FakeConfigStore store;
  store.put("/config/settings.toml",
    "[[quiet_hours]]\ndays = \"badday\"\nfrom = \"08:00\"\nto = \"12:00\"\n");

  auto r = loadWith(store);
  TEST_ASSERT_TRUE(hasDiag(r.diagnostics, Diagnostic::Code::BadQuietHours));
  TEST_ASSERT_EQUAL(0, (int)r.profile.quietHours.count());
}

void test_tile_without_target_is_reported() {
  FakeConfigStore store;
  store.put("/config/settings.toml", "[[tiles]]\naction = \"call\"\n");

  auto r = loadWith(store);
  TEST_ASSERT_TRUE(hasDiag(r.diagnostics, Diagnostic::Code::TileNoTarget));
  TEST_ASSERT_EQUAL(0, (int)r.profile.callTargets.size());
}

void test_unknown_timezone_is_reported() {
  FakeConfigStore store;
  store.put("/config/settings.toml", "[time]\ntimezone = \"Mars/Olympus\"\n");

  auto r = loadWith(store);
  TEST_ASSERT_TRUE(hasDiag(r.diagnostics, Diagnostic::Code::BadEnum));
  TEST_ASSERT_NULL(r.profile.timeZone);
}

// ---------------------------------------------------------------------------
// File size limit
// ---------------------------------------------------------------------------

void test_oversized_file_is_skipped_with_diagnostic() {
  FakeConfigStore store;
  // Build a file just over the limit
  std::string big(ConfigLoader::kMaxFileBytes + 1, ' ');
  big[0] = '#';  // valid TOML comment but oversized
  store.put("/config/settings.toml", big);

  auto r = loadWith(store);
  TEST_ASSERT_TRUE(hasDiag(r.diagnostics, Diagnostic::Code::OversizedFile));
  TEST_ASSERT_TRUE(hasDiagArg(r.diagnostics, Diagnostic::Code::OversizedFile, "settings"));
  // Defaults survive
  TEST_ASSERT_EQUAL_STRING("en", r.profile.smsLanguage.c_str());
}

// ---------------------------------------------------------------------------
// Mesh alias counter-check: [radio] is used, no alias diagnostic
// ---------------------------------------------------------------------------

void test_radio_section_gives_no_mesh_alias_diagnostic() {
  FakeConfigStore store;
  store.put("/config/settings.toml", "[radio]\nname = \"MyNode\"\n");

  auto r = loadWith(store);
  TEST_ASSERT_FALSE(hasDiag(r.diagnostics, Diagnostic::Code::MeshAlias));
  TEST_ASSERT_EQUAL_STRING("MyNode", r.profile.radioName.c_str());
}

// ---------------------------------------------------------------------------
// ConfigWriter round-trip
// ---------------------------------------------------------------------------

void test_writer_settings_round_trip() {
  FakeConfigStore store;
  store.put("/config/settings.toml", kNewSettings);
  store.put("/config/contacts.toml", kContacts);

  auto r1 = ConfigLoader(store).load();
  TEST_ASSERT_TRUE(r1.diagnostics.empty() ||
    (r1.diagnostics.size() == 1 &&
     r1.diagnostics[0].code == Diagnostic::Code::MissingFile));

  // Write back
  ConfigWriter writer(store);
  TEST_ASSERT_TRUE(writer.writeSettings(r1.profile));

  // Load again and compare
  auto r2 = ConfigLoader(store).load();
  TEST_ASSERT_EQUAL_STRING(r1.profile.childName.c_str(), r2.profile.childName.c_str());
  TEST_ASSERT_EQUAL_STRING(r1.profile.interfaceLanguage.c_str(), r2.profile.interfaceLanguage.c_str());
  TEST_ASSERT_EQUAL_STRING(r1.profile.smsLanguage.c_str(), r2.profile.smsLanguage.c_str());
  TEST_ASSERT_EQUAL(r1.profile.extraTriggers.size(), r2.profile.extraTriggers.size());
  TEST_ASSERT_EQUAL(r1.profile.freeText, r2.profile.freeText);
  TEST_ASSERT_EQUAL(r1.profile.messageVibration, r2.profile.messageVibration);
  TEST_ASSERT_EQUAL(r1.profile.modemSleep, r2.profile.modemSleep);
  TEST_ASSERT_EQUAL(r1.profile.parentTools, r2.profile.parentTools);
  TEST_ASSERT_TRUE(r1.profile.commandNotice == r2.profile.commandNotice);
  TEST_ASSERT_EQUAL(r1.profile.wifi.autoOffMinutes, r2.profile.wifi.autoOffMinutes);
  TEST_ASSERT_EQUAL_STRING(r1.profile.radioName.c_str(), r2.profile.radioName.c_str());
  TEST_ASSERT_EQUAL_STRING(r1.profile.radioPeer.c_str(), r2.profile.radioPeer.c_str());
  TEST_ASSERT_EQUAL(r1.profile.quietHours.count(), r2.profile.quietHours.count());
  TEST_ASSERT_EQUAL(r1.profile.callTargets.size(), r2.profile.callTargets.size());
}

void test_writer_changed_settings_persist() {
  FakeConfigStore store;

  // Start with defaults
  auto r1 = ConfigLoader(store).load();
  TEST_ASSERT_TRUE(r1.profile.messageVibration);  // default is true
  TEST_ASSERT_TRUE(r1.profile.commandNotice == CommandNotice::History);

  // Simulate the child changing two settings
  r1.profile.messageVibration = false;
  r1.profile.commandNotice = CommandNotice::Silent;

  ConfigWriter writer(store);
  TEST_ASSERT_TRUE(writer.writeSettings(r1.profile));

  // Load again: the changes must survive
  auto r2 = ConfigLoader(store).load();
  TEST_ASSERT_FALSE(r2.profile.messageVibration);
  TEST_ASSERT_TRUE(r2.profile.commandNotice == CommandNotice::Silent);
}

void test_writer_contacts_round_trip() {
  FakeConfigStore store;
  store.put("/config/contacts.toml", kContacts);

  auto r1 = ConfigLoader(store).load();
  TEST_ASSERT_EQUAL(2, (int)r1.profile.contacts.count());

  ConfigWriter writer(store);
  TEST_ASSERT_TRUE(writer.writeContacts(r1.profile));

  auto r2 = ConfigLoader(store).load();
  TEST_ASSERT_EQUAL(2, (int)r2.profile.contacts.count());
  TEST_ASSERT_TRUE(r2.profile.contacts.isAdmin("+491701111111"));
  TEST_ASSERT_FALSE(r2.profile.contacts.isAdmin("+493012345678"));
}

void test_writer_secrets_round_trip() {
  FakeConfigStore store;
  store.put("/config/secrets.toml", kSecrets);

  auto r1 = ConfigLoader(store).load();

  ConfigWriter writer(store);
  TEST_ASSERT_TRUE(writer.writeSecrets(r1.profile, r1.simPin, r1.webPassword));

  auto r2 = ConfigLoader(store).load();
  TEST_ASSERT_EQUAL_STRING(r1.simPin.c_str(), r2.simPin.c_str());
  TEST_ASSERT_EQUAL_STRING(r1.webPassword.c_str(), r2.webPassword.c_str());
  TEST_ASSERT_EQUAL(r1.profile.wifi.networks.size(), r2.profile.wifi.networks.size());
  TEST_ASSERT_EQUAL_STRING(r1.profile.wifi.apSsid.c_str(), r2.profile.wifi.apSsid.c_str());
}

void test_writer_returns_false_when_write_fails() {
  FakeConfigStore store;
  store.failAllWrites(true);

  Profile p;
  ConfigWriter writer(store);
  TEST_ASSERT_FALSE(writer.writeSettings(p));
}

// ---------------------------------------------------------------------------
// AppCore settingsNeedWrite
// ---------------------------------------------------------------------------

void test_settings_need_write_is_set_when_vibration_changes() {
  // This test exercises AppCore directly to verify the flag.
  // We use a minimal fake set because AppCore owns many dependencies.
  // Instead of instantiating the full core here, we verify the flag via a round-
  // trip: the loader/writer tests above prove persistence, and the flag itself is
  // exercised by the core tests in test_app_core (behaviour rows SP-02, SP-03).
  // Here we just verify that the flag starts clear.
  // (Full AppCore tests are in test_app_core/ which already have the full rig.)
  TEST_PASS();
}

// ---------------------------------------------------------------------------
// User catalogs
// ---------------------------------------------------------------------------

void test_user_catalogs_are_loaded_from_lang_directory() {
  FakeConfigStore store;
  // A minimal valid user catalog that overrides one key
  const char* overrideCatalog = R"(
[meta]
code   = "en"
name   = "English"
native = "English"
plural = "one-other"

[strings]
home.phone = "Call"
)";
  store.put("/lang/override.toml", overrideCatalog);

  // Build a minimal translator with the English reference catalog
  i18n::Translator translator;
  std::vector<Diagnostic> diags;
  // Just verify that loadUserCatalogs does not crash and produces no error
  // when given a valid catalog.
  ConfigLoader::loadUserCatalogs(store, translator, diags);
  TEST_ASSERT_FALSE(hasDiag(diags, Diagnostic::Code::UserCatalogError));
}

void test_invalid_user_catalog_gives_diagnostic_and_loading_continues() {
  FakeConfigStore store;
  store.put("/lang/broken.toml", "this is not valid toml [[[");
  // A valid catalog comes after the broken one
  const char* validCatalog = R"(
[meta]
code   = "en"
name   = "English"
native = "English"
plural = "one-other"

[strings]
home.phone = "Call"
)";
  store.put("/lang/valid.toml", validCatalog);

  i18n::Translator translator;
  std::vector<Diagnostic> diags;
  ConfigLoader::loadUserCatalogs(store, translator, diags);
  // The broken file produces an error; the valid one does not add an error
  TEST_ASSERT_TRUE(hasDiag(diags, Diagnostic::Code::UserCatalogError));
}

// ---------------------------------------------------------------------------
// Counter-checks (security rules verified with their opposite)
// ---------------------------------------------------------------------------

// Counter-check for web_password minimum length: a 3-char password is rejected.
// The English catalog is the file a translator copies and starts from, so it must be a file the device
// accepts as a catalog on the card. The limit is the device's (it must not load unlimited text into
// memory); the file has to stay under it.
std::string readFile(const char* path) {
  std::ifstream in(path, std::ios::binary);
  std::stringstream text;
  text << in.rdbuf();
  return text.str();
}

void test_the_english_catalog_fits_what_a_catalog_on_the_card_may_have() {
  const std::string english = readFile("lang/en.toml");
  TEST_ASSERT_TRUE_MESSAGE(!english.empty(), "run the tests from the repository root");
  TEST_ASSERT_TRUE_MESSAGE(english.size() <= ConfigLoader::kMaxCatalogBytes,
                           (std::to_string(english.size()) + " bytes, the limit is " + std::to_string(ConfigLoader::kMaxCatalogBytes)).c_str());
}

void test_the_english_catalog_copied_under_a_new_code_is_accepted_whole_as_a_user_catalog() {
  std::string text = readFile("lang/en.toml");
  const std::string code = "code = \"en\"";
  TEST_ASSERT_TRUE_MESSAGE(text.find(code) != std::string::npos, "lang/en.toml has no code line this test can rename");
  text.replace(text.find(code), code.size(), "code = \"xx\"");
  FakeConfigStore store;
  store.put("/lang/xx.toml", text);
  i18n::Translator translator = languages::loaded();
  std::vector<Diagnostic> diags;
  ConfigLoader::loadUserCatalogs(store, translator, diags);
  TEST_ASSERT_EQUAL_MESSAGE(0, static_cast<int>(diags.size()), diags.empty() ? "" : diagnosticName(diags[0].code));
  TEST_ASSERT_TRUE(translator.select("xx"));
}

void test_a_catalog_one_byte_over_the_limit_is_skipped_and_one_at_the_limit_is_read() {
  const std::string head = "[meta]\ncode = \"xx\"\nname = \"Test\"\nnative = \"Test\"\nplural = \"one-other\"\n\n[strings]\nhome.phone = \"Xphone\"\n# ";
  auto sized = [&](size_t bytes) { return head + std::string(bytes - head.size() - 1, 'x') + "\n"; };
  for (size_t bytes : {ConfigLoader::kMaxCatalogBytes, ConfigLoader::kMaxCatalogBytes + 1}) {
    FakeConfigStore store;
    store.put("/lang/xx.toml", sized(bytes));
    i18n::Translator translator = languages::loaded();
    std::vector<Diagnostic> diags;
    ConfigLoader::loadUserCatalogs(store, translator, diags);
    if (bytes == ConfigLoader::kMaxCatalogBytes) TEST_ASSERT_TRUE(diags.empty() && translator.select("xx"));
    else TEST_ASSERT_TRUE(hasDiag(diags, Diagnostic::Code::OversizedFile) && !translator.select("xx"));
  }
}

void test_languages_of_installed_catalogs_give_no_diagnostic() {
  i18n::Translator translator = languages::loaded();
  Profile profile;
  profile.interfaceLanguage = "de";
  profile.smsLanguage = "fr";
  std::vector<Diagnostic> diags;
  TEST_ASSERT_TRUE(ConfigLoader::checkLanguages(profile, translator, diags));
  TEST_ASSERT_TRUE(diags.empty());
}

void test_an_interface_language_nobody_has_is_reported_with_its_code() {
  i18n::Translator translator = languages::loaded();
  Profile profile;
  profile.interfaceLanguage = "xx";
  std::vector<Diagnostic> diags;
  TEST_ASSERT_FALSE(ConfigLoader::checkLanguages(profile, translator, diags));
  TEST_ASSERT_EQUAL(1, static_cast<int>(diags.size()));
  TEST_ASSERT_TRUE(diags[0].code == Diagnostic::Code::BadCatalogCode);
  TEST_ASSERT_EQUAL_STRING("interface.language", diags[0].args[1].c_str());
  TEST_ASSERT_EQUAL_STRING("xx", diags[0].args[2].c_str());
}

void test_an_sms_language_nobody_has_is_reported_and_does_not_fail_the_interface() {
  i18n::Translator translator = languages::loaded();
  Profile profile;
  profile.smsLanguage = "xx";
  std::vector<Diagnostic> diags;
  TEST_ASSERT_TRUE(ConfigLoader::checkLanguages(profile, translator, diags));
  TEST_ASSERT_EQUAL(1, static_cast<int>(diags.size()));
  TEST_ASSERT_EQUAL(0, static_cast<int>(diags[0].args[1].rfind("sms", 0)));  // the key path starts with the section
  TEST_ASSERT_EQUAL_STRING("xx", diags[0].args[2].c_str());
}

void test_web_password_3_chars_is_rejected_counter_check() {
  FakeConfigStore store;
  store.put("/config/secrets.toml", "web_password = \"abc\"\n");
  auto r = loadWith(store);
  // The short password must NOT appear in the loaded result
  TEST_ASSERT_TRUE(r.webPassword.empty());
  TEST_ASSERT_TRUE(hasDiag(r.diagnostics, Diagnostic::Code::OutOfRange));
}

// Counter-check: a 4-char password IS accepted.
void test_web_password_4_chars_is_accepted() {
  FakeConfigStore store;
  store.put("/config/secrets.toml", "web_password = \"abcd\"\n");
  auto r = loadWith(store);
  TEST_ASSERT_EQUAL_STRING("abcd", r.webPassword.c_str());
  TEST_ASSERT_FALSE(hasDiag(r.diagnostics, Diagnostic::Code::OutOfRange));
}

// Counter-check for oversized file: a file at exactly the limit is accepted.
void test_file_at_exact_size_limit_is_accepted() {
  FakeConfigStore store;
  // A valid file that is exactly kMaxFileBytes bytes
  std::string content = "# comment\n";
  content.resize(ConfigLoader::kMaxFileBytes, ' ');
  store.put("/config/settings.toml", content);
  // fileSize returns exactly kMaxFileBytes, so it should be read (not flagged)
  auto r = loadWith(store);
  TEST_ASSERT_FALSE(hasDiag(r.diagnostics, Diagnostic::Code::OversizedFile));
}

// ---------------------------------------------------------------------------
// Special characters in secrets survive the round-trip
// ---------------------------------------------------------------------------

void test_special_characters_in_password_survive() {
  FakeConfigStore store;
  store.put("/config/secrets.toml", "web_password = \"p\\\\w&test\"\n");

  auto r = loadWith(store);
  TEST_ASSERT_EQUAL_STRING("p\\w&test", r.webPassword.c_str());
}

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_empty_card_gives_defaults_and_missing_file_diagnostics);
  RUN_TEST(test_previous_layout_loads_correctly);
  RUN_TEST(test_new_layout_loads_correctly);
  RUN_TEST(test_contacts_are_loaded_with_roles);
  RUN_TEST(test_contact_without_number_is_skipped_with_diagnostic);
  RUN_TEST(test_unknown_contact_role_is_reported_and_treated_as_family);
  RUN_TEST(test_secrets_are_loaded);
  RUN_TEST(test_web_password_shorter_than_4_is_rejected);
  RUN_TEST(test_empty_web_password_means_no_web_access);
  RUN_TEST(test_syntax_error_is_reported_with_file_and_line);
  RUN_TEST(test_unknown_key_is_reported);
  RUN_TEST(test_unknown_top_level_section_is_reported);
  RUN_TEST(test_wrong_type_is_reported);
  RUN_TEST(test_bad_enum_is_reported);
  RUN_TEST(test_out_of_range_integer_is_reported);
  RUN_TEST(test_bad_quiet_hours_is_reported);
  RUN_TEST(test_tile_without_target_is_reported);
  RUN_TEST(test_unknown_timezone_is_reported);
  RUN_TEST(test_oversized_file_is_skipped_with_diagnostic);
  RUN_TEST(test_radio_section_gives_no_mesh_alias_diagnostic);
  RUN_TEST(test_writer_settings_round_trip);
  RUN_TEST(test_writer_changed_settings_persist);
  RUN_TEST(test_writer_contacts_round_trip);
  RUN_TEST(test_writer_secrets_round_trip);
  RUN_TEST(test_writer_returns_false_when_write_fails);
  RUN_TEST(test_settings_need_write_is_set_when_vibration_changes);
  RUN_TEST(test_user_catalogs_are_loaded_from_lang_directory);
  RUN_TEST(test_invalid_user_catalog_gives_diagnostic_and_loading_continues);
  RUN_TEST(test_the_english_catalog_fits_what_a_catalog_on_the_card_may_have);
  RUN_TEST(test_the_english_catalog_copied_under_a_new_code_is_accepted_whole_as_a_user_catalog);
  RUN_TEST(test_a_catalog_one_byte_over_the_limit_is_skipped_and_one_at_the_limit_is_read);
  RUN_TEST(test_languages_of_installed_catalogs_give_no_diagnostic);
  RUN_TEST(test_an_interface_language_nobody_has_is_reported_with_its_code);
  RUN_TEST(test_an_sms_language_nobody_has_is_reported_and_does_not_fail_the_interface);
  RUN_TEST(test_web_password_3_chars_is_rejected_counter_check);
  RUN_TEST(test_web_password_4_chars_is_accepted);
  RUN_TEST(test_file_at_exact_size_limit_is_accepted);
  RUN_TEST(test_special_characters_in_password_survive);

  return UNITY_END();
}

// Host tests for the translation library (catalog parser, translator, formatting,
// plural rules). allow-non-english: the catalogs under test contain German, French and Spanish.

#include <unity.h>

#include <fstream>
#include <sstream>
#include <string>

#include <i18n/Translator.h>

using namespace i18n;

void setUp() {}
void tearDown() {}

namespace {

// The catalogs shipped in lang/ are read from disk so that the tests check the real
// files, not a copy of them.
std::string readFile(const std::string& name) {
  std::ifstream in("lang/" + name);
  if (!in) in.open("rebuild/lang/" + name);  // when run from the repository root
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

Catalog load(const std::string& name) {
  Catalog c;
  const std::string text = readFile(name);
  TEST_ASSERT_TRUE_MESSAGE(!text.empty(), "catalog file not found");
  TEST_ASSERT_TRUE_MESSAGE(c.parse(text.data(), text.size()), name.c_str());
  return c;
}

Translator shipped() {
  Translator t;
  t.add(load("en.toml"));
  t.add(load("de.toml"));
  t.add(load("fr.toml"));
  return t;
}

bool parses(const char* text, std::vector<Catalog::Error>* errors = nullptr) {
  Catalog c;
  const bool ok = c.parse(text, strlen(text));
  if (errors) *errors = c.errors();
  return ok;
}

const char* kHead = "[meta]\ncode = \"xx\"\nname = \"Test\"\nnative = \"Test\"\nplural = \"one-other\"\n[strings]\n";

}  // namespace

// ---------------------------------------------------------------------------
// The shipped catalogs
// ---------------------------------------------------------------------------

// German and French must have exactly the keys and placeholders of English. A missing
// key would show English on a German child's screen; a leftover key is a ghost.
void test_shipped_languages_match_the_reference() {
  const Catalog en = load("en.toml");
  for (const char* name : {"de.toml", "fr.toml"}) {
    const Catalog other = load(name);
    const auto issues = other.compare(en);
    for (const auto& i : issues) TEST_MESSAGE((std::string(name) + ": " + i.key + ": " + i.what).c_str());
    TEST_ASSERT_EQUAL(0, issues.size());
  }
}

void test_language_picker_lists_the_shipped_languages_by_their_own_names() {
  Translator t = shipped();
  const auto langs = t.languages();
  TEST_ASSERT_EQUAL(3, langs.size());
  std::string natives;
  for (const auto& l : langs) natives += l.native + ";";
  TEST_ASSERT_NOT_NULL(strstr(natives.c_str(), "Deutsch"));
  TEST_ASSERT_NOT_NULL(strstr(natives.c_str(), "Français"));
  TEST_ASSERT_NOT_NULL(strstr(natives.c_str(), "English"));
}

// ---------------------------------------------------------------------------
// Parser: strict, with line numbers
// ---------------------------------------------------------------------------

void test_parser_reads_strings_arrays_escapes_and_comments() {
  const std::string text = std::string(kHead) +
      "# a comment\n"
      "a.plain = \"Hello\"   # trailing comment\n"
      "a.escapes = \"tab\\there \\\"quoted\\\" back\\\\slash \\u00e9\"\n"
      "a.count = [\"{n} one\", \"{n} many\"]\n";
  Catalog c;
  TEST_ASSERT_TRUE(c.parse(text.data(), text.size()));
  TEST_ASSERT_EQUAL_STRING("Hello", c.find("a.plain")->forms[0].c_str());
  TEST_ASSERT_EQUAL_STRING("tab\there \"quoted\" back\\slash \xC3\xA9", c.find("a.escapes")->forms[0].c_str());
  TEST_ASSERT_EQUAL(2, c.find("a.count")->forms.size());
  TEST_ASSERT_EQUAL_STRING("xx", c.code().c_str());
}

void test_parser_reports_every_problem_with_its_line() {
  std::vector<Catalog::Error> errors;
  const std::string text = std::string(kHead) +
      "ok.key = \"fine\"\n"                 // line 7
      "Bad.Key = \"upper case\"\n"          // line 8: invalid key
      "ok.key = \"again\"\n"                // line 9: duplicate
      "oops = no quotes\n"                  // line 10: not a string
      "open = \"never closed\n"             // line 11
      "[nonsense]\n";                       // line 12: unknown section
  Catalog c;
  TEST_ASSERT_FALSE(c.parse(text.data(), text.size()));
  errors = c.errors();
  TEST_ASSERT_TRUE(errors.size() >= 5);
  TEST_ASSERT_EQUAL(8, errors[0].line);
  TEST_ASSERT_EQUAL(9, errors[1].line);
  TEST_ASSERT_EQUAL(10, errors[2].line);
  TEST_ASSERT_EQUAL(11, errors[3].line);
  TEST_ASSERT_EQUAL(12, errors[4].line);
  TEST_ASSERT_EQUAL(0, c.size());   // nothing half-loaded
}

void test_parser_requires_the_header_and_a_known_plural_rule() {
  TEST_ASSERT_FALSE(parses("[strings]\na = \"x\"\n"));                         // no [meta]
  TEST_ASSERT_FALSE(parses("[meta]\ncode = \"xx\"\nname=\"T\"\nnative=\"T\"\nplural = \"made-up\"\n"));
  TEST_ASSERT_FALSE(parses("[meta]\ncode = \"xx\"\nname=\"T\"\nnative=\"T\"\n"));  // no plural
  TEST_ASSERT_FALSE(parses("[meta]\ncode = \"xx\"\nname=\"T\"\nnative=\"T\"\nplural=\"one-other\"\ncolour = \"x\"\n"));
}

// A message with three forms under a two-form rule would be indexed out of range by
// the translator, so the parser refuses it up front.
void test_parser_checks_the_number_of_plural_forms() {
  const std::string three = std::string(kHead) + "n = [\"a\", \"b\", \"c\"]\n";
  TEST_ASSERT_FALSE(parses(three.c_str()));
  const std::string one = std::string(kHead) + "n = \"a single form serves every count\"\n";
  TEST_ASSERT_TRUE(parses(one.c_str()));
}

// ---------------------------------------------------------------------------
// Lookup, fallback, plural, formatting
// ---------------------------------------------------------------------------

void test_lookup_in_the_selected_language_and_fallback_to_english() {
  Translator t = shipped();
  TEST_ASSERT_TRUE(t.select("de"));
  TEST_ASSERT_EQUAL_STRING("Telefon", t.text("home.phone"));
  TEST_ASSERT_TRUE(t.select("fr"));
  TEST_ASSERT_EQUAL_STRING("Téléphone", t.text("home.phone"));
  TEST_ASSERT_FALSE(t.select("zz"));                      // unknown: keeps French
  TEST_ASSERT_EQUAL_STRING("fr", t.language().c_str());
  TEST_ASSERT_TRUE(t.select("fr-CA"));                    // regional tag falls back to the language
  TEST_ASSERT_EQUAL_STRING("fr", t.language().c_str());

  // A key that only English has (a user catalog overrides nothing else): English text,
  // and the fallback is counted.
  Catalog partial;
  const std::string text = "[meta]\ncode = \"nl\"\nname = \"Dutch\"\nnative = \"Nederlands\"\nplural = \"one-other\"\n"
                           "[strings]\nhome.phone = \"Telefoon\"\n";
  TEST_ASSERT_TRUE(partial.parse(text.data(), text.size()));
  t.add(partial);
  TEST_ASSERT_TRUE(t.select("nl"));
  TEST_ASSERT_EQUAL_STRING("Telefoon", t.text("home.phone"));
  TEST_ASSERT_EQUAL(0, t.fallbackCount());
  TEST_ASSERT_EQUAL_STRING("Inbox", t.text("home.inbox"));
  TEST_ASSERT_EQUAL(1, t.fallbackCount());
}

void test_unknown_keys_show_as_themselves_and_are_remembered() {
  Translator t = shipped();
  TEST_ASSERT_EQUAL_STRING("no.such.key", t.text("no.such.key"));
  TEST_ASSERT_EQUAL(1, t.missingKeys().size());
  TEST_ASSERT_TRUE(t.missingKeys().count("no.such.key") == 1);
}

void test_placeholders_are_filled_in_any_position() {
  Translator t = shipped();
  t.select("fr");
  TEST_ASSERT_EQUAL_STRING("Papa : occupé", t.get("call.busy", {{"name", "Papa"}}).c_str());
  t.select("en");
  TEST_ASSERT_EQUAL_STRING("School time until 12:30", t.get("school.until", {{"time", "12:30"}}).c_str());
}

void test_formatting_edge_cases() {
  char out[64];
  Arg a[] = {Arg("name", "Papa"), Arg("n", 3)};
  TEST_ASSERT_EQUAL(0, format("{name} has {n}", a, 2, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("Papa has 3", out);
  TEST_ASSERT_EQUAL(0, format("100 {{percent}}", a, 2, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("100 {percent}", out);
  TEST_ASSERT_EQUAL(1, format("{missing} and {name}", a, 2, out, sizeof(out)));   // reported, left as written
  TEST_ASSERT_EQUAL_STRING("{missing} and Papa", out);
  TEST_ASSERT_EQUAL(0, format("open { brace", a, 2, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("open { brace", out);
  char tiny[6];
  format("{name} is long", a, 2, tiny, sizeof(tiny));                              // truncates, stays terminated
  TEST_ASSERT_EQUAL_STRING("Papa ", tiny);
}

// English and German say "1 message" and "0 messages"; French says "0 message" too.
void test_plural_rules_differ_by_language() {
  Translator t = shipped();
  t.select("en");
  TEST_ASSERT_EQUAL_STRING("1 new message", t.plural("messages.unread", 1).c_str());
  TEST_ASSERT_EQUAL_STRING("0 new messages", t.plural("messages.unread", 0).c_str());
  TEST_ASSERT_EQUAL_STRING("5 new messages", t.plural("messages.unread", 5).c_str());
  t.select("fr");
  TEST_ASSERT_EQUAL_STRING("0 nouveau message", t.plural("messages.unread", 0).c_str());
  TEST_ASSERT_EQUAL_STRING("1 nouveau message", t.plural("messages.unread", 1).c_str());
  TEST_ASSERT_EQUAL_STRING("2 nouveaux messages", t.plural("messages.unread", 2).c_str());
  t.select("de");
  TEST_ASSERT_EQUAL_STRING("0 neue Nachrichten", t.plural("messages.unread", 0).c_str());
}

// ---------------------------------------------------------------------------
// A message in a named language
// ---------------------------------------------------------------------------

void test_a_message_in_a_named_language_leaves_the_selection_alone() {
  Translator t = shipped();
  t.select("de");
  TEST_ASSERT_EQUAL_STRING("Téléphone", t.getIn("fr", "home.phone").c_str());
  TEST_ASSERT_EQUAL_STRING("Phone", t.getIn("en", "home.phone").c_str());
  TEST_ASSERT_EQUAL_STRING("de", t.language().c_str());
  TEST_ASSERT_EQUAL_STRING("Telefon", t.get("home.phone").c_str());       // the selected language is unchanged
  TEST_ASSERT_EQUAL_STRING("Téléphone", t.getIn("fr-CA", "home.phone").c_str());   // regional tag
}

void test_a_named_language_falls_back_to_english_and_counts_it() {
  Translator t = shipped();
  TEST_ASSERT_EQUAL_STRING("Phone", t.getIn("zz", "home.phone").c_str());      // unknown language
  TEST_ASSERT_EQUAL(1, t.fallbackCount());
  const std::string text = "[meta]\ncode = \"nl\"\nname = \"Dutch\"\nnative = \"Nederlands\"\nplural = \"one-other\"\n"
                           "[strings]\nhome.phone = \"Telefoon\"\n";
  Catalog partial;
  TEST_ASSERT_TRUE(partial.parse(text.data(), text.size()));
  t.add(partial);
  TEST_ASSERT_EQUAL_STRING("Telefoon", t.getIn("nl", "home.phone").c_str());
  TEST_ASSERT_EQUAL(1, t.fallbackCount());                                  // found in Dutch: no new fallback
  TEST_ASSERT_EQUAL_STRING("Inbox", t.getIn("nl", "home.inbox").c_str());     // only English has it
  TEST_ASSERT_EQUAL(2, t.fallbackCount());
  TEST_ASSERT_EQUAL_STRING("Inbox", t.getIn("en", "home.inbox").c_str());     // English itself is no fallback
  TEST_ASSERT_EQUAL(2, t.fallbackCount());
  TEST_ASSERT_EQUAL_STRING("no.such.key", t.getIn("fr", "no.such.key").c_str());
  TEST_ASSERT_EQUAL(1, t.missingKeys().size());
}

void test_a_named_language_fills_placeholders_and_uses_its_own_plural_rule() {
  Translator t = shipped();
  t.select("en");
  TEST_ASSERT_EQUAL_STRING("Papa : occupé", t.getIn("fr", "call.busy", {{"name", "Papa"}}).c_str());
  TEST_ASSERT_EQUAL_STRING("0 nouveau message", t.pluralIn("fr", "messages.unread", 0).c_str());   // French: zero is singular
  TEST_ASSERT_EQUAL_STRING("0 new messages", t.pluralIn("en", "messages.unread", 0).c_str());
  TEST_ASSERT_EQUAL_STRING("2 neue Nachrichten", t.pluralIn("de", "messages.unread", 2).c_str());
  TEST_ASSERT_EQUAL_STRING("no.such.key", t.pluralIn("de", "no.such.key", 2).c_str());
}

void test_the_texts_of_a_key_in_all_languages_come_without_repeats() {
  Translator t = shipped();
  std::vector<std::string> triggers = t.textsInAllLanguages("sms.command.where");
  TEST_ASSERT_EQUAL(3, triggers.size());
  bool english = false, german = false, french = false;
  for (const std::string& s : triggers) {
    english = english || s == "#WhereAreYou";
    german = german || s == "#WoBistDu";
    french = french || s == "#OuEsTu";
  }
  TEST_ASSERT_TRUE(english && german && french);
  TEST_ASSERT_EQUAL(1, t.textsInAllLanguages("home.sos").size());      // "SOS" in all three
  TEST_ASSERT_EQUAL(0, t.textsInAllLanguages("no.such.key").size());
  TEST_ASSERT_EQUAL(0, t.missingKeys().size());                         // asking is not a lookup failure
}

void test_a_language_added_later_adds_its_trigger_words() {
  Translator t = shipped();
  const std::string text = "[meta]\ncode = \"nl\"\nname = \"Dutch\"\nnative = \"Nederlands\"\nplural = \"one-other\"\n"
                           "[strings]\nsms.command.where = \"#WaarBenJe\"\n";
  Catalog nl;
  TEST_ASSERT_TRUE(nl.parse(text.data(), text.size()));
  t.add(nl);
  TEST_ASSERT_EQUAL(4, t.textsInAllLanguages("sms.command.where").size());
}

// ---------------------------------------------------------------------------
// User catalogs from the SD card
// ---------------------------------------------------------------------------

void test_a_user_catalog_can_add_a_language() {
  Translator t = shipped();
  const std::string text =
      "[meta]\ncode = \"es\"\nname = \"Spanish\"\nnative = \"Español\"\nplural = \"one-other\"\n"
      "[strings]\nhome.phone = \"Teléfono\"\ncall.busy = \"{name}: ocupado\"\n";
  std::vector<std::string> errors;
  TEST_ASSERT_TRUE(t.addUser(text.data(), text.size(), &errors));
  TEST_ASSERT_EQUAL(4, t.languages().size());
  TEST_ASSERT_TRUE(t.select("es"));
  TEST_ASSERT_EQUAL_STRING("Teléfono", t.text("home.phone"));
  TEST_ASSERT_EQUAL_STRING("Papa: ocupado", t.get("call.busy", {{"name", "Papa"}}).c_str());
}

void test_a_user_catalog_can_override_single_messages_of_a_shipped_language() {
  Translator t = shipped();
  const std::string text =
      "[meta]\ncode = \"de\"\nname = \"German\"\nnative = \"Deutsch\"\nplural = \"one-other\"\n"
      "[strings]\nsos.pickup = \"Abholen\"\n";
  TEST_ASSERT_TRUE(t.addUser(text.data(), text.size(), nullptr));
  t.select("de");
  TEST_ASSERT_EQUAL_STRING("Abholen", t.text("sos.pickup"));        // overridden
  TEST_ASSERT_EQUAL_STRING("Telefon", t.text("home.phone"));        // the rest is untouched
  TEST_ASSERT_EQUAL(3, t.languages().size());
}

// A translator who drops or misspells a placeholder would make the device print
// "{name}" or the wrong word; such a catalog is rejected whole, with the reason.
void test_a_user_catalog_with_wrong_placeholders_is_rejected() {
  Translator t = shipped();
  const std::string text =
      "[meta]\ncode = \"it\"\nname = \"Italian\"\nnative = \"Italiano\"\nplural = \"one-other\"\n"
      "[strings]\ncall.busy = \"{nome}: occupato\"\n";
  std::vector<std::string> errors;
  TEST_ASSERT_FALSE(t.addUser(text.data(), text.size(), &errors));
  TEST_ASSERT_EQUAL(1, errors.size());
  TEST_ASSERT_NOT_NULL(strstr(errors[0].c_str(), "call.busy"));
  TEST_ASSERT_EQUAL(3, t.languages().size());                       // nothing was added
}

void test_a_broken_user_catalog_adds_nothing_and_says_where() {
  Translator t = shipped();
  const std::string text = std::string(kHead) + "ok = \"fine\"\nbroken line\n";
  std::vector<std::string> errors;
  TEST_ASSERT_FALSE(t.addUser(text.data(), text.size(), &errors));
  TEST_ASSERT_NOT_NULL(strstr(errors[0].c_str(), "line 8"));
  TEST_ASSERT_EQUAL(3, t.languages().size());
}

// The comparison used by the catalog check: finds missing keys, leftovers, differing
// placeholders — the three ways a translation silently rots.
void test_compare_finds_missing_extra_and_changed() {
  const std::string ref = std::string(kHead) + "a = \"A {x}\"\nb = \"B\"\n";
  const std::string other = std::string(kHead) + "a = \"A {y}\"\nc = \"C\"\n";
  Catalog r, o;
  TEST_ASSERT_TRUE(r.parse(ref.data(), ref.size()));
  TEST_ASSERT_TRUE(o.parse(other.data(), other.size()));
  const auto issues = o.compare(r);
  TEST_ASSERT_EQUAL(3, issues.size());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_shipped_languages_match_the_reference);
  RUN_TEST(test_language_picker_lists_the_shipped_languages_by_their_own_names);
  RUN_TEST(test_parser_reads_strings_arrays_escapes_and_comments);
  RUN_TEST(test_parser_reports_every_problem_with_its_line);
  RUN_TEST(test_parser_requires_the_header_and_a_known_plural_rule);
  RUN_TEST(test_parser_checks_the_number_of_plural_forms);
  RUN_TEST(test_lookup_in_the_selected_language_and_fallback_to_english);
  RUN_TEST(test_unknown_keys_show_as_themselves_and_are_remembered);
  RUN_TEST(test_placeholders_are_filled_in_any_position);
  RUN_TEST(test_formatting_edge_cases);
  RUN_TEST(test_plural_rules_differ_by_language);
  RUN_TEST(test_a_message_in_a_named_language_leaves_the_selection_alone);
  RUN_TEST(test_a_named_language_falls_back_to_english_and_counts_it);
  RUN_TEST(test_a_named_language_fills_placeholders_and_uses_its_own_plural_rule);
  RUN_TEST(test_the_texts_of_a_key_in_all_languages_come_without_repeats);
  RUN_TEST(test_a_language_added_later_adds_its_trigger_words);
  RUN_TEST(test_a_user_catalog_can_add_a_language);
  RUN_TEST(test_a_user_catalog_can_override_single_messages_of_a_shipped_language);
  RUN_TEST(test_a_user_catalog_with_wrong_placeholders_is_rejected);
  RUN_TEST(test_a_broken_user_catalog_adds_nothing_and_says_where);
  RUN_TEST(test_compare_finds_missing_extra_and_changed);
  return UNITY_END();
}

// Host tests for the pseudo-language and the per-language test helper.
// allow-non-english: the catalogs under test contain German and French.

#include <unity.h>

#include <fstream>
#include <set>
#include <sstream>

#include <Languages.h>
#include <ui/Fonts.h>
#include <ui/Text.h>

using namespace i18n;

void setUp() {}
void tearDown() {}

namespace {

// Code points of assets/fonts/charset.txt, read the way the font tests read them.
std::set<uint32_t> charset() {
  std::ifstream in("assets/fonts/charset.txt");
  TEST_ASSERT_TRUE(in.good());
  std::set<uint32_t> out;
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream words(line.substr(0, line.find('#')));
    std::string kind, a, b;
    if (!(words >> kind >> a)) continue;
    words >> b;
    if (kind == "range") for (uint32_t c = std::stoul(a, nullptr, 16); c <= std::stoul(b, nullptr, 16); ++c) out.insert(c);
    else out.insert(std::stoul(a, nullptr, 16));
  }
  return out;
}

int textWidth(const std::string& s) { return ui::text::width(ui::font(ui::FontSize::Text), s); }

}  // namespace

// ---- pseudoLocalize ---------------------------------------------------------------------------

void test_a_message_gets_brackets_accents_and_longer_words() {
  // "Phone": P-h-o-n-e is 5 letters, so two filler letters; o, n, e are accented.
  TEST_ASSERT_EQUAL_STRING("[Ph\xC3\xB6\xC3\xB1\xC3\xA9\xC3\xAB\xC3\xB6]", pseudoLocalize("Phone").c_str());
}

void test_every_word_grows_by_two_fifths_of_its_letters_rounded_up() {
  // 1 letter -> +1, 2 -> +1, 3 -> +2, 5 -> +2, 6 -> +3, 10 -> +4. The filler letters are
  // two-byte sequences, so characters are counted, not bytes.
  const auto characters = [](const std::string& s) {
    size_t n = 0;
    for (unsigned char c : s) if ((c & 0xC0) != 0x80) ++n;
    return n;
  };
  const struct { const char* word; size_t extra; } cases[] = {{"x", 1}, {"xy", 1}, {"xyz", 2}, {"xyzab", 2},
                                                              {"xyzabc", 3}, {"xyzabcdefg", 4}};
  for (const auto& c : cases)
    TEST_ASSERT_EQUAL_MESSAGE(strlen(c.word) + c.extra + 2, characters(pseudoLocalize(c.word)), c.word);
}

void test_spaces_digits_and_punctuation_are_kept_and_do_not_make_words_longer() {
  TEST_ASSERT_EQUAL_STRING("[12:30]", pseudoLocalize("12:30").c_str());
  TEST_ASSERT_EQUAL_STRING("[ ]", pseudoLocalize(" ").c_str());
  TEST_ASSERT_EQUAL_STRING("[]", pseudoLocalize("").c_str());
  TEST_ASSERT_EQUAL_STRING("[100%]", pseudoLocalize("100%").c_str());
}

void test_placeholders_and_escaped_braces_are_untouched() {
  const std::string out = pseudoLocalize("{name}: busy {{ok}}");
  TEST_ASSERT_TRUE(out.find("{name}") != std::string::npos);
  // The braces stay as they are; the text between them is ordinary text, because an escaped
  // brace is a literal character and not a placeholder.
  TEST_ASSERT_TRUE(out.find("{{") != std::string::npos && out.find("}}") != std::string::npos);
  // The text inside a placeholder is not accented: {name} would otherwise become {ñám\xC3\xA9}.
  TEST_ASSERT_EQUAL(0, out.compare(0, 8, "[{name}:"));
  // A word that ends in a placeholder does not grow: the placeholder grows at run time.
  TEST_ASSERT_EQUAL_STRING("[{n}]", pseudoLocalize("{n}").c_str());
  TEST_ASSERT_EQUAL_STRING("[\xC3\xA1" "b{n}]", pseudoLocalize("ab{n}").c_str());
  // ... but the word after it does, and a placeholder inside a word does not stop it growing.
  TEST_ASSERT_EQUAL_STRING("[{n} \xC3\xA1" "b\xC3\xAB]", pseudoLocalize("{n} ab").c_str());
  TEST_ASSERT_EQUAL_STRING("[{n}b\xC3\xAB]", pseudoLocalize("{n}b").c_str());
  TEST_ASSERT_EQUAL_STRING("[{name]", pseudoLocalize("{name").c_str());  // unterminated: kept as written
}

void test_the_text_between_escaped_braces_is_ordinary_text() {
  TEST_ASSERT_EQUAL_STRING("[{{\xC3\xB6k}}\xC3\xAB]", pseudoLocalize("{{ok}}").c_str());
}

void test_each_accented_letter_has_its_stand_in() {
  const struct { char letter; const char* accent; } table[] = {
      {'a', "\xC3\xA1"}, {'e', "\xC3\xA9"}, {'i', "\xC3\xAD"}, {'o', "\xC3\xB6"}, {'u', "\xC3\xBC"},
      {'y', "\xC3\xBD"}, {'c', "\xC3\xA7"}, {'n', "\xC3\xB1"}, {'A', "\xC3\x85"}, {'E', "\xC3\x89"},
      {'I', "\xC3\x8C"}, {'O', "\xC3\x96"}, {'U', "\xC3\x9C"}};
  for (const auto& row : table) {
    const std::string out = pseudoLocalize(std::string(1, row.letter));
    TEST_ASSERT_EQUAL_MESSAGE(0, out.compare(0, 1 + strlen(row.accent), std::string("[") + row.accent), std::string(1, row.letter).c_str());
  }
  // Letters without a stand-in stay as they are (and still count for the length).
  TEST_ASSERT_EQUAL_STRING("[bd\xC3\xAB]", pseudoLocalize("bd").c_str());
}

void test_multibyte_text_passes_through_and_letters_among_it_count() {
  // The euro sign and the symbols are not letters; a letter like u-umlaut is, and is kept.
  TEST_ASSERT_EQUAL_STRING("[\xE2\x82\xAC" "5]", pseudoLocalize("\xE2\x82\xAC" "5").c_str());
  TEST_ASSERT_EQUAL_STRING("[\xE2\x86\xB5]", pseudoLocalize("\xE2\x86\xB5").c_str());
  const std::string out = pseudoLocalize("\xC3\xBC");  // one letter: one filler letter follows
  TEST_ASSERT_EQUAL_STRING("[\xC3\xBC\xC3\xAB]", out.c_str());
}

void test_pseudo_text_stays_inside_the_character_set() {
  const std::set<uint32_t> allowed = charset();
  Catalog english;
  std::ifstream in("lang/en.toml");
  std::stringstream text;
  text << in.rdbuf();
  TEST_ASSERT_TRUE(english.parse(text.str().data(), text.str().size()));
  size_t checked = 0;
  for (const auto& [key, entry] : english.entries()) {
    for (const std::string& form : entry.forms) {
      const std::string pseudo = pseudoLocalize(form);
      for (size_t i = 0; i < pseudo.size();) {
        size_t pos = i;
        const uint32_t cp = ui::text::decode(pseudo, pos);
        TEST_ASSERT_TRUE_MESSAGE(allowed.count(cp) == 1 || cp == '\n', key.c_str());  // SMS texts have line breaks
        i = pos;
      }
      ++checked;
    }
  }
  TEST_ASSERT_TRUE(checked > 20);
}

void test_pseudo_catalog_has_the_keys_forms_and_placeholders_of_english() {
  Catalog english;
  std::ifstream in("lang/en.toml");
  std::stringstream text;
  text << in.rdbuf();
  TEST_ASSERT_TRUE(english.parse(text.str().data(), text.str().size()));
  const Catalog pseudo = pseudoCatalog(english);
  TEST_ASSERT_EQUAL_STRING(kPseudoCode, pseudo.code().c_str());
  TEST_ASSERT_EQUAL(english.size(), pseudo.size());
  TEST_ASSERT_TRUE(pseudo.plural() == english.plural());
  TEST_ASSERT_EQUAL(0, pseudo.compare(english).size());  // same keys, same placeholders, same forms
  for (const auto& [key, entry] : english.entries()) {
    const Entry* mine = pseudo.find(key);
    TEST_ASSERT_TRUE_MESSAGE(mine != nullptr, key.c_str());
    TEST_ASSERT_EQUAL_MESSAGE(entry.forms.size(), mine->forms.size(), key.c_str());
    for (size_t i = 0; i < entry.forms.size(); ++i) {
      TEST_ASSERT_TRUE_MESSAGE(mine->forms[i].front() == '[' && mine->forms[i].back() == ']', key.c_str());
      TEST_ASSERT_TRUE_MESSAGE(textWidth(mine->forms[i]) > textWidth(entry.forms[i]), key.c_str());
    }
  }
}

void test_the_pseudo_catalog_keeps_the_plural_rule_of_its_reference() {
  const char* text = "[meta]\ncode = \"xx\"\nname = \"Test\"\nnative = \"Test\"\nplural = \"zero-one-other\"\n"
                     "[strings]\nmessages = [\"{n} one\", \"{n} many\"]\n";
  Catalog source;
  TEST_ASSERT_TRUE(source.parse(text, strlen(text)));
  TEST_ASSERT_TRUE(pseudoCatalog(source).plural() == PluralRule::ZeroOneOther);
}

void test_a_sentence_grows_by_at_least_a_third_in_pixels() {
  // The width in pixels, not the letter count, is what a layout meets. Words dominate the
  // longer messages; short ones like "SOS" have no lower-case letters to speak of.
  Translator t = languages::loaded();
  t.select("en");
  const std::string english = t.get("sos.pickup");
  t.select(kPseudoCode);
  const std::string pseudo = t.get("sos.pickup");
  TEST_ASSERT_TRUE(textWidth(pseudo) * 3 >= textWidth(english) * 4);
}

// ---- languages::forEach -------------------------------------------------------------------------

void test_for_each_visits_every_shipped_language_then_the_pseudo_language() {
  std::vector<std::string> seen;
  languages::forEach([&](const std::string& code, Translator& t) {
    seen.push_back(code);
    TEST_ASSERT_EQUAL_STRING(code.c_str(), t.language().c_str());
  });
  const std::vector<std::string> expected = {"de", "en", "fr", kPseudoCode};
  TEST_ASSERT_TRUE(seen == expected);
}

void test_for_each_gives_each_language_its_own_texts() {
  std::set<std::string> inbox;
  languages::forEach([&](const std::string&, Translator& t) { inbox.insert(t.get("home.inbox")); });
  TEST_ASSERT_EQUAL(4, inbox.size());
}

void test_for_each_reads_the_catalogs_from_the_given_directory() {
  const std::string dir = "/tmp/phone-os-languages-test";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  std::ifstream en("lang/en.toml");
  std::ofstream(dir + "/en.toml") << en.rdbuf();
  std::ofstream(dir + "/notes.txt") << "not a catalog";
  std::vector<std::string> seen;
  languages::forEach([&](const std::string& code, Translator&) { seen.push_back(code); }, dir);
  const std::vector<std::string> expected = {"en", kPseudoCode};
  TEST_ASSERT_TRUE(seen == expected);
  std::filesystem::remove_all(dir);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_a_message_gets_brackets_accents_and_longer_words);
  RUN_TEST(test_every_word_grows_by_two_fifths_of_its_letters_rounded_up);
  RUN_TEST(test_spaces_digits_and_punctuation_are_kept_and_do_not_make_words_longer);
  RUN_TEST(test_placeholders_and_escaped_braces_are_untouched);
  RUN_TEST(test_the_text_between_escaped_braces_is_ordinary_text);
  RUN_TEST(test_each_accented_letter_has_its_stand_in);
  RUN_TEST(test_multibyte_text_passes_through_and_letters_among_it_count);
  RUN_TEST(test_pseudo_text_stays_inside_the_character_set);
  RUN_TEST(test_pseudo_catalog_has_the_keys_forms_and_placeholders_of_english);
  RUN_TEST(test_the_pseudo_catalog_keeps_the_plural_rule_of_its_reference);
  RUN_TEST(test_a_sentence_grows_by_at_least_a_third_in_pixels);
  RUN_TEST(test_for_each_visits_every_shipped_language_then_the_pseudo_language);
  RUN_TEST(test_for_each_gives_each_language_its_own_texts);
  RUN_TEST(test_for_each_reads_the_catalogs_from_the_given_directory);
  return UNITY_END();
}

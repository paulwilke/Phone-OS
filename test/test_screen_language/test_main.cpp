// Host tests for the language page: every language by its own name, the one in use, choosing at once through the
// core, languages that parents put on the card, a long name, and the layout.
// allow-non-english: the catalogs under test contain German and French.

#include <unity.h>

#include <DeviceRig.h>
#include <Shows.h>
#include <ui/Fonts.h>
#include <ui/ListView.h>
#include <ui/PageTitle.h>
#include <ui/Text.h>
#include <ui/screens/LanguageScreen.h>

using namespace people;
using namespace ui;

void setUp() {}
void tearDown() {}

namespace {

const Rect kContent = ScreenLayout::standard().content;

// A catalog a parent put on the card: a language of its own with one message.
std::string userCatalog(const std::string& code, const std::string& native) {
  return "[meta]\ncode = \"" + code + "\"\nname = \"" + code + "\"\nnative = \"" + native + "\"\nplural = \"one-other\"\n[strings]\nhome.phone = \"x\"\n";
}

void addUserLanguage(i18n::Translator& translator, const std::string& code, const std::string& native) {
  const std::string text = userCatalog(code, native);
  TEST_ASSERT_TRUE_MESSAGE(translator.addUser(text.data(), text.size(), nullptr), code.c_str());
}

struct Page {
  DeviceRig rig;
  LanguageScreen screen;
  Page() : rig(ProfileBuilder().childName("Alex").build()), screen(rig.context()) { start(); }
  void start() {
    rig.poll();
    screen.onEnter();
    draw();
  }
  void draw() { screen.render(rig.frame); }
  void key(Key k) {
    screen.handle(Input::pressed(k));
    draw();
  }
  void down(int times) {
    for (int i = 0; i < times; ++i) key(Key::Down);
  }
  bool says(const std::string& words, FontSize size = FontSize::Text, bool onBlack = false) {
    return shows::text(rig.frame, words, size, kContent, onBlack);
  }
};

}  // namespace

// ---- what is listed -------------------------------------------------------------------------------------------

void test_every_language_is_listed_by_its_own_name_with_the_one_in_use_in_focus() {
  Page page;
  TEST_ASSERT_TRUE(page.says("Language", FontSize::Small));
  TEST_ASSERT_TRUE(page.says("Now: English", FontSize::Small));
  TEST_ASSERT_TRUE(page.says("Deutsch"));
  TEST_ASSERT_TRUE(page.says("English", FontSize::Text, true));          // the focus starts on the language in use
  TEST_ASSERT_TRUE(page.says("Fran\xC3\xA7" "ais"));
  TEST_ASSERT_TRUE(page.says("[Pseudo]"));
}

void test_the_page_follows_the_language_in_use_when_it_opens() {
  Page page;
  page.rig.language("fr");
  page.screen.onEnter();
  page.draw();
  TEST_ASSERT_TRUE(page.says("Fran\xC3\xA7" "ais", FontSize::Text, true));
  TEST_ASSERT_TRUE(page.says("Langue", FontSize::Small));
  TEST_ASSERT_TRUE(page.says("Actuel : Fran\xC3\xA7" "ais", FontSize::Small));
}

// ---- choosing ---------------------------------------------------------------------------------------------------

void test_choosing_applies_at_once_through_the_core_and_the_page_stays() {
  Page page;
  page.key(Key::Up);                                                    // English -> Deutsch
  page.key(Key::Select);
  TEST_ASSERT_EQUAL_STRING("de", page.rig.translator.language().c_str());
  TEST_ASSERT_EQUAL_STRING("de", page.rig.core.profile().interfaceLanguage.c_str());   // the choice is held in the profile
  TEST_ASSERT_TRUE(page.says("Sprache", FontSize::Small));              // this very page is already in German
  TEST_ASSERT_TRUE(page.says("Jetzt: Deutsch", FontSize::Small));
  TEST_ASSERT_TRUE(page.says("Deutsch", FontSize::Text, true));
  TEST_ASSERT_EQUAL(0, page.rig.nav.calls.size());                      // it does not close itself
  TEST_ASSERT_EQUAL_STRING("W\xC3\xA4hlen", page.screen.softKeys().action.c_str());
}

void test_the_page_only_asks_the_core_and_does_not_switch_the_translator_itself() {
  DeviceRig rig;                                                         // the core's translator
  i18n::Translator seenByScreen = languages::loaded();                   // another one, which only the screen reads
  ui::Session session;
  LanguageScreen screen(ScreenContext{rig.core, seenByScreen, rig.nav, session});
  rig.poll();
  screen.onEnter();
  screen.render(rig.frame);
  screen.handle(Input::pressed(Key::Up));
  screen.handle(Input::pressed(Key::Select));
  TEST_ASSERT_EQUAL_STRING("de", rig.translator.language().c_str());     // the request arrived at the core
  TEST_ASSERT_EQUAL_STRING("de", rig.core.profile().interfaceLanguage.c_str());
  TEST_ASSERT_EQUAL_STRING("en", seenByScreen.language().c_str());       // and nothing else happened
}

void test_choosing_the_language_in_use_changes_nothing() {
  Page page;
  page.key(Key::Select);
  TEST_ASSERT_EQUAL_STRING("en", page.rig.translator.language().c_str());
  TEST_ASSERT_TRUE(page.says("Now: English", FontSize::Small));
}

void test_the_sms_language_stays_whatever_the_child_chooses() {
  Page page;
  page.key(Key::Up);
  page.key(Key::Select);
  TEST_ASSERT_EQUAL_STRING("en", page.rig.core.profile().smsLanguage.c_str());
}

void test_a_tap_on_a_language_chooses_it() {
  Page page;
  ListView probe;
  probe.setItems({{"a", true}});
  const int header = pageTitleHeight(kContent, page.rig.translator.get("language.title")) + 16 + 4;
  const Rect row = probe.rowRect({kContent.x, kContent.y + header, kContent.w, kContent.h - header}, 2);   // the third: Francais
  TEST_ASSERT_TRUE(page.screen.handle(Input::touch(row.x + 10, row.y + 5)));
  TEST_ASSERT_EQUAL_STRING("fr", page.rig.translator.language().c_str());
}

void test_back_goes_back() {
  Page page;
  TEST_ASSERT_EQUAL_STRING("Back", page.screen.softKeys().back.c_str());
  TEST_ASSERT_EQUAL_STRING("Pick", page.screen.softKeys().action.c_str());
  TEST_ASSERT_TRUE(page.screen.softKeys().scroll);
  TEST_ASSERT_TRUE(page.screen.handle(Input::pressed(Key::Back)));
  TEST_ASSERT_EQUAL_STRING("back", page.rig.nav.last().c_str());
}

// ---- languages from the card ------------------------------------------------------------------------------------------

void test_a_language_that_parents_put_on_the_card_is_listed_and_can_be_chosen() {
  Page page;
  addUserLanguage(page.rig.translator, "es", "Espa\xC3\xB1" "ol");
  page.draw();
  TEST_ASSERT_TRUE(page.says("Espa\xC3\xB1" "ol"));
  page.screen.onEnter();                                                 // the focus on English; by code the order is de en es fr
  page.draw();
  page.key(Key::Down);
  page.key(Key::Select);
  TEST_ASSERT_EQUAL_STRING("es", page.rig.translator.language().c_str());
  TEST_ASSERT_EQUAL_STRING("es", page.rig.core.profile().interfaceLanguage.c_str());
  TEST_ASSERT_TRUE(page.says("Now: Espa\xC3\xB1" "ol", FontSize::Small));
}

void test_many_languages_scroll_with_the_focus_and_the_last_can_be_reached() {
  Page page;
  for (int i = 0; i < 10; ++i) addUserLanguage(page.rig.translator, std::string("x") + static_cast<char>('a' + i), std::string("Language ") + static_cast<char>('A' + i));
  page.draw();
  page.down(20);
  TEST_ASSERT_TRUE(page.says("Language J", FontSize::Text, true));      // the last by code
  page.key(Key::Select);
  TEST_ASSERT_EQUAL_STRING("xj", page.rig.translator.language().c_str());
}

void test_a_name_from_a_card_that_does_not_fit_is_shortened_inside_the_row() {
  Page page;
  addUserLanguage(page.rig.translator, "es", "Un nombre de idioma demasiado largo para caber en la fila");
  page.draw();
  FrameBuffer fb;
  page.screen.render(fb);
  TEST_ASSERT_FALSE_MESSAGE(fb.clipped(), fb.describeClips().c_str());
  const Font& body = font(FontSize::Text);
  TEST_ASSERT_TRUE(page.says(text::ellipsize(body, "Un nombre de idioma demasiado largo para caber en la fila", kContent.w - 6 - 8)));
}

// ---- layout ---------------------------------------------------------------------------------------------------------------

void test_the_page_is_drawn_inside_the_screen_in_every_language() {
  languages::forEach([](const std::string& language, i18n::Translator&) {
    Page page;
    page.rig.language(language);
    addUserLanguage(page.rig.translator, "es", "Espa\xC3\xB1" "ol");
    FrameBuffer fb;
    page.draw();
    page.screen.render(fb);
    // The pseudo-language is 40 % longer than any real one; the one action word beside the arrows may run
    // into the soft-key bar there (see docs/screen-language.md), the page itself may not.
    bool outside = false;
    for (const auto& e : fb.clipEvents())
      if (language != "qps-ploc" || e.firstY < ScreenLayout::standard().softKeys.y) outside = true;
    TEST_ASSERT_FALSE_MESSAGE(outside, (language + ":\n" + fb.describeClips()).c_str());
  });
}

void test_the_pictures_in_english_and_german() {
  for (const char* language : {"en", "de"}) {
    Page page;
    page.rig.language(language);
    page.screen.onEnter();
    page.draw();
    golden::expect(page.rig.frame, std::string("language_") + language);
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_every_language_is_listed_by_its_own_name_with_the_one_in_use_in_focus);
  RUN_TEST(test_the_page_follows_the_language_in_use_when_it_opens);
  RUN_TEST(test_choosing_applies_at_once_through_the_core_and_the_page_stays);
  RUN_TEST(test_the_page_only_asks_the_core_and_does_not_switch_the_translator_itself);
  RUN_TEST(test_choosing_the_language_in_use_changes_nothing);
  RUN_TEST(test_the_sms_language_stays_whatever_the_child_chooses);
  RUN_TEST(test_a_tap_on_a_language_chooses_it);
  RUN_TEST(test_back_goes_back);
  RUN_TEST(test_a_language_that_parents_put_on_the_card_is_listed_and_can_be_chosen);
  RUN_TEST(test_many_languages_scroll_with_the_focus_and_the_last_can_be_reached);
  RUN_TEST(test_a_name_from_a_card_that_does_not_fit_is_shortened_inside_the_row);
  RUN_TEST(test_the_page_is_drawn_inside_the_screen_in_every_language);
  RUN_TEST(test_the_pictures_in_english_and_german);
  return UNITY_END();
}

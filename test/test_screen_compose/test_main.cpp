// Host tests for the compose page: the recipient, typing and the character set, the limits, sending
// (SMS, radio), ready-made messages where free text is off, and the layout in every language.
// allow-non-english: the catalogs under test contain German and French.

#include <unity.h>

#include <DeviceRig.h>
#include <RefreshRig.h>
#include <Shows.h>
#include <app/Utf8.h>
#include <ui/Fonts.h>
#include <ui/Text.h>
#include <ui/screens/ComposeScreen.h>

using namespace people;
using namespace ui;

void setUp() {}
void tearDown() {}

namespace {

const Rect kContent = ScreenLayout::standard().content;

Profile freeText() { return ProfileBuilder().childName("Alex").freeText(true).build(); }

struct Page {
  DeviceRig rig;
  ComposeScreen screen;
  Page() : rig(freeText()), screen(rig.context()) { start(); }
  explicit Page(Profile profile) : rig(std::move(profile)), screen(rig.context()) { start(); }
  void start() {
    screen.onEnter();
    rig.poll();
    screen.render(rig.frame);
  }
  void type(const std::string& utf8) {
    for (size_t pos = 0; pos < utf8.size();) screen.handle(Input::text(text::decode(utf8, pos)));
    screen.render(rig.frame);
  }
  void key(Key k) {
    screen.handle(Input::pressed(k));
    screen.render(rig.frame);
  }
  bool says(const std::string& words, FontSize size = FontSize::Text) { return shows::text(rig.frame, words, size, kContent); }
};

}  // namespace

// ---- the recipient ------------------------------------------------------------------------------

void test_with_nobody_chosen_it_starts_with_the_first_call_target() {
  Page page;
  TEST_ASSERT_EQUAL_STRING(kParentOne, page.rig.session.recipient.c_str());
  TEST_ASSERT_TRUE(page.says("To", FontSize::Small));
  TEST_ASSERT_TRUE(page.says("Parent One"));
}

void test_without_call_targets_it_starts_with_the_first_contact_and_with_nobody_it_says_so() {
  Page contacts(ProfileBuilder().callTargets({}).freeText(true).build());
  TEST_ASSERT_EQUAL_STRING(kParentOne, contacts.rig.session.recipient.c_str());
  Page nobody(ProfileBuilder().noContacts().freeText(true).build());
  TEST_ASSERT_EQUAL_STRING("", nobody.rig.session.recipient.c_str());
  TEST_ASSERT_TRUE(nobody.says("Choose who to write to"));
  nobody.type("Hello");
  TEST_ASSERT_TRUE(nobody.screen.softKeys().action.empty());          // nothing to send it to
  nobody.key(Key::Select);
  TEST_ASSERT_EQUAL(0, nobody.rig.modem.sent().size());
}

void test_a_chosen_recipient_is_not_replaced() {
  DeviceRig rig(freeText());
  rig.session.recipient = kGrandma;
  ComposeScreen screen(rig.context());
  screen.onEnter();
  TEST_ASSERT_EQUAL_STRING(kGrandma, rig.session.recipient.c_str());
}

void test_a_number_in_no_book_is_shown_in_full_and_a_tap_on_the_recipient_opens_the_choice() {
  Page page;
  page.rig.session.recipient = kStranger;
  page.screen.render(page.rig.frame);
  TEST_ASSERT_TRUE(page.says(kStranger));
  const Rect line = shows::where(page.rig.frame, kStranger, FontSize::Text, kContent).value();
  TEST_ASSERT_TRUE(page.screen.handle(Input::touch(line.x + 4, line.y + 4)));
  TEST_ASSERT_TRUE(page.rig.nav.opened(screen::kRecipient));
}

void test_the_draft_stays_while_the_page_is_left_and_entered_again() {
  Page page;
  page.type("Hello");
  page.screen.onLeave();
  page.screen.onEnter();
  TEST_ASSERT_EQUAL_STRING("Hello", page.screen.text().c_str());
  TEST_ASSERT_EQUAL_STRING(kParentOne, page.rig.session.recipient.c_str());
}

// ---- typing -----------------------------------------------------------------------------------

void test_typing_shows_the_text_with_the_count_and_a_bar_after_it() {
  Page page;
  page.type("Hi mum");
  TEST_ASSERT_EQUAL_STRING("Hi mum", page.screen.text().c_str());
  TEST_ASSERT_TRUE(page.says("Hi mum_"));
  TEST_ASSERT_TRUE(page.says("6/160", FontSize::Small));
  TEST_ASSERT_EQUAL_STRING("Delete", page.screen.softKeys().back.c_str());
  TEST_ASSERT_EQUAL_STRING("Send", page.screen.softKeys().action.c_str());
}

void test_backspace_deletes_a_whole_character_and_on_an_empty_page_goes_back() {
  Page page;
  page.type("a\xC3\xA4" "\xE2\x82\xAC" "b");                        // a, ä, €, b: one, two, three and one bytes
  page.key(Key::Back);
  page.key(Key::Back);
  TEST_ASSERT_EQUAL_STRING("a\xC3\xA4", page.screen.text().c_str());   // the euro sign went whole
  page.key(Key::Back);
  page.key(Key::Back);
  TEST_ASSERT_EQUAL_STRING("", page.screen.text().c_str());
  TEST_ASSERT_EQUAL(0, page.rig.nav.calls.size());
  TEST_ASSERT_EQUAL_STRING("Back", page.screen.softKeys().back.c_str());
  TEST_ASSERT_TRUE(page.screen.softKeys().action.empty());
  page.key(Key::Back);
  TEST_ASSERT_EQUAL_STRING("back", page.rig.nav.last().c_str());
}

// ---- the character set --------------------------------------------------------------------------

void test_what_the_display_can_draw_is_taken_and_nothing_else() {
  Page page;
  // Latin-1, the euro sign, the ligatures and Y with diaeresis: all in the character set.
  page.type("\xC3\x9F\xC3\xA9\xC3\xB1\xC5\x93\xC5\x92\xC5\xB8\xE2\x82\xAC");
  TEST_ASSERT_EQUAL(7, page.screen.characters());
  Page other;
  other.type(std::string("\x07") + "a\n" + "\xF0\x9F\x98\x80" + "\xD0\x96" + "\xE4\xB8\xAD" + "b");   // bell, newline, emoji, Zhe, a Chinese character
  TEST_ASSERT_EQUAL_STRING("ab", other.screen.text().c_str());
  TEST_ASSERT_TRUE(other.screen.handle(Input::text(0x7F)));            // used up, and not taken
  TEST_ASSERT_EQUAL_STRING("ab", other.screen.text().c_str());
}

void test_a_message_in_each_language_goes_out_exactly_as_typed() {
  const struct { const char* language; const char* text; } messages[] = {
      {"en", "I'm at the caf\xC3\xA9, back at 5 \xE2\x82\xAC"},
      {"de", "Sch\xC3\xB6ne Gr\xC3\xBC\xC3\x9F" "e! \xC3\x84rger im B\xC3\xBC" "ro"},
      {"fr", "O\xC3\xB9 es-tu ? C'est l'\xC5\x93uvre de Zo\xC3\xAB \xC5\xB8 \xC3\xA0 3 \xE2\x82\xAC"}};
  for (const auto& m : messages) {
    Page page;
    page.rig.language(m.language);
    page.type(m.text);
    TEST_ASSERT_EQUAL_STRING(m.text, page.screen.text().c_str());
    // Every line of the wrapped text is on the screen, accents and all.
    const std::string shown = std::string(m.text) + "_";
    for (std::string_view line : text::wrap(font(FontSize::Text), shown, kContent.w - 8))
      TEST_ASSERT_TRUE_MESSAGE(page.says(std::string(line)), (std::string(m.language) + ": " + std::string(line)).c_str());
    page.key(Key::Select);
    TEST_ASSERT_EQUAL(1, page.rig.modem.sent().size());
    TEST_ASSERT_EQUAL_STRING(m.text, page.rig.modem.sent()[0].body.c_str());
  }
}

// ---- the limits ------------------------------------------------------------------------------------

void test_plain_text_takes_160_characters_and_the_161st_is_not_taken() {
  Page page;
  page.type(std::string(160, 'a'));
  TEST_ASSERT_EQUAL(160, page.screen.characters());
  page.type("b");
  TEST_ASSERT_EQUAL(160, page.screen.text().size());
  TEST_ASSERT_TRUE(page.says("160/160", FontSize::Small));
}

void test_the_first_accent_lowers_the_limit_to_70_and_the_page_says_why() {
  Page page;
  page.type("hello");
  TEST_ASSERT_EQUAL(160, page.screen.limit());
  TEST_ASSERT_FALSE(page.says("With accents only 70 characters", FontSize::Small));
  page.type("\xC3\xA4");
  TEST_ASSERT_EQUAL(70, page.screen.limit());
  TEST_ASSERT_TRUE(page.says("6/70", FontSize::Small));
  TEST_ASSERT_TRUE(page.says("With accents only", FontSize::Small));
  page.type(std::string(70, 'x'));                                   // only room for 64 more
  TEST_ASSERT_EQUAL(70, page.screen.characters());
  page.key(Key::Back);                                               // delete one, the limit stays while the accent is there
  TEST_ASSERT_EQUAL(69, page.screen.characters());
  page.type("\xC3\xB6");
  TEST_ASSERT_EQUAL(70, page.screen.characters());
}

void test_an_accent_is_not_taken_when_the_text_is_already_longer_than_the_wide_limit() {
  Page page;
  page.type(std::string(100, 'a'));
  page.type("\xC3\xA9");
  TEST_ASSERT_EQUAL_STRING(std::string(100, 'a').c_str(), page.screen.text().c_str());   // 71 characters or more cannot become wide
  TEST_ASSERT_EQUAL(160, page.screen.limit());
  page.key(Key::Back);                                               // deleting one does not help either: still over
  page.type("\xC3\xA9");
  TEST_ASSERT_EQUAL(99, page.screen.characters());
}

void test_what_the_history_keeps_is_the_second_limit() {
  Page euro;
  for (int i = 0; i < 70; ++i) euro.type("\xE2\x82\xAC");            // euro signs, three bytes each
  TEST_ASSERT_TRUE(euro.screen.text().size() <= ComposeScreen::kMaxBytes);
  TEST_ASSERT_EQUAL(53, euro.screen.characters());                    // 53 x 3 = 159 bytes: the 54th does not fit
  TEST_ASSERT_EQUAL(0, euro.screen.text().size() % 3);               // and nothing was cut in half
}

// ---- sending ------------------------------------------------------------------------------------------

void test_enter_sends_and_the_page_goes_home_with_nothing_left_behind() {
  Page page;
  page.type("On my way");
  page.key(Key::Select);
  TEST_ASSERT_EQUAL(1, page.rig.modem.sent().size());
  TEST_ASSERT_EQUAL_STRING(kParentOne, page.rig.modem.sent()[0].phone.c_str());
  TEST_ASSERT_EQUAL_STRING("On my way", page.rig.modem.sent()[0].body.c_str());
  TEST_ASSERT_EQUAL_STRING("home", page.rig.nav.last().c_str());
  TEST_ASSERT_EQUAL_STRING("", page.screen.text().c_str());
  TEST_ASSERT_EQUAL_STRING("", page.rig.session.recipient.c_str());
  TEST_ASSERT_TRUE(page.rig.core.state().notice.none());               // an SMS that left needs no notice
}

void test_nothing_is_sent_with_nothing_written() {
  Page page;
  TEST_ASSERT_FALSE(page.screen.handle(Input::pressed(Key::Select)));
  TEST_ASSERT_EQUAL(0, page.rig.modem.sent().size());
}

void test_without_a_network_the_message_waits_and_the_page_still_goes_home() {
  Page page;
  page.rig.modem.setNetwork("", 0);
  page.rig.poll();
  page.type("Hello");
  page.key(Key::Select);
  TEST_ASSERT_EQUAL(0, page.rig.modem.sent().size());
  TEST_ASSERT_EQUAL(1, page.rig.core.state().queued);
  TEST_ASSERT_EQUAL_STRING("home", page.rig.nav.last().c_str());
}

void test_when_the_core_cannot_take_the_message_the_text_stays() {
  Page page;
  page.rig.core.setCellularEnabled(false);
  for (size_t i = 0; i < Messaging::kOutboxSize; ++i) TEST_ASSERT_TRUE(page.rig.core.send(kParentTwo, "x"));
  page.type("Hello");
  TEST_ASSERT_FALSE(page.screen.handle(Input::pressed(Key::Select)));   // the queue is full
  TEST_ASSERT_EQUAL_STRING("Hello", page.screen.text().c_str());
  TEST_ASSERT_EQUAL(0, page.rig.nav.calls.size());
  TEST_ASSERT_TRUE(page.rig.core.state().notice.kind == NoticeKind::QueueFull);
  TEST_ASSERT_EQUAL_STRING(kParentOne, page.rig.session.recipient.c_str());
}

void test_school_time_does_not_stop_a_message() {
  Page page(ProfileBuilder().freeText(true).schoolTime("tue", "14:00", "15:00").build());
  page.rig.poll();
  TEST_ASSERT_TRUE(page.rig.core.state().schoolTime);
  page.type("Hello");
  page.key(Key::Select);
  TEST_ASSERT_EQUAL(1, page.rig.modem.sent().size());
}

void test_a_radio_contact_gets_the_message_over_the_radio() {
  Page page(ProfileBuilder().freeText(true).radio(true).build());
  page.rig.radio.setUp(true);
  page.rig.radio.addPeer("Node One", true, 0);
  page.rig.core.setRadio(&page.rig.radio);
  page.rig.session.recipient = "Node One";
  page.rig.session.viaRadio = true;
  page.rig.poll();
  page.screen.render(page.rig.frame);
  TEST_ASSERT_TRUE(page.says("To (radio)", FontSize::Small));
  TEST_ASSERT_TRUE(page.says("Node One"));
  page.type("Hello");
  page.key(Key::Select);
  TEST_ASSERT_EQUAL(1, page.rig.radio.sent().size());
  TEST_ASSERT_EQUAL_STRING("Hello", page.rig.radio.sent()[0].text.c_str());
  TEST_ASSERT_EQUAL(0, page.rig.modem.sent().size());
  // A radio contact has no number to choose another from: a tap on the name does nothing.
  Page again(ProfileBuilder().freeText(true).radio(true).build());
  again.rig.session.recipient = "Node One";
  again.rig.session.viaRadio = true;
  again.screen.render(again.rig.frame);
  const Rect line = shows::where(again.rig.frame, "Node One", FontSize::Text, kContent).value();
  TEST_ASSERT_FALSE(again.screen.handle(Input::touch(line.x + 3, line.y + 3)));
}

// ---- ready-made messages --------------------------------------------------------------------------

void test_without_free_text_it_offers_ready_made_messages_and_a_tap_sends_one() {
  Page page(ProfileBuilder().childName("Alex").freeText(false).build());
  TEST_ASSERT_TRUE(page.says("Send one of these:", FontSize::Small));
  for (const char* words : {"I am on my way", "I have arrived", "Please call me", "I will be late", "OK"})   // five fit the window, the sixth is behind it
    TEST_ASSERT_TRUE_MESSAGE(page.says(words) || shows::text(page.rig.frame, words, FontSize::Text, kContent, true), words);
  TEST_ASSERT_EQUAL_STRING("", page.screen.text().c_str());
  page.type("free text");                                            // typing is not offered
  TEST_ASSERT_EQUAL_STRING("", page.screen.text().c_str());
  TEST_ASSERT_TRUE(page.screen.softKeys().scroll);
  TEST_ASSERT_EQUAL_STRING("Send", page.screen.softKeys().action.c_str());
  page.key(Key::Down);
  page.key(Key::Select);
  TEST_ASSERT_EQUAL(1, page.rig.modem.sent().size());
  TEST_ASSERT_EQUAL_STRING("I have arrived", page.rig.modem.sent()[0].body.c_str());
  TEST_ASSERT_EQUAL_STRING("home", page.rig.nav.last().c_str());
}

void test_a_ready_made_message_goes_out_in_the_parents_language_not_the_childs() {
  Page page(ProfileBuilder().childName("Alex").freeText(false).smsLanguage("de").build());
  page.rig.language("fr");
  page.screen.render(page.rig.frame);
  TEST_ASSERT_TRUE(page.says("Je suis en route") || shows::text(page.rig.frame, "Je suis en route", FontSize::Text, kContent, true));
  page.key(Key::Select);
  TEST_ASSERT_EQUAL_STRING("Ich bin unterwegs", page.rig.modem.sent()[0].body.c_str());
}

void test_a_tap_on_a_ready_made_message_sends_it_and_back_goes_back() {
  Page page(ProfileBuilder().childName("Alex").freeText(false).build());
  const Rect first = shows::where(page.rig.frame, "I am on my way", FontSize::Text, kContent, true).value();
  TEST_ASSERT_TRUE(page.screen.handle(Input::touch(first.x + 5, first.y + 5)));      // the first row, shown selected
  TEST_ASSERT_EQUAL_STRING("I am on my way", page.rig.modem.sent()[0].body.c_str());
  page.rig.session.recipient = kParentOne;
  for (int i = 0; i < 5; ++i) page.key(Key::Down);                                   // the sixth is behind the window
  const Rect row = shows::where(page.rig.frame, "Thank you", FontSize::Text, kContent, true).value();
  TEST_ASSERT_TRUE(page.screen.handle(Input::touch(row.x + 5, row.y + 5)));
  TEST_ASSERT_EQUAL_STRING("Thank you", page.rig.modem.sent()[1].body.c_str());
  Page second(ProfileBuilder().freeText(false).build());
  second.key(Key::Back);
  TEST_ASSERT_EQUAL_STRING("back", second.rig.nav.last().c_str());
}

void test_the_ready_made_messages_are_all_plain_sms_text_in_every_shipped_language() {
  // They go out as one SMS: a text with an accent is sent as UCS-2, which holds 70 characters, so each must fit
  // in 70 and be made of printable characters. (A word like "später" is right German; plain ASCII is not asked for.)
  i18n::Translator t = languages::loaded();
  for (const std::string& code : {std::string("en"), std::string("de"), std::string("fr")}) {
    for (const char* key : {"sms.canned.on-my-way", "sms.canned.arrived", "sms.canned.call-me", "sms.canned.late", "sms.canned.ok", "sms.canned.thanks"}) {
      const std::string text = t.getIn(code, key);
      TEST_ASSERT_TRUE_MESSAGE(!text.empty() && text != key, (code + " " + key).c_str());
      TEST_ASSERT_TRUE_MESSAGE(utf8::count(text.c_str(), text.size()) <= 70, (code + " " + key).c_str());
      for (unsigned char c : text) TEST_ASSERT_TRUE_MESSAGE(c >= 0x20 && c != 0x7F, (code + " " + key).c_str());
    }
  }
}

// ---- what it costs the panel -------------------------------------------------------------------------

void test_typing_160_characters_costs_at_most_one_refresh_a_key_and_few_full_ones() {
  Page page;
  RefreshRig panel;
  panel.frame(0, [&](Display& d) { page.screen.render(d); });
  panel.mark();
  for (int i = 0; i < 160; ++i) {
    page.screen.handle(Input::text(static_cast<uint32_t>('a' + i % 26)));
    panel.frame(300u * static_cast<uint32_t>(i + 1), [&](Display& d) { page.screen.render(d); });
  }
  TEST_ASSERT_EQUAL(160, page.screen.characters());
  TEST_ASSERT_TRUE_MESSAGE(panel.partial() + panel.full() <= 161, panel.history().c_str());
  TEST_ASSERT_TRUE_MESSAGE(panel.full() <= 17, panel.history().c_str());
}

// ---- soft keys and layout -----------------------------------------------------------------------------

void test_the_soft_keys_work_by_touch() {
  Page page;
  page.type("Hi");
  const SoftKeySlots slots = softKeySlots(page.screen.softKeys(), ScreenLayout::standard().softKeys);
  page.screen.handle(Input::touch(slots.back.x + 3, slots.back.y + 3));
  TEST_ASSERT_EQUAL_STRING("H", page.screen.text().c_str());
  page.screen.handle(Input::touch(slots.action.x + 3, slots.action.y + 3));
  TEST_ASSERT_EQUAL(1, page.rig.modem.sent().size());
}

void test_the_page_is_drawn_inside_the_screen_in_every_language() {
  languages::forEach([](const std::string& language, i18n::Translator&) {
    for (int kind = 0; kind < 5; ++kind) {
      DeviceRig rig(kind == 4 ? ProfileBuilder().freeText(false).build() : freeText());
      rig.language(language);
      ComposeScreen page(rig.context());
      if (kind == 3) {
        rig.session.recipient = kStranger;
      }
      page.onEnter();
      rig.poll();
      if (kind == 1) for (char c : std::string(160, 'w')) page.handle(Input::text(c));
      if (kind == 2) {
        for (char c : std::string("m")) page.handle(Input::text(c));
        page.handle(Input::text(0xE4));
        for (char c : std::string(68, 'W')) page.handle(Input::text(c));
      }
      FrameBuffer fb;
      page.render(fb);
      TEST_ASSERT_FALSE_MESSAGE(fb.clipped(), (language + " " + std::to_string(kind) + ":\n" + fb.describeClips()).c_str());
    }
  });
}

void test_the_pictures_in_english_and_german() {
  for (const char* language : {"en", "de"}) {
    DeviceRig rig(freeText());
    rig.language(language);
    ComposeScreen page(rig.context());
    page.onEnter();
    rig.poll();
    for (char c : std::string("Hello, I am at the station")) page.handle(Input::text(c));
    page.render(rig.frame);
    golden::expect(rig.frame, std::string("compose_text_") + language);
    page.handle(Input::text(0xE4));
    page.render(rig.frame);
    golden::expect(rig.frame, std::string("compose_wide_") + language);
    DeviceRig canned(ProfileBuilder().freeText(false).build());
    canned.language(language);
    ComposeScreen list(canned.context());
    list.onEnter();
    canned.poll();
    list.render(canned.frame);
    golden::expect(canned.frame, std::string("compose_canned_") + language);
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_with_nobody_chosen_it_starts_with_the_first_call_target);
  RUN_TEST(test_without_call_targets_it_starts_with_the_first_contact_and_with_nobody_it_says_so);
  RUN_TEST(test_a_chosen_recipient_is_not_replaced);
  RUN_TEST(test_a_number_in_no_book_is_shown_in_full_and_a_tap_on_the_recipient_opens_the_choice);
  RUN_TEST(test_the_draft_stays_while_the_page_is_left_and_entered_again);
  RUN_TEST(test_typing_shows_the_text_with_the_count_and_a_bar_after_it);
  RUN_TEST(test_backspace_deletes_a_whole_character_and_on_an_empty_page_goes_back);
  RUN_TEST(test_what_the_display_can_draw_is_taken_and_nothing_else);
  RUN_TEST(test_a_message_in_each_language_goes_out_exactly_as_typed);
  RUN_TEST(test_plain_text_takes_160_characters_and_the_161st_is_not_taken);
  RUN_TEST(test_the_first_accent_lowers_the_limit_to_70_and_the_page_says_why);
  RUN_TEST(test_an_accent_is_not_taken_when_the_text_is_already_longer_than_the_wide_limit);
  RUN_TEST(test_what_the_history_keeps_is_the_second_limit);
  RUN_TEST(test_enter_sends_and_the_page_goes_home_with_nothing_left_behind);
  RUN_TEST(test_nothing_is_sent_with_nothing_written);
  RUN_TEST(test_without_a_network_the_message_waits_and_the_page_still_goes_home);
  RUN_TEST(test_when_the_core_cannot_take_the_message_the_text_stays);
  RUN_TEST(test_school_time_does_not_stop_a_message);
  RUN_TEST(test_a_radio_contact_gets_the_message_over_the_radio);
  RUN_TEST(test_without_free_text_it_offers_ready_made_messages_and_a_tap_sends_one);
  RUN_TEST(test_a_ready_made_message_goes_out_in_the_parents_language_not_the_childs);
  RUN_TEST(test_a_tap_on_a_ready_made_message_sends_it_and_back_goes_back);
  RUN_TEST(test_the_ready_made_messages_are_all_plain_sms_text_in_every_shipped_language);
  RUN_TEST(test_typing_160_characters_costs_at_most_one_refresh_a_key_and_few_full_ones);
  RUN_TEST(test_the_soft_keys_work_by_touch);
  RUN_TEST(test_the_page_is_drawn_inside_the_screen_in_every_language);
  RUN_TEST(test_the_pictures_in_english_and_german);
  return UNITY_END();
}

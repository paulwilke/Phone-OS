// Host tests for the message history screen: the lists, the marks, words from codes, the time, reading,
// reply and call back, delete, touch, the character set of received text, and the layout in every
// language.
// allow-non-english: the catalogs under test contain German and French.

#include <unity.h>

#include <DeviceRig.h>
#include <Shows.h>
#include <app/SystemEntry.h>
#include <ui/Fonts.h>
#include <ui/Text.h>
#include <ui/screens/MessageListScreen.h>

using namespace people;
using namespace ui;

void setUp() {}
void tearDown() {}

namespace {

const Rect kContent = ScreenLayout::standard().content;
const char* const kEnvelope = "\xE2\x9C\x89";

struct Page {
  DeviceRig rig;
  MessageListScreen screen;
  Page() : rig(ProfileBuilder().childName("Alex").build()), screen(rig.context()) { start(); }
  explicit Page(Profile profile) : rig(std::move(profile)), screen(rig.context()) { start(); }
  void start() {
    rig.run(10);
    screen.onEnter();
    screen.render(rig.frame);
  }
  void draw() { screen.render(rig.frame); }
  void receive(const char* from, const char* text) {
    rig.modem.injectSms(from, text, 1);
    rig.poll();
    draw();
  }
  void key(Key k) {
    screen.handle(Input::pressed(k));
    draw();
  }
  void letter(char c) {
    screen.handle(Input::text(static_cast<uint32_t>(c)));
    draw();
  }
  bool says(const std::string& words, FontSize size, bool onBlack = false) {
    return shows::text(rig.frame, words, size, kContent, onBlack);
  }
  // What a row shows of a text: shortened with dots when it does not fit its line.
  static std::string inRow(const std::string& words) { return text::ellipsize(font(FontSize::Text), words, kContent.w - 8); }
  // A text drawn black on white or, when the row is the selected one, white on black.
  bool saysEither(const std::string& words, FontSize size) { return says(words, size) || says(words, size, true); }
};

}  // namespace

// ---- the lists --------------------------------------------------------------------------------

void test_an_empty_history_says_so_in_both_lists() {
  Page page;
  TEST_ASSERT_TRUE(page.says("No messages", FontSize::Text));
  TEST_ASSERT_TRUE(page.says("Inbox  (O: Sent)", FontSize::Small));
  TEST_ASSERT_TRUE(page.screen.softKeys().action.empty());
  TEST_ASSERT_FALSE(page.screen.softKeys().scroll);
  page.letter('o');
  TEST_ASSERT_TRUE(page.screen.showingSent());
  TEST_ASSERT_TRUE(page.says("Nothing sent", FontSize::Text));
  TEST_ASSERT_TRUE(page.says("Sent  (O: Inbox)", FontSize::Small));
}

void test_the_newest_message_is_first_and_unread_ones_are_marked() {
  Page page;
  page.receive(kParentOne, "Older message");
  page.receive(kParentTwo, "Newer message");
  const Rect newer = shows::where(page.rig.frame, "Newer message", FontSize::Text, kContent, true).value();   // selected: first
  const Rect older = shows::where(page.rig.frame, "Older message", FontSize::Text, kContent).value();
  TEST_ASSERT_TRUE(newer.y < older.y);
  // The mark and the name: the head line of the newer one.
  TEST_ASSERT_TRUE(page.says(std::string(kEnvelope) + " Parent Two  14:32", FontSize::Small, true));
  TEST_ASSERT_TRUE(page.says(std::string(kEnvelope) + " Parent One  14:32", FontSize::Small));
  TEST_ASSERT_EQUAL(2, page.rig.core.state().unread);
}

void test_reading_a_message_marks_the_history_read() {
  Page page;
  page.receive(kParentOne, "Hello");
  page.key(Key::Select);
  TEST_ASSERT_TRUE(page.screen.mode() == MessageListScreen::Mode::Message);
  TEST_ASSERT_EQUAL(0, page.rig.core.state().unread);
  page.key(Key::Back);
  TEST_ASSERT_FALSE(page.says(std::string(kEnvelope) + " Parent One  14:32", FontSize::Small, true));
  TEST_ASSERT_TRUE(page.says("Parent One  14:32", FontSize::Small, true));          // the same row, no mark
}

void test_the_selection_moves_and_the_window_follows_in_a_long_list() {
  Page page;
  for (int i = 1; i <= 9; ++i) page.receive(kParentOne, ("Message number " + std::to_string(i)).c_str());
  TEST_ASSERT_TRUE(page.saysEither("Message number 9", FontSize::Text));
  TEST_ASSERT_FALSE(page.saysEither("Message number 1", FontSize::Text));        // behind the window
  for (int i = 0; i < 8; ++i) page.key(Key::Down);
  TEST_ASSERT_TRUE(page.saysEither("Message number 1", FontSize::Text));         // the window followed
  page.key(Key::Down);                                                           // the end: it stops
  TEST_ASSERT_TRUE(page.saysEither("Message number 1", FontSize::Text));
  for (int i = 0; i < 12; ++i) page.key(Key::Up);
  TEST_ASSERT_TRUE(page.saysEither("Message number 9", FontSize::Text));
}

void test_what_was_sent_is_in_the_other_list_with_what_became_of_it() {
  Page page;
  page.rig.core.setCellularEnabled(false);
  page.rig.core.send(kParentOne, "Waiting text");
  page.rig.poll();
  page.letter('o');
  TEST_ASSERT_TRUE(page.saysEither("Waiting text", FontSize::Text));
  TEST_ASSERT_TRUE(page.says("You  14:32 (waiting)", FontSize::Small, true));
  page.rig.core.setCellularEnabled(true);
  page.rig.run(5);                                                                // the queue goes out
  page.draw();
  TEST_ASSERT_TRUE(page.says("You  14:32", FontSize::Small, true));
  TEST_ASSERT_FALSE(page.says("You  14:32 (waiting)", FontSize::Small, true));
}

void test_a_radio_message_says_whether_it_arrived_and_an_sms_says_nothing() {
  Page page(ProfileBuilder().radio(true).freeText(true).build());
  page.rig.radio.setUp(true);
  page.rig.radio.addPeer("Node One", true, 0);
  page.rig.core.setRadio(&page.rig.radio);
  page.rig.poll();
  page.rig.core.sendRadio("Node One", "By radio");
  page.rig.core.send(kParentOne, "By sms");
  page.rig.radio.confirm(true);
  page.rig.poll();
  page.letter('o');
  TEST_ASSERT_TRUE(page.says("~ You  14:32 arrived", FontSize::Small) || page.says("~ You  14:32 arrived", FontSize::Small, true));
  TEST_ASSERT_TRUE(page.saysEither("You  14:32", FontSize::Small));                // the SMS: no mark
  Page unconfirmed(ProfileBuilder().radio(true).freeText(true).build());
  unconfirmed.rig.radio.setUp(true);
  unconfirmed.rig.radio.addPeer("Node One", true, 0);
  unconfirmed.rig.core.setRadio(&unconfirmed.rig.radio);
  unconfirmed.rig.poll();
  unconfirmed.rig.core.sendRadio("Node One", "By radio");
  unconfirmed.rig.radio.confirm(false);
  unconfirmed.rig.poll();
  unconfirmed.letter('o');
  TEST_ASSERT_TRUE(unconfirmed.saysEither("~ You  14:32 unconfirmed", FontSize::Small));
}

void test_a_radio_message_that_came_in_is_marked_and_named_by_its_radio_name() {
  Page page(ProfileBuilder().radio(true).build());
  page.rig.radio.setUp(true);
  page.rig.radio.addPeer("Node One", true, 0);
  page.rig.core.setRadio(&page.rig.radio);
  page.rig.radio.deliver("Node One", "Over the air", 1);
  page.rig.poll();
  page.draw();
  TEST_ASSERT_TRUE(page.says(std::string(kEnvelope) + " ~ Node One  14:32", FontSize::Small, true));
}

// ---- entries of the device ----------------------------------------------------------------------------

void test_a_missed_call_is_worded_from_its_code_in_the_language_of_the_moment() {
  Page page;
  page.rig.modem.injectIncomingCall(kParentOne);
  page.rig.poll();
  page.rig.modem.remoteHangup();
  page.rig.poll();
  page.draw();
  TEST_ASSERT_TRUE(page.saysEither(Page::inRow("Missed call from Parent One"), FontSize::Text));
  TEST_ASSERT_TRUE(page.saysEither(std::string(kEnvelope) + " ! 14:32", FontSize::Small));
  page.rig.language("de");
  page.draw();
  TEST_ASSERT_TRUE(page.saysEither(Page::inRow("Verpasster Anruf von Parent One"), FontSize::Text));    // yesterday's entry, today's words
  page.rig.language("fr");
  page.draw();
  TEST_ASSERT_TRUE(page.saysEither(Page::inRow("Appel manqu\xC3\xA9 de Parent One"), FontSize::Text));
}

void test_an_entry_that_is_open_changes_its_words_when_the_language_changes() {
  Page page;
  page.rig.modem.injectIncomingCall(kParentOne);
  page.rig.poll();
  page.rig.modem.remoteHangup();
  page.rig.poll();
  page.draw();
  page.key(Key::Select);                                                       // read it: the whole entry
  page.rig.language("de");
  page.draw();
  for (std::string_view line : text::wrap(font(FontSize::Text), "Verpasster Anruf von Parent One", kContent.w - 8))
    TEST_ASSERT_TRUE_MESSAGE(page.saysEither(std::string(line), FontSize::Text), std::string(line).c_str());
  page.rig.language("en");
  page.draw();
  for (std::string_view line : text::wrap(font(FontSize::Text), "Missed call from Parent One", kContent.w - 8))
    TEST_ASSERT_TRUE_MESSAGE(page.saysEither(std::string(line), FontSize::Text), std::string(line).c_str());
}

void test_the_whole_entry_is_in_the_message_view_and_the_failed_location_reply_is_there_too() {
  Page page;
  sysentry::record(const_cast<MessageStore&>(page.rig.core.history()), rig::kTuesdayAfternoon, sysentry::kLocationQueryFailed, kParentTwo);
  page.rig.poll();
  page.draw();
  page.key(Key::Select);
  for (std::string_view line : text::wrap(font(FontSize::Text), "Location reply to Parent Two failed", kContent.w - 8))
    TEST_ASSERT_TRUE_MESSAGE(page.saysEither(std::string(line), FontSize::Text), std::string(line).c_str());
  TEST_ASSERT_TRUE(page.says("14:32", FontSize::Small));
}

// ---- the time -------------------------------------------------------------------------------------------

void test_an_earlier_day_has_its_date_and_an_unknown_time_is_dashes() {
  Page page;
  page.rig.clock.setEpochUtc(rig::kTuesdayAfternoon - 86400);                  // yesterday, the 14th
  page.receive(kParentOne, "Yesterday");
  page.rig.clock.setEpochUtc(0);
  page.receive(kParentOne, "No time then");
  page.rig.clock.setEpochUtc(rig::kTuesdayAfternoon);
  page.rig.poll();
  page.draw();
  TEST_ASSERT_TRUE(page.saysEither(std::string(kEnvelope) + " Parent One  09/14 14:32", FontSize::Small));
  TEST_ASSERT_TRUE(page.saysEither(std::string(kEnvelope) + " Parent One  --:--", FontSize::Small));
  page.rig.language("de");
  page.draw();
  TEST_ASSERT_TRUE(page.saysEither(std::string(kEnvelope) + " Parent One  14.09. 14:32", FontSize::Small));
}

// ---- reading ------------------------------------------------------------------------------------------------

void test_the_message_view_has_the_head_the_whole_text_and_the_hint() {
  Page page;
  page.receive(kParentOne, "Dinner is at six. Bring your bag and do not forget the keys.");
  page.key(Key::Select);
  TEST_ASSERT_TRUE(page.says("14:32 from Parent One", FontSize::Small));
  TEST_ASSERT_TRUE(page.says("X: delete message", FontSize::Small));
  for (std::string_view line : text::wrap(font(FontSize::Text), "Dinner is at six. Bring your bag and do not forget the keys.", kContent.w - 8))
    TEST_ASSERT_TRUE_MESSAGE(page.says(std::string(line), FontSize::Text), std::string(line).c_str());
  TEST_ASSERT_EQUAL_STRING("Back", page.screen.softKeys().back.c_str());
  page.key(Key::Back);
  TEST_ASSERT_TRUE(page.screen.mode() == MessageListScreen::Mode::List);
  TEST_ASSERT_EQUAL(0, page.rig.nav.calls.size());                         // back from a message is not back from the page
  page.key(Key::Back);
  TEST_ASSERT_EQUAL_STRING("back", page.rig.nav.last().c_str());
}

void test_a_long_message_is_scrolled_to_its_end() {
  Page page;
  std::string text;
  for (int i = 1; i <= 14; ++i) text += "WWWWWWW" + std::string(1, static_cast<char>('A' + i - 1)) + " ";   // wide words: one to a line, and 160 bytes at most are kept
  text += "theend";
  page.receive(kParentOne, text.c_str());
  page.key(Key::Select);
  TEST_ASSERT_FALSE(page.says("theend", FontSize::Text));            // below the window
  for (int i = 0; i < 14; ++i) page.key(Key::Down);
  // The text block wraps to its width less the scroll bar: its last line is on the screen.
  const auto lines = text::wrap(font(FontSize::Text), text, kContent.w - 8);
  TEST_ASSERT_TRUE(page.says(std::string(lines.back()), FontSize::Text));
  TEST_ASSERT_FALSE(page.says("WWWWWWWA", FontSize::Text));    // the start is scrolled out
  TEST_ASSERT_TRUE(page.screen.softKeys().scroll);
}

void test_a_long_name_is_shortened_in_the_row_and_in_the_head_and_a_number_never() {
  // Only a number the device accepts reaches the history; a call target is one that is no contact.
  Page page(ProfileBuilder().noContacts().contact(kGrandma, "Grandma Elisabeth Maria", false).callTargets({kStranger}).build());
  page.receive(kGrandma, "Hello");
  page.receive(kStranger, "Who is this");
  FrameBuffer fb;
  page.screen.render(fb);
  TEST_ASSERT_FALSE_MESSAGE(fb.clipped(), fb.describeClips().c_str());
  TEST_ASSERT_TRUE(page.saysEither(std::string(kEnvelope) + " " + kStranger + "  14:32", FontSize::Small));   // the number, whole
  page.key(Key::Down);
  page.key(Key::Select);
  const std::string room = "14:32 from ";
  TEST_ASSERT_TRUE(page.says(room + "Grandma Elisabeth Maria", FontSize::Small) ||
                   page.says(text::ellipsize(font(FontSize::Small), room + "Grandma Elisabeth Maria", kContent.w - 8), FontSize::Small));
}

// ---- reply and call back -------------------------------------------------------------------------------------------

void test_reply_goes_to_the_sender_who_is_a_contact_even_without_free_text() {
  Page page;
  page.receive(kGrandma, "Hello dear");
  page.key(Key::Select);
  TEST_ASSERT_EQUAL_STRING("Reply", page.screen.softKeys().action.c_str());
  page.key(Key::Select);
  TEST_ASSERT_EQUAL_STRING(kGrandma, page.rig.session.recipient.c_str());
  TEST_ASSERT_FALSE(page.rig.session.viaRadio);
  TEST_ASSERT_TRUE(page.rig.nav.opened(screen::kCompose));
}

void test_a_stranger_may_be_answered_only_where_free_recipients_are_allowed() {
  Page closed(ProfileBuilder().childName("Alex").callTargets({kStranger}).build());
  closed.receive(kStranger, "Hi");
  closed.key(Key::Select);
  TEST_ASSERT_TRUE(closed.screen.softKeys().action.empty());
  TEST_ASSERT_FALSE(closed.screen.handle(Input::pressed(Key::Select)));
  TEST_ASSERT_EQUAL(0, closed.rig.nav.calls.size());
  Page open(ProfileBuilder().freeText(true).callTargets({kStranger}).build());
  open.receive(kStranger, "Hi");
  open.key(Key::Select);
  TEST_ASSERT_EQUAL_STRING("Reply", open.screen.softKeys().action.c_str());
  open.key(Key::Select);
  TEST_ASSERT_EQUAL_STRING(kStranger, open.rig.session.recipient.c_str());
}

void test_a_radio_message_is_answered_over_the_radio() {
  Page page(ProfileBuilder().radio(true).freeText(true).build());
  page.rig.radio.setUp(true);
  page.rig.radio.addPeer("Node One", true, 0);
  page.rig.core.setRadio(&page.rig.radio);
  page.rig.radio.deliver("Node One", "Over the air", 1);
  page.rig.poll();
  page.draw();
  page.key(Key::Select);
  page.key(Key::Select);
  TEST_ASSERT_EQUAL_STRING("Node One", page.rig.session.recipient.c_str());
  TEST_ASSERT_TRUE(page.rig.session.viaRadio);
  Page closed(ProfileBuilder().radio(true).freeText(false).build());
  closed.rig.radio.setUp(true);
  closed.rig.radio.addPeer("Node One", true, 0);
  closed.rig.core.setRadio(&closed.rig.radio);
  closed.rig.radio.deliver("Node One", "Over the air", 1);
  closed.rig.poll();
  closed.draw();
  closed.key(Key::Select);
  TEST_ASSERT_TRUE(closed.screen.softKeys().action.empty());       // a radio contact is not in the book
}

void test_a_missed_call_can_be_called_back_when_the_number_may_be_called() {
  Page page;
  page.rig.modem.injectIncomingCall(kParentOne);
  page.rig.poll();
  page.rig.modem.remoteHangup();
  page.rig.poll();
  page.draw();
  page.key(Key::Select);
  TEST_ASSERT_EQUAL_STRING("Call back", page.screen.softKeys().action.c_str());
  page.key(Key::Select);
  TEST_ASSERT_TRUE(page.rig.nav.opened(screen::kCall));
  TEST_ASSERT_EQUAL_STRING(kParentOne, page.rig.core.state().callPeer.c_str());
  Page stranger;
  stranger.rig.modem.injectIncomingCall(kStranger);
  stranger.rig.poll();
  stranger.rig.modem.remoteHangup();
  stranger.rig.poll();
  stranger.draw();
  stranger.key(Key::Select);
  TEST_ASSERT_TRUE(stranger.screen.softKeys().action.empty());      // a number the child may not call is not offered
}

void test_nothing_is_offered_for_an_outgoing_message() {
  Page page;
  page.rig.core.send(kParentOne, "Mine");
  page.rig.poll();
  page.letter('o');
  page.key(Key::Select);
  TEST_ASSERT_TRUE(page.screen.mode() == MessageListScreen::Mode::Message);
  TEST_ASSERT_TRUE(page.screen.softKeys().action.empty());
  TEST_ASSERT_TRUE(page.says("14:32 to Parent One", FontSize::Small));
}

// ---- delete ----------------------------------------------------------------------------------------------------------

void test_x_asks_first_and_keep_changes_nothing() {
  Page page;
  page.receive(kParentOne, "Keep me");
  page.letter('x');
  TEST_ASSERT_TRUE(page.screen.mode() == MessageListScreen::Mode::Delete);
  TEST_ASSERT_TRUE(page.says("Delete this message?", FontSize::Text));
  TEST_ASSERT_EQUAL_STRING("Keep", page.screen.softKeys().back.c_str());
  TEST_ASSERT_EQUAL_STRING("Delete", page.screen.softKeys().action.c_str());
  page.key(Key::Back);
  TEST_ASSERT_TRUE(page.screen.mode() == MessageListScreen::Mode::List);
  TEST_ASSERT_EQUAL(1, page.rig.core.history().count());
}

void test_delete_removes_the_message_and_the_selection_stays_in_the_list() {
  Page page;
  page.receive(kParentOne, "First");
  page.receive(kParentOne, "Second");
  page.receive(kParentOne, "Third");
  page.key(Key::Down);
  page.key(Key::Down);                                              // the oldest
  page.letter('x');
  page.key(Key::Select);
  TEST_ASSERT_EQUAL(2, page.rig.core.history().count());
  TEST_ASSERT_TRUE(page.screen.mode() == MessageListScreen::Mode::List);
  TEST_ASSERT_TRUE(page.saysEither("Second", FontSize::Text));       // the selection moved up to the last one left
  TEST_ASSERT_FALSE(page.saysEither("First", FontSize::Text));
}

void test_deleting_from_the_message_view_returns_to_the_list_and_keep_returns_to_the_message() {
  Page page;
  page.receive(kParentOne, "To read");
  page.key(Key::Select);
  page.letter('X');
  page.key(Key::Back);
  TEST_ASSERT_TRUE(page.screen.mode() == MessageListScreen::Mode::Message);
  page.letter('x');
  page.key(Key::Select);
  TEST_ASSERT_TRUE(page.screen.mode() == MessageListScreen::Mode::List);
  TEST_ASSERT_EQUAL(0, page.rig.core.history().count());
  TEST_ASSERT_TRUE(page.says("No messages", FontSize::Text));
  page.letter('x');                                                 // nothing to delete
  TEST_ASSERT_TRUE(page.screen.mode() == MessageListScreen::Mode::List);
}

void test_deleting_a_waiting_message_does_not_cancel_its_sending() {
  Page page;
  page.rig.core.setCellularEnabled(false);
  page.rig.core.send(kParentOne, "Must still go");
  page.rig.poll();
  page.letter('o');
  page.letter('x');
  page.key(Key::Select);
  TEST_ASSERT_EQUAL(0, page.rig.core.history().count());
  page.rig.core.setCellularEnabled(true);
  page.rig.run(5);
  TEST_ASSERT_EQUAL(1, page.rig.modem.sent().size());
  TEST_ASSERT_EQUAL_STRING("Must still go", page.rig.modem.sent()[0].body.c_str());
}

// ---- touch ---------------------------------------------------------------------------------------------------------------

void test_a_tap_on_a_row_opens_it_a_tap_on_the_header_changes_the_list_and_a_tap_below_does_nothing() {
  Page page;
  page.receive(kParentOne, "Tap me");
  const Rect row = shows::where(page.rig.frame, "Tap me", FontSize::Text, kContent, true).value();
  TEST_ASSERT_TRUE(page.screen.handle(Input::touch(row.x + 5, row.y + 5)));
  TEST_ASSERT_TRUE(page.screen.mode() == MessageListScreen::Mode::Message);
  page.key(Key::Back);
  TEST_ASSERT_FALSE(page.screen.handle(Input::touch(120, kContent.bottom() - 4)));
  TEST_ASSERT_TRUE(page.screen.mode() == MessageListScreen::Mode::List);
  TEST_ASSERT_TRUE(page.screen.handle(Input::touch(100, kContent.y + 4)));
  TEST_ASSERT_TRUE(page.screen.showingSent());
}

void test_the_soft_keys_work_by_touch_and_name_the_action() {
  Page page;
  page.receive(kParentOne, "Hi");
  TEST_ASSERT_EQUAL_STRING("Read", page.screen.softKeys().action.c_str());
  const SoftKeySlots slots = softKeySlots(page.screen.softKeys(), ScreenLayout::standard().softKeys);
  page.screen.handle(Input::touch(slots.action.x + 3, slots.action.y + 3));
  TEST_ASSERT_TRUE(page.screen.mode() == MessageListScreen::Mode::Message);
}

void test_the_page_opens_on_the_inbox_each_time() {
  Page page;
  page.letter('o');
  page.screen.onLeave();
  page.screen.onEnter();
  TEST_ASSERT_FALSE(page.screen.showingSent());
}

// ---- the character set of what arrives ---------------------------------------------------------------------------------------

void test_a_message_in_each_language_is_shown_with_every_character_it_has() {
  const struct { const char* language; const char* text; } messages[] = {
      {"en", "At the caf\xC3\xA9 - 5 \xE2\x82\xAC"},
      {"de", "Gr\xC3\xBC\xC3\x9F" "e aus K\xC3\xB6ln"},
      {"fr", "O\xC3\xB9 \xC5\x93uvre \xC5\xB8 \xC3\xA0 3 \xE2\x82\xAC"}};
  for (const auto& m : messages) {
    Page page;
    page.rig.language(m.language);
    page.receive(kParentOne, m.text);
    TEST_ASSERT_TRUE_MESSAGE(page.saysEither(m.text, FontSize::Text), m.language);
    page.key(Key::Select);
    TEST_ASSERT_TRUE_MESSAGE(page.saysEither(m.text, FontSize::Text), m.language);
  }
}

void test_what_the_font_cannot_show_is_a_box_and_the_rest_of_the_text_is_there() {
  // An emoji and a letter of another alphabet: the font's box, one for each, as docs/i18n.md says;
  // the characters around them are as they were.
  Page page;
  page.receive(kParentOne, "A\xF0\x9F\x98\x80" "B\xD0\x96" "C");
  FrameBuffer fb;
  page.screen.render(fb);
  TEST_ASSERT_FALSE(fb.clipped());
  TEST_ASSERT_TRUE(page.saysEither("A\xF0\x9F\x98\x80" "B\xD0\x96" "C", FontSize::Text));   // drawn by the same engine: boxes
  const std::string withBoxes = "A\xEF\xBF\xBD" "B\xEF\xBF\xBD" "C";                         // U+FFFD is the replacement, the same glyph
  TEST_ASSERT_TRUE(page.saysEither(withBoxes, FontSize::Text));
  page.key(Key::Select);
  TEST_ASSERT_TRUE(page.saysEither(withBoxes, FontSize::Text));
}

// ---- layout -----------------------------------------------------------------------------------------------------------

void test_every_view_is_drawn_inside_the_screen_in_every_language() {
  languages::forEach([](const std::string& language, i18n::Translator&) {
    DeviceRig rig(ProfileBuilder().noContacts().contact(kGrandma, "Grandma Elisabeth Maria", false).contact(kParentOne, "Parent One", true).callTargets({kStranger}).radio(true).build());
    rig.language(language);
    rig.run(10);
    rig.modem.injectSms(kGrandma, "A message that is long enough to be shortened in its row because it does not fit one line", 1);
    rig.modem.injectSms(kStranger, "From a number", 2);
    rig.modem.injectIncomingCall(kParentOne);
    rig.poll();
    rig.modem.remoteHangup();
    rig.poll();
    rig.core.send(kParentOne, "Out");
    rig.poll();
    MessageListScreen page(rig.context());
    page.onEnter();
    for (int view = 0; view < 6; ++view) {
      FrameBuffer fb;
      page.render(fb);
      // The pseudo-language is exempt from the width of the action word beside the arrows (94 pixels
      // with its symbol: "[Rëplýëö]" does not fit); everything else must stay inside the display.
      bool clipped = false;
      for (const auto& e : fb.clipEvents())
        clipped = clipped || language != i18n::kPseudoCode || e.firstY < ScreenLayout::standard().softKeys.y;
      TEST_ASSERT_FALSE_MESSAGE(clipped, (language + " view " + std::to_string(view) + ":\n" + fb.describeClips()).c_str());
      switch (view) {
        case 0: page.handle(Input::pressed(Key::Select)); break;                      // a message
        case 1: page.handle(Input::text('x')); break;                                  // its delete question
        case 2: page.handle(Input::pressed(Key::Back)); page.handle(Input::pressed(Key::Back)); page.handle(Input::text('o')); break;   // sent
        case 3: page.handle(Input::text('o')); page.handle(Input::pressed(Key::Down)); page.handle(Input::pressed(Key::Down)); break;  // a system entry
        case 4: page.handle(Input::pressed(Key::Select)); break;
        default: break;
      }
    }
  });
}

void test_the_pictures_in_english_and_german() {
  for (const char* language : {"en", "de"}) {
    DeviceRig rig;
    rig.language(language);
    rig.run(10);
    rig.modem.injectSms(kParentOne, "School is out, I will pick you up at the gate", 1);
    rig.modem.injectSms(kGrandma, "Hello dear, how was the day?", 2);
    rig.modem.injectIncomingCall(kParentTwo);
    rig.poll();
    rig.modem.remoteHangup();
    rig.poll();
    MessageListScreen page(rig.context());
    page.onEnter();
    page.render(rig.frame);
    golden::expect(rig.frame, std::string("messages_list_") + language);
    page.handle(Input::pressed(Key::Down));
    page.handle(Input::pressed(Key::Select));
    page.render(rig.frame);
    golden::expect(rig.frame, std::string("messages_read_") + language);
    page.handle(Input::text('x'));
    page.render(rig.frame);
    golden::expect(rig.frame, std::string("messages_delete_") + language);
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_an_empty_history_says_so_in_both_lists);
  RUN_TEST(test_the_newest_message_is_first_and_unread_ones_are_marked);
  RUN_TEST(test_reading_a_message_marks_the_history_read);
  RUN_TEST(test_the_selection_moves_and_the_window_follows_in_a_long_list);
  RUN_TEST(test_what_was_sent_is_in_the_other_list_with_what_became_of_it);
  RUN_TEST(test_a_radio_message_says_whether_it_arrived_and_an_sms_says_nothing);
  RUN_TEST(test_a_radio_message_that_came_in_is_marked_and_named_by_its_radio_name);
  RUN_TEST(test_a_missed_call_is_worded_from_its_code_in_the_language_of_the_moment);
  RUN_TEST(test_an_entry_that_is_open_changes_its_words_when_the_language_changes);
  RUN_TEST(test_the_whole_entry_is_in_the_message_view_and_the_failed_location_reply_is_there_too);
  RUN_TEST(test_an_earlier_day_has_its_date_and_an_unknown_time_is_dashes);
  RUN_TEST(test_the_message_view_has_the_head_the_whole_text_and_the_hint);
  RUN_TEST(test_a_long_message_is_scrolled_to_its_end);
  RUN_TEST(test_a_long_name_is_shortened_in_the_row_and_in_the_head_and_a_number_never);
  RUN_TEST(test_reply_goes_to_the_sender_who_is_a_contact_even_without_free_text);
  RUN_TEST(test_a_stranger_may_be_answered_only_where_free_recipients_are_allowed);
  RUN_TEST(test_a_radio_message_is_answered_over_the_radio);
  RUN_TEST(test_a_missed_call_can_be_called_back_when_the_number_may_be_called);
  RUN_TEST(test_nothing_is_offered_for_an_outgoing_message);
  RUN_TEST(test_x_asks_first_and_keep_changes_nothing);
  RUN_TEST(test_delete_removes_the_message_and_the_selection_stays_in_the_list);
  RUN_TEST(test_deleting_from_the_message_view_returns_to_the_list_and_keep_returns_to_the_message);
  RUN_TEST(test_deleting_a_waiting_message_does_not_cancel_its_sending);
  RUN_TEST(test_a_tap_on_a_row_opens_it_a_tap_on_the_header_changes_the_list_and_a_tap_below_does_nothing);
  RUN_TEST(test_the_soft_keys_work_by_touch_and_name_the_action);
  RUN_TEST(test_the_page_opens_on_the_inbox_each_time);
  RUN_TEST(test_a_message_in_each_language_is_shown_with_every_character_it_has);
  RUN_TEST(test_what_the_font_cannot_show_is_a_box_and_the_rest_of_the_text_is_there);
  RUN_TEST(test_every_view_is_drawn_inside_the_screen_in_every_language);
  RUN_TEST(test_the_pictures_in_english_and_german);
  return UNITY_END();
}

// Host tests for the number page: typing by key and by touch, the limits of a number, the way on to
// writing the message, the profile switch, and the layout in every language.
// allow-non-english: the catalogs under test contain German and French.

#include <unity.h>

#include <DeviceRig.h>
#include <Shows.h>
#include <ui/Fonts.h>
#include <ui/Text.h>
#include <ui/screens/NumberScreen.h>

using namespace people;
using namespace ui;

void setUp() {}
void tearDown() {}

namespace {

// The longest number the page takes, a plus and fifteen digits: made, not written, so that it is plainly no real one.
const std::string kLongest = "+" + std::string(15, '7');

const Rect kContent = ScreenLayout::standard().content;

struct Page {
  DeviceRig rig;
  NumberScreen screen;
  Page() : rig(ProfileBuilder().childName("Alex").freeText(true).build()), screen(rig.context()) { start(); }
  explicit Page(Profile profile) : rig(std::move(profile)), screen(rig.context()) { start(); }
  void start() {
    screen.onEnter();
    rig.poll();
    screen.render(rig.frame);
  }
  void type(const std::string& keys) {
    for (char c : keys) screen.handle(Input::text(static_cast<uint32_t>(c)));
    screen.render(rig.frame);
  }
  void key(Key k) {
    screen.handle(Input::pressed(k));
    screen.render(rig.frame);
  }
  void draw() { screen.render(rig.frame); }
  // The twelve keys: where each is, found by tapping nothing but asking the same layout.
  bool tapKey(size_t index) {
    // The keys are drawn in a 3 x 4 grid below the field; probe the grid for the one whose label is `index`.
    const int top = kContent.y + 16 + 2 + font(FontSize::Tile).lineHeight() + 6 + 2 + 16 + 2;
    const int cellWidth = (kContent.w - 4 * 4) / 3;
    const int cellHeight = (kContent.bottom() - top - 5 * 4) / 4;
    const int col = static_cast<int>(index % 3), row = static_cast<int>(index / 3);
    return screen.handle(Input::touch(kContent.x + 4 + col * (cellWidth + 4) + cellWidth / 2, top + 4 + row * (cellHeight + 4) + cellHeight / 2));
  }
};

// Whatever size the page chose for the number, all of it is there.
bool showsNumber(const FrameBuffer& fb, const std::string& number) {
  for (FontSize size : {FontSize::Tile, FontSize::Text, FontSize::Small})
    if (shows::text(fb, number, size, kContent)) return true;
  return false;
}

}  // namespace

// ---- typing ----------------------------------------------------------------------------------

void test_the_keyboard_types_digits_and_a_plus_in_front() {
  Page page;
  TEST_ASSERT_TRUE(shows::text(page.rig.frame, "Enter a number", FontSize::Small, kContent));
  page.type("+49170");
  TEST_ASSERT_EQUAL_STRING("+49170", page.screen.number().c_str());
  TEST_ASSERT_TRUE(showsNumber(page.rig.frame, "+49170"));
}

void test_a_plus_anywhere_but_in_front_is_not_taken_and_other_characters_neither() {
  Page page;
  page.type("+");
  page.type("+");                                                    // a second plus
  page.type("4a9 -(1)7#0");                                          // letters, blanks and punctuation
  TEST_ASSERT_EQUAL_STRING("+4917" "0", page.screen.number().c_str());
  Page later;
  later.type("12+");                                                 // a plus in the middle
  TEST_ASSERT_EQUAL_STRING("12", later.screen.number().c_str());
  later.type("\xC3\xA9");                                            // a character outside ASCII is used up and ignored
  TEST_ASSERT_EQUAL_STRING("12", later.screen.number().c_str());
}

void test_a_number_has_at_most_fifteen_digits_and_the_sixteenth_is_ignored_not_cut_into_another_number() {
  Page page;
  page.type("+" + std::string(17, '7'));                             // plus and 17 digits
  TEST_ASSERT_EQUAL_STRING(kLongest.c_str(), page.screen.number().c_str());
  TEST_ASSERT_EQUAL(16, page.screen.number().size());                // the plus and fifteen digits
  TEST_ASSERT_TRUE(showsNumber(page.rig.frame, kLongest));           // all of it is there, in whatever size fits
  page.key(Key::Back);                                               // room again
  page.type("9");
  TEST_ASSERT_EQUAL_STRING(("+" + std::string(14, '7') + "9").c_str(), page.screen.number().c_str());
}

void test_the_full_number_is_drawn_in_a_size_that_fits_and_never_shortened() {
  Page page;
  page.type("+491709999999");
  TEST_ASSERT_TRUE(showsNumber(page.rig.frame, "+491709999999"));
  Page longest;
  longest.type(kLongest);
  TEST_ASSERT_TRUE(showsNumber(longest.rig.frame, kLongest));
  FrameBuffer fb;
  longest.screen.render(fb);
  TEST_ASSERT_FALSE(fb.clipped());
}

void test_an_empty_field_stays_empty() {
  Page page;
  FrameBuffer typed;
  Page other;
  other.type("0");
  const int first = kContent.y + 16;
  const Rect field{kContent.x + 4, first + 2, kContent.w - 8, font(FontSize::Tile).lineHeight()};
  TEST_ASSERT_EQUAL(0, page.rig.frame.blackPixels({field.x + 6, field.y + 6, field.w - 12, field.h - 12}));
  TEST_ASSERT_TRUE(other.rig.frame.blackPixels({field.x + 6, field.y + 6, field.w - 12, field.h - 12}) > 0);
}

void test_back_deletes_and_with_an_empty_field_goes_back() {
  Page page;
  page.type("+49");
  page.key(Key::Back);
  TEST_ASSERT_EQUAL_STRING("+4", page.screen.number().c_str());
  TEST_ASSERT_EQUAL_STRING("Delete", page.screen.softKeys().back.c_str());
  page.key(Key::Back);
  page.key(Key::Back);
  TEST_ASSERT_EQUAL_STRING("", page.screen.number().c_str());
  TEST_ASSERT_EQUAL(0, page.rig.nav.calls.size());                   // deleting never leaves
  TEST_ASSERT_EQUAL_STRING("Back", page.screen.softKeys().back.c_str());
  page.key(Key::Back);
  TEST_ASSERT_EQUAL_STRING("back", page.rig.nav.last().c_str());
}

void test_the_page_is_opened_empty_each_time() {
  Page page;
  page.type("0170");
  page.key(Key::Select);                                             // four digits go on to the message
  page.screen.onLeave();
  page.screen.onEnter();
  TEST_ASSERT_EQUAL_STRING("", page.screen.number().c_str());
}

// ---- the keypad --------------------------------------------------------------------------------

void test_the_twelve_keys_are_the_digits_the_plus_and_delete() {
  const char* expected[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "+", "0", "\xE2\x8C\xAB"};
  for (size_t i = 0; i < 12; ++i) TEST_ASSERT_EQUAL_STRING(expected[i], NumberScreen::keyLabel(i));
  TEST_ASSERT_EQUAL_STRING("", NumberScreen::keyLabel(12));
}

void test_a_tap_on_a_key_types_it_and_the_last_key_deletes() {
  Page page;
  TEST_ASSERT_TRUE(page.tapKey(9));                                  // +
  TEST_ASSERT_TRUE(page.tapKey(3));                                  // 4
  TEST_ASSERT_TRUE(page.tapKey(8));                                  // 9
  TEST_ASSERT_TRUE(page.tapKey(10));                                 // 0
  page.draw();
  TEST_ASSERT_EQUAL_STRING("+490", page.screen.number().c_str());
  TEST_ASSERT_TRUE(showsNumber(page.rig.frame, "+490"));
  TEST_ASSERT_TRUE(page.tapKey(11));                                 // delete
  TEST_ASSERT_EQUAL_STRING("+49", page.screen.number().c_str());
  TEST_ASSERT_TRUE(page.tapKey(9));                                  // a plus after digits: not taken
  TEST_ASSERT_EQUAL_STRING("+49", page.screen.number().c_str());
}

void test_every_key_of_the_keypad_is_drawn_and_the_tap_beside_the_keys_does_nothing() {
  Page page;
  for (size_t i = 0; i < 12; ++i) {
    const char* label = NumberScreen::keyLabel(i);
    TEST_ASSERT_TRUE_MESSAGE(shows::text(page.rig.frame, label, FontSize::Tile, kContent), label);
  }
  TEST_ASSERT_FALSE(page.screen.handle(Input::touch(120, kContent.y + 2)));   // the title
  TEST_ASSERT_EQUAL_STRING("", page.screen.number().c_str());
}

// ---- going on ---------------------------------------------------------------------------------

void test_next_hands_the_number_in_its_canonical_form_to_the_message_and_opens_it() {
  Page page;
  page.type("017012345");
  page.key(Key::Select);
  TEST_ASSERT_EQUAL_STRING("+4917012345", page.rig.session.recipient.c_str());
  TEST_ASSERT_FALSE(page.rig.session.viaRadio);
  TEST_ASSERT_TRUE(page.rig.nav.opened(screen::kCompose));
  TEST_ASSERT_FALSE(page.rig.core.state().callActive);               // never a call
}

void test_a_number_written_with_the_country_prefix_is_handed_on_as_it_is() {
  Page page;
  page.type(std::string("0049") + "1701111111");                      // the book's number with 00 for the plus
  page.key(Key::Select);
  TEST_ASSERT_EQUAL_STRING(kParentOne, page.rig.session.recipient.c_str());   // the same conversation as the book's contact
}

void test_a_short_number_is_a_slip_of_the_finger_and_the_page_says_so_under_the_field() {
  Page page;
  page.type("12");
  page.key(Key::Select);
  TEST_ASSERT_EQUAL(0, page.rig.nav.calls.size());
  TEST_ASSERT_EQUAL_STRING("", page.rig.session.recipient.c_str());
  TEST_ASSERT_TRUE(shows::text(page.rig.frame, "Still too short", FontSize::Small, kContent));
  page.type("3");                                                    // typing again takes the note away
  TEST_ASSERT_FALSE(shows::text(page.rig.frame, "Still too short", FontSize::Small, kContent));
  page.key(Key::Select);                                             // three digits are enough
  TEST_ASSERT_TRUE(page.rig.nav.opened(screen::kCompose));
  TEST_ASSERT_EQUAL_STRING("123", page.rig.session.recipient.c_str());       // a short code is a number as it is
}

void test_only_the_digits_count_for_the_minimum() {
  Page page;
  page.type("+1");                                                   // two characters, one digit
  page.key(Key::Select);
  TEST_ASSERT_EQUAL(0, page.rig.nav.calls.size());
  page.type("22");
  page.key(Key::Select);
  TEST_ASSERT_TRUE(page.rig.nav.opened(screen::kCompose));
}

void test_without_the_profiles_leave_to_write_to_anybody_the_page_leads_nowhere() {
  Page page(ProfileBuilder().childName("Alex").freeText(false).build());
  page.type("017012345");
  page.key(Key::Select);
  TEST_ASSERT_EQUAL(0, page.rig.nav.calls.size());
  TEST_ASSERT_EQUAL_STRING("", page.rig.session.recipient.c_str());
}

void test_the_soft_keys_work_by_touch() {
  Page page;
  page.type("0170");
  const SoftKeySlots slots = softKeySlots(page.screen.softKeys(), ScreenLayout::standard().softKeys);
  TEST_ASSERT_EQUAL_STRING("Next", page.screen.softKeys().action.c_str());
  page.screen.handle(Input::touch(slots.back.x + 3, slots.back.y + 3));            // delete
  TEST_ASSERT_EQUAL_STRING("017", page.screen.number().c_str());
  page.screen.handle(Input::touch(slots.action.x + 3, slots.action.y + 3));        // next
  TEST_ASSERT_TRUE(page.rig.nav.opened(screen::kCompose));
}

// ---- layout ---------------------------------------------------------------------------------------

void test_the_page_is_drawn_inside_the_screen_in_every_language() {
  languages::forEach([](const std::string& language, i18n::Translator&) {
    for (int state = 0; state < 3; ++state) {
      DeviceRig rig(ProfileBuilder().freeText(true).build());
      rig.language(language);
      NumberScreen page(rig.context());
      page.onEnter();
      rig.poll();
      if (state >= 1) for (char c : kLongest) page.handle(Input::text(c));
      if (state == 2) page.handle(Input::pressed(Key::Select));
      if (state == 2) { page.handle(Input::pressed(Key::Back)); for (int i = 0; i < 14; ++i) page.handle(Input::pressed(Key::Back)); page.handle(Input::pressed(Key::Select)); }
      FrameBuffer fb;
      page.render(fb);
      TEST_ASSERT_FALSE_MESSAGE(fb.clipped(), (language + ":\n" + fb.describeClips()).c_str());
    }
  });
}

void test_the_pictures_in_english_and_german() {
  for (const char* language : {"en", "de"}) {
    DeviceRig rig(ProfileBuilder().freeText(true).build());
    rig.language(language);
    NumberScreen page(rig.context());
    page.onEnter();
    rig.poll();
    page.render(rig.frame);
    golden::expect(rig.frame, std::string("number_empty_") + language);
    for (char c : std::string("+49")) page.handle(Input::text(c));
    page.handle(Input::pressed(Key::Select));                        // two digits are too short: the note under the field
    page.render(rig.frame);
    golden::expect(rig.frame, std::string("number_short_") + language);
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_the_keyboard_types_digits_and_a_plus_in_front);
  RUN_TEST(test_a_plus_anywhere_but_in_front_is_not_taken_and_other_characters_neither);
  RUN_TEST(test_a_number_has_at_most_fifteen_digits_and_the_sixteenth_is_ignored_not_cut_into_another_number);
  RUN_TEST(test_the_full_number_is_drawn_in_a_size_that_fits_and_never_shortened);
  RUN_TEST(test_an_empty_field_stays_empty);
  RUN_TEST(test_back_deletes_and_with_an_empty_field_goes_back);
  RUN_TEST(test_the_page_is_opened_empty_each_time);
  RUN_TEST(test_the_twelve_keys_are_the_digits_the_plus_and_delete);
  RUN_TEST(test_a_tap_on_a_key_types_it_and_the_last_key_deletes);
  RUN_TEST(test_every_key_of_the_keypad_is_drawn_and_the_tap_beside_the_keys_does_nothing);
  RUN_TEST(test_next_hands_the_number_in_its_canonical_form_to_the_message_and_opens_it);
  RUN_TEST(test_a_number_written_with_the_country_prefix_is_handed_on_as_it_is);
  RUN_TEST(test_a_short_number_is_a_slip_of_the_finger_and_the_page_says_so_under_the_field);
  RUN_TEST(test_only_the_digits_count_for_the_minimum);
  RUN_TEST(test_without_the_profiles_leave_to_write_to_anybody_the_page_leads_nowhere);
  RUN_TEST(test_the_soft_keys_work_by_touch);
  RUN_TEST(test_the_page_is_drawn_inside_the_screen_in_every_language);
  RUN_TEST(test_the_pictures_in_english_and_german);
  return UNITY_END();
}

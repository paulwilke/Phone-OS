// Host tests for the phone page: who is on it, in which order, what choosing does, what the core's
// refusals look like, touch, and the layout in every language.
// allow-non-english: the catalogs under test contain German and French.

#include <unity.h>

#include <DeviceRig.h>
#include <Shows.h>
#include <ui/Fonts.h>
#include <ui/Text.h>
#include <ui/screens/PhoneScreen.h>

using namespace people;
using namespace ui;

void setUp() {}
void tearDown() {}

namespace {

const Rect kContent = ScreenLayout::standard().content;
const char* const kPhoneSymbol = "\xE2\x98\x8E";

// Is the row drawn (black on white, or inverted when it is the selected one)?
bool hasRow(const FrameBuffer& fb, const std::string& label) {
  const std::string text = std::string(kPhoneSymbol) + " " + label;
  return shows::text(fb, text, FontSize::Text, kContent) || shows::text(fb, text, FontSize::Text, kContent, true);
}
bool hasWord(const FrameBuffer& fb, const std::string& word) {
  return shows::text(fb, word, FontSize::Text, kContent) || shows::text(fb, word, FontSize::Text, kContent, true);
}
bool title(const FrameBuffer& fb, const std::string& words) { return shows::text(fb, words, FontSize::Small, kContent); }

void press(PhoneScreen& page, Key key, int times = 1) {
  for (int i = 0; i < times; ++i) page.handle(Input::pressed(key));
}

// The page, rendered once so that touch has something to hit.
struct Page {
  DeviceRig rig;
  PhoneScreen screen;
  explicit Page(Profile profile = ProfileBuilder().childName("Alex").build())
      : rig(std::move(profile)), screen(rig.context()) {
    screen.onEnter();
    rig.poll();
    screen.render(rig.frame);
  }
  void draw() { screen.render(rig.frame); }
};

Profile targets(std::vector<std::string> list) { return ProfileBuilder().childName("Alex").callTargets(std::move(list)).build(); }

}  // namespace

// ---- who is on the page ---------------------------------------------------------------------

void test_the_short_list_is_the_call_targets_then_other_and_cancel() {
  Page page;
  TEST_ASSERT_TRUE(title(page.rig.frame, "Whom to call?"));
  TEST_ASSERT_TRUE(hasRow(page.rig.frame, "Parent One"));
  TEST_ASSERT_TRUE(hasWord(page.rig.frame, "Other"));
  TEST_ASSERT_TRUE(hasWord(page.rig.frame, "Cancel"));
  TEST_ASSERT_FALSE(hasRow(page.rig.frame, "Parent Two"));          // the rest of the book is behind "Other"
  TEST_ASSERT_FALSE(hasRow(page.rig.frame, "Grandma"));
}

void test_the_call_targets_are_in_the_order_the_parents_arranged_them() {
  Page page(targets({kGrandma, kParentTwo, kParentOne}));
  const Rect first = shows::where(page.rig.frame, std::string(kPhoneSymbol) + " Grandma", FontSize::Text, kContent, true).value();
  const Rect second = shows::where(page.rig.frame, std::string(kPhoneSymbol) + " Parent Two", FontSize::Text, kContent).value();
  const Rect third = shows::where(page.rig.frame, std::string(kPhoneSymbol) + " Parent One", FontSize::Text, kContent).value();
  TEST_ASSERT_TRUE(first.y < second.y && second.y < third.y);
  TEST_ASSERT_FALSE(hasWord(page.rig.frame, "Other"));              // everybody is already on the list
}

void test_other_opens_the_rest_of_the_book_and_back_returns_to_the_short_list() {
  Page page;
  press(page.screen, Key::Down);                                    // Other
  page.screen.handle(Input::pressed(Key::Select));
  page.draw();
  TEST_ASSERT_TRUE(title(page.rig.frame, "Whom to call? (Other)"));
  TEST_ASSERT_TRUE(hasRow(page.rig.frame, "Parent Two"));
  TEST_ASSERT_TRUE(hasRow(page.rig.frame, "Grandma"));
  TEST_ASSERT_FALSE(hasRow(page.rig.frame, "Parent One"));
  TEST_ASSERT_FALSE(hasWord(page.rig.frame, "Cancel"));             // Back is the way out of this list
  page.screen.handle(Input::pressed(Key::Back));
  TEST_ASSERT_EQUAL(0, page.rig.nav.calls.size());                  // it did not leave the page
  page.draw();
  TEST_ASSERT_TRUE(title(page.rig.frame, "Whom to call?"));
  TEST_ASSERT_TRUE(hasRow(page.rig.frame, "Parent One"));
  page.screen.handle(Input::pressed(Key::Back));
  TEST_ASSERT_EQUAL_STRING("back", page.rig.nav.last().c_str());
}

void test_cancel_goes_back() {
  Page page;
  press(page.screen, Key::Down, 2);                                 // Cancel
  page.screen.handle(Input::pressed(Key::Select));
  TEST_ASSERT_EQUAL_STRING("back", page.rig.nav.last().c_str());
  TEST_ASSERT_FALSE(page.rig.core.state().callActive);
}

void test_without_call_targets_everybody_is_on_the_short_list() {
  Page page(targets({}));
  TEST_ASSERT_TRUE(hasRow(page.rig.frame, "Parent One"));
  TEST_ASSERT_TRUE(hasRow(page.rig.frame, "Parent Two"));
  TEST_ASSERT_TRUE(hasRow(page.rig.frame, "Grandma"));
  TEST_ASSERT_FALSE(hasWord(page.rig.frame, "Other"));
  TEST_ASSERT_TRUE(hasWord(page.rig.frame, "Cancel"));
}

void test_a_call_target_in_no_book_is_shown_by_its_number_and_may_be_called() {
  Page page(ProfileBuilder().noContacts().callTargets({kParentOne}).build());
  TEST_ASSERT_TRUE(hasRow(page.rig.frame, kParentOne));             // the whole number
  page.screen.handle(Input::pressed(Key::Select));
  TEST_ASSERT_TRUE(page.rig.nav.opened(screen::kCall));
  TEST_ASSERT_EQUAL_STRING(kParentOne, page.rig.core.state().callPeer.c_str());
}

void test_a_number_listed_twice_in_other_spellings_is_listed_once() {
  // The target twice in two spellings, and the contact of that same number: one row, then Cancel.
  Page page(ProfileBuilder().noContacts().contact(kParentOne, "Parent One", true).callTargets({"0170 1111111", kParentOne}).build());
  TEST_ASSERT_FALSE(hasWord(page.rig.frame, "Other"));              // nothing else is left to put behind it
  press(page.screen, Key::Down);                                    // the second row is Cancel, not the number again
  page.screen.handle(Input::pressed(Key::Select));
  TEST_ASSERT_EQUAL_STRING("back", page.rig.nav.last().c_str());
  TEST_ASSERT_FALSE(page.rig.core.state().callActive);
}

void test_the_page_offers_only_numbers_the_child_may_call() {
  // Every row of both lists dials, and none is refused as "not allowed".
  Page page;
  for (int list = 0; list < 2; ++list) {
    for (int row = 0; row < 3; ++row) {
      page.rig.core.hangup();
      page.rig.poll();
      page.rig.nav.clear();
      PhoneScreen fresh(page.rig.context());
      fresh.onEnter();
      fresh.render(page.rig.frame);
      if (list == 1) {
        press(fresh, Key::Down);
        fresh.handle(Input::pressed(Key::Select));               // into "Other"
        fresh.render(page.rig.frame);
        if (row > 1) continue;                                   // two contacts there
      }
      press(fresh, Key::Down, row == 0 ? 0 : row);
      const bool contactRow = list == 0 ? row == 0 : true;
      if (!contactRow) continue;                                 // Other and Cancel are not numbers
      fresh.handle(Input::pressed(Key::Select));
      TEST_ASSERT_TRUE(page.rig.nav.opened(screen::kCall));
      TEST_ASSERT_TRUE(page.rig.core.state().notice.kind != NoticeKind::CallNotAllowed);
    }
  }
}

void test_names_that_are_too_long_are_shortened_and_numbers_never() {
  const char* longName = "Grandma Elisabeth Maria";               // 23 of the 24 bytes a name can have
  Page page(ProfileBuilder().noContacts().contact(kGrandma, longName, false).callTargets({kGrandma, kStranger}).build());
  const Font& text = font(FontSize::Text);
  const std::string shortened = text::ellipsize(text, std::string(kPhoneSymbol) + " " + longName, 226);
  TEST_ASSERT_TRUE(shortened.find("...") != std::string::npos);
  TEST_ASSERT_TRUE(shows::text(page.rig.frame, shortened, FontSize::Text, kContent, true));
  TEST_ASSERT_TRUE(hasRow(page.rig.frame, kStranger));              // a number is there in full
}

void test_with_nobody_to_call_the_page_says_so() {
  Page page(ProfileBuilder().noContacts().build());
  TEST_ASSERT_TRUE(hasWord(page.rig.frame, "No numbers to call"));
  TEST_ASSERT_TRUE(page.screen.softKeys().action.empty());
  TEST_ASSERT_FALSE(page.screen.softKeys().scroll);
  TEST_ASSERT_EQUAL_STRING("Back", page.screen.softKeys().back.c_str());
  page.screen.handle(Input::pressed(Key::Select));
  page.screen.handle(Input::pressed(Key::Down));
  TEST_ASSERT_EQUAL(0, page.rig.nav.calls.size());
  page.screen.handle(Input::pressed(Key::Back));
  TEST_ASSERT_EQUAL_STRING("back", page.rig.nav.last().c_str());
}

void test_the_page_is_opened_afresh_each_time() {
  Page page;
  press(page.screen, Key::Down);
  page.screen.handle(Input::pressed(Key::Select));                  // inside "Other"
  page.screen.onLeave();
  page.screen.onEnter();
  page.draw();
  TEST_ASSERT_TRUE(title(page.rig.frame, "Whom to call?"));
  TEST_ASSERT_TRUE(hasRow(page.rig.frame, "Parent One"));
  page.screen.handle(Input::pressed(Key::Select));                  // the first row again, not where it was left
  TEST_ASSERT_EQUAL_STRING(kParentOne, page.rig.core.state().callPeer.c_str());
}

// ---- choosing --------------------------------------------------------------------------------

void test_choosing_a_name_dials_it_and_opens_the_call_page() {
  Page page;
  page.screen.handle(Input::pressed(Key::Select));
  TEST_ASSERT_TRUE(page.rig.nav.opened(screen::kCall));
  TEST_ASSERT_TRUE(page.rig.core.state().callState == ModemState::Dialing);
  TEST_ASSERT_EQUAL_STRING(kParentOne, page.rig.core.state().callPeer.c_str());
}

void test_nothing_dials_until_a_row_is_chosen() {
  Page page;
  press(page.screen, Key::Down, 3);
  press(page.screen, Key::Up, 2);
  page.screen.handle(Input::text('x'));
  TEST_ASSERT_FALSE(page.rig.core.state().callActive);
  TEST_ASSERT_EQUAL(0, page.rig.nav.calls.size());
}

void test_in_school_time_the_core_refuses_and_the_page_says_until_when() {
  Page page(ProfileBuilder().schoolTime("tue", "14:00", "15:00").build());
  page.screen.handle(Input::pressed(Key::Select));
  TEST_ASSERT_EQUAL(0, page.rig.nav.calls.size());                  // it stays
  TEST_ASSERT_FALSE(page.rig.core.state().callActive);
  page.draw();
  TEST_ASSERT_EQUAL_STRING("School time until 15:00", page.screen.noticeLine().c_str());
  TEST_ASSERT_TRUE(shows::text(page.rig.frame, "School time until 15:00", FontSize::Small, ScreenLayout::standard().alert, true));
}

void test_when_cellular_is_off_or_there_is_no_network_the_page_stays_and_says_so() {
  Page off;
  off.rig.core.setCellularEnabled(false);
  off.screen.handle(Input::pressed(Key::Select));
  TEST_ASSERT_EQUAL(0, off.rig.nav.calls.size());
  off.draw();
  TEST_ASSERT_EQUAL_STRING("Cellular is off", off.screen.noticeLine().c_str());

  Page none;
  none.rig.modem.setNetwork("", 0);
  none.rig.poll();
  none.screen.handle(Input::pressed(Key::Select));
  TEST_ASSERT_EQUAL(0, none.rig.nav.calls.size());
  none.draw();
  TEST_ASSERT_EQUAL_STRING("No network", none.screen.noticeLine().c_str());
}

// ---- touch and the keys under the screen -------------------------------------------------------

void test_a_tap_on_a_name_calls_it() {
  Page page;
  const Rect row = shows::where(page.rig.frame, std::string(kPhoneSymbol) + " Parent One", FontSize::Text, kContent, true).value();
  TEST_ASSERT_TRUE(page.screen.handle(Input::touch(row.x + 10, row.y + 5)));
  TEST_ASSERT_TRUE(page.rig.nav.opened(screen::kCall));
  TEST_ASSERT_EQUAL_STRING(kParentOne, page.rig.core.state().callPeer.c_str());
}

void test_a_tap_on_other_opens_it_and_a_tap_on_cancel_goes_back() {
  Page page;
  const Rect other = shows::where(page.rig.frame, "Other", FontSize::Text, kContent).value();
  page.screen.handle(Input::touch(other.x + 5, other.y + 5));
  page.draw();
  TEST_ASSERT_TRUE(title(page.rig.frame, "Whom to call? (Other)"));
  page.screen.handle(Input::pressed(Key::Back));
  page.draw();
  const Rect cancel = shows::where(page.rig.frame, "Cancel", FontSize::Text, kContent).value();
  page.screen.handle(Input::touch(cancel.x + 5, cancel.y + 5));
  TEST_ASSERT_EQUAL_STRING("back", page.rig.nav.last().c_str());
}

void test_a_tap_beside_the_rows_does_nothing() {
  Page page;
  TEST_ASSERT_FALSE(page.screen.handle(Input::touch(120, kContent.bottom() - 5)));
  TEST_ASSERT_EQUAL(0, page.rig.nav.calls.size());
  TEST_ASSERT_FALSE(page.rig.core.state().callActive);
}

void test_the_soft_keys_are_back_the_arrows_and_pick() {
  Page page;
  TEST_ASSERT_EQUAL_STRING("Back", page.screen.softKeys().back.c_str());
  TEST_ASSERT_EQUAL_STRING("Pick", page.screen.softKeys().action.c_str());
  TEST_ASSERT_TRUE(page.screen.softKeys().scroll);
  const SoftKeySlots slots = softKeySlots(page.screen.softKeys(), ScreenLayout::standard().softKeys);
  page.screen.handle(Input::touch(slots.down.x + 2, slots.down.y + 2));       // down to "Other"
  page.screen.handle(Input::touch(slots.action.x + 2, slots.action.y + 2));   // pick it
  page.draw();
  TEST_ASSERT_TRUE(title(page.rig.frame, "Whom to call? (Other)"));
  page.screen.handle(Input::touch(slots.back.x + 2, slots.back.y + 2));
  page.draw();
  TEST_ASSERT_TRUE(title(page.rig.frame, "Whom to call?"));
}

// ---- the layout ----------------------------------------------------------------------------------

void test_the_page_is_drawn_inside_the_screen_in_every_language() {
  languages::forEach([](const std::string& language, i18n::Translator&) {
    for (int kind = 0; kind < 3; ++kind) {
      Profile profile = kind == 2 ? ProfileBuilder().noContacts().build() : ProfileBuilder().childName("Alex").build();
      DeviceRig rig(profile);
      rig.language(language);
      PhoneScreen page(rig.context());
      page.onEnter();
      rig.poll();
      if (kind == 1) {                                           // the list behind "Other"
        page.render(rig.frame);
        press(page, Key::Down);
        page.handle(Input::pressed(Key::Select));
      }
      FrameBuffer fb;
      page.render(fb);
      TEST_ASSERT_FALSE_MESSAGE(fb.clipped(), (language + ":\n" + fb.describeClips()).c_str());
    }
  });
}

void test_the_pictures_in_english_and_german() {
  for (const char* language : {"en", "de"}) {
    DeviceRig rig;
    rig.language(language);
    PhoneScreen page(rig.context());
    page.onEnter();
    rig.poll();
    page.render(rig.frame);
    golden::expect(rig.frame, std::string("phone_short_") + language);
    press(page, Key::Down);
    page.handle(Input::pressed(Key::Select));
    page.render(rig.frame);
    golden::expect(rig.frame, std::string("phone_other_") + language);
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_the_short_list_is_the_call_targets_then_other_and_cancel);
  RUN_TEST(test_the_call_targets_are_in_the_order_the_parents_arranged_them);
  RUN_TEST(test_other_opens_the_rest_of_the_book_and_back_returns_to_the_short_list);
  RUN_TEST(test_cancel_goes_back);
  RUN_TEST(test_without_call_targets_everybody_is_on_the_short_list);
  RUN_TEST(test_a_call_target_in_no_book_is_shown_by_its_number_and_may_be_called);
  RUN_TEST(test_a_number_listed_twice_in_other_spellings_is_listed_once);
  RUN_TEST(test_the_page_offers_only_numbers_the_child_may_call);
  RUN_TEST(test_names_that_are_too_long_are_shortened_and_numbers_never);
  RUN_TEST(test_with_nobody_to_call_the_page_says_so);
  RUN_TEST(test_the_page_is_opened_afresh_each_time);
  RUN_TEST(test_choosing_a_name_dials_it_and_opens_the_call_page);
  RUN_TEST(test_nothing_dials_until_a_row_is_chosen);
  RUN_TEST(test_in_school_time_the_core_refuses_and_the_page_says_until_when);
  RUN_TEST(test_when_cellular_is_off_or_there_is_no_network_the_page_stays_and_says_so);
  RUN_TEST(test_a_tap_on_a_name_calls_it);
  RUN_TEST(test_a_tap_on_other_opens_it_and_a_tap_on_cancel_goes_back);
  RUN_TEST(test_a_tap_beside_the_rows_does_nothing);
  RUN_TEST(test_the_soft_keys_are_back_the_arrows_and_pick);
  RUN_TEST(test_the_page_is_drawn_inside_the_screen_in_every_language);
  RUN_TEST(test_the_pictures_in_english_and_german);
  return UNITY_END();
}

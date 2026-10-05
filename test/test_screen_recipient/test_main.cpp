// Host tests for the recipient page: who is on it, "New number" only where the profile allows any
// recipient, what choosing leaves in the session, and the layout in every language.
// allow-non-english: the catalogs under test contain German and French.

#include <unity.h>

#include <DeviceRig.h>
#include <Shows.h>
#include <ui/Fonts.h>
#include <ui/Text.h>
#include <ui/screens/RecipientScreen.h>

using namespace people;
using namespace ui;

void setUp() {}
void tearDown() {}

namespace {

const Rect kContent = ScreenLayout::standard().content;

bool hasRow(const FrameBuffer& fb, const std::string& label) {
  return shows::text(fb, label, FontSize::Text, kContent) || shows::text(fb, label, FontSize::Text, kContent, true);
}

void press(RecipientScreen& page, Key key, int times = 1) {
  for (int i = 0; i < times; ++i) page.handle(Input::pressed(key));
}

struct Page {
  DeviceRig rig;
  RecipientScreen screen;
  Page() : rig(ProfileBuilder().childName("Alex").build()), screen(rig.context()) { start(); }
  explicit Page(Profile profile) : rig(std::move(profile)), screen(rig.context()) { start(); }
  void start() {
    screen.onEnter();
    rig.poll();
    screen.render(rig.frame);
  }
  void draw() { screen.render(rig.frame); }
};

}  // namespace

void test_the_short_list_is_the_call_targets_then_other_and_cancel() {
  Page page;
  TEST_ASSERT_TRUE(shows::text(page.rig.frame, "To whom?", FontSize::Small, kContent));
  TEST_ASSERT_TRUE(hasRow(page.rig.frame, "Parent One"));
  TEST_ASSERT_TRUE(hasRow(page.rig.frame, "Other"));
  TEST_ASSERT_TRUE(hasRow(page.rig.frame, "Cancel"));
  TEST_ASSERT_FALSE(hasRow(page.rig.frame, "Grandma"));
  TEST_ASSERT_FALSE(hasRow(page.rig.frame, "New number"));          // free recipients are the profile's switch
}

void test_choosing_a_contact_leaves_the_number_in_the_session_and_opens_the_message() {
  Page page;
  page.screen.handle(Input::pressed(Key::Select));
  TEST_ASSERT_EQUAL_STRING(kParentOne, page.rig.session.recipient.c_str());
  TEST_ASSERT_FALSE(page.rig.session.viaRadio);
  TEST_ASSERT_TRUE(page.rig.nav.opened(screen::kCompose));
  TEST_ASSERT_FALSE(page.rig.core.state().callActive);              // writing is not calling
}

void test_a_contact_behind_other_is_chosen_with_its_canonical_number() {
  Page page;
  press(page.screen, Key::Down);
  page.screen.handle(Input::pressed(Key::Select));                  // Other
  page.draw();
  TEST_ASSERT_TRUE(shows::text(page.rig.frame, "To whom? (Other)", FontSize::Small, kContent));
  TEST_ASSERT_TRUE(hasRow(page.rig.frame, "Grandma"));
  press(page.screen, Key::Down);                                    // Parent Two, Grandma
  page.screen.handle(Input::pressed(Key::Select));
  TEST_ASSERT_EQUAL_STRING(kGrandma, page.rig.session.recipient.c_str());
}

void test_back_from_other_returns_to_the_short_list_and_cancel_goes_back() {
  Page page;
  press(page.screen, Key::Down);
  page.screen.handle(Input::pressed(Key::Select));
  page.screen.handle(Input::pressed(Key::Back));
  TEST_ASSERT_EQUAL(0, page.rig.nav.calls.size());
  page.draw();
  TEST_ASSERT_TRUE(shows::text(page.rig.frame, "To whom?", FontSize::Small, kContent));
  press(page.screen, Key::Down, 2);
  page.screen.handle(Input::pressed(Key::Select));                  // Cancel
  TEST_ASSERT_EQUAL_STRING("back", page.rig.nav.last().c_str());
  TEST_ASSERT_EQUAL_STRING("", page.rig.session.recipient.c_str());
}

void test_new_number_ends_the_list_behind_other_only_where_any_recipient_is_allowed() {
  Page free(ProfileBuilder().freeText(true).build());
  press(free.screen, Key::Down);
  free.screen.handle(Input::pressed(Key::Select));                  // Other
  free.draw();
  TEST_ASSERT_TRUE(hasRow(free.rig.frame, "New number"));
  press(free.screen, Key::Down, 2);                                 // Grandma is last but one, New number last
  press(free.screen, Key::Down);
  free.screen.handle(Input::pressed(Key::Select));
  TEST_ASSERT_TRUE(free.rig.nav.opened(screen::kNumber));

  Page canned(ProfileBuilder().freeText(false).build());
  press(canned.screen, Key::Down);
  canned.screen.handle(Input::pressed(Key::Select));
  canned.draw();
  TEST_ASSERT_FALSE(hasRow(canned.rig.frame, "New number"));
}

void test_with_nobody_in_any_list_free_text_still_offers_a_new_number() {
  Page free(ProfileBuilder().noContacts().freeText(true).build());
  TEST_ASSERT_TRUE(hasRow(free.rig.frame, "New number"));
  TEST_ASSERT_TRUE(hasRow(free.rig.frame, "Cancel"));
  Page none(ProfileBuilder().noContacts().freeText(false).build());
  TEST_ASSERT_TRUE(hasRow(none.rig.frame, "Nobody to write to"));
  TEST_ASSERT_TRUE(none.screen.softKeys().action.empty());
}

void test_a_target_in_no_book_is_shown_by_its_number() {
  Page page(ProfileBuilder().noContacts().callTargets({kParentOne}).build());
  TEST_ASSERT_TRUE(hasRow(page.rig.frame, kParentOne));
  page.screen.handle(Input::pressed(Key::Select));
  TEST_ASSERT_EQUAL_STRING(kParentOne, page.rig.session.recipient.c_str());
}

void test_without_call_targets_everybody_is_on_the_short_list() {
  Page page(ProfileBuilder().callTargets({}).build());
  TEST_ASSERT_TRUE(hasRow(page.rig.frame, "Parent One"));
  TEST_ASSERT_TRUE(hasRow(page.rig.frame, "Grandma"));
  TEST_ASSERT_FALSE(hasRow(page.rig.frame, "Other"));
}

void test_a_tap_on_a_row_chooses_it_and_the_page_opens_afresh() {
  Page page;
  const Rect row = shows::where(page.rig.frame, "Parent One", FontSize::Text, kContent, true).value();
  TEST_ASSERT_TRUE(page.screen.handle(Input::touch(row.x + 5, row.y + 5)));
  TEST_ASSERT_TRUE(page.rig.nav.opened(screen::kCompose));
  press(page.screen, Key::Down);
  page.screen.handle(Input::pressed(Key::Select));                  // inside Other
  page.screen.onLeave();
  page.screen.onEnter();
  page.draw();
  TEST_ASSERT_TRUE(shows::text(page.rig.frame, "To whom?", FontSize::Small, kContent));
}

void test_soft_keys_and_their_words() {
  Page page;
  TEST_ASSERT_EQUAL_STRING("Back", page.screen.softKeys().back.c_str());
  TEST_ASSERT_EQUAL_STRING("Pick", page.screen.softKeys().action.c_str());
  TEST_ASSERT_TRUE(page.screen.softKeys().scroll);
  page.rig.language("de");
  page.draw();
  TEST_ASSERT_TRUE(shows::text(page.rig.frame, "An wen?", FontSize::Small, kContent));
  TEST_ASSERT_EQUAL_STRING("W\xC3\xA4hlen", page.screen.softKeys().action.c_str());
}

void test_the_page_is_drawn_inside_the_screen_in_every_language() {
  languages::forEach([](const std::string& language, i18n::Translator&) {
    for (int kind = 0; kind < 4; ++kind) {
      DeviceRig rig(kind == 3 ? ProfileBuilder().noContacts().build() : ProfileBuilder().freeText(kind == 2).build());
      rig.language(language);
      RecipientScreen page(rig.context());
      page.onEnter();
      rig.poll();
      if (kind >= 1 && kind <= 2) {
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
    DeviceRig rig(ProfileBuilder().freeText(true).build());
    rig.language(language);
    RecipientScreen page(rig.context());
    page.onEnter();
    rig.poll();
    page.render(rig.frame);
    golden::expect(rig.frame, std::string("recipient_short_") + language);
    press(page, Key::Down);
    page.handle(Input::pressed(Key::Select));
    page.render(rig.frame);
    golden::expect(rig.frame, std::string("recipient_other_") + language);
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_the_short_list_is_the_call_targets_then_other_and_cancel);
  RUN_TEST(test_choosing_a_contact_leaves_the_number_in_the_session_and_opens_the_message);
  RUN_TEST(test_a_contact_behind_other_is_chosen_with_its_canonical_number);
  RUN_TEST(test_back_from_other_returns_to_the_short_list_and_cancel_goes_back);
  RUN_TEST(test_new_number_ends_the_list_behind_other_only_where_any_recipient_is_allowed);
  RUN_TEST(test_with_nobody_in_any_list_free_text_still_offers_a_new_number);
  RUN_TEST(test_a_target_in_no_book_is_shown_by_its_number);
  RUN_TEST(test_without_call_targets_everybody_is_on_the_short_list);
  RUN_TEST(test_a_tap_on_a_row_chooses_it_and_the_page_opens_afresh);
  RUN_TEST(test_soft_keys_and_their_words);
  RUN_TEST(test_the_page_is_drawn_inside_the_screen_in_every_language);
  RUN_TEST(test_the_pictures_in_english_and_german);
  return UNITY_END();
}

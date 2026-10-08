// Host tests for the call page: what it says in each phase of a call and after one that failed,
// the buttons, the volume, a number that is never shortened, and the layout in every language.
// allow-non-english: the catalogs under test contain German and French.

#include <unity.h>

#include <DeviceRig.h>
#include <RefreshRig.h>
#include <Shows.h>
#include <ui/Fonts.h>
#include <ui/Text.h>
#include <ui/screens/CallScreen.h>

using namespace people;
using namespace ui;

void setUp() {}
void tearDown() {}

namespace {

const Rect kContent = ScreenLayout::standard().content;

bool says(const FrameBuffer& fb, const std::string& words, FontSize size) {
  return shows::text(fb, words, size, kContent);
}
// Whatever font the page chose for a name, the whole of it is there.
bool saysWhole(const FrameBuffer& fb, const std::string& words) {
  for (FontSize size : {FontSize::Large, FontSize::Tile, FontSize::Text, FontSize::Small})
    if (says(fb, words, size)) return true;
  return false;
}

struct Page {
  DeviceRig rig;
  CallScreen screen;
  Page() : rig(ProfileBuilder().childName("Alex").build()), screen(rig.context()) { rig.poll(); }
  explicit Page(Profile profile) : rig(std::move(profile)), screen(rig.context()) { rig.poll(); }
  void draw() { screen.render(rig.frame); }
  void dial(const char* who = kParentOne) {
    TEST_ASSERT_TRUE(rig.core.dial(who));
    rig.poll();
    draw();
  }
  void ring(const char* who = kParentOne) {
    TEST_ASSERT_TRUE(rig.modem.injectIncomingCall(who));
    rig.poll();
    draw();
  }
  void connect() {
    rig.modem.remoteAnswer();
    rig.poll();
    draw();
  }
};

// The filled boxes of the volume row: the labels' line is found, the boxes lie below it.
int filledBoxes(const FrameBuffer& fb) {
  const Rect label = shows::where(fb, "Volume", FontSize::Text, kContent).value();
  const int y = label.bottom() + 8;
  const int total = Calls::kMaxVolume * 22 + (Calls::kMaxVolume - 1) * 6;
  int x = kContent.x + (kContent.w - total) / 2;
  int filled = 0;
  for (int i = 0; i < Calls::kMaxVolume; ++i, x += 28) filled += fb.pixel(x + 11, y + 9) ? 1 : 0;
  return filled;
}

}  // namespace

// ---- dialling ---------------------------------------------------------------------------------

void test_while_dialling_it_says_so_names_the_one_called_and_offers_hang_up() {
  Page page;
  page.dial();
  TEST_ASSERT_TRUE(says(page.rig.frame, "Calling ...", FontSize::Text));
  TEST_ASSERT_TRUE(says(page.rig.frame, "Parent One", FontSize::Large));
  TEST_ASSERT_EQUAL_STRING("Hang up", page.screen.softKeys().back.c_str());
  TEST_ASSERT_TRUE(page.screen.softKeys().action.empty());
  TEST_ASSERT_FALSE(page.screen.softKeys().scroll);                // no volume before the call is connected
  TEST_ASSERT_FALSE(page.screen.handle(Input::pressed(Key::Select)));
  TEST_ASSERT_TRUE(page.rig.core.state().callState == ModemState::Dialing);
}

void test_back_while_dialling_hangs_up_and_does_not_leave() {
  Page page;
  page.dial();
  TEST_ASSERT_TRUE(page.screen.handle(Input::pressed(Key::Back)));
  TEST_ASSERT_FALSE(page.rig.core.state().callActive);
  TEST_ASSERT_EQUAL(0, page.rig.nav.calls.size());
}

// ---- ringing ----------------------------------------------------------------------------------

void test_a_ringing_call_offers_answer_and_reject() {
  Page page;
  page.ring();
  TEST_ASSERT_TRUE(says(page.rig.frame, "Incoming call", FontSize::Text));
  TEST_ASSERT_TRUE(says(page.rig.frame, "Parent One", FontSize::Large));
  TEST_ASSERT_EQUAL_STRING("Reject", page.screen.softKeys().back.c_str());
  TEST_ASSERT_EQUAL_STRING("Answer", page.screen.softKeys().action.c_str());
}

void test_enter_answers_a_ringing_call() {
  Page page;
  page.ring();
  TEST_ASSERT_TRUE(page.screen.handle(Input::pressed(Key::Select)));
  page.rig.poll();
  TEST_ASSERT_TRUE(page.rig.core.state().callState == ModemState::InCall);
  page.draw();
  TEST_ASSERT_TRUE(says(page.rig.frame, "In call", FontSize::Text));
}

void test_back_rejects_a_ringing_call() {
  Page page;
  page.ring();
  page.screen.handle(Input::pressed(Key::Back));
  page.rig.poll();
  TEST_ASSERT_FALSE(page.rig.core.state().callActive);
  TEST_ASSERT_EQUAL(0, page.rig.nav.calls.size());
}

void test_a_caller_who_is_no_contact_is_shown_by_the_whole_number() {
  // Only a call target that is no contact can ring and be in no book.
  Page page(ProfileBuilder().childName("Alex").callTargets({kStranger}).build());
  page.ring(kStranger);
  TEST_ASSERT_TRUE(saysWhole(page.rig.frame, kStranger));
}

void test_a_stranger_and_a_hidden_number_are_not_on_the_page_at_all() {
  Page page;
  page.ring(kStranger);
  TEST_ASSERT_FALSE(page.rig.core.state().callActive);
  TEST_ASSERT_FALSE(saysWhole(page.rig.frame, kStranger));
  Page none;
  none.ring("");
  TEST_ASSERT_FALSE(none.rig.core.state().callActive);
  TEST_ASSERT_FALSE(says(none.rig.frame, "Unknown", FontSize::Large));
}

void test_a_number_that_does_not_fit_the_large_font_is_drawn_smaller_not_shortened() {
  const std::string wide = "+491709999999";
  Page page(ProfileBuilder().childName("Alex").callTargets({wide}).build());
  TEST_ASSERT_TRUE(text::width(font(FontSize::Large), wide) > kContent.w - 16);   // too wide for the large font ...
  page.ring(wide.c_str());
  TEST_ASSERT_FALSE(says(page.rig.frame, wide, FontSize::Large));
  TEST_ASSERT_TRUE(says(page.rig.frame, wide, FontSize::Tile));                  // ... so the next one down has all of it
  TEST_ASSERT_TRUE(wide.find("...") == std::string::npos);
}

// ---- connected and the volume --------------------------------------------------------------------

// Owner, 2026-10-08: "Hang up" is the whole key bar, and the volume has buttons on the page; the keys Up and Down
// change it as before, so the call works without touch too.
void test_a_connected_call_shows_the_volume_as_boxes_and_the_arrows_change_it() {
  Page page;
  page.rig.core.setDefaultVolume(4);
  page.dial();
  page.connect();
  TEST_ASSERT_TRUE(says(page.rig.frame, "In call", FontSize::Text));
  TEST_ASSERT_TRUE(says(page.rig.frame, "Volume", FontSize::Text));
  TEST_ASSERT_EQUAL_STRING("Hang up", page.screen.softKeys().back.c_str());
  TEST_ASSERT_FALSE(page.screen.softKeys().scroll);
  TEST_ASSERT_TRUE(softKeySlots(page.screen.softKeys(), ScreenLayout::standard().softKeys).back == ScreenLayout::standard().softKeys);
  TEST_ASSERT_TRUE(says(page.rig.frame, "Quieter", FontSize::Small));
  TEST_ASSERT_TRUE(says(page.rig.frame, "Louder", FontSize::Small));
  TEST_ASSERT_EQUAL(4, page.rig.core.state().volume);
  TEST_ASSERT_EQUAL(4, filledBoxes(page.rig.frame));

  TEST_ASSERT_TRUE(page.screen.handle(Input::pressed(Key::Up)));
  TEST_ASSERT_EQUAL(5, page.rig.core.state().volume);
  page.draw();
  TEST_ASSERT_EQUAL(5, filledBoxes(page.rig.frame));                 // one more box is filled
  page.screen.handle(Input::pressed(Key::Down));
  page.screen.handle(Input::pressed(Key::Down));
  TEST_ASSERT_EQUAL(3, page.rig.core.state().volume);
  page.draw();
  TEST_ASSERT_EQUAL(3, filledBoxes(page.rig.frame));
}

void test_the_volume_stops_at_both_ends() {
  Page page;
  page.dial();
  page.connect();
  for (int i = 0; i < 10; ++i) page.screen.handle(Input::pressed(Key::Up));
  TEST_ASSERT_EQUAL(Calls::kMaxVolume, page.rig.core.state().volume);
  TEST_ASSERT_FALSE(page.screen.handle(Input::pressed(Key::Up)));    // nothing changed, so nothing was used
  page.draw();
  TEST_ASSERT_EQUAL(Calls::kMaxVolume, filledBoxes(page.rig.frame));
  for (int i = 0; i < 10; ++i) page.screen.handle(Input::pressed(Key::Down));
  TEST_ASSERT_EQUAL(0, page.rig.core.state().volume);
  TEST_ASSERT_FALSE(page.screen.handle(Input::pressed(Key::Down)));
  page.draw();
  TEST_ASSERT_EQUAL(0, filledBoxes(page.rig.frame));
}

void test_there_is_no_volume_before_the_call_is_connected_and_the_arrows_change_nothing() {
  Page page;
  page.ring();
  const int before = page.rig.core.state().volume;
  TEST_ASSERT_FALSE(says(page.rig.frame, "Volume", FontSize::Text));
  TEST_ASSERT_FALSE(page.screen.handle(Input::pressed(Key::Up)));
  TEST_ASSERT_FALSE(page.screen.handle(Input::pressed(Key::Down)));
  TEST_ASSERT_EQUAL(before, page.rig.core.state().volume);
}

void test_without_a_volume_control_in_the_modem_there_are_no_boxes_and_no_arrows() {
  Page page;
  page.rig.modem.setVolumeSupported(false);
  page.dial();
  page.connect();
  TEST_ASSERT_FALSE(says(page.rig.frame, "Volume", FontSize::Text));
  TEST_ASSERT_FALSE(page.screen.softKeys().scroll);
}

void test_back_hangs_up_a_connected_call() {
  Page page;
  page.dial();
  page.connect();
  page.screen.handle(Input::pressed(Key::Back));
  page.rig.poll();
  TEST_ASSERT_FALSE(page.rig.core.state().callActive);
  TEST_ASSERT_EQUAL(0, page.rig.nav.calls.size());
}

// ---- a call that did not happen -------------------------------------------------------------------

void test_a_busy_number_stays_on_the_page_with_its_name_and_the_reason() {
  Page page;
  page.dial();
  page.rig.modem.remoteBusy();
  page.rig.poll();
  page.draw();
  TEST_ASSERT_FALSE(page.rig.core.state().callActive);
  TEST_ASSERT_TRUE(says(page.rig.frame, "Call failed", FontSize::Text));
  TEST_ASSERT_TRUE(says(page.rig.frame, "Parent One", FontSize::Large));
  TEST_ASSERT_TRUE(says(page.rig.frame, "busy", FontSize::Text));
  TEST_ASSERT_EQUAL_STRING("Back", page.screen.softKeys().back.c_str());
  TEST_ASSERT_TRUE(page.screen.softKeys().action.empty());
  TEST_ASSERT_TRUE(page.screen.handle(Input::pressed(Key::Back)));
  TEST_ASSERT_EQUAL_STRING("back", page.rig.nav.last().c_str());       // leaving is the child's choice, not the page's
}

void test_every_reason_is_told_in_the_language_of_the_moment() {
  struct Case {
    const char* name;
    const char* catalogKey;
    void (*script)(Page&);
  };
  const Case cases[] = {
      {"busy", "call.reason-busy", [](Page& p) { p.rig.modem.remoteBusy(); }},
      {"no answer", "call.reason-no-answer", [](Page& p) { p.rig.modem.remoteNoAnswer(); }},
      {"no network", "call.reason-no-network", [](Page& p) { p.rig.modem.setNetwork("", 0); p.rig.modem.failDial(""); }},
      {"sms only", "call.reason-sms-only", [](Page& p) { p.rig.modem.setNetwork("Example Mobile", 4, VoiceStatus::SmsOnly); p.rig.modem.failDial(""); }},
      {"not ready", "call.reason-not-ready", [](Page& p) { p.rig.modem.failDial("", DialFailReason::NotReady); }},
      {"modem restarted", "call.reason-modem-restarted", [](Page& p) { p.rig.modem.restartDuringCall(); }},
      {"not connected", "call.reason-not-connected", [](Page& p) { p.rig.modem.failDial("rejected"); }}};
  for (const Case& c : cases) {
    for (const char* language : {"en", "de", "fr"}) {
      Page page;
      page.rig.language(language);
      page.dial();
      c.script(page);
      page.rig.poll();
      page.draw();
      const std::string phrase = page.rig.translator.get(c.catalogKey);
      TEST_ASSERT_TRUE_MESSAGE(says(page.rig.frame, phrase, FontSize::Text) ||
                                   shows::text(page.rig.frame, phrase.substr(0, phrase.find(' ')), FontSize::Text, kContent),
                               (std::string(c.name) + " in " + language + ": " + phrase).c_str());
    }
  }
}

void test_a_failed_call_is_only_shown_while_the_core_says_so_and_then_it_is_no_call() {
  Page page;
  page.dial();
  page.rig.modem.remoteNoAnswer();
  page.rig.poll();
  page.draw();
  TEST_ASSERT_TRUE(says(page.rig.frame, "Call failed", FontSize::Text));
  TEST_ASSERT_TRUE(says(page.rig.frame, "no answer", FontSize::Text));
  page.rig.run(Notices::kSeconds + 1);                              // the notice goes by itself
  page.draw();
  TEST_ASSERT_TRUE(says(page.rig.frame, "No call", FontSize::Text));
  TEST_ASSERT_FALSE(says(page.rig.frame, "Parent One", FontSize::Large));
  TEST_ASSERT_FALSE(says(page.rig.frame, "no answer", FontSize::Text));
  page.screen.handle(Input::pressed(Key::Back));
  TEST_ASSERT_EQUAL_STRING("back", page.rig.nav.last().c_str());
}

void test_a_call_that_was_only_missed_is_no_failure() {
  Page page;
  page.ring();
  page.rig.modem.remoteHangup();                                    // the caller gave up
  page.rig.poll();
  page.draw();
  TEST_ASSERT_TRUE(says(page.rig.frame, "No call", FontSize::Text));
  TEST_ASSERT_FALSE(says(page.rig.frame, "Call failed", FontSize::Text));
}

void test_with_no_call_the_page_says_so_and_back_goes_back() {
  Page page;
  page.draw();
  TEST_ASSERT_TRUE(says(page.rig.frame, "No call", FontSize::Text));
  TEST_ASSERT_EQUAL_STRING("Back", page.screen.softKeys().back.c_str());
  TEST_ASSERT_TRUE(page.screen.handle(Input::pressed(Key::Back)));
  TEST_ASSERT_EQUAL_STRING("back", page.rig.nav.last().c_str());
  TEST_ASSERT_FALSE(page.screen.handle(Input::pressed(Key::Select)));
}

// ---- soft keys by touch and the words in each language ---------------------------------------------

void test_the_buttons_work_by_touch_and_have_their_words_in_every_language() {
  const struct { const char* code; const char* hangUp; const char* reject; const char* answer; const char* quieter; const char* louder; } words[] = {
      {"en", "Hang up", "Reject", "Answer", "Quieter", "Louder"}, {"de", "Auflegen", "Ablehnen", "Annehmen", "Leiser", "Lauter"},
      {"fr", "Raccrocher", "Refuser", "R\xC3\xA9" "pondre", "Moins fort", "Plus fort"}};
  for (const auto& w : words) {
    Page ringing;
    ringing.rig.language(w.code);
    ringing.ring();
    TEST_ASSERT_EQUAL_STRING(w.reject, ringing.screen.softKeys().back.c_str());
    TEST_ASSERT_EQUAL_STRING(w.answer, ringing.screen.softKeys().action.c_str());
    const SoftKeySlots slots = softKeySlots(ringing.screen.softKeys(), ScreenLayout::standard().softKeys);
    TEST_ASSERT_TRUE(ringing.screen.handle(Input::touch(slots.action.x + 3, slots.action.y + 3)));
    ringing.rig.poll();
    TEST_ASSERT_TRUE(ringing.rig.core.state().callState == ModemState::InCall);
    ringing.draw();
    TEST_ASSERT_EQUAL_STRING(w.hangUp, ringing.screen.softKeys().back.c_str());
    const SoftKeySlots inCall = softKeySlots(ringing.screen.softKeys(), ScreenLayout::standard().softKeys);
    TEST_ASSERT_TRUE(inCall.back == ScreenLayout::standard().softKeys);                           // hang up is the whole bar
    const int before = ringing.rig.core.state().volume;
    const auto louder = shows::where(ringing.rig.frame, w.louder, FontSize::Small);
    const auto quieter = shows::where(ringing.rig.frame, w.quieter, FontSize::Small);
    TEST_ASSERT_TRUE_MESSAGE(louder.has_value(), (std::string(w.code) + " louder").c_str());
    TEST_ASSERT_TRUE_MESSAGE(quieter.has_value(), (std::string(w.code) + " quieter").c_str());
    TEST_ASSERT_TRUE(ringing.screen.handle(Input::touch(louder->x + 2, louder->y + 2)));         // louder by touch
    TEST_ASSERT_EQUAL(before + 1, ringing.rig.core.state().volume);
    TEST_ASSERT_TRUE(ringing.screen.handle(Input::touch(quieter->x + 2, quieter->y + 2)));       // and quieter
    TEST_ASSERT_EQUAL(before, ringing.rig.core.state().volume);
    TEST_ASSERT_TRUE(ringing.screen.handle(Input::touch(inCall.back.right() - 3, inCall.back.y + 3)));  // hang up, at the right end
    ringing.rig.poll();
    TEST_ASSERT_FALSE(ringing.rig.core.state().callActive);
  }
}

// ---- the lock --------------------------------------------------------------------------------------

void test_a_call_in_progress_keeps_the_device_from_locking() {
  DeviceRig rig;
  ScreenStack stack(screen::kHome);
  const ScreenContext context = rig.context(stack);
  CallScreen call(context);
  stack.add(screen::kHome, call);
  stack.add(screen::kLock, call);
  stack.setLockScreen(screen::kLock);
  stack.setAutoLockSeconds(60);
  stack.begin(0);
  rig.poll();
  stack.poll(1000);
  TEST_ASSERT_TRUE(stack.locked());                                   // no call: it locks
  stack.unlock();
  stack.handle(Input::pressed(Key::Select), 1000);
  rig.core.dial(kParentOne);
  rig.poll();
  TEST_ASSERT_TRUE(call.preventsAutoLock());
  stack.poll(5000);
  TEST_ASSERT_FALSE(stack.locked());                                  // a call: it does not, however long
  rig.modem.remoteAnswer();
  rig.poll();
  TEST_ASSERT_TRUE(call.preventsAutoLock());
  rig.core.hangup();
  rig.poll();
  TEST_ASSERT_FALSE(call.preventsAutoLock());
  stack.poll(5000 + 61);
  TEST_ASSERT_TRUE(stack.locked());
}

// ---- layout ------------------------------------------------------------------------------------------

void test_every_phase_is_drawn_inside_the_screen_in_every_language() {
  languages::forEach([](const std::string& language, i18n::Translator&) {
    for (int phase = 0; phase < 6; ++phase) {
      Page page;
      page.rig.language(language);
      switch (phase) {
        case 0: page.dial(); break;
        case 1: page.ring(); break;
        case 2: page.dial(); page.connect(); break;
        case 3: page.dial(); page.rig.modem.remoteBusy(); page.rig.poll(); break;
        case 4: page.draw(); break;
        case 5: page.ring(kStranger); break;                         // a number in the large font's place
      }
      FrameBuffer fb;
      page.screen.render(fb);
      TEST_ASSERT_FALSE_MESSAGE(fb.clipped(), (language + " phase " + std::to_string(phase) + ":\n" + fb.describeClips()).c_str());
    }
  });
}

void test_a_long_name_is_drawn_whole_or_shortened_inside_the_screen() {
  Page page(ProfileBuilder().noContacts().contact(kGrandma, "Grandma Elisabeth Maria", false).build());
  page.ring(kGrandma);
  FrameBuffer fb;
  page.screen.render(fb);
  TEST_ASSERT_FALSE_MESSAGE(fb.clipped(), fb.describeClips().c_str());
  TEST_ASSERT_TRUE(saysWhole(page.rig.frame, "Grandma Elisabeth Maria"));
}

void test_the_pictures_in_english_and_german() {
  for (const char* language : {"en", "de"}) {
    Page ringing;
    ringing.rig.language(language);
    ringing.ring();
    golden::expect(ringing.rig.frame, std::string("call_ringing_") + language);
    Page connected;
    connected.rig.language(language);
    connected.rig.core.setDefaultVolume(4);
    connected.dial();
    connected.connect();
    golden::expect(connected.rig.frame, std::string("call_connected_") + language);
    Page failed;
    failed.rig.language(language);
    failed.dial();
    failed.rig.modem.remoteBusy();
    failed.rig.poll();
    failed.draw();
    golden::expect(failed.rig.frame, std::string("call_failed_") + language);
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_while_dialling_it_says_so_names_the_one_called_and_offers_hang_up);
  RUN_TEST(test_back_while_dialling_hangs_up_and_does_not_leave);
  RUN_TEST(test_a_ringing_call_offers_answer_and_reject);
  RUN_TEST(test_enter_answers_a_ringing_call);
  RUN_TEST(test_back_rejects_a_ringing_call);
  RUN_TEST(test_a_caller_who_is_no_contact_is_shown_by_the_whole_number);
  RUN_TEST(test_a_stranger_and_a_hidden_number_are_not_on_the_page_at_all);
  RUN_TEST(test_a_number_that_does_not_fit_the_large_font_is_drawn_smaller_not_shortened);
  RUN_TEST(test_a_connected_call_shows_the_volume_as_boxes_and_the_arrows_change_it);
  RUN_TEST(test_the_volume_stops_at_both_ends);
  RUN_TEST(test_there_is_no_volume_before_the_call_is_connected_and_the_arrows_change_nothing);
  RUN_TEST(test_without_a_volume_control_in_the_modem_there_are_no_boxes_and_no_arrows);
  RUN_TEST(test_back_hangs_up_a_connected_call);
  RUN_TEST(test_a_busy_number_stays_on_the_page_with_its_name_and_the_reason);
  RUN_TEST(test_every_reason_is_told_in_the_language_of_the_moment);
  RUN_TEST(test_a_failed_call_is_only_shown_while_the_core_says_so_and_then_it_is_no_call);
  RUN_TEST(test_a_call_that_was_only_missed_is_no_failure);
  RUN_TEST(test_with_no_call_the_page_says_so_and_back_goes_back);
  RUN_TEST(test_the_buttons_work_by_touch_and_have_their_words_in_every_language);
  RUN_TEST(test_a_call_in_progress_keeps_the_device_from_locking);
  RUN_TEST(test_every_phase_is_drawn_inside_the_screen_in_every_language);
  RUN_TEST(test_a_long_name_is_drawn_whole_or_shortened_inside_the_screen);
  RUN_TEST(test_the_pictures_in_english_and_german);
  return UNITY_END();
}

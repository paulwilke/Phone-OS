// Host tests for the frame every screen shares (status bar, notice line, soft keys, touch
// translation) and for what the status bar takes from the core's state.
// allow-non-english: the catalogs under test contain German and French.

#include <unity.h>

#include <DeviceRig.h>
#include <Shows.h>
#include <ui/AppStatus.h>
#include <ui/Fonts.h>
#include <ui/Text.h>
#include <ui/Wording.h>

using namespace people;
using namespace ui;
using i18n::Translator;

void setUp() {}
void tearDown() {}

namespace {

// A screen with nothing of its own but a word in the content and the soft keys it is told to
// have, recording what reached it.
class Probe : public AppScreen {
public:
  using AppScreen::AppScreen;
  SoftKeys softKeyBar{"key.back", true, "key.open"};
  std::vector<Input> received;
  Rect contentSeen;

protected:
  void draw(Display& display, const Rect& area) override {
    text::draw(display, font(FontSize::Text), area.x + 8, area.y + 8, t().get("home.phone"));
  }
  SoftKeys keys() override {
    SoftKeys k;
    k.back = t().get(softKeyBar.back.c_str());
    k.scroll = softKeyBar.scroll;
    k.action = softKeyBar.action.empty() ? std::string() : t().get(softKeyBar.action.c_str());
    return k;
  }
  bool onInput(const Input& input, const Rect& area) override {
    received.push_back(input);
    contentSeen = area;
    return true;
  }
};

}  // namespace

// ---- what the status bar takes from the state ------------------------------------------------------

void test_the_sim_states_become_the_bars_sim_states() {
  AppCore::State s;
  const struct { SimStatus in; StatusModel::Sim out; } table[] = {
      {SimStatus::Unknown, StatusModel::Sim::Ready},         {SimStatus::Ready, StatusModel::Sim::Ready},
      {SimStatus::Missing, StatusModel::Sim::Missing},       {SimStatus::PinRequired, StatusModel::Sim::PinRequired},
      {SimStatus::PinWrong, StatusModel::Sim::PinRequired},  {SimStatus::PukRequired, StatusModel::Sim::Locked}};
  for (const auto& row : table) {
    s.sim = row.in;
    TEST_ASSERT_TRUE_MESSAGE(statusModelOf(s).sim == row.out, std::to_string(static_cast<int>(row.in)).c_str());
  }
}

void test_the_network_label_is_taken_as_data() {
  using Kind = Connectivity::NetworkLabel::Kind;
  AppCore::State s;
  s.network.kind = Kind::Operator;
  s.network.name = "Example Mobile";
  StatusModel m = statusModelOf(s);
  TEST_ASSERT_TRUE(m.link == StatusModel::Link::Operator);
  TEST_ASSERT_EQUAL_STRING("Example Mobile", m.operatorName.c_str());
  s.network.kind = Kind::Radio;
  TEST_ASSERT_TRUE(statusModelOf(s).link == StatusModel::Link::Radio);
  s.network.kind = Kind::RadioOnly;
  TEST_ASSERT_TRUE(statusModelOf(s).link == StatusModel::Link::RadioOnly);
  s.network.kind = Kind::None;
  TEST_ASSERT_TRUE(statusModelOf(s).link == StatusModel::Link::None);
}

void test_school_time_voice_position_battery_and_date_are_taken_over() {
  AppCore::State s;
  s.schoolTime = true;
  s.voice = VoiceStatus::Ready;
  s.positioningActive = true;
  s.batteryPercent = 42;
  s.wall.known = true;
  s.wall.month = 9;
  s.wall.day = 15;
  s.wall.weekday = 2;
  StatusModel m = statusModelOf(s);
  TEST_ASSERT_TRUE(m.quietHours);
  TEST_ASSERT_TRUE(m.voice == StatusModel::Voice::Ready);
  TEST_ASSERT_TRUE(m.locating);
  TEST_ASSERT_EQUAL(42, m.batteryPercent);
  TEST_ASSERT_TRUE(m.dateKnown);
  TEST_ASSERT_EQUAL(9, m.month);
  TEST_ASSERT_EQUAL(15, m.day);
  TEST_ASSERT_EQUAL(2, m.weekday);
  s = AppCore::State();
  m = statusModelOf(s);
  TEST_ASSERT_FALSE(m.quietHours);
  TEST_ASSERT_FALSE(m.locating);
  TEST_ASSERT_FALSE(m.dateKnown);
  TEST_ASSERT_EQUAL(-1, m.batteryPercent);
}

// No verdict yet is no complaint: the bar says nothing about calls until the modem has asked.
void test_every_voice_status_has_its_place_in_the_bar() {
  AppCore::State s;
  s.voice = VoiceStatus::Unknown;
  TEST_ASSERT_TRUE(statusModelOf(s).voice == StatusModel::Voice::Ready);
  s.voice = VoiceStatus::SmsOnly;
  TEST_ASSERT_TRUE(statusModelOf(s).voice == StatusModel::Voice::SmsOnly);
  s.voice = VoiceStatus::ImsMissing;
  TEST_ASSERT_TRUE(statusModelOf(s).voice == StatusModel::Voice::NotReady);
}

void test_the_bar_of_a_running_device_shows_its_state() {
  DeviceRig rig(ProfileBuilder().schoolTime("tue", "14:00", "15:00").build());
  rig.poll();
  const StatusModel m = statusModelOf(rig.core.state());
  TEST_ASSERT_TRUE(m.link == StatusModel::Link::Operator);
  TEST_ASSERT_EQUAL_STRING("Example", m.operatorName.c_str());   // the core shortens the name for the line
  TEST_ASSERT_TRUE(m.quietHours);                       // 14:32 on a Tuesday
  TEST_ASSERT_TRUE(m.dateKnown);
  TEST_ASSERT_EQUAL(15, m.day);
  rig.modem.setNetwork("", 0);
  rig.poll();
  TEST_ASSERT_TRUE(statusModelOf(rig.core.state()).link == StatusModel::Link::None);
}

// ---- the frame --------------------------------------------------------------------------------

void test_the_frame_draws_the_status_bar_the_notice_and_the_soft_keys_around_the_content() {
  DeviceRig rig;
  Probe screen(rig.context());
  rig.core.dial(kStranger);                                // not on any list: a notice says so
  rig.poll();
  screen.render(rig.frame);

  const ScreenLayout l = ScreenLayout::standard();
  FrameBuffer expected;
  expected.fillRect({0, 0, 240, 320}, false);
  drawStatusBar(expected, l.status, statusModelOf(rig.core.state()), rig.translator);
  drawAlertBar(expected, l.alert, "Number not allowed");
  TEST_ASSERT_EQUAL_STRING("Number not allowed", screen.noticeLine().c_str());
  // What the child reads: the notice on the black line, the network on the status bar, the soft keys.
  TEST_ASSERT_TRUE(shows::text(rig.frame, "Number not allowed", FontSize::Small, l.alert, true));
  TEST_ASSERT_TRUE(shows::text(rig.frame, "Example", FontSize::Small, l.status));
  TEST_ASSERT_TRUE(rig.frame.toAscii(l.status) == expected.toAscii(l.status));
  TEST_ASSERT_TRUE(rig.frame.toAscii(l.alert) == expected.toAscii(l.alert));
  drawSoftKeys(expected, l.softKeys, screen.softKeys());
  TEST_ASSERT_TRUE(rig.frame.toAscii(l.softKeys) == expected.toAscii(l.softKeys));
  TEST_ASSERT_TRUE(rig.frame.blackPixels(l.content) > 0);   // the screen's own part is there
}

void test_the_notice_line_is_reserved_and_empty_without_a_notice() {
  DeviceRig rig;
  Probe screen(rig.context());
  rig.poll();
  screen.render(rig.frame);
  TEST_ASSERT_EQUAL_STRING("", screen.noticeLine().c_str());
  TEST_ASSERT_EQUAL(0, rig.frame.blackPixels(ScreenLayout::standard().alert));
}

void test_the_notice_follows_the_language_at_the_next_frame() {
  DeviceRig rig;
  Probe screen(rig.context());
  rig.core.dial(kStranger);
  rig.poll();
  screen.render(rig.frame);
  TEST_ASSERT_EQUAL_STRING("Number not allowed", screen.noticeLine().c_str());
  const Rect alert = ScreenLayout::standard().alert;
  TEST_ASSERT_TRUE(shows::text(rig.frame, "Number not allowed", FontSize::Small, alert, true));
  rig.language("de");
  screen.render(rig.frame);
  TEST_ASSERT_EQUAL_STRING("Nummer nicht erlaubt", screen.noticeLine().c_str());
  TEST_ASSERT_TRUE(shows::text(rig.frame, "Nummer nicht erlaubt", FontSize::Small, alert, true));
  TEST_ASSERT_FALSE(shows::text(rig.frame, "Number not allowed", FontSize::Small, alert, true));   // nothing of the old language is left
  rig.language("fr");
  screen.render(rig.frame);
  TEST_ASSERT_EQUAL_STRING("Num\xC3\xA9ro non autoris\xC3\xA9", screen.noticeLine().c_str());
  TEST_ASSERT_TRUE(shows::text(rig.frame, "Num\xC3\xA9ro non autoris\xC3\xA9", FontSize::Small, alert, true));
}

void test_a_notice_names_the_contact_from_the_profile() {
  DeviceRig rig;
  Probe screen(rig.context());
  rig.modem.setNetwork("Example Mobile", 4);
  rig.core.dial(kParentOne);                               // rings out, nobody answers
  rig.poll();
  rig.modem.remoteNoAnswer();
  rig.poll();
  screen.render(rig.frame);
  TEST_ASSERT_EQUAL_STRING("Parent One: no answer", screen.noticeLine().c_str());
}

void test_a_frame_paints_the_whole_screen_so_nothing_of_an_earlier_frame_shows() {
  DeviceRig rig;
  Probe screen(rig.context());
  rig.poll();
  FrameBuffer clean;
  screen.render(clean);
  FrameBuffer dirty;
  dirty.fillRect({0, 0, 240, 320}, true);
  screen.render(dirty);
  TEST_ASSERT_TRUE(clean == dirty);
}

void test_the_frame_is_drawn_inside_the_screen_in_every_language() {
  languages::forEach([](const std::string& language, i18n::Translator&) {
    DeviceRig rig;
    rig.language(language);
    Probe screen(rig.context());
    rig.core.dial(kStranger);
    rig.poll();
    screen.render(rig.frame);
    TEST_ASSERT_FALSE_MESSAGE(rig.frame.clipped(), (language + ":\n" + rig.frame.describeClips()).c_str());
  });
}

void test_a_notice_with_a_very_long_name_is_shortened_inside_the_bar_in_every_language() {
  // The longest name a contact can have (24 bytes) in the longest notice; the bar shortens the
  // line with "..." and draws nothing outside itself.
  Profile profile = ProfileBuilder().noContacts().contact(kParentTwo, "Parent Two", true).contact(kGrandma, "Grandma Elisabeth Maria", false).build();
  languages::forEach([&](const std::string& language, i18n::Translator&) {
    DeviceRig rig(profile);
    rig.language(language);
    Probe screen(rig.context());
    rig.poll();
    rig.modem.dial(kGrandma);
    rig.poll();
    rig.modem.remoteNoAnswer();
    rig.poll();
    FrameBuffer fb;
    fb.beginRegion(ScreenLayout::standard().alert);
    drawAlertBar(fb, ScreenLayout::standard().alert, wording::notice(rig.core.state().notice, rig.core.profile().contacts, rig.translator));
    fb.endRegion();
    TEST_ASSERT_FALSE_MESSAGE(fb.clipped(), (language + ":\n" + fb.describeClips()).c_str());
    screen.render(rig.frame);
    TEST_ASSERT_TRUE_MESSAGE(rig.frame.blackPixels(ScreenLayout::standard().alert) > 0, language.c_str());
  });
}

void test_a_frame_with_a_notice_in_english_and_german() {
  for (const char* language : {"en", "de"}) {
    DeviceRig rig;
    rig.language(language);
    Probe screen(rig.context());
    rig.core.dial(kStranger);
    rig.poll();
    screen.render(rig.frame);
    golden::expect(rig.frame, std::string("app_screen_frame_") + language);
  }
}

void test_school_time_and_a_locked_sim_are_named_on_the_bar_in_every_language() {
  const struct { const char* code; const char* school; const char* locked; } words[] = {
      {"en", "School time", "SIM locked"}, {"de", "Schulzeit", "SIM gesperrt"},
      {"fr", "En classe", "SIM verrouill\xC3\xA9" "e"}};
  for (const auto& w : words) {
    DeviceRig rig(ProfileBuilder().schoolTime("tue", "14:00", "15:00").build());
    rig.language(w.code);
    rig.poll();
    TEST_ASSERT_EQUAL_STRING(w.school, statusLeft(statusModelOf(rig.core.state()), rig.translator).text.c_str());
    rig.modem.setSimStatus(SimStatus::PukRequired);          // a locked SIM is not "no network", and it wins over school time
    rig.poll();
    TEST_ASSERT_EQUAL_STRING(w.locked, statusLeft(statusModelOf(rig.core.state()), rig.translator).text.c_str());
  }
  DeviceRig outside;                                         // without a school-time window the bar says nothing of it
  outside.poll();
  TEST_ASSERT_EQUAL_STRING("Example", statusLeft(statusModelOf(outside.core.state()), outside.translator).text.c_str());
}

void test_every_notice_fits_the_alert_bar_in_the_shipped_languages_for_a_name_a_long_name_and_a_number() {
  // What the device itself writes must fit, so that the words the child needs are never cut: a
  // long name is shortened inside the line, and a number (which is never shortened) must leave
  // room for the words around it.
  const int room = ScreenLayout::standard().alert.w - kAlertBarMargin;
  const phone::ContactBook contacts =
      ProfileBuilder().noContacts().contact(kParentTwo, "Parent Two", true).contact(kGrandma, "Grandma Elisabeth Maria", false).build().contacts;
  std::string tooLong;
  languages::forEach([&](const std::string& language, i18n::Translator& t) {
    if (language == i18n::kPseudoCode) return;   // the pseudo-language is longer than any real one on purpose
    // A number appears in a notice where the one who asked is a stranger; everywhere else it is a name.
    for (const char* peer : {kParentTwo, kGrandma, kStranger}) {
      for (int k = 0; k < static_cast<int>(NoticeKind::Count); ++k) {
        if (std::string(peer) == kStranger && static_cast<NoticeKind>(k) != NoticeKind::QueryRefused) continue;
        Notice n{static_cast<NoticeKind>(k), peer, 13 * 60 + 30};
        const std::string text = wording::notice(n, contacts, t, room);
        const int width = text::width(font(FontSize::Small), text);
        if (width > room)
          tooLong += language + " kind " + std::to_string(k) + " " + peer + ": " + text + " (" + std::to_string(width) + " px)\n";
      }
    }
  });
  TEST_ASSERT_TRUE_MESSAGE(tooLong.empty(), ("wider than " + std::to_string(room) + " px:\n" + tooLong).c_str());
}

void test_a_long_name_is_shortened_inside_the_line_and_the_words_after_it_stay() {
  Translator t = languages::loaded();
  const phone::ContactBook contacts =
      ProfileBuilder().noContacts().contact(kParentTwo, "Parent Two", true).contact(kGrandma, "Grandma Elisabeth Maria", false).build().contacts;
  const Notice n{NoticeKind::CallNoAnswer, kGrandma, -1};
  const std::string whole = wording::notice(n, contacts, t);
  TEST_ASSERT_EQUAL_STRING("Grandma Elisabeth Maria: no answer", whole.c_str());
  const std::string line = wording::notice(n, contacts, t, 200);
  TEST_ASSERT_TRUE(text::width(font(FontSize::Small), line) <= 200);
  TEST_ASSERT_TRUE(line.find("Grandma") == 0);
  TEST_ASSERT_TRUE_MESSAGE(line.find("...: no answer") != std::string::npos, line.c_str());
  // Room enough: not touched.
  TEST_ASSERT_EQUAL_STRING(whole.c_str(), wording::notice(n, contacts, t, 400).c_str());
}

void test_a_number_is_never_shortened_in_a_notice() {
  Translator t = languages::loaded();
  const phone::ContactBook contacts = ProfileBuilder().build().contacts;
  const Notice n{NoticeKind::QueryRefused, kStranger, -1};
  for (const char* language : {"en", "de", "fr"}) {
    t.select(language);
    // Narrow enough that a name would be shortened, wide enough that "..." would have room.
    const std::string line = wording::notice(n, contacts, t, 150);
    TEST_ASSERT_TRUE_MESSAGE(line.find(kStranger) != std::string::npos, (std::string(language) + ": " + line).c_str());
    TEST_ASSERT_TRUE(line.find("...") == std::string::npos);
  }
}

void test_a_notice_does_not_move_the_content() {
  DeviceRig rig;
  Probe screen(rig.context());
  rig.poll();
  screen.render(rig.frame);
  const Rect content = ScreenLayout::standard().content;
  const std::string without = rig.frame.toAscii(content);
  rig.core.dial(kStranger);
  rig.poll();
  screen.render(rig.frame);
  TEST_ASSERT_TRUE(rig.frame.blackPixels(ScreenLayout::standard().alert) > 0);
  TEST_ASSERT_TRUE(without == rig.frame.toAscii(content));
}

// ---- finding a text in a picture ------------------------------------------------------------------

void test_a_text_is_found_where_it_is_drawn_and_not_elsewhere() {
  FrameBuffer fb;
  text::draw(fb, font(FontSize::Text), 37, 101, "Inbox");
  const std::optional<Rect> at = shows::where(fb, "Inbox", FontSize::Text);
  TEST_ASSERT_TRUE(at.has_value());
  TEST_ASSERT_EQUAL(37, at->x);
  TEST_ASSERT_EQUAL(101, at->y);
  TEST_ASSERT_FALSE(shows::text(fb, "Inbox", FontSize::Small));              // another font is another picture
  TEST_ASSERT_FALSE(shows::text(fb, "inbox", FontSize::Text));
  TEST_ASSERT_FALSE(shows::text(fb, "Inbox", FontSize::Text, Rect{0, 0, 240, 100}));   // searched where it is not
  TEST_ASSERT_TRUE(shows::text(fb, "Inbox", FontSize::Text, Rect{30, 95, 100, 30}));
}

void test_a_word_is_not_found_inside_a_longer_one_or_when_cut_off() {
  FrameBuffer fb;
  text::draw(fb, font(FontSize::Text), 10, 10, "Tests");
  TEST_ASSERT_FALSE(shows::text(fb, "Test", FontSize::Text));                // the "s" is ink in the box
  TEST_ASSERT_TRUE(shows::text(fb, "Tests", FontSize::Text));
  FrameBuffer cut;
  text::draw(cut, font(FontSize::Text), 10, 10, "Test");
  TEST_ASSERT_FALSE(shows::text(cut, "Tests", FontSize::Text));
  TEST_ASSERT_FALSE(shows::text(cut, "", FontSize::Text));                   // nothing is not found anywhere
}

void test_white_on_black_is_found_only_when_asked_for() {
  FrameBuffer fb;
  fb.fillRect({0, 0, 120, 20}, true);
  text::draw(fb, font(FontSize::Small), 4, 2, "Light", false);
  TEST_ASSERT_TRUE(shows::text(fb, "Light", FontSize::Small, Rect{}, true));
  TEST_ASSERT_FALSE(shows::text(fb, "Light", FontSize::Small));
}

// ---- what the screens hand to each other -------------------------------------------------------------

void test_screens_of_one_context_share_one_session() {
  struct Writer : Probe {
    using Probe::Probe;
    void choose(const std::string& who) { session().recipient = who; session().viaRadio = true; }
  };
  struct Reader : Probe {
    using Probe::Probe;
    std::string who() { return session().recipient; }
    bool radio() { return session().viaRadio; }
  };
  DeviceRig rig;
  Writer writer(rig.context());
  Reader reader(rig.context());
  TEST_ASSERT_EQUAL_STRING("", reader.who().c_str());
  writer.choose(kParentOne);
  TEST_ASSERT_EQUAL_STRING(kParentOne, reader.who().c_str());
  TEST_ASSERT_TRUE(reader.radio());
  rig.session.clear();                                      // a finished task leaves nothing behind
  TEST_ASSERT_EQUAL_STRING("", reader.who().c_str());
  TEST_ASSERT_FALSE(reader.radio());
}

void test_a_screen_can_read_what_it_was_given_in_a_const_function() {
  struct Reader : Probe {
    using Probe::Probe;
    std::string child() const { return context().app.profile().childName; }
  };
  DeviceRig rig;
  Reader reader(rig.context());
  TEST_ASSERT_EQUAL_STRING("Alex", reader.child().c_str());
}

// ---- input ------------------------------------------------------------------------------------

void test_a_key_reaches_the_screen_as_it_is() {
  DeviceRig rig;
  Probe screen(rig.context());
  rig.poll();
  screen.render(rig.frame);
  TEST_ASSERT_TRUE(screen.handle(Input::pressed(Key::Down)));
  TEST_ASSERT_EQUAL(1, screen.received.size());
  TEST_ASSERT_TRUE(screen.received[0].isKey(Key::Down));
}

void test_a_tap_on_a_soft_key_reaches_the_screen_as_the_key() {
  DeviceRig rig;
  Probe screen(rig.context());
  rig.poll();
  screen.render(rig.frame);
  const ScreenLayout l = ScreenLayout::standard();
  const SoftKeySlots slots = softKeySlots(screen.softKeys(), l.softKeys);
  TEST_ASSERT_TRUE(screen.handle(Input::touch(slots.back.x + 3, slots.back.y + 3)));
  TEST_ASSERT_TRUE(screen.handle(Input::touch(slots.down.x + 3, slots.down.y + 3)));
  TEST_ASSERT_TRUE(screen.handle(Input::touch(slots.up.x + 3, slots.up.y + 3)));
  TEST_ASSERT_TRUE(screen.handle(Input::touch(slots.action.x + 3, slots.action.y + 3)));
  TEST_ASSERT_EQUAL(4, screen.received.size());
  TEST_ASSERT_TRUE(screen.received[0].isKey(Key::Back));
  TEST_ASSERT_TRUE(screen.received[1].isKey(Key::Down));
  TEST_ASSERT_TRUE(screen.received[2].isKey(Key::Up));
  TEST_ASSERT_TRUE(screen.received[3].isKey(Key::Select));
}

void test_a_tap_between_the_soft_keys_or_without_buttons_is_nothing() {
  DeviceRig rig;
  Probe screen(rig.context());
  screen.softKeyBar = SoftKeys{"key.back", false, ""};     // only Back
  rig.poll();
  screen.render(rig.frame);
  const ScreenLayout l = ScreenLayout::standard();
  const SoftKeySlots slots = softKeySlots(screen.softKeys(), l.softKeys);
  TEST_ASSERT_FALSE(screen.handle(Input::touch(slots.back.right() + 20, l.softKeys.y + 3)));
  TEST_ASSERT_EQUAL(0, screen.received.size());
}

void test_a_tap_on_the_content_reaches_the_screen_as_a_touch_with_the_content_area() {
  DeviceRig rig;
  Probe screen(rig.context());
  rig.poll();
  screen.render(rig.frame);
  const ScreenLayout l = ScreenLayout::standard();
  TEST_ASSERT_TRUE(screen.handle(Input::touch(100, l.content.y + 40)));
  TEST_ASSERT_EQUAL(1, screen.received.size());
  TEST_ASSERT_TRUE(screen.received[0].kind == Input::Kind::Touch);
  TEST_ASSERT_EQUAL(100, screen.received[0].x);
  TEST_ASSERT_TRUE(screen.contentSeen == l.content);
}

void test_a_tap_on_the_bars_is_not_for_the_screen() {
  DeviceRig rig;
  Probe screen(rig.context());
  rig.poll();
  screen.render(rig.frame);
  const ScreenLayout l = ScreenLayout::standard();
  TEST_ASSERT_FALSE(screen.handle(Input::touch(100, l.status.y + 3)));
  TEST_ASSERT_FALSE(screen.handle(Input::touch(100, l.alert.y + 3)));
  TEST_ASSERT_FALSE(screen.handle(Input::touch(100, l.content.bottom() + 1)));   // the gap above the keys
  TEST_ASSERT_EQUAL(0, screen.received.size());
}

void test_the_screen_sees_the_state_of_the_moment_of_the_input() {
  struct Reader : Probe {
    using Probe::Probe;
    int unreadSeen = -1;
    bool onInput(const Input&, const Rect&) override {
      unreadSeen = static_cast<int>(state().unread);
      return true;
    }
  };
  DeviceRig rig;
  Reader screen(rig.context());
  rig.poll();
  screen.render(rig.frame);
  rig.modem.injectSms(kParentOne, "Hello", 1);
  rig.poll();
  screen.handle(Input::pressed(Key::Select));      // no frame in between: the input still sees the new message
  TEST_ASSERT_EQUAL(1, screen.unreadSeen);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_the_sim_states_become_the_bars_sim_states);
  RUN_TEST(test_the_network_label_is_taken_as_data);
  RUN_TEST(test_school_time_voice_position_battery_and_date_are_taken_over);
  RUN_TEST(test_every_voice_status_has_its_place_in_the_bar);
  RUN_TEST(test_the_bar_of_a_running_device_shows_its_state);
  RUN_TEST(test_the_frame_draws_the_status_bar_the_notice_and_the_soft_keys_around_the_content);
  RUN_TEST(test_the_notice_line_is_reserved_and_empty_without_a_notice);
  RUN_TEST(test_the_notice_follows_the_language_at_the_next_frame);
  RUN_TEST(test_a_notice_names_the_contact_from_the_profile);
  RUN_TEST(test_a_frame_paints_the_whole_screen_so_nothing_of_an_earlier_frame_shows);
  RUN_TEST(test_the_frame_is_drawn_inside_the_screen_in_every_language);
  RUN_TEST(test_a_notice_with_a_very_long_name_is_shortened_inside_the_bar_in_every_language);
  RUN_TEST(test_a_frame_with_a_notice_in_english_and_german);
  RUN_TEST(test_school_time_and_a_locked_sim_are_named_on_the_bar_in_every_language);
  RUN_TEST(test_every_notice_fits_the_alert_bar_in_the_shipped_languages_for_a_name_a_long_name_and_a_number);
  RUN_TEST(test_a_long_name_is_shortened_inside_the_line_and_the_words_after_it_stay);
  RUN_TEST(test_a_number_is_never_shortened_in_a_notice);
  RUN_TEST(test_a_notice_does_not_move_the_content);
  RUN_TEST(test_a_text_is_found_where_it_is_drawn_and_not_elsewhere);
  RUN_TEST(test_a_word_is_not_found_inside_a_longer_one_or_when_cut_off);
  RUN_TEST(test_white_on_black_is_found_only_when_asked_for);
  RUN_TEST(test_screens_of_one_context_share_one_session);
  RUN_TEST(test_a_screen_can_read_what_it_was_given_in_a_const_function);
  RUN_TEST(test_a_key_reaches_the_screen_as_it_is);
  RUN_TEST(test_a_tap_on_a_soft_key_reaches_the_screen_as_the_key);
  RUN_TEST(test_a_tap_between_the_soft_keys_or_without_buttons_is_nothing);
  RUN_TEST(test_a_tap_on_the_content_reaches_the_screen_as_a_touch_with_the_content_area);
  RUN_TEST(test_a_tap_on_the_bars_is_not_for_the_screen);
  RUN_TEST(test_the_screen_sees_the_state_of_the_moment_of_the_input);
  return UNITY_END();
}

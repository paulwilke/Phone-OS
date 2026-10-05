// Host tests for the emergency component: the SOS button with its choice, countdown and school-time
// confirmation, the ping that sends the position, and the reply to a parent's location query.

#include <unity.h>

#include <fstream>
#include <sstream>
#include <vector>

#include <CoreFakes.h>
#include <app/Emergency.h>
#include <app/SystemEntry.h>

using namespace people;

void setUp() {}
void tearDown() {}

namespace {

std::string readFile(const std::string& name) {
  std::ifstream in("lang/" + name);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

void loadCatalogs(i18n::Translator& translator) {
  for (const char* file : {"en.toml", "de.toml", "fr.toml"}) {
    i18n::Catalog catalog;
    const std::string text = readFile(file);
    TEST_ASSERT_TRUE_MESSAGE(!text.empty() && catalog.parse(text.data(), text.size()), file);
    translator.add(catalog);
  }
}

struct Rig : CoreFakes {
  Positioning positioning;
  Messaging messaging;
  i18n::Translator translator;
  Emergency emergency;

  // What Messaging asks of the core goes to the emergency component, as the application core
  // will wire it.
  struct Bridge : Messaging::Hooks {
    Emergency* emergency = nullptr;
    bool startLocationReply(const std::string& phone) override { return emergency->startLocationReply(phone); }
    bool smsSendFailed(const std::string& phone) override { return emergency->noteSendFailed(phone); }
  } bridge;

  explicit Rig(Profile p = ProfileBuilder().childName("Alex").build())
      : CoreFakes(std::move(p), 100),
        positioning(clock, power, gnss, logger),
        messaging(clock, modem, power, store, profile, school, notices, haptic, logger),
        emergency((loadCatalogs(translator), clock), positioning, messaging, school, store, profile, translator, haptic, logger) {
    bridge.emergency = &emergency;
    messaging.setHooks(&bridge);
    messaging.configure({"#WhereAreYou"});
    gnss.fixAfter(2, 48.1, 11.5, 1.5);
  }

  // One pass of the main loop.
  void poll() {
    messaging.poll();
    positioning.poll();
    emergency.poll();
  }
  // Seconds pass, polled every second as the device does.
  void run(uint32_t seconds) {
    for (uint32_t i = 0; i < seconds; ++i) {
      advance(1);
      poll();
    }
  }
  size_t sentCount() const { return modem.sent().size(); }
  const FakeModem::SentSms& lastSent() const { return modem.sent().back(); }
  bool receiverOn() { return power.isEnabled(PowerDomain::Positioning); }
  bool logged(const std::string& part) const { return log.contains(part); }
};

Profile inSchool() { return ProfileBuilder().childName("Alex").schoolTime("tue", "14:00", "15:00").build(); }

bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

}  // namespace

// ---- the button and the choice ------------------------------------------------------------------

void test_without_a_recipient_sos_cannot_begin() {
  Rig r(ProfileBuilder().noContacts().build());
  TEST_ASSERT_FALSE(r.emergency.begin());
  TEST_ASSERT_TRUE(SosFlow::Phase::Idle == r.emergency.phase());
  TEST_ASSERT_FALSE(r.receiverOn());
  // An explicit target is a recipient, with or without contacts.
  TEST_ASSERT_TRUE(r.emergency.begin(kParentTwo));
  TEST_ASSERT_TRUE(r.emergency.choose(SosPing::Kind::Emergency));
  r.run(30);
  TEST_ASSERT_EQUAL_STRING(kParentTwo, r.lastSent().phone.c_str());
}

void test_the_first_call_target_is_the_default_recipient() {
  Rig r(ProfileBuilder().callTargets({kParentTwo, kParentOne}).build());
  TEST_ASSERT_TRUE(r.emergency.begin());
  r.emergency.choose(SosPing::Kind::Emergency);
  r.run(30);
  TEST_ASSERT_EQUAL_STRING(kParentTwo, r.lastSent().phone.c_str());
}

void test_pressing_sos_starts_the_receiver_at_once() {
  Rig r;
  TEST_ASSERT_FALSE(r.receiverOn());
  TEST_ASSERT_TRUE(r.emergency.begin());
  TEST_ASSERT_TRUE(SosFlow::Phase::Choosing == r.emergency.phase());
  TEST_ASSERT_TRUE(r.receiverOn());                                  // before anybody has chosen
  TEST_ASSERT_TRUE(r.positioning.wants(Positioning::Reason::SosChoice));
  TEST_ASSERT_EQUAL(0, r.sentCount());
}

void test_choosing_starts_the_ping() {
  Rig r;
  r.emergency.begin();
  TEST_ASSERT_TRUE(r.emergency.choose(SosPing::Kind::Emergency));
  TEST_ASSERT_TRUE(SosFlow::Phase::Idle == r.emergency.phase());
  TEST_ASSERT_TRUE(r.emergency.ping().active());
  TEST_ASSERT_TRUE(r.emergency.emergencyRunning());
  // The ping holds the receiver, and it was never off in between.
  TEST_ASSERT_FALSE(r.positioning.wants(Positioning::Reason::SosChoice));
  TEST_ASSERT_TRUE(r.positioning.wants(Positioning::Reason::SosPing));
  TEST_ASSERT_TRUE(r.receiverOn());
  TEST_ASSERT_EQUAL(1, r.power.switchOnCount(PowerDomain::Positioning));
  TEST_ASSERT_TRUE(r.logged("SOS ping (emergency) started to Parent One"));
}

void test_the_ping_sends_the_position() {
  Rig r;
  r.emergency.begin();
  r.emergency.choose(SosPing::Kind::Emergency);
  r.run(24);
  TEST_ASSERT_EQUAL(0, r.sentCount());                               // a good fix, but too early
  r.run(2);
  TEST_ASSERT_EQUAL(1, r.sentCount());
  TEST_ASSERT_EQUAL_STRING(kParentOne, r.lastSent().phone.c_str());
  TEST_ASSERT_TRUE(contains(r.lastSent().body, "Alex sends SOS"));
  TEST_ASSERT_TRUE(contains(r.lastSent().body, "48.10000,11.50000"));
  TEST_ASSERT_TRUE(contains(r.lastSent().body, "accurate to 8 m"));
  TEST_ASSERT_TRUE(SosPing::Stage::Done == r.emergency.ping().stage());
  TEST_ASSERT_TRUE(r.logged("SOS ping sent"));
}

void test_the_receiver_is_off_after_the_ping() {
  Rig r;
  r.emergency.begin();
  r.emergency.choose(SosPing::Kind::Emergency);
  r.run(25);
  TEST_ASSERT_EQUAL(1, r.sentCount());
  TEST_ASSERT_FALSE(r.receiverOn());                                 // in the same pass in which it was sent
  TEST_ASSERT_FALSE(r.positioning.wants(Positioning::Reason::SosPing));
}

void test_leaving_the_choice_sends_nothing() {
  Rig r;
  r.emergency.begin();
  r.run(5);
  r.emergency.leave();
  TEST_ASSERT_TRUE(SosFlow::Phase::Idle == r.emergency.phase());
  TEST_ASSERT_FALSE(r.receiverOn());
  r.run(60);
  TEST_ASSERT_EQUAL(0, r.sentCount());
  TEST_ASSERT_FALSE(r.emergency.ping().active());
}

void test_the_emergency_goes_out_by_itself_after_40_seconds() {
  Rig r;
  r.emergency.begin();
  r.run(SosFlow::kAutoSeconds - 1);
  TEST_ASSERT_TRUE(SosFlow::Phase::Choosing == r.emergency.phase());
  TEST_ASSERT_EQUAL_UINT32(1, r.emergency.autoLeft());
  r.run(1);
  TEST_ASSERT_TRUE(SosFlow::Phase::Idle == r.emergency.phase());
  TEST_ASSERT_TRUE(r.emergency.ping().active());
  TEST_ASSERT_TRUE(r.emergency.ping().kind() == SosPing::Kind::Emergency);
  r.run(30);
  TEST_ASSERT_EQUAL(1, r.sentCount());
  TEST_ASSERT_TRUE(contains(r.lastSent().body, "SOS"));
}

void test_pickup_sends_the_other_text() {
  Rig r;
  r.emergency.begin();
  r.emergency.choose(SosPing::Kind::Pickup);
  TEST_ASSERT_TRUE(r.logged("SOS ping (pickup) started"));
  r.run(30);
  TEST_ASSERT_TRUE(contains(r.lastSent().body, "Alex asks: please pick me up"));
  TEST_ASSERT_FALSE(contains(r.lastSent().body, "SOS"));
}

void test_pressing_sos_again_keeps_the_countdown_and_a_running_ping() {
  Rig r;
  r.emergency.begin();
  r.run(10);
  TEST_ASSERT_TRUE(r.emergency.begin());                             // a second press
  TEST_ASSERT_EQUAL_UINT32(30, r.emergency.autoLeft());              // the countdown goes on
  r.emergency.choose(SosPing::Kind::Emergency);
  TEST_ASSERT_TRUE(r.emergency.begin());                             // while the ping runs: nothing
  TEST_ASSERT_TRUE(SosFlow::Phase::Idle == r.emergency.phase());
  r.run(30);
  TEST_ASSERT_EQUAL(1, r.sentCount());
}

// ---- school time --------------------------------------------------------------------------------

void test_sos_works_in_school_time() {
  Rig r(inSchool());
  TEST_ASSERT_TRUE(r.school.active());
  TEST_ASSERT_TRUE(r.emergency.begin());
  TEST_ASSERT_TRUE(SosFlow::Phase::QuietConfirm == r.emergency.phase());
  TEST_ASSERT_EQUAL(SosFlow::kOverrideTaps, r.emergency.tapsLeft());
  TEST_ASSERT_TRUE(r.receiverOn());                                  // the search has begun
  r.run(120);                                                        // no automatic emergency here
  TEST_ASSERT_EQUAL(0, r.sentCount());
  TEST_ASSERT_TRUE(SosFlow::Phase::QuietConfirm == r.emergency.phase());
}

void test_five_taps_in_school_time_lift_the_lock() {
  Rig r(inSchool());
  r.emergency.begin();
  for (int i = 0; i < SosFlow::kOverrideTaps - 1; ++i) TEST_ASSERT_FALSE(r.emergency.tap());
  TEST_ASSERT_TRUE(r.school.active());                               // four taps do nothing
  TEST_ASSERT_FALSE(r.emergency.ping().active());
  TEST_ASSERT_TRUE(r.emergency.tap());
  TEST_ASSERT_TRUE(r.emergency.emergencyRunning());
  TEST_ASSERT_FALSE(r.school.active());                              // the lock is lifted ...
  TEST_ASSERT_TRUE(r.positioning.wants(Positioning::Reason::SosPing));
  r.run(30);
  TEST_ASSERT_EQUAL(1, r.sentCount());                               // ... and the emergency is out
  TEST_ASSERT_FALSE(r.school.active());
}

void test_the_lift_is_logged_and_ends_with_the_school_time() {
  // Two windows: the lift of the first must not carry over into the second.
  Rig r(ProfileBuilder().childName("Alex").schoolTime("tue", "14:00", "15:00").schoolTime("tue", "16:00", "17:00").build());
  r.emergency.begin();
  for (int i = 0; i < SosFlow::kOverrideTaps; ++i) r.emergency.tap();
  TEST_ASSERT_TRUE(r.logged("Quiet hours lifted by emergency until 15:00"));
  TEST_ASSERT_EQUAL_UINT32(coretest::kTuesdayAfternoon + 28 * 60, r.school.liftedUntil());
  r.advance(28 * 60 + 90 * 60);                                      // 16:30 local
  TEST_ASSERT_TRUE(r.school.active());
}

void test_leaving_the_school_time_warning_has_no_effect() {
  Rig r(inSchool());
  r.emergency.begin();
  r.emergency.tap();
  r.emergency.tap();
  r.emergency.leave();
  TEST_ASSERT_TRUE(SosFlow::Phase::Idle == r.emergency.phase());
  TEST_ASSERT_TRUE(r.school.active());                               // the lock stays
  TEST_ASSERT_EQUAL_UINT32(0, r.school.liftedUntil());
  TEST_ASSERT_FALSE(r.receiverOn());
  r.emergency.begin();                                               // a new press starts the count again
  TEST_ASSERT_EQUAL(SosFlow::kOverrideTaps, r.emergency.tapsLeft());
  r.run(60);
  TEST_ASSERT_EQUAL(0, r.sentCount());
}

// ---- the outcome --------------------------------------------------------------------------------

void test_the_outcome_is_vibrated_once() {
  Rig r;
  r.emergency.begin();
  r.emergency.choose(SosPing::Kind::Emergency);
  r.run(25);
  TEST_ASSERT_EQUAL(1, r.played(HapticPattern::Sent));
  r.run(60);                                                         // and not again while time goes by
  TEST_ASSERT_EQUAL(1, r.played(HapticPattern::Sent));
  TEST_ASSERT_EQUAL(0, r.played(HapticPattern::Failed));
}

void test_a_text_the_network_refuses_is_vibrated_as_failed_once() {
  Rig r;
  r.modem.failNextSms("+CMS ERROR: 304");
  r.emergency.begin();
  r.emergency.choose(SosPing::Kind::Emergency);
  r.run(25);
  TEST_ASSERT_EQUAL(1, r.played(HapticPattern::Sent));               // it looked like success for a moment
  r.run(2);
  TEST_ASSERT_TRUE(SosPing::Stage::Failed == r.emergency.ping().stage());
  TEST_ASSERT_EQUAL(1, r.played(HapticPattern::Failed));             // Messaging's, not a second one
  TEST_ASSERT_TRUE(r.logged("SOS ping FAILED: the network refused the text"));
  r.run(60);
  TEST_ASSERT_EQUAL(1, r.played(HapticPattern::Failed));
}

void test_a_text_that_cannot_even_be_queued_is_vibrated_as_failed() {
  Rig r;
  r.power.setEnabled(PowerDomain::Cellular, false);                  // no path: texts wait in the queue
  for (size_t i = 0; i < Messaging::kOutboxSize; ++i) TEST_ASSERT_TRUE(r.messaging.sendText(kParentTwo, "x"));
  r.emergency.begin();
  r.emergency.choose(SosPing::Kind::Emergency);
  r.run(25);
  TEST_ASSERT_TRUE(SosPing::Stage::Failed == r.emergency.ping().stage());
  TEST_ASSERT_EQUAL(1, r.played(HapticPattern::Failed));
  TEST_ASSERT_EQUAL(0, r.played(HapticPattern::Sent));
  TEST_ASSERT_FALSE(r.receiverOn());
  TEST_ASSERT_TRUE(r.logged("SOS ping FAILED"));
}

void test_without_a_position_a_text_goes_out_anyway() {
  Rig r;
  r.gnss.never();
  r.emergency.begin();
  r.emergency.choose(SosPing::Kind::Emergency);
  r.run(119);
  TEST_ASSERT_EQUAL(0, r.sentCount());
  r.run(1);
  TEST_ASSERT_EQUAL(1, r.sentCount());
  TEST_ASSERT_TRUE(contains(r.lastSent().body, "No GPS signal after 120 s"));
}

// ---- the reply to a query -----------------------------------------------------------------------

void test_a_remote_query_is_answered_like_an_sos_ping() {
  Rig r;
  r.modem.injectSms(kParentTwo, "#WhereAreYou", 1);
  r.messaging.poll();
  TEST_ASSERT_TRUE(r.receiverOn());
  TEST_ASSERT_TRUE(r.positioning.wants(Positioning::Reason::RemoteQuery));
  TEST_ASSERT_FALSE(r.positioning.wants(Positioning::Reason::SosPing));
  TEST_ASSERT_TRUE(r.emergency.ping().isQuery());
  TEST_ASSERT_FALSE(r.emergency.emergencyRunning());                 // the child sees nothing of it
  r.run(25);
  TEST_ASSERT_EQUAL(1, r.sentCount());
  TEST_ASSERT_EQUAL_STRING(kParentTwo, r.lastSent().phone.c_str()); // to whoever asked
  TEST_ASSERT_TRUE(contains(r.lastSent().body, "Alex is here"));
  TEST_ASSERT_TRUE(contains(r.lastSent().body, "48.10000,11.50000"));
  TEST_ASSERT_FALSE(contains(r.lastSent().body, "SOS"));
  TEST_ASSERT_FALSE(r.receiverOn());
  TEST_ASSERT_TRUE(r.logged("Remote query answer sent"));
  TEST_ASSERT_TRUE(r.logged("Remote query: searching, answer to Parent Two"));
}

void test_a_remote_query_never_vibrates() {
  Rig r;
  r.modem.injectSms(kParentOne, "#WhereAreYou", 1);
  r.run(40);
  TEST_ASSERT_EQUAL(1, r.sentCount());
  TEST_ASSERT_EQUAL(0, r.haptic.played().size());                    // not at the start, not at the end
  TEST_ASSERT_TRUE(r.notices.current().none());                      // the default setting is the history only
}

namespace {

// The system entries of a kind in the history, as the peers they name.
std::vector<std::string> entriesOf(const MessageStore& store, const char* kind) {
  std::vector<std::string> peers;
  for (size_t i = 0; i < store.count(); ++i) {
    sysentry::Entry e;
    if (sysentry::parse(store.at(i), e) && e.kind == kind) peers.push_back(store.at(i).peer);
  }
  return peers;
}

}  // namespace

void test_the_failure_of_a_query_reply_is_logged_and_told_in_the_history_but_not_to_the_child() {
  Rig r;
  r.modem.failNextSms("+CMS ERROR: 304");
  r.modem.injectSms(kParentOne, "#WhereAreYou", 1);
  r.run(40);
  TEST_ASSERT_EQUAL(0, r.haptic.played().size());
  TEST_ASSERT_TRUE(r.notices.current().none());
  TEST_ASSERT_TRUE(SosPing::Stage::Failed == r.emergency.ping().stage());
  TEST_ASSERT_TRUE(r.logged("Remote query answer FAILED"));
  TEST_ASSERT_FALSE(r.logged("SOS ping FAILED"));
  // The parents find the failure in the history, once, naming who asked; the refused text itself
  // is not an entry of its own.
  const auto failed = entriesOf(r.store, sysentry::kLocationQueryFailed);
  TEST_ASSERT_EQUAL(1, failed.size());
  TEST_ASSERT_EQUAL_STRING(kParentOne, failed[0].c_str());
  TEST_ASSERT_EQUAL(0, entriesOf(r.store, sysentry::kSmsRejected).size());
  r.run(60);
  TEST_ASSERT_EQUAL(1, entriesOf(r.store, sysentry::kLocationQueryFailed).size());   // not again later
}

void test_a_query_reply_that_cannot_even_be_queued_is_told_in_the_history() {
  Rig r;
  r.power.setEnabled(PowerDomain::Cellular, false);
  for (size_t i = 0; i < Messaging::kOutboxSize; ++i) TEST_ASSERT_TRUE(r.messaging.sendText(kParentTwo, "x"));
  TEST_ASSERT_TRUE(r.emergency.startLocationReply(kParentOne));
  r.run(130);
  TEST_ASSERT_TRUE(SosPing::Stage::Failed == r.emergency.ping().stage());
  const auto failed = entriesOf(r.store, sysentry::kLocationQueryFailed);
  TEST_ASSERT_EQUAL(1, failed.size());
  TEST_ASSERT_EQUAL_STRING(kParentOne, failed[0].c_str());
  TEST_ASSERT_EQUAL(0, r.haptic.played().size());
}

void test_a_query_that_is_kept_out_of_the_history_keeps_its_failure_out_too() {
  Rig r(ProfileBuilder().childName("Alex").commandNotice(CommandNotice::Silent).build());
  r.modem.failNextSms("+CMS ERROR: 304");
  r.modem.injectSms(kParentOne, "#WhereAreYou", 1);
  r.run(40);
  TEST_ASSERT_TRUE(SosPing::Stage::Failed == r.emergency.ping().stage());
  TEST_ASSERT_TRUE(r.logged("Remote query answer FAILED"));      // the log has it whatever the profile says
  TEST_ASSERT_EQUAL(0, entriesOf(r.store, sysentry::kLocationQueryFailed).size());
}

void test_a_failed_emergency_is_not_a_failed_query() {
  Rig r;
  r.modem.failNextSms("+CMS ERROR: 304");
  r.emergency.begin();
  r.emergency.choose(SosPing::Kind::Emergency);
  r.run(40);
  TEST_ASSERT_TRUE(SosPing::Stage::Failed == r.emergency.ping().stage());
  TEST_ASSERT_EQUAL(0, entriesOf(r.store, sysentry::kLocationQueryFailed).size());
}

void test_only_the_reply_to_a_query_is_silent_when_the_network_refuses_it() {
  Rig r;
  r.emergency.startLocationReply(kParentTwo);
  r.run(5);
  // The child's own message to the same parent fails while the reply still searches: not silent.
  TEST_ASSERT_FALSE(r.emergency.noteSendFailed(kParentTwo));
  r.run(25);
  TEST_ASSERT_TRUE(SosPing::Stage::Done == r.emergency.ping().stage());
  TEST_ASSERT_FALSE(r.emergency.noteSendFailed(kParentOne));        // another number
  TEST_ASSERT_TRUE(SosPing::Stage::Done == r.emergency.ping().stage());
  TEST_ASSERT_TRUE(r.emergency.noteSendFailed(kParentTwo));         // the reply itself
  TEST_ASSERT_TRUE(SosPing::Stage::Failed == r.emergency.ping().stage());
}

void test_a_second_query_is_busy() {
  Rig r;
  TEST_ASSERT_TRUE(r.emergency.startLocationReply(kParentOne));
  TEST_ASSERT_FALSE(r.emergency.startLocationReply(kParentTwo));
  r.run(30);
  TEST_ASSERT_EQUAL(1, r.sentCount());
  TEST_ASSERT_EQUAL_STRING(kParentOne, r.lastSent().phone.c_str());
  TEST_ASSERT_TRUE(r.emergency.startLocationReply(kParentTwo));      // free again afterwards
}

void test_an_emergency_replaces_a_query_reply() {
  Rig r;
  r.emergency.startLocationReply(kParentTwo);
  r.run(5);
  TEST_ASSERT_TRUE(r.emergency.begin());                             // the child presses while the reply waits
  TEST_ASSERT_TRUE(SosFlow::Phase::Choosing == r.emergency.phase());
  TEST_ASSERT_TRUE(r.emergency.choose(SosPing::Kind::Emergency));
  TEST_ASSERT_TRUE(r.emergency.emergencyRunning());
  TEST_ASSERT_TRUE(r.positioning.wants(Positioning::Reason::SosPing));
  TEST_ASSERT_FALSE(r.positioning.wants(Positioning::Reason::RemoteQuery));
  r.run(30);
  TEST_ASSERT_EQUAL(1, r.sentCount());                               // one text: the emergency
  TEST_ASSERT_EQUAL_STRING(kParentOne, r.lastSent().phone.c_str());
  TEST_ASSERT_TRUE(contains(r.lastSent().body, "SOS"));
  TEST_ASSERT_EQUAL(1, r.played(HapticPattern::Sent));
  TEST_ASSERT_FALSE(r.receiverOn());
}

void test_a_query_during_an_sos_is_busy() {
  Rig r;
  r.emergency.begin();
  r.emergency.choose(SosPing::Kind::Emergency);
  TEST_ASSERT_FALSE(r.emergency.startLocationReply(kParentTwo));
  r.modem.injectSms(kParentTwo, "#WhereAreYou", 1);                  // by SMS: logged as busy, no second search
  r.messaging.poll();
  TEST_ASSERT_TRUE(r.logged("Remote query from"));
  TEST_ASSERT_TRUE(r.emergency.emergencyRunning());
  r.run(30);
  TEST_ASSERT_EQUAL(1, r.sentCount());
  TEST_ASSERT_EQUAL_STRING(kParentOne, r.lastSent().phone.c_str());
}

void test_a_query_while_the_choice_is_open_is_answered_and_the_choice_stays() {
  Rig r;
  r.emergency.begin();
  TEST_ASSERT_TRUE(r.emergency.startLocationReply(kParentTwo));
  TEST_ASSERT_TRUE(SosFlow::Phase::Choosing == r.emergency.phase());
  r.run(30);
  TEST_ASSERT_EQUAL(1, r.sentCount());
  TEST_ASSERT_EQUAL_STRING(kParentTwo, r.lastSent().phone.c_str());
  TEST_ASSERT_TRUE(r.receiverOn());                                  // the open choice still holds it
  TEST_ASSERT_TRUE(r.positioning.wants(Positioning::Reason::SosChoice));
}

// ---- the wording --------------------------------------------------------------------------------

void test_the_sms_is_worded_in_the_sms_language() {
  Rig r(ProfileBuilder().childName("Alex").smsLanguage("de").build());
  TEST_ASSERT_TRUE(r.translator.select("fr"));                       // the screen speaks French ...
  r.emergency.begin();
  r.emergency.choose(SosPing::Kind::Emergency);
  r.run(30);
  TEST_ASSERT_TRUE(contains(r.lastSent().body, "Alex sendet SOS"));  // ... the parents read German
  TEST_ASSERT_TRUE(contains(r.lastSent().body, "auf 8 m genau"));
  TEST_ASSERT_EQUAL_STRING("fr", r.translator.language().c_str());   // and the screen's language is untouched
}

void test_an_unknown_sms_language_falls_back_to_english() {
  Rig r(ProfileBuilder().childName("Alex").smsLanguage("xx").build());
  r.emergency.begin();
  r.emergency.choose(SosPing::Kind::Emergency);
  r.run(30);
  TEST_ASSERT_TRUE(contains(r.lastSent().body, "Alex sends SOS"));
}

void test_the_name_and_the_map_template_go_into_the_text() {
  Rig r(ProfileBuilder().childName("Alex").mapTemplate("https://maps.example/?q={lat},{lon}").build());
  r.emergency.begin();
  r.emergency.choose(SosPing::Kind::Emergency);
  r.run(30);
  TEST_ASSERT_TRUE(contains(r.lastSent().body, "https://maps.example/?q=48.10000,11.50000"));

  Rig anonymous(ProfileBuilder().build());                           // no name: the anonymous wording
  anonymous.emergency.begin();
  anonymous.emergency.choose(SosPing::Kind::Emergency);
  anonymous.run(30);
  TEST_ASSERT_TRUE(contains(anonymous.lastSent().body, "SOS, I need help"));
  TEST_ASSERT_FALSE(contains(anonymous.lastSent().body, "{"));
}

void test_a_changed_name_is_used_at_the_next_press() {
  Rig r;
  r.profile.childName = "Sam";                                       // the profile may change while the device runs
  r.emergency.begin();
  r.emergency.choose(SosPing::Kind::Emergency);
  r.run(30);
  TEST_ASSERT_TRUE(contains(r.lastSent().body, "Sam sends SOS"));
}

// ---- the screen closes --------------------------------------------------------------------------

void test_leave_releases_the_receiver() {
  Rig r(inSchool());
  r.emergency.begin();
  TEST_ASSERT_TRUE(r.receiverOn());
  r.emergency.leave();
  TEST_ASSERT_FALSE(r.receiverOn());
  TEST_ASSERT_FALSE(r.positioning.wants(Positioning::Reason::SosChoice));
}

void test_leave_does_not_stop_a_running_ping() {
  Rig r;
  r.emergency.begin();
  r.emergency.choose(SosPing::Kind::Emergency);
  r.emergency.leave();                                               // the screen closes after the choice
  r.run(30);
  TEST_ASSERT_EQUAL(1, r.sentCount());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_without_a_recipient_sos_cannot_begin);
  RUN_TEST(test_the_first_call_target_is_the_default_recipient);
  RUN_TEST(test_pressing_sos_starts_the_receiver_at_once);
  RUN_TEST(test_choosing_starts_the_ping);
  RUN_TEST(test_the_ping_sends_the_position);
  RUN_TEST(test_the_receiver_is_off_after_the_ping);
  RUN_TEST(test_leaving_the_choice_sends_nothing);
  RUN_TEST(test_the_emergency_goes_out_by_itself_after_40_seconds);
  RUN_TEST(test_pickup_sends_the_other_text);
  RUN_TEST(test_pressing_sos_again_keeps_the_countdown_and_a_running_ping);
  RUN_TEST(test_sos_works_in_school_time);
  RUN_TEST(test_five_taps_in_school_time_lift_the_lock);
  RUN_TEST(test_the_lift_is_logged_and_ends_with_the_school_time);
  RUN_TEST(test_leaving_the_school_time_warning_has_no_effect);
  RUN_TEST(test_the_outcome_is_vibrated_once);
  RUN_TEST(test_a_text_the_network_refuses_is_vibrated_as_failed_once);
  RUN_TEST(test_a_text_that_cannot_even_be_queued_is_vibrated_as_failed);
  RUN_TEST(test_without_a_position_a_text_goes_out_anyway);
  RUN_TEST(test_a_remote_query_is_answered_like_an_sos_ping);
  RUN_TEST(test_a_remote_query_never_vibrates);
  RUN_TEST(test_the_failure_of_a_query_reply_is_logged_and_told_in_the_history_but_not_to_the_child);
  RUN_TEST(test_a_query_reply_that_cannot_even_be_queued_is_told_in_the_history);
  RUN_TEST(test_a_query_that_is_kept_out_of_the_history_keeps_its_failure_out_too);
  RUN_TEST(test_a_failed_emergency_is_not_a_failed_query);
  RUN_TEST(test_only_the_reply_to_a_query_is_silent_when_the_network_refuses_it);
  RUN_TEST(test_a_second_query_is_busy);
  RUN_TEST(test_an_emergency_replaces_a_query_reply);
  RUN_TEST(test_a_query_during_an_sos_is_busy);
  RUN_TEST(test_a_query_while_the_choice_is_open_is_answered_and_the_choice_stays);
  RUN_TEST(test_the_sms_is_worded_in_the_sms_language);
  RUN_TEST(test_an_unknown_sms_language_falls_back_to_english);
  RUN_TEST(test_the_name_and_the_map_template_go_into_the_text);
  RUN_TEST(test_a_changed_name_is_used_at_the_next_press);
  RUN_TEST(test_leave_releases_the_receiver);
  RUN_TEST(test_leave_does_not_stop_a_running_ping);
  return UNITY_END();
}

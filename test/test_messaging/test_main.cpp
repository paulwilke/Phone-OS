// Host tests for messaging: incoming messages and remote queries, the routing rule and the
// queue, delivery reports from the radio, modem send failures, deletion.

#include <unity.h>

#include <cstring>

#include <CoreFakes.h>
#include <app/Messaging.h>
#include <app/LogLevel.h>
#include <app/SystemEntry.h>

using namespace people;

void setUp() {}
void tearDown() {}

namespace {

struct Hooks : Messaging::Hooks {
  std::vector<std::string> queries, failures;
  bool accept = true;
  bool startLocationReply(const std::string& phone) override {
    queries.push_back(phone);
    return accept;
  }
  bool smsSendFailed(const std::string& phone) override {
    failures.push_back(phone);
    return silent;
  }
  bool silent = false;
};

struct Rig : CoreFakes {
  Hooks hooks;
  Messaging messaging;

  explicit Rig(Profile p = ProfileBuilder().build())
      : CoreFakes(std::move(p)), messaging(clock, modem, power, store, profile, school, notices, haptic, logger, drops) {
    messaging.setRadio(&radio);
    messaging.setHooks(&hooks);
    messaging.configure({"#WhereAreYou", "#WoBistDu"});
  }
  void receive(const std::string& from, const std::string& text) { modem.injectSms(from, text, 0); }
  const Message& at(size_t i) { return store.at(i); }
  bool noticeIs(NoticeKind kind) { return notices.current().kind == kind; }
  std::string textOf(size_t i) { return store.at(i).text; }
};

Profile withRadio() { return ProfileBuilder().radio(true).radioPeer("Node One").build(); }
Profile inSchool() { return ProfileBuilder().schoolTime("tue", "14:00", "15:00").build(); }

}  // namespace

// ---- incoming ------------------------------------------------------------------------------------

void test_an_incoming_sms_is_stored() {
  Rig r;
  r.receive(kParentOne, "School is cancelled");
  r.messaging.poll();
  TEST_ASSERT_EQUAL(1, r.store.count());
  TEST_ASSERT_EQUAL_STRING("School is cancelled", r.at(0).text);
  TEST_ASSERT_EQUAL_STRING(kParentOne, r.at(0).peer);
  TEST_ASSERT_TRUE(r.at(0).dir == MsgDir::In);
  TEST_ASSERT_TRUE(r.at(0).source == MsgSource::Sms);
  TEST_ASSERT_EQUAL(coretest::kTuesdayAfternoon, r.at(0).time);
  TEST_ASSERT_EQUAL(1, r.store.unread());
}

void test_a_remote_query_is_not_stored_as_a_message() {
  Rig r;
  r.receive(kParentOne, "#WhereAreYou");
  r.receive(kParentTwo, "  #wobistdu ");               // another language, case and blanks do not matter
  r.messaging.poll();
  TEST_ASSERT_EQUAL(2, r.hooks.queries.size());
  for (size_t i = 0; i < r.store.count(); ++i) {
    TEST_ASSERT_TRUE(r.at(i).source == MsgSource::System);   // history entries, never the query text
    TEST_ASSERT_NULL(strstr(r.at(i).text, "Where"));
  }
}

void test_a_remote_query_is_logged_recorded_and_noticed_as_the_profile_says() {
  struct Case { CommandNotice setting; size_t entries; bool notice; };
  for (const Case& c : {Case{CommandNotice::Silent, 0, false}, Case{CommandNotice::History, 1, false},
                        Case{CommandNotice::Notify, 1, true}}) {
    Rig r(ProfileBuilder().commandNotice(c.setting).build());
    r.messaging.configure({"#WhereAreYou"});
    r.receive(kParentOne, "#WhereAreYou");
    r.messaging.poll();
    TEST_ASSERT_TRUE(r.log.contains("Remote query from +491701111111"));     // the log gets it on every setting
    TEST_ASSERT_EQUAL(c.entries, r.store.count());
    TEST_ASSERT_EQUAL(c.notice, r.noticeIs(NoticeKind::LocationQueried));
    if (c.entries) {
      sysentry::Entry e;
      TEST_ASSERT_TRUE(sysentry::parse(r.at(0), e));
      TEST_ASSERT_EQUAL_STRING(sysentry::kLocationQueried, e.kind.c_str());
      TEST_ASSERT_EQUAL_STRING(kParentOne, r.at(0).peer);
    }
    if (c.notice) TEST_ASSERT_EQUAL_STRING(kParentOne, r.notices.current().peer.c_str());
  }
}

void test_a_refused_remote_query_is_recorded() {
  Rig r(ProfileBuilder().commandNotice(CommandNotice::Notify).build());
  r.messaging.configure({"#WhereAreYou"});
  r.receive(kGrandma, "#WhereAreYou");                        // in the book, but no admin
  r.messaging.poll();
  TEST_ASSERT_EQUAL(0, r.hooks.queries.size());              // no search, no reply
  TEST_ASSERT_EQUAL(0, r.modem.sent().size());
  TEST_ASSERT_TRUE(r.log.contains("refused"));
  TEST_ASSERT_EQUAL(1, r.store.count());
  sysentry::Entry e;
  TEST_ASSERT_TRUE(sysentry::parse(r.at(0), e));
  TEST_ASSERT_EQUAL_STRING(sysentry::kQueryRefused, e.kind.c_str());
  TEST_ASSERT_EQUAL_STRING(kGrandma, r.at(0).peer);
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::QueryRefused));
}

void test_a_remote_query_while_a_search_runs_is_logged_as_busy() {
  Rig r;
  r.hooks.accept = false;
  r.receive(kParentOne, "#WhereAreYou");
  r.messaging.poll();
  TEST_ASSERT_TRUE(r.log.contains("busy"));
  TEST_ASSERT_EQUAL(1, r.store.count());                      // still recorded: a parent asked
}

void test_a_remote_query_does_not_vibrate() {
  Rig r;
  r.receive(kParentOne, "#WhereAreYou");
  r.messaging.poll();
  TEST_ASSERT_EQUAL(0, r.haptic.played().size());
}

void test_an_incoming_sms_vibrates_the_message_pattern() {
  Rig r;
  r.receive(kParentOne, "Hello");
  r.messaging.poll();
  TEST_ASSERT_EQUAL(1, r.haptic.played().size());
  TEST_ASSERT_TRUE(r.haptic.played()[0] == HapticPattern::Message);
}

void test_message_vibration_can_be_switched_off() {
  Rig r(ProfileBuilder().messageVibration(false).build());
  r.receive(kParentOne, "Hello");
  r.messaging.poll();
  TEST_ASSERT_EQUAL(0, r.haptic.played().size());
  TEST_ASSERT_EQUAL(1, r.store.count());
}

void test_in_school_time_only_parents_vibrate() {
  Rig r(inSchool());
  TEST_ASSERT_TRUE(r.school.active());
  r.receive(kGrandma, "Hello");
  r.messaging.poll();
  TEST_ASSERT_EQUAL(0, r.haptic.played().size());
  TEST_ASSERT_EQUAL(1, r.store.count());                      // stored, only quiet
  r.receive(kParentOne, "Come home");
  r.messaging.poll();
  TEST_ASSERT_EQUAL(1, r.haptic.played().size());
  r.receive(kStranger, "Hello?");                              // neither a parent nor listed
  r.messaging.poll();
  TEST_ASSERT_EQUAL(1, r.haptic.played().size());
}

void test_a_switched_off_window_lets_everybody_vibrate() {
  Rig r(ProfileBuilder().schoolTime("tue", "14:00", "15:00", false).build());
  r.receive(kGrandma, "Hello");
  r.messaging.poll();
  TEST_ASSERT_EQUAL(1, r.haptic.played().size());
}

void test_after_school_time_everybody_vibrates_again() {
  Rig r(inSchool());
  r.advance(3600);                                             // 15:32
  r.receive(kGrandma, "Hello");
  r.messaging.poll();
  TEST_ASSERT_EQUAL(1, r.haptic.played().size());
}

void test_an_incoming_sms_means_the_modem_is_awake() {
  Rig r;
  TEST_ASSERT_FALSE(r.messaging.poll());                       // nothing happened
  r.receive(kParentOne, "Hello");
  TEST_ASSERT_TRUE(r.messaging.poll());
  TEST_ASSERT_FALSE(r.messaging.poll());                       // and it was reported once
}

// ---- only the contact book may write to the child -----------------------------------------------

// Nothing of a dropped message may be left behind: not in the history, not as unread, not as a
// vibration, not as a notice, not as a reply.
void expectNothingLeft(Rig& r) {
  TEST_ASSERT_EQUAL(0, r.store.count());
  TEST_ASSERT_EQUAL(0, r.store.unread());
  TEST_ASSERT_EQUAL(0, r.haptic.played().size());
  TEST_ASSERT_TRUE(r.notices.current().none());
  TEST_ASSERT_EQUAL(0, r.modem.sent().size());
  TEST_ASSERT_EQUAL(0, r.hooks.queries.size());
}

void test_an_sms_from_a_stranger_is_dropped_without_a_trace() {
  Rig r;
  r.receive(kStranger, "Win a prize");
  r.messaging.poll();
  expectNothingLeft(r);
}

void test_the_drop_is_logged_with_the_number_and_the_start_of_the_text() {
  Rig r;
  r.receive(kStranger, "Win a prize");
  r.messaging.poll();
  TEST_ASSERT_TRUE(r.log.contains("SMS dropped: sender not in the contact book (+491709999999): Win a prize"));
  TEST_ASSERT_TRUE(loglevel::classify("SMS dropped: sender not in the contact book (+491709999999): Win a prize") ==
                   loglevel::Importance::Always);
}

void test_a_stranger_in_another_spelling_is_logged_in_the_canonical_form() {
  Rig r;
  r.receive("0170 9999999", "Hello");
  r.messaging.poll();
  expectNothingLeft(r);
  TEST_ASSERT_TRUE(r.log.contains("(+491709999999)"));
}

// A sender that is no number is one sender by its text, and the text of what it sent is in the log: a
// balance, a tariff, a PIN notice (the owner's decision of 2026-10-06, docs/log-level.md).
void test_a_sender_that_is_no_number_is_dropped_and_logged_by_its_name() {
  Rig r;
  r.receive("Provider", "Your balance is 3.00");
  r.receive("12345", "Premium offer");
  r.receive("", "Nobody sends this");
  r.messaging.poll();
  expectNothingLeft(r);
  TEST_ASSERT_TRUE(r.log.contains("(non-numeric sender Provider): Your balance is 3.00"));
  TEST_ASSERT_TRUE(r.log.contains("(12345): Premium offer"));                      // a short code is a number: its digits
  TEST_ASSERT_TRUE(r.log.contains("(non-numeric sender): Nobody sends this"));     // no sender at all
}

// A flood must not fill the card: the first text of a sender is a line, with the start of its text; the rest
// is counted, and the count is one more line when the ten minutes are over. Nothing is stored or felt.
void test_a_flood_of_a_hundred_texts_from_a_stranger_is_one_line_and_one_summary() {
  Rig r;
  for (int i = 0; i < 100; ++i) {
    r.receive(kStranger, "spam " + std::to_string(i));
    r.messaging.poll();
    r.clock.advanceSeconds(1);
  }
  expectNothingLeft(r);
  TEST_ASSERT_EQUAL(1, r.log.entries().size());
  TEST_ASSERT_TRUE(r.log.contains("(+491709999999): spam 0"));
  r.clock.advanceSeconds(DropLog::kWindowSeconds);
  r.drops.tick();
  TEST_ASSERT_EQUAL(2, r.log.entries().size());
  TEST_ASSERT_TRUE(r.log.contains("(99 more in the last 10 minutes)"));
}

void test_the_text_of_a_dropped_message_is_in_the_log_cut_clean_and_without_control_characters() {
  Rig r;
  r.receive("Provider", "Balance:\n3.00 EUR\tPIN 1234");
  r.receive(kStranger, std::string(500, 'a'));
  r.receive("+4917012345678", "");
  r.messaging.poll();
  TEST_ASSERT_TRUE(r.log.contains("(non-numeric sender Provider): Balance: 3.00 EUR PIN 1234"));
  TEST_ASSERT_TRUE(r.log.contains("(+491709999999): " + std::string(120, 'a')));
  TEST_ASSERT_FALSE(r.log.contains(std::string(121, 'a')));
  TEST_ASSERT_TRUE(r.log.contains("(+4917012345678)"));
  for (const auto& e : r.log.entries()) TEST_ASSERT_TRUE(e.line.find("(+4917012345678)") == std::string::npos || e.line.find(':', e.line.find("(+4917012345678)")) == std::string::npos);
}

void test_an_empty_text_from_a_stranger_is_dropped() {
  Rig r;
  r.receive(kStranger, "");
  r.receive("Provider", "");
  r.messaging.poll();
  expectNothingLeft(r);
}

void test_an_empty_text_from_a_contact_is_stored_as_before() {
  Rig r;
  r.receive(kGrandma, "");
  r.messaging.poll();
  TEST_ASSERT_EQUAL(1, r.store.count());
  TEST_ASSERT_EQUAL(1, r.store.unread());
  TEST_ASSERT_EQUAL(1, r.haptic.played().size());
}

void test_every_spelling_of_a_contact_is_let_through() {
  Rig r;
  r.receive("0170 1111111", "a");
  r.receive("0049 170 1111111", "b");
  r.receive("+49 170 1111111", "c");
  r.messaging.poll();
  TEST_ASSERT_EQUAL(3, r.store.count());
}

void test_the_target_of_a_call_action_may_write() {
  Rig r(ProfileBuilder().callTargets({"+491701234567"}).build());
  r.receive("0170 1234567", "Hello");
  r.messaging.poll();
  TEST_ASSERT_EQUAL(1, r.store.count());
}

void test_a_stranger_in_school_time_leaves_nothing_either() {
  Rig r(inSchool());
  TEST_ASSERT_TRUE(r.school.active());
  r.receive(kStranger, "Hello?");
  r.messaging.poll();
  expectNothingLeft(r);
}

void test_a_stranger_cannot_use_the_remote_query() {
  Rig r(ProfileBuilder().commandNotice(CommandNotice::Notify).build());
  r.receive(kStranger, "#WhereAreYou");
  r.receive(kStranger, "#WoBistDu");
  r.messaging.poll();
  expectNothingLeft(r);                                       // not even a refusal entry or notice
  TEST_ASSERT_FALSE(r.log.contains("Remote query"));
}

void test_a_flood_from_a_stranger_leaves_the_history_untouched() {
  Rig r;
  r.receive(kParentOne, "Come home");
  r.messaging.poll();
  TEST_ASSERT_EQUAL(1, r.store.count());
  for (int i = 0; i < 100; ++i) r.receive(kStranger, "Buy now " + std::to_string(i));
  r.messaging.poll();
  TEST_ASSERT_EQUAL(1, r.store.count());                      // the ring holds what it held
  TEST_ASSERT_EQUAL_STRING("Come home", r.at(0).text);
  TEST_ASSERT_EQUAL(1, r.store.unread());
  TEST_ASSERT_EQUAL(1, r.haptic.played().size());              // the parent's message, nothing since
  TEST_ASSERT_TRUE(r.notices.current().none());
}

void test_a_dropped_sms_still_proves_the_modem_awake() {
  Rig r;
  r.receive(kStranger, "x");
  TEST_ASSERT_TRUE(r.messaging.poll());
}

// ---- routing -----------------------------------------------------------------------------------------

void test_a_message_goes_by_cellular() {
  Rig r;
  TEST_ASSERT_TRUE(r.messaging.sendText(kParentOne, "On my way"));
  TEST_ASSERT_EQUAL(1, r.modem.sent().size());
  TEST_ASSERT_EQUAL_STRING(kParentOne, r.modem.sent()[0].phone.c_str());
  TEST_ASSERT_EQUAL_STRING("On my way", r.modem.sent()[0].body.c_str());
  TEST_ASSERT_EQUAL(1, r.store.count());
  TEST_ASSERT_FALSE(r.at(0).pending);
  TEST_ASSERT_TRUE(r.at(0).dir == MsgDir::Out);
  TEST_ASSERT_EQUAL(0, r.messaging.pending());
}

void test_cellular_has_priority_over_the_radio() {
  Rig r(withRadio());
  r.radioUp();
  TEST_ASSERT_TRUE(r.messaging.sendText(kParentOne, "On my way"));
  TEST_ASSERT_EQUAL(1, r.modem.sent().size());
  TEST_ASSERT_EQUAL(0, r.radio.sent().size());
  TEST_ASSERT_TRUE(r.at(0).source == MsgSource::Sms);
}

void test_the_radio_takes_over_when_cellular_is_gone() {
  Rig r(withRadio());
  r.radioUp("Node One");
  r.networkLost();
  TEST_ASSERT_TRUE(r.messaging.sendText(kParentOne, "Pick me up"));
  TEST_ASSERT_EQUAL(0, r.modem.sent().size());
  TEST_ASSERT_EQUAL(1, r.radio.sent().size());
  TEST_ASSERT_EQUAL_STRING("Pick me up", r.radio.sent()[0].text.c_str());
  TEST_ASSERT_EQUAL_STRING("Node One", r.radio.sent()[0].peer.c_str());   // the radio's own contact, no phone number
  TEST_ASSERT_EQUAL(0, r.messaging.pending());                           // nothing waits: it went out
  TEST_ASSERT_TRUE(r.at(0).source == MsgSource::Radio);
  TEST_ASSERT_FALSE(r.at(0).pending);
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::MessageSentRadio));
}

void test_cellular_off_uses_the_radio() {
  Rig r(withRadio());
  r.radioUp();
  r.power.setEnabled(PowerDomain::Cellular, false);
  TEST_ASSERT_FALSE(r.messaging.registered());
  TEST_ASSERT_TRUE(r.messaging.sendText(kParentOne, "Arrived"));
  TEST_ASSERT_EQUAL(0, r.modem.sent().size());
  TEST_ASSERT_EQUAL(1, r.radio.sent().size());
}

void test_without_a_path_a_message_is_queued() {
  Rig r;
  r.networkLost();
  TEST_ASSERT_TRUE(r.messaging.sendText(kParentOne, "Pick me up"));
  TEST_ASSERT_EQUAL(0, r.modem.sent().size());
  TEST_ASSERT_EQUAL(1, r.messaging.pending());
  TEST_ASSERT_TRUE(r.at(0).pending);
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::MessageWaiting));
  r.messaging.poll();                                            // still no network
  TEST_ASSERT_EQUAL(1, r.messaging.pending());
  TEST_ASSERT_TRUE(r.at(0).pending);
}

void test_a_queued_message_goes_out_when_the_network_returns() {
  Rig r;
  r.networkLost();
  r.messaging.sendText(kParentOne, "Pick me up");
  r.networkBack();
  r.messaging.poll();
  TEST_ASSERT_EQUAL(1, r.modem.sent().size());
  TEST_ASSERT_EQUAL(0, r.messaging.pending());
  TEST_ASSERT_FALSE(r.at(0).pending);
  TEST_ASSERT_TRUE(r.at(0).source == MsgSource::Sms);
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::MessageSent));         // "waits for network" does not stay
}

void test_a_queued_message_goes_out_by_radio() {
  Rig r(withRadio());
  r.networkLost();
  r.messaging.sendText(kParentOne, "Pick me up");
  TEST_ASSERT_EQUAL(1, r.messaging.pending());
  r.radioUp();
  r.messaging.poll();
  TEST_ASSERT_EQUAL(0, r.messaging.pending());
  TEST_ASSERT_EQUAL(1, r.radio.sent().size());
  TEST_ASSERT_TRUE(r.at(0).source == MsgSource::Radio);          // the entry says how it left
  TEST_ASSERT_FALSE(r.at(0).pending);
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::MessageSentRadio));
  r.radio.confirm(true);                                         // and it is awaited like any radio message
  r.messaging.poll();
  TEST_ASSERT_TRUE(r.at(0).delivery == MsgDelivery::Confirmed);
}

void test_the_queue_is_bounded() {
  Rig r;
  r.networkLost();
  for (size_t i = 0; i < Messaging::kOutboxSize; ++i) TEST_ASSERT_TRUE(r.messaging.sendText(kParentOne, "full"));
  TEST_ASSERT_FALSE(r.messaging.sendText(kParentOne, "one too many"));
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::QueueFull));
  TEST_ASSERT_EQUAL(Messaging::kOutboxSize, r.store.count());   // the refused one is not in the history either
  TEST_ASSERT_EQUAL(Messaging::kOutboxSize, r.messaging.pending());
}

void test_the_queue_goes_out_in_order_and_keeps_what_cannot_leave_yet() {
  Rig r;
  r.networkLost();
  r.messaging.sendText(kParentOne, "first");
  r.messaging.sendText(kParentTwo, "second");
  r.networkBack();
  r.messaging.poll();
  TEST_ASSERT_EQUAL(2, r.modem.sent().size());
  TEST_ASSERT_EQUAL_STRING("first", r.modem.sent()[0].body.c_str());
  TEST_ASSERT_EQUAL_STRING("second", r.modem.sent()[1].body.c_str());
  TEST_ASSERT_EQUAL_STRING(kParentTwo, r.modem.sent()[1].phone.c_str());
}

void test_a_message_without_any_path_does_not_keep_the_modem_awake() {
  Rig r;
  r.networkLost();
  r.messaging.sendText(kParentOne, "Hello");
  TEST_ASSERT_TRUE(r.messaging.hasWork());                       // the modem is powered: it may register
  r.power.setEnabled(PowerDomain::Cellular, false);
  TEST_ASSERT_FALSE(r.messaging.hasWork());                      // no modem, no radio: it cannot leave
}

void test_a_message_with_a_path_is_pending_work() {
  Rig r(withRadio());
  r.power.setEnabled(PowerDomain::Cellular, false);
  r.messaging.sendText(kParentOne, "Hello");
  TEST_ASSERT_FALSE(r.messaging.hasWork());
  r.radioUp();
  TEST_ASSERT_TRUE(r.messaging.hasWork());                       // the radio could carry it
  r.messaging.poll();
  TEST_ASSERT_FALSE(r.messaging.hasWork());                      // and now it is out
}

void test_without_a_radio_everything_works_as_before() {
  CoreFakes f;
  Hooks hooks;
  Messaging m(f.clock, f.modem, f.power, f.store, f.profile, f.school, f.notices, f.haptic, f.logger, f.drops);   // no radio attached
  f.networkLost();
  TEST_ASSERT_FALSE(m.radioUsable());
  TEST_ASSERT_TRUE(m.sendText(kParentOne, "Hello"));
  TEST_ASSERT_EQUAL(1, m.pending());
  m.poll();
  TEST_ASSERT_FALSE(m.sendRadioText("", "Hello"));
}

void test_a_profile_without_the_radio_never_sends_by_radio() {
  Rig r;                                                          // the default profile does not allow the radio
  r.radioUp();
  r.networkLost();
  TEST_ASSERT_FALSE(r.messaging.radioUsable());
  TEST_ASSERT_TRUE(r.messaging.sendText(kParentOne, "Hello"));
  TEST_ASSERT_EQUAL(0, r.radio.sent().size());
  TEST_ASSERT_EQUAL(1, r.messaging.pending());
  TEST_ASSERT_FALSE(r.messaging.sendRadioText("", "Hello"));
  TEST_ASSERT_EQUAL(0, r.radio.sent().size());
}

void test_a_profile_without_the_radio_ignores_incoming_radio_messages() {
  Rig r;
  r.radioUp();
  r.radio.deliver("Node One", "On our way", 0);
  r.messaging.poll();
  TEST_ASSERT_EQUAL(0, r.store.count());
}

void test_an_incoming_radio_message_is_stored() {
  Rig r(withRadio());
  r.radioUp();
  r.radio.deliver("Node One", "On our way", 0);
  r.messaging.poll();
  TEST_ASSERT_EQUAL(1, r.store.count());
  TEST_ASSERT_TRUE(r.at(0).source == MsgSource::Radio);
  TEST_ASSERT_TRUE(r.at(0).dir == MsgDir::In);
  TEST_ASSERT_EQUAL_STRING("On our way", r.at(0).text);
  TEST_ASSERT_EQUAL_STRING("Node One", r.at(0).peer);
  TEST_ASSERT_EQUAL(1, r.store.unread());
  TEST_ASSERT_EQUAL(0, r.haptic.played().size());               // a radio message does not vibrate (it never did)
}

// ---- only the configured peer may write over the radio -----------------------------------------------------------

void test_a_radio_message_from_the_configured_peer_is_stored_and_counted_unread() {
  Rig r(withRadio());
  r.radioUp("Node One");
  r.radio.deliver("Node One", "On our way", 0);
  r.messaging.poll();
  TEST_ASSERT_EQUAL(1, r.store.count());
  TEST_ASSERT_EQUAL(1, r.store.unread());
}

void test_a_radio_message_from_another_name_is_dropped_like_a_strangers_text() {
  Rig r(withRadio());
  r.radioUp("Node One");
  r.radio.addPeer("Node Two", true, 0);
  r.radio.deliver("Node Two", "Meet me at the gate", 0);
  r.messaging.poll();
  TEST_ASSERT_EQUAL(0, r.store.count());
  TEST_ASSERT_EQUAL(0, r.store.unread());
  TEST_ASSERT_EQUAL(0, r.haptic.played().size());
  TEST_ASSERT_TRUE(r.log.contains("Radio message dropped: sender not the configured radio peer (Node Two): Meet me at the gate"));
  TEST_ASSERT_TRUE(loglevel::classify("Radio message dropped: sender not the configured radio peer (Node Two): Meet me at the gate") ==
                   loglevel::Importance::Always);
}

void test_with_no_radio_peer_configured_no_radio_message_is_accepted() {
  Rig r(ProfileBuilder().radio(true).build());
  r.radioUp("Node One");
  r.radio.deliver("Node One", "Hello", 0);
  r.messaging.poll();
  TEST_ASSERT_EQUAL(0, r.store.count());
  TEST_ASSERT_EQUAL(0, r.store.unread());
  TEST_ASSERT_TRUE(r.log.contains("no radio peer is configured: Hello"));
}

void test_a_flood_from_another_radio_name_is_one_line_and_one_summary_and_the_peer_still_gets_through() {
  Rig r(withRadio());
  r.radioUp("Node One");
  r.radio.addPeer("Intruder", true, 0);
  for (int i = 0; i < 50; ++i) {
    r.radio.deliver("Intruder", "noise " + std::to_string(i), 0);
    r.messaging.poll();
    r.clock.advanceSeconds(2);
  }
  r.radio.deliver("Node One", "still here", 0);
  r.messaging.poll();
  TEST_ASSERT_EQUAL(1, r.store.count());                                  // the peer's message, and only that
  TEST_ASSERT_EQUAL_STRING("still here", r.at(0).text);
  TEST_ASSERT_EQUAL(1, r.log.entries().size());
  TEST_ASSERT_TRUE(r.log.contains("(Intruder): noise 0"));
  r.clock.advanceSeconds(DropLog::kWindowSeconds);
  r.drops.tick();
  TEST_ASSERT_TRUE(r.log.contains("(Intruder) ... (49 more in the last 10 minutes)"));
}

// The name is compared as the radio reports it, character for character: the configured name in another
// case, or with a space, is another sender. (A name can be spoofed within range: docs/messaging.md.)
void test_the_peer_name_is_compared_exactly_and_the_case_is_the_radios() {
  Rig r(withRadio());
  r.radioUp("Node One");
  for (const char* name : {"node one", "NODE ONE", "Node One ", " Node One", "Node  One", "Node One2", "Node"}) {
    r.radio.addPeer(name, true, 0);
    r.radio.deliver(name, "hi", 0);
    r.messaging.poll();
  }
  TEST_ASSERT_EQUAL(0, r.store.count());
  r.radio.deliver("Node One", "hi", 0);
  r.messaging.poll();
  TEST_ASSERT_EQUAL(1, r.store.count());
}

void test_an_explicit_radio_send() {
  Rig r(withRadio());
  r.radioUp("Node One");
  r.radio.addPeer("Node Two", false, 0);
  TEST_ASSERT_TRUE(r.messaging.sendRadioText("Node Two", "Here"));
  TEST_ASSERT_EQUAL_STRING("Node Two", r.radio.sent()[0].peer.c_str());
  TEST_ASSERT_EQUAL(0, r.modem.sent().size());                  // cellular is not asked, it works
  TEST_ASSERT_EQUAL_STRING("Node Two", r.at(0).peer);
  TEST_ASSERT_TRUE(r.at(0).source == MsgSource::Radio);
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::MessageSentRadio));
  TEST_ASSERT_TRUE(r.messaging.sendRadioText("", "Again"));      // empty = the default contact, named in the entry
  TEST_ASSERT_EQUAL_STRING("Node One", r.at(1).peer);
  // Refusals leave no entry.
  TEST_ASSERT_FALSE(r.messaging.sendRadioText("Nobody", "Hello"));
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::RadioUnreachable));
  TEST_ASSERT_FALSE(r.messaging.sendRadioText("Node One", ""));
  r.radio.setUp(false);
  TEST_ASSERT_FALSE(r.messaging.sendRadioText("Node One", "Hello"));
  TEST_ASSERT_EQUAL(2, r.store.count());
}

// ---- reports from the radio ----------------------------------------------------------------------

void test_a_radio_message_is_marked_confirmed() {
  Rig r(withRadio());
  r.radioUp();
  r.networkLost();
  r.messaging.sendText(kParentOne, "Pick me up");
  TEST_ASSERT_TRUE(r.at(0).delivery == MsgDelivery::Unknown);    // no answer yet
  r.radio.confirm(true);
  r.messaging.poll();
  TEST_ASSERT_TRUE(r.at(0).delivery == MsgDelivery::Confirmed);
  TEST_ASSERT_TRUE(r.log.contains("delivered by radio"));
}

void test_a_radio_message_without_confirmation_says_so() {
  Rig r(withRadio());
  r.radioUp();
  r.networkLost();
  r.messaging.sendText(kParentOne, "Pick me up");
  r.radio.confirm(false);
  r.messaging.poll();
  TEST_ASSERT_TRUE(r.at(0).delivery == MsgDelivery::Unconfirmed);
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::RadioUnconfirmed));
  TEST_ASSERT_TRUE(r.log.contains("not confirmed"));
}

void test_two_radio_messages_keep_their_order() {
  Rig r(withRadio());
  r.radioUp();
  r.networkLost();
  r.messaging.sendText(kParentOne, "first");
  r.messaging.sendText(kParentOne, "second");
  r.radio.confirm(false);
  r.radio.confirm(true);
  r.messaging.poll();
  TEST_ASSERT_TRUE(r.at(0).delivery == MsgDelivery::Unconfirmed);
  TEST_ASSERT_TRUE(r.at(1).delivery == MsgDelivery::Confirmed);
}

void test_a_report_nobody_waits_for_is_ignored() {
  Rig r(withRadio());
  r.radioUp();
  TEST_ASSERT_TRUE(r.radio.send("", "sent by somebody else"));   // not through messaging: no message awaits a report
  r.radio.confirm(true);
  r.messaging.poll();
  TEST_ASSERT_EQUAL(0, r.store.count());
  TEST_ASSERT_FALSE(r.noticeIs(NoticeKind::RadioUnconfirmed));
}

void test_the_oldest_awaited_report_is_forgotten() {
  Rig r(withRadio());
  r.radioUp();
  r.networkLost();
  for (size_t i = 0; i < Messaging::kAwaitingSize + 1; ++i) r.messaging.sendText(kParentOne, "m" + std::to_string(i));
  for (size_t i = 0; i < Messaging::kAwaitingSize + 1; ++i) r.radio.confirm(true);
  r.messaging.poll();                                            // five reports, four awaited
  TEST_ASSERT_TRUE(r.at(0).delivery == MsgDelivery::Unknown);    // the oldest was forgotten: it stays unconfirmed
  for (size_t i = 1; i <= Messaging::kAwaitingSize; ++i) TEST_ASSERT_TRUE(r.at(i).delivery == MsgDelivery::Confirmed);
}

void test_an_sms_has_no_delivery_state() {
  Rig r;
  r.messaging.sendText(kParentOne, "On my way");
  r.messaging.poll();
  TEST_ASSERT_TRUE(r.at(0).source == MsgSource::Sms);
  TEST_ASSERT_TRUE(r.at(0).delivery == MsgDelivery::Unknown);
}

// ---- failures ---------------------------------------------------------------------------------------------

void test_a_modem_send_failure_is_recorded_noticed_and_reported() {
  Rig r;
  r.modem.failNextSms("+CMS ERROR: 304");
  TEST_ASSERT_TRUE(r.messaging.sendText(kParentOne, "Help"));     // accepted ...
  TEST_ASSERT_TRUE(r.messaging.poll());                           // ... and refused afterwards
  TEST_ASSERT_TRUE(r.log.contains("SMS to Parent One rejected: +CMS ERROR: 304"));
  TEST_ASSERT_EQUAL(2, r.store.count());
  sysentry::Entry e;
  TEST_ASSERT_TRUE(sysentry::parse(r.at(1), e));
  TEST_ASSERT_EQUAL_STRING(sysentry::kSmsRejected, e.kind.c_str());
  TEST_ASSERT_EQUAL_STRING(kParentOne, r.at(1).peer);
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::SmsRejected));
  TEST_ASSERT_EQUAL(1, r.played(HapticPattern::Failed));
  TEST_ASSERT_EQUAL(1, r.hooks.failures.size());
  TEST_ASSERT_EQUAL_STRING(kParentOne, r.hooks.failures[0].c_str());
  TEST_ASSERT_FALSE(r.messaging.poll());                          // once
  TEST_ASSERT_EQUAL(1, r.hooks.failures.size());
}

void test_the_failure_of_a_silent_reply_stays_out_of_the_childs_sight() {
  Rig r;
  r.hooks.silent = true;
  r.modem.failNextSms("+CMS ERROR: 304");
  r.messaging.sendText(kParentOne, "Location");
  TEST_ASSERT_TRUE(r.messaging.poll());
  TEST_ASSERT_TRUE(r.log.contains("SMS to Parent One rejected"));  // the log keeps it
  TEST_ASSERT_EQUAL(1, r.store.count());                           // the sent text only: no system entry
  TEST_ASSERT_TRUE(r.notices.current().none());
  TEST_ASSERT_EQUAL(0, r.played(HapticPattern::Failed));
  TEST_ASSERT_EQUAL(1, r.hooks.failures.size());                   // and it was asked about
}

void test_sending_is_logged() {
  Rig r;
  r.messaging.sendText(kParentOne, "On my way");
  TEST_ASSERT_TRUE(r.log.contains("Message sent to Parent One (SMS): On my way"));
  Rig q(withRadio());
  q.radioUp();
  q.networkLost();
  q.messaging.sendText(kParentOne, "Radio now");
  TEST_ASSERT_TRUE(q.log.contains("(radio): Radio now"));
  Rig w;
  w.networkLost();
  w.messaging.sendText(kParentOne, "Later");
  TEST_ASSERT_TRUE(w.log.contains("Message waiting"));
}

void test_empty_recipient_or_text_is_refused() {
  Rig r;
  TEST_ASSERT_FALSE(r.messaging.sendText("", "Hello"));
  TEST_ASSERT_FALSE(r.messaging.sendText(kParentOne, ""));
  TEST_ASSERT_EQUAL(0, r.store.count());
  TEST_ASSERT_EQUAL(0, r.modem.sent().size());
  TEST_ASSERT_EQUAL(0, r.messaging.pending());
}

void test_a_message_can_be_deleted() {
  Rig r;
  r.receive(kParentOne, "Hello");
  r.messaging.poll();
  const uint32_t seq = r.at(0).seq;
  TEST_ASSERT_TRUE(r.messaging.remove(seq));
  TEST_ASSERT_EQUAL(0, r.store.count());
  TEST_ASSERT_FALSE(r.messaging.remove(seq));
}

void test_deleting_a_waiting_message_does_not_cancel_it() {
  Rig r;
  r.networkLost();
  r.messaging.sendText(kParentOne, "Pick me up");
  TEST_ASSERT_TRUE(r.messaging.remove(r.at(0).seq));
  TEST_ASSERT_EQUAL(1, r.messaging.pending());                   // the promise stands
  r.networkBack();
  r.messaging.poll();
  TEST_ASSERT_EQUAL(1, r.modem.sent().size());
}

void test_a_message_that_left_the_history_while_waiting_is_still_sent() {
  Rig r;
  r.networkLost();
  r.messaging.sendText(kParentOne, "Pick me up");
  for (size_t i = 0; i < MessageStore::kCapacity + 5; ++i) r.receive(kParentTwo, "noise " + std::to_string(i));
  r.messaging.poll();                                             // the history ring has pushed the waiting entry out
  TEST_ASSERT_EQUAL(MessageStore::kCapacity, r.store.count());
  r.networkBack();
  r.messaging.poll();
  TEST_ASSERT_EQUAL(1, r.modem.sent().size());                    // sent all the same, without a crash
  TEST_ASSERT_EQUAL(0, r.messaging.pending());
}

void test_a_long_queued_message_keeps_its_full_text() {
  Rig r;
  r.networkLost();
  const std::string longText(250, 'x');                           // more than the history keeps (160)
  r.messaging.sendText(kParentOne, longText);
  TEST_ASSERT_EQUAL(160, strlen(r.at(0).text));
  r.networkBack();
  r.messaging.poll();
  TEST_ASSERT_EQUAL(250, r.modem.sent()[0].body.size());
}

void test_the_history_text_is_cut_on_a_character_boundary() {
  Rig r;
  std::string text(159, 'x');
  text += "\xC3\xBC";                                              // an umlaut would cross the 160-byte limit
  r.messaging.sendText(kParentOne, text);
  TEST_ASSERT_EQUAL(159, strlen(r.at(0).text));
}

// A modem that refuses the n-th message handed to it, to see what the queue keeps.
struct FlakyModem : FakeModem {
  using FakeModem::FakeModem;
  int refuseNumber = -1;
  int calls = 0;
  bool sendSms(const std::string& phone, const std::string& body) override {
    if (calls++ == refuseNumber) return false;
    return FakeModem::sendSms(phone, body);
  }
};

void test_a_message_the_modem_refuses_stays_in_the_queue_while_the_others_go() {
  CoreFakes f;
  FlakyModem flaky(f.clock);
  flaky.begin();
  flaky.setNetwork("Example Mobile", 4);
  Hooks hooks;
  Messaging m(f.clock, flaky, f.power, f.store, f.profile, f.school, f.notices, f.haptic, f.logger, f.drops);
  flaky.setNetwork("", 0);
  m.sendText(kParentOne, "first");
  m.sendText(kParentOne, "second");
  m.sendText(kParentOne, "third");
  flaky.setNetwork("Example Mobile", 4);
  flaky.refuseNumber = 1;                                          // the second hand-over is refused
  m.poll();
  TEST_ASSERT_EQUAL(2, flaky.sent().size());
  TEST_ASSERT_EQUAL(1, m.pending());
  TEST_ASSERT_TRUE(f.store.at(0).pending == false && f.store.at(1).pending == true && f.store.at(2).pending == false);
  m.poll();                                                        // and it goes at the next poll
  TEST_ASSERT_EQUAL(3, flaky.sent().size());
  TEST_ASSERT_EQUAL_STRING("second", flaky.sent()[2].body.c_str());
  TEST_ASSERT_EQUAL(0, m.pending());
}

void test_a_text_longer_than_the_modem_takes_is_refused_instead_of_waiting_for_ever() {
  Rig r;
  r.networkLost();
  TEST_ASSERT_FALSE(r.messaging.sendText(kParentOne, std::string(kMaxSmsBytes + 1, 'x')));
  TEST_ASSERT_EQUAL(0, r.messaging.pending());
  TEST_ASSERT_EQUAL(0, r.store.count());
  TEST_ASSERT_TRUE(r.messaging.sendText(kParentOne, std::string(kMaxSmsBytes, 'x')));    // the limit itself is fine
  TEST_ASSERT_EQUAL(1, r.messaging.pending());
}

void test_an_empty_radio_text_is_refused_quietly() {
  Rig r(withRadio());
  r.radioUp();
  r.notices.clear();
  TEST_ASSERT_FALSE(r.messaging.sendRadioText("Node One", ""));
  TEST_ASSERT_TRUE(r.notices.current().none());                   // nothing was attempted, so nothing failed
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_an_incoming_sms_is_stored);
  RUN_TEST(test_a_remote_query_is_not_stored_as_a_message);
  RUN_TEST(test_a_remote_query_is_logged_recorded_and_noticed_as_the_profile_says);
  RUN_TEST(test_a_refused_remote_query_is_recorded);
  RUN_TEST(test_a_remote_query_while_a_search_runs_is_logged_as_busy);
  RUN_TEST(test_a_remote_query_does_not_vibrate);
  RUN_TEST(test_an_incoming_sms_vibrates_the_message_pattern);
  RUN_TEST(test_message_vibration_can_be_switched_off);
  RUN_TEST(test_in_school_time_only_parents_vibrate);
  RUN_TEST(test_a_switched_off_window_lets_everybody_vibrate);
  RUN_TEST(test_after_school_time_everybody_vibrates_again);
  RUN_TEST(test_an_incoming_sms_means_the_modem_is_awake);
  RUN_TEST(test_an_sms_from_a_stranger_is_dropped_without_a_trace);
  RUN_TEST(test_the_drop_is_logged_with_the_number_and_the_start_of_the_text);
  RUN_TEST(test_a_stranger_in_another_spelling_is_logged_in_the_canonical_form);
  RUN_TEST(test_a_sender_that_is_no_number_is_dropped_and_logged_by_its_name);
  RUN_TEST(test_a_flood_of_a_hundred_texts_from_a_stranger_is_one_line_and_one_summary);
  RUN_TEST(test_the_text_of_a_dropped_message_is_in_the_log_cut_clean_and_without_control_characters);
  RUN_TEST(test_an_empty_text_from_a_stranger_is_dropped);
  RUN_TEST(test_an_empty_text_from_a_contact_is_stored_as_before);
  RUN_TEST(test_every_spelling_of_a_contact_is_let_through);
  RUN_TEST(test_the_target_of_a_call_action_may_write);
  RUN_TEST(test_a_stranger_in_school_time_leaves_nothing_either);
  RUN_TEST(test_a_stranger_cannot_use_the_remote_query);
  RUN_TEST(test_a_flood_from_a_stranger_leaves_the_history_untouched);
  RUN_TEST(test_a_dropped_sms_still_proves_the_modem_awake);
  RUN_TEST(test_a_message_goes_by_cellular);
  RUN_TEST(test_cellular_has_priority_over_the_radio);
  RUN_TEST(test_the_radio_takes_over_when_cellular_is_gone);
  RUN_TEST(test_cellular_off_uses_the_radio);
  RUN_TEST(test_without_a_path_a_message_is_queued);
  RUN_TEST(test_a_queued_message_goes_out_when_the_network_returns);
  RUN_TEST(test_a_queued_message_goes_out_by_radio);
  RUN_TEST(test_the_queue_is_bounded);
  RUN_TEST(test_the_queue_goes_out_in_order_and_keeps_what_cannot_leave_yet);
  RUN_TEST(test_a_message_without_any_path_does_not_keep_the_modem_awake);
  RUN_TEST(test_a_message_with_a_path_is_pending_work);
  RUN_TEST(test_without_a_radio_everything_works_as_before);
  RUN_TEST(test_a_profile_without_the_radio_never_sends_by_radio);
  RUN_TEST(test_a_profile_without_the_radio_ignores_incoming_radio_messages);
  RUN_TEST(test_an_incoming_radio_message_is_stored);
  RUN_TEST(test_a_radio_message_from_the_configured_peer_is_stored_and_counted_unread);
  RUN_TEST(test_a_radio_message_from_another_name_is_dropped_like_a_strangers_text);
  RUN_TEST(test_with_no_radio_peer_configured_no_radio_message_is_accepted);
  RUN_TEST(test_a_flood_from_another_radio_name_is_one_line_and_one_summary_and_the_peer_still_gets_through);
  RUN_TEST(test_the_peer_name_is_compared_exactly_and_the_case_is_the_radios);
  RUN_TEST(test_an_explicit_radio_send);
  RUN_TEST(test_a_radio_message_is_marked_confirmed);
  RUN_TEST(test_a_radio_message_without_confirmation_says_so);
  RUN_TEST(test_two_radio_messages_keep_their_order);
  RUN_TEST(test_a_report_nobody_waits_for_is_ignored);
  RUN_TEST(test_the_oldest_awaited_report_is_forgotten);
  RUN_TEST(test_an_sms_has_no_delivery_state);
  RUN_TEST(test_a_modem_send_failure_is_recorded_noticed_and_reported);
  RUN_TEST(test_the_failure_of_a_silent_reply_stays_out_of_the_childs_sight);
  RUN_TEST(test_sending_is_logged);
  RUN_TEST(test_empty_recipient_or_text_is_refused);
  RUN_TEST(test_a_message_can_be_deleted);
  RUN_TEST(test_deleting_a_waiting_message_does_not_cancel_it);
  RUN_TEST(test_a_message_that_left_the_history_while_waiting_is_still_sent);
  RUN_TEST(test_a_long_queued_message_keeps_its_full_text);
  RUN_TEST(test_the_history_text_is_cut_on_a_character_boundary);
  RUN_TEST(test_a_message_the_modem_refuses_stays_in_the_queue_while_the_others_go);
  RUN_TEST(test_a_text_longer_than_the_modem_takes_is_refused_instead_of_waiting_for_ever);
  RUN_TEST(test_an_empty_radio_text_is_refused_quietly);
  return UNITY_END();
}

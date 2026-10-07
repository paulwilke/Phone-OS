// Host tests for calls: the dialling rules, school time, ringing, the report of a call that did
// not happen, and the volume.

#include <unity.h>

#include <deque>

#include <CoreFakes.h>
#include <app/Calls.h>
#include <app/LogLevel.h>
#include <app/SystemEntry.h>

using namespace people;

void setUp() {}
void tearDown() {}

namespace {

struct Rig : CoreFakes {
  Calls calls;

  explicit Rig(Profile p = ProfileBuilder().build(), uint32_t startSeconds = 100)
      : CoreFakes(std::move(p), startSeconds), calls(clock, modem, power, store, profile, school, notices, haptic, logger, drops) {}

  bool noticeIs(NoticeKind kind) { return notices.current().kind == kind; }
  // The events the modem has produced so far reach the core at the next poll.
  bool poll() { return calls.poll(); }
  sysentry::Entry entry(size_t i) {
    sysentry::Entry e;
    sysentry::parse(store.at(i), e);
    return e;
  }
};

Profile inSchool() { return ProfileBuilder().schoolTime("tue", "14:00", "15:00").build(); }
// The same window, switched off by the parents.
Profile inSwitchedOffSchool() { return ProfileBuilder().schoolTime("tue", "14:00", "15:00", false).build(); }

}  // namespace

// ---- dialling -----------------------------------------------------------------------------------

void test_only_allowed_numbers_are_dialled() {
  Rig r;
  TEST_ASSERT_FALSE(r.calls.call(kStranger));
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::CallNotAllowed));
  TEST_ASSERT_EQUAL_STRING("", r.modem.callPhone().c_str());       // nothing was dialled
  TEST_ASSERT_TRUE(r.calls.call(kParentOne));
  TEST_ASSERT_EQUAL_STRING(kParentOne, r.modem.callPhone().c_str());
  TEST_ASSERT_EQUAL_STRING(kParentOne, r.calls.peer().c_str());
  TEST_ASSERT_TRUE(r.calls.active());
}

void test_a_number_in_another_spelling_and_a_listed_non_parent_may_be_called() {
  Rig r;
  TEST_ASSERT_TRUE(r.calls.call("0170 1111111"));
  r.calls.hangup();
  TEST_ASSERT_TRUE(r.calls.call(kGrandma));
}

void test_dialling_is_refused_in_school_time() {
  Rig r(inSchool());
  TEST_ASSERT_FALSE(r.calls.call(kParentOne));
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::SchoolTimeUntil));
  TEST_ASSERT_EQUAL(15 * 60, r.notices.current().minute);
  TEST_ASSERT_EQUAL_STRING("", r.modem.callPhone().c_str());
  TEST_ASSERT_TRUE(r.calls.refuseInSchoolTime());                  // the same question for anything else the child starts
}

void test_a_switched_off_window_does_not_stop_dialling_or_ringing() {
  Rig r(inSwitchedOffSchool());
  TEST_ASSERT_FALSE(r.calls.refuseInSchoolTime());
  TEST_ASSERT_TRUE(r.calls.call(kParentOne));
  Rig q(inSwitchedOffSchool());
  q.modem.injectIncomingCall(kGrandma);
  q.poll();
  TEST_ASSERT_FALSE(q.log.contains("rejected in quiet hours"));
  TEST_ASSERT_FALSE(q.noticeIs(NoticeKind::CallerInSchoolTime));
}

void test_dialling_works_outside_school_time() {
  Rig r(inSchool());
  r.advance(3600);                                                 // 15:32
  TEST_ASSERT_FALSE(r.calls.refuseInSchoolTime());
  TEST_ASSERT_TRUE(r.calls.call(kParentOne));
  Rig q(inSchool());
  q.clock.setEpochUtc(0);                                          // no known time: no lock
  TEST_ASSERT_TRUE(q.calls.call(kParentOne));
}

void test_dialling_is_refused_with_cellular_off() {
  Rig r;
  r.power.setEnabled(PowerDomain::Cellular, false);
  TEST_ASSERT_FALSE(r.calls.call(kParentOne));
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::CellularIsOff));
  TEST_ASSERT_EQUAL_STRING("", r.modem.callPhone().c_str());
}

void test_dialling_is_refused_without_a_network() {
  Rig r;
  r.networkLost();
  TEST_ASSERT_FALSE(r.calls.call(kParentOne));
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::NoNetwork));
}

void test_the_rules_are_applied_in_this_order() {
  Rig r(inSchool());
  r.networkLost();
  r.calls.call(kStranger);
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::SchoolTimeUntil));       // school time first
  Rig q;
  q.networkLost();
  q.power.setEnabled(PowerDomain::Cellular, false);
  q.calls.call(kStranger);
  TEST_ASSERT_TRUE(q.noticeIs(NoticeKind::CallNotAllowed));        // then the number
  q.calls.call(kParentOne);
  TEST_ASSERT_TRUE(q.noticeIs(NoticeKind::CellularIsOff));         // then power, then the network
}

void test_a_refused_dial_leaves_no_call() {
  Rig r;
  r.modem.setNetwork("Example Mobile", 4, VoiceStatus::SmsOnly);                  // registered for SMS only: the modem will not dial
  TEST_ASSERT_FALSE(r.calls.call(kParentOne));
  TEST_ASSERT_EQUAL_STRING("", r.calls.peer().c_str());
  TEST_ASSERT_FALSE(r.calls.active());
  TEST_ASSERT_TRUE(r.log.contains("refused by the modem"));
  r.modem.remoteHangup();
  r.poll();
  TEST_ASSERT_EQUAL(0, r.store.count());                           // and nothing is reported as failed
}

// A modem that is registered but has no IMS would hang after ATD: the dial is refused before it starts,
// and the child sees why instead of a call that silently did not happen.
void test_a_dial_without_ims_is_refused_with_the_reason_and_starts_nothing() {
  Rig r;
  r.modem.setNetwork("Example Mobile", 4, VoiceStatus::ImsMissing);
  TEST_ASSERT_FALSE(r.calls.call(kParentOne));
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::CallNotReady));
  TEST_ASSERT_EQUAL_STRING(kParentOne, r.notices.current().peer.c_str());
  TEST_ASSERT_EQUAL_STRING("", r.modem.callPhone().c_str());
  TEST_ASSERT_FALSE(r.calls.active());
  r.poll();
  TEST_ASSERT_EQUAL(0, r.store.count());                           // a refusal is no failed call in the history
}

void test_a_dial_on_an_sms_only_network_is_refused_with_that_reason() {
  Rig r;
  r.modem.setNetwork("Example Mobile", 4, VoiceStatus::SmsOnly);
  TEST_ASSERT_FALSE(r.calls.call(kParentOne));
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::CallSmsOnly));
}

void test_a_dial_failure_that_the_modem_calls_not_ready_says_so() {
  Rig r;
  r.calls.call(kParentOne);
  r.modem.failDial("", DialFailReason::NotReady);
  r.poll();
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::CallNotReady));
  TEST_ASSERT_EQUAL_STRING(sysentry::kTelephonyNotReady, r.entry(0).argument.c_str());
}

// Without a code from the modem, a missing IMS at the time of the failure is the best explanation.
void test_a_dial_failure_without_a_code_while_the_ims_is_missing_says_not_ready() {
  Rig r;
  r.calls.call(kParentOne);
  r.modem.failDial("NO CARRIER");
  r.modem.setNetwork("Example Mobile", 4, VoiceStatus::ImsMissing);
  r.poll();
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::CallNotReady));
}

void test_a_modem_restart_while_dialling_is_reported_as_such_and_logged() {
  Rig r;
  r.calls.call(kParentOne);
  r.modem.restartDuringCall();
  r.poll();
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::CallModemRestarted));
  TEST_ASSERT_EQUAL_STRING(sysentry::kModemRestarted, r.entry(0).argument.c_str());
  TEST_ASSERT_EQUAL(1, r.played(HapticPattern::Failed));
  TEST_ASSERT_TRUE(r.log.contains("| modem: *ATREADY"));
  TEST_ASSERT_FALSE(r.calls.active());
}

// A conversation that the restart cut off was a conversation: it ends, it is not a failed call.
void test_a_modem_restart_in_a_connected_call_is_an_end_not_a_failure() {
  Rig r;
  r.calls.call(kParentOne);
  r.modem.remoteAnswer();
  r.modem.restartDuringCall();
  r.poll();
  TEST_ASSERT_TRUE(r.notices.current().none());
  TEST_ASSERT_EQUAL(0, r.store.count());
  TEST_ASSERT_FALSE(r.calls.active());
}

void test_a_refused_dial_with_a_code_of_refused_says_not_connected() {
  Rig r;
  r.calls.call(kParentOne);
  r.modem.failDial("+CME ERROR: 30", DialFailReason::Refused);
  r.poll();
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::CallFailed));
  TEST_ASSERT_EQUAL_STRING(sysentry::kNotConnected, r.entry(0).argument.c_str());
}

// ---- incoming ---------------------------------------------------------------------------------------

void test_an_incoming_call_names_the_peer() {
  Rig r;
  r.modem.injectIncomingCall(kGrandma);
  TEST_ASSERT_TRUE(r.poll());
  TEST_ASSERT_EQUAL_STRING(kGrandma, r.calls.peer().c_str());
  TEST_ASSERT_TRUE(r.log.contains("Call from Grandma"));
  TEST_ASSERT_FALSE(r.poll());                                      // events are reported once
}

void test_a_missed_call_is_recorded() {
  Rig r;
  r.modem.injectIncomingCall(kGrandma);
  r.poll();
  r.modem.remoteHangup();                                           // she gives up
  r.poll();
  TEST_ASSERT_EQUAL_STRING("", r.calls.peer().c_str());
  TEST_ASSERT_EQUAL(1, r.store.count());
  TEST_ASSERT_EQUAL_STRING(sysentry::kMissedCall, r.entry(0).kind.c_str());
  TEST_ASSERT_EQUAL_STRING(kGrandma, r.store.at(0).peer);           // the number, so that a callback is possible
  TEST_ASSERT_EQUAL(coretest::kTuesdayAfternoon, r.store.at(0).time);
}

void test_answer_and_hang_up() {
  Rig r;
  r.modem.injectIncomingCall(kParentOne);
  r.poll();
  TEST_ASSERT_TRUE(r.calls.answer());
  TEST_ASSERT_TRUE(r.calls.state() == ModemState::InCall);
  r.poll();
  TEST_ASSERT_TRUE(r.log.contains("Call connected"));
  TEST_ASSERT_TRUE(r.calls.hangup());
  TEST_ASSERT_EQUAL_STRING("", r.calls.peer().c_str());
  r.poll();
  TEST_ASSERT_TRUE(r.log.contains("Call ended: Parent One"));
  TEST_ASSERT_TRUE(r.notices.current().none());                     // a normal call is no failure
  TEST_ASSERT_FALSE(r.calls.hangup());                              // nothing to end now
  TEST_ASSERT_FALSE(r.calls.answer());
}

void test_ringing_vibrates_until_it_stops() {
  Rig r;
  r.poll();
  r.advance(60);
  r.poll();
  TEST_ASSERT_EQUAL(0, r.haptic.played().size());                   // nothing happens: no vibration
  r.modem.injectIncomingCall(kParentOne);
  r.poll();
  TEST_ASSERT_EQUAL(1, r.played(HapticPattern::Ring));
  r.poll();
  TEST_ASSERT_EQUAL(1, r.played(HapticPattern::Ring));              // not again at once
  r.advance(Calls::kRingIntervalSeconds - 1);
  r.poll();
  TEST_ASSERT_EQUAL(1, r.played(HapticPattern::Ring));
  r.advance(1);
  r.poll();
  TEST_ASSERT_EQUAL(2, r.played(HapticPattern::Ring));              // every two seconds
  r.calls.answer();
  r.advance(10);
  r.poll();
  TEST_ASSERT_EQUAL(2, r.played(HapticPattern::Ring));              // answered: quiet
  TEST_ASSERT_EQUAL(2, r.haptic.played().size());
}

void test_call_vibration_can_be_switched_off() {
  Rig r(ProfileBuilder().callVibration(false).build());
  r.modem.injectIncomingCall(kParentOne);
  r.poll();
  TEST_ASSERT_EQUAL(0, r.haptic.played().size());
}

// ---- school time ---------------------------------------------------------------------------------------

void test_every_call_in_school_time_is_rejected_silently_and_named() {
  for (const char* who : {kParentOne, kGrandma}) {
    Rig r(inSchool());
    r.modem.injectIncomingCall(who);
    r.poll();
    TEST_ASSERT_TRUE(r.calls.state() == ModemState::Ready);         // hung up
    TEST_ASSERT_EQUAL(0, r.haptic.played().size());                 // neither ringing nor vibrating
    TEST_ASSERT_TRUE(r.log.contains("rejected in quiet hours"));
    TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::CallerInSchoolTime));
    TEST_ASSERT_EQUAL_STRING(who, r.notices.current().peer.c_str());
  }
}

void test_a_rejected_call_is_in_the_history_as_missed() {
  Rig r(inSchool());
  r.modem.injectIncomingCall(kParentOne);
  r.poll();
  r.poll();                                                          // the modem's "missed" arrives with the next events
  TEST_ASSERT_EQUAL(1, r.store.count());
  TEST_ASSERT_EQUAL_STRING(sysentry::kMissedCall, r.entry(0).kind.c_str());
  TEST_ASSERT_EQUAL_STRING(kParentOne, r.store.at(0).peer);
}

void test_call_vibration_in_school_time_is_a_setting() {
  Rig r(ProfileBuilder().schoolTime("tue", "14:00", "15:00").quietCallVibration(true).build());
  r.modem.injectIncomingCall(kParentOne);
  r.poll();
  TEST_ASSERT_TRUE(r.calls.state() == ModemState::Ready);           // still rejected
  TEST_ASSERT_TRUE(r.played(HapticPattern::Ring) >= 1);
  Rig q(ProfileBuilder().schoolTime("tue", "14:00", "15:00").callVibration(true).quietCallVibration(false).build());
  q.modem.injectIncomingCall(kParentOne);
  q.poll();
  TEST_ASSERT_EQUAL(0, q.haptic.played().size());                   // the school-time switch rules, not the everyday one
}

void test_a_call_without_a_number_yet_is_neither_rung_nor_rejected() {
  Rig r(inSchool());
  r.modem.injectIncomingCall("");
  r.poll();
  TEST_ASSERT_TRUE(r.modem.state() == ModemState::RingingIn);        // not hung up yet
  TEST_ASSERT_EQUAL(0, r.haptic.played().size());
  TEST_ASSERT_TRUE(r.notices.current().none());
  r.advance(1);
  r.modem.announceCaller(kParentOne);                                // the number arrives
  r.poll();
  TEST_ASSERT_TRUE(r.calls.state() == ModemState::Ready);           // now it is rejected, and named
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::CallerInSchoolTime));
}

void test_a_call_without_a_number_is_rejected_after_three_seconds() {
  Rig r(inSchool());
  r.modem.injectIncomingCall("");
  r.poll();
  r.advance(Calls::kQuietRejectSeconds - 1);
  r.poll();
  TEST_ASSERT_TRUE(r.modem.state() == ModemState::RingingIn);
  r.advance(1);
  r.poll();
  TEST_ASSERT_TRUE(r.modem.state() == ModemState::Ready);
  TEST_ASSERT_EQUAL(0, r.haptic.played().size());
  TEST_ASSERT_TRUE(r.notices.current().none());                      // the child is told nothing
  TEST_ASSERT_TRUE(r.log.contains("Call rejected: number withheld"));
}

void test_an_incoming_call_is_rejected_once() {
  Rig r(inSchool());
  r.modem.injectIncomingCall("");
  r.poll();
  r.modem.announceCaller(kParentOne);
  r.modem.announceCaller(kParentOne);                                // reported twice with the number
  r.poll();
  size_t rejections = 0;
  for (const auto& line : r.log.lines()) rejections += line.find("rejected in quiet hours: ") != std::string::npos ? 1 : 0;
  TEST_ASSERT_EQUAL(1, rejections);
}

void test_a_call_after_school_time_rings_and_is_not_rejected() {
  Rig r(inSchool());
  r.advance(3600);
  r.modem.injectIncomingCall(kParentOne);
  r.poll();
  TEST_ASSERT_TRUE(r.calls.state() == ModemState::RingingIn);
  TEST_ASSERT_EQUAL(1, r.played(HapticPattern::Ring));
}

// ---- only the contact book may call the child ---------------------------------------------------

// A rejected stranger leaves nothing: no ring, no vibration, no notice, no history entry, no peer.
void expectNoTrace(Rig& r) {
  TEST_ASSERT_EQUAL(0, r.haptic.played().size());
  TEST_ASSERT_TRUE(r.notices.current().none());
  TEST_ASSERT_EQUAL(0, r.store.count());
  TEST_ASSERT_EQUAL_STRING("", r.calls.peer().c_str());
  TEST_ASSERT_TRUE(r.calls.state() == ModemState::Ready);
  TEST_ASSERT_FALSE(r.calls.active());
  TEST_ASSERT_TRUE(r.modem.state() == ModemState::Ready);            // hung up
}

void test_a_stranger_is_hung_up_at_once_without_a_trace() {
  Rig r;
  r.modem.injectIncomingCall(kStranger);
  r.poll();
  expectNoTrace(r);
  r.poll();                                                          // the modem's "missed" arrives later
  expectNoTrace(r);
  r.advance(10);
  r.poll();
  expectNoTrace(r);
}

void test_the_rejection_of_a_stranger_is_one_log_line_with_the_number() {
  Rig r;
  r.modem.injectIncomingCall(kStranger);
  r.poll();
  r.poll();
  size_t lines = 0;
  for (const auto& line : r.log.lines()) lines += line.find(kStranger) != std::string::npos ? 1 : 0;
  TEST_ASSERT_EQUAL(1, lines);
  TEST_ASSERT_TRUE(r.log.contains("Call rejected: caller not in the contact book (+491709999999)"));
  TEST_ASSERT_TRUE(loglevel::classify("Call rejected: caller not in the contact book (+491709999999)") ==
                   loglevel::Importance::Always);
  TEST_ASSERT_TRUE(loglevel::classify("Call rejected: number withheld") == loglevel::Importance::Always);
}

void test_a_stranger_is_rejected_in_school_time_too_and_without_a_notice() {
  Rig r(inSchool());
  TEST_ASSERT_TRUE(r.school.active());
  r.modem.injectIncomingCall(kStranger);
  r.poll();
  r.poll();
  expectNoTrace(r);
}

void test_a_stranger_does_not_vibrate_even_when_school_time_vibration_is_on() {
  Rig r(ProfileBuilder().schoolTime("tue", "14:00", "15:00").quietCallVibration(true).build());
  r.modem.injectIncomingCall(kStranger);
  r.poll();
  expectNoTrace(r);
}

void test_a_stranger_who_is_announced_late_is_rejected_when_the_number_arrives() {
  Rig r;
  r.modem.injectIncomingCall("");
  r.poll();
  TEST_ASSERT_EQUAL(0, r.haptic.played().size());                    // nothing rang while nobody was known
  r.advance(1);
  r.modem.announceCaller(kStranger);
  r.poll();
  expectNoTrace(r);
}

void test_a_hidden_number_is_rejected_after_three_seconds_at_any_time_of_day() {
  Rig r;                                                             // no school time
  r.modem.injectIncomingCall("");
  r.poll();
  TEST_ASSERT_EQUAL(0, r.haptic.played().size());
  TEST_ASSERT_TRUE(r.calls.state() == ModemState::Ready);           // the child's view: no call
  r.advance(Calls::kQuietRejectSeconds - 1);
  r.poll();
  TEST_ASSERT_TRUE(r.modem.state() == ModemState::RingingIn);
  r.advance(1);
  r.poll();
  r.poll();
  expectNoTrace(r);
  TEST_ASSERT_TRUE(r.log.contains("Call rejected: number withheld"));
}

void test_a_hidden_number_is_rejected_once_and_leaves_no_missed_call() {
  Rig r;
  r.modem.injectIncomingCall("");
  r.poll();
  r.advance(5);
  r.poll();
  r.poll();
  size_t lines = 0;
  for (const auto& line : r.log.lines()) lines += line.find("number withheld") != std::string::npos ? 1 : 0;
  TEST_ASSERT_EQUAL(1, lines);
  TEST_ASSERT_EQUAL(0, r.store.count());
}

void test_a_contact_whose_number_arrives_late_rings_from_then_on() {
  Rig r;
  r.modem.injectIncomingCall("");
  r.poll();
  r.advance(1);
  r.modem.announceCaller("0170 1111111");                            // another spelling
  r.poll();
  TEST_ASSERT_TRUE(r.calls.state() == ModemState::RingingIn);
  TEST_ASSERT_EQUAL(1, r.played(HapticPattern::Ring));
}

void test_the_target_of_a_call_action_may_call() {
  Rig r(ProfileBuilder().callTargets({"+491701234567"}).build());
  r.modem.injectIncomingCall("0170 1234567");
  r.poll();
  TEST_ASSERT_TRUE(r.calls.state() == ModemState::RingingIn);
}

void test_a_stranger_calling_again_is_rejected_again() {
  Rig r;
  for (int i = 0; i < 3; ++i) {
    r.modem.injectIncomingCall(kStranger);
    r.poll();
    r.poll();
    expectNoTrace(r);
  }
}

// A flood of calls from one stranger must not fill the card: one line, and one summary when the ten minutes
// are over; the calls are hung up all the same.
void test_a_flood_of_calls_from_a_stranger_is_one_line_and_one_summary() {
  Rig r;
  for (int i = 0; i < 40; ++i) {
    r.modem.injectIncomingCall(kStranger);
    r.poll();
    r.poll();
    expectNoTrace(r);
    r.clock.advanceSeconds(5);
  }
  size_t lines = 0;
  for (const auto& line : r.log.lines()) lines += line.find(kStranger) != std::string::npos ? 1 : 0;
  TEST_ASSERT_EQUAL(1, lines);
  r.clock.advanceSeconds(DropLog::kWindowSeconds);
  r.drops.tick();
  TEST_ASSERT_TRUE(r.log.contains("(+491709999999) ... (39 more in the last 10 minutes)"));
  r.modem.injectIncomingCall(kStranger);                                  // the summary is a line: ten minutes from it, not before
  r.poll();
  r.poll();
  TEST_ASSERT_EQUAL(2, r.log.entries().size());
  r.clock.advanceSeconds(DropLog::kWindowSeconds);
  r.modem.injectIncomingCall(kStranger);
  r.poll();
  r.poll();
  TEST_ASSERT_EQUAL(4, r.log.entries().size());                           // the summary of the one that was counted, and the new line
}

void test_a_stranger_does_not_disturb_the_calls_of_contacts() {
  Rig r;
  r.modem.injectIncomingCall(kStranger);
  r.poll();
  r.modem.injectIncomingCall(kParentOne);
  r.poll();
  TEST_ASSERT_TRUE(r.calls.state() == ModemState::RingingIn);
  TEST_ASSERT_EQUAL_STRING(kParentOne, r.calls.peer().c_str());
  TEST_ASSERT_EQUAL(1, r.played(HapticPattern::Ring));
}

void test_a_call_we_dialled_is_not_hidden_by_the_rule() {
  Rig r;
  r.calls.call(kGrandma);
  r.modem.remoteBusy();
  r.poll();
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::CallBusy));
}

// ---- a call that did not happen --------------------------------------------------------------------------

void test_a_busy_line_is_reported() {
  Rig r;
  r.calls.call(kParentOne);
  r.modem.remoteBusy();
  r.poll();
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::CallBusy));
  TEST_ASSERT_EQUAL_STRING(kParentOne, r.notices.current().peer.c_str());
  TEST_ASSERT_EQUAL(1, r.played(HapticPattern::Failed));
  TEST_ASSERT_EQUAL(1, r.store.count());                              // the notice goes, the history stays
  TEST_ASSERT_EQUAL_STRING(sysentry::kCallFailed, r.entry(0).kind.c_str());
  TEST_ASSERT_EQUAL_STRING(sysentry::kBusy, r.entry(0).argument.c_str());
  TEST_ASSERT_EQUAL_STRING(kParentOne, r.store.at(0).peer);
  TEST_ASSERT_EQUAL_STRING("", r.calls.peer().c_str());
  TEST_ASSERT_TRUE(r.log.contains("failed: busy"));
}

void test_no_answer_is_reported() {
  Rig r;
  r.calls.call(kParentOne);
  r.modem.remoteNoAnswer();
  r.poll();
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::CallNoAnswer));
  TEST_ASSERT_EQUAL_STRING(sysentry::kNoAnswer, r.entry(0).argument.c_str());
}

void test_a_dial_failure_on_an_sms_only_network_says_so() {
  Rig r;
  r.calls.call(kParentOne);
  r.modem.failDial("NO CARRIER");
  r.modem.setNetwork("Example Mobile", 4, VoiceStatus::SmsOnly);                      // the network allows SMS only
  r.poll();
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::CallSmsOnly));
  TEST_ASSERT_EQUAL_STRING(sysentry::kSmsOnly, r.entry(0).argument.c_str());
}

void test_a_dial_failure_without_a_network_says_so() {
  Rig r;
  r.calls.call(kParentOne);
  r.modem.failDial("NO CARRIER");
  r.networkLost();
  r.poll();
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::CallNoNetwork));
  TEST_ASSERT_EQUAL_STRING(sysentry::kNoNetwork, r.entry(0).argument.c_str());
}

void test_a_dial_failure_is_still_reported() {
  Rig r;
  r.calls.call(kParentOne);
  r.modem.failDial("");
  r.poll();
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::CallFailed));
  TEST_ASSERT_EQUAL_STRING(sysentry::kNotConnected, r.entry(0).argument.c_str());
  TEST_ASSERT_EQUAL(1, r.played(HapticPattern::Failed));
}

void test_an_unconnected_end_is_a_failure() {
  Rig r;
  r.calls.call(kParentOne);
  r.modem.hangup();                                                    // ended without ever connecting, not by the child
  r.poll();
  TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::CallFailed));

  Rig connected;                                                       // a conversation that took place and ended
  connected.calls.call(kParentOne);
  connected.modem.remoteAnswer();
  connected.modem.remoteHangup();
  connected.poll();
  TEST_ASSERT_TRUE(connected.notices.current().none());
  TEST_ASSERT_EQUAL(0, connected.store.count());

  Rig own;                                                             // the child ends it before anybody answers
  own.calls.call(kParentOne);
  own.calls.hangup();
  own.poll();
  TEST_ASSERT_TRUE(own.notices.current().none());
  TEST_ASSERT_EQUAL(0, own.store.count());
  TEST_ASSERT_EQUAL(0, own.haptic.played().size());
}

void test_the_modems_reason_is_logged() {
  Rig r;
  r.calls.call(kParentOne);
  r.modem.failDial("+CME ERROR: 30");
  r.poll();
  TEST_ASSERT_TRUE(r.log.contains("| modem: +CME ERROR: 30"));
}

void test_calls_are_logged() {
  Rig r;
  r.calls.call(kParentOne);
  r.modem.remoteAnswer();
  r.modem.remoteHangup();
  r.poll();
  TEST_ASSERT_TRUE(r.log.contains("Call connected: Parent One"));
  TEST_ASSERT_TRUE(r.log.contains("Call ended: Parent One"));
}

void test_the_state_after_a_failed_call_is_clean() {
  Rig r;
  r.calls.call(kParentOne);
  r.modem.remoteBusy();
  r.poll();
  TEST_ASSERT_TRUE(r.calls.call(kParentOne));                           // a second try works
  r.modem.remoteAnswer();
  r.modem.remoteHangup();
  r.poll();
  TEST_ASSERT_EQUAL(1, r.store.count());                                // and the normal end of it reports nothing more
}

// ---- volume ----------------------------------------------------------------------------------------------------

void test_volume_only_during_a_call_and_within_range() {
  Rig r;
  TEST_ASSERT_EQUAL(4, r.calls.volume());
  TEST_ASSERT_FALSE(r.calls.changeVolume(+1));                           // no call
  TEST_ASSERT_EQUAL(4, r.calls.volume());
  r.modem.injectIncomingCall(kParentOne);
  TEST_ASSERT_FALSE(r.calls.changeVolume(+1));                           // it rings: nothing to adjust yet
  r.calls.answer();
  TEST_ASSERT_TRUE(r.calls.changeVolume(+1));
  TEST_ASSERT_EQUAL(5, r.calls.volume());
  TEST_ASSERT_TRUE(r.calls.changeVolume(-2));
  TEST_ASSERT_EQUAL(3, r.calls.volume());
  for (int i = 0; i < 10; ++i) r.calls.changeVolume(+1);
  TEST_ASSERT_EQUAL(Calls::kMaxVolume, r.calls.volume());
  TEST_ASSERT_FALSE(r.calls.changeVolume(+1));                           // at the stop: nothing to do
  for (int i = 0; i < 10; ++i) r.calls.changeVolume(-1);
  TEST_ASSERT_EQUAL(0, r.calls.volume());
  TEST_ASSERT_FALSE(r.calls.changeVolume(-1));
  TEST_ASSERT_TRUE(r.calls.changeVolume(+100));                          // a big step is clamped, not refused
  TEST_ASSERT_EQUAL(Calls::kMaxVolume, r.calls.volume());
}

void test_a_modem_without_volume_support() {
  Rig r;
  r.modem.setVolumeSupported(false);
  r.modem.injectIncomingCall(kParentOne);
  r.calls.answer();
  TEST_ASSERT_EQUAL(-1, r.calls.volume());
  TEST_ASSERT_FALSE(r.calls.volumeAdjustable());
  TEST_ASSERT_FALSE(r.calls.changeVolume(+1));
  TEST_ASSERT_FALSE(r.calls.setDefaultVolume(5));
}

void test_the_default_volume_is_set() {
  Rig r;
  TEST_ASSERT_TRUE(r.calls.setDefaultVolume(5));
  TEST_ASSERT_EQUAL(5, r.calls.volume());
  TEST_ASSERT_TRUE(r.calls.volumeAdjustable());
}

void test_a_call_is_active_while_dialling_ringing_or_connected() {
  Rig r;
  TEST_ASSERT_FALSE(r.calls.active());
  r.calls.call(kParentOne);
  TEST_ASSERT_TRUE(r.calls.active());                                    // dialling
  r.modem.remoteAnswer();
  TEST_ASSERT_TRUE(r.calls.active());                                    // connected
  r.modem.remoteHangup();
  TEST_ASSERT_FALSE(r.calls.active());
  r.modem.injectIncomingCall(kParentTwo);
  TEST_ASSERT_TRUE(r.calls.active());                                    // ringing
}

// A modem whose call events say nothing about the other party, as some real ones do.
struct AnonymousModem : FakeModem {
  using FakeModem::FakeModem;
  std::deque<CallEvent> scripted;
  bool pollCallEvent(CallEvent& out) override {
    if (scripted.empty()) return false;
    out = scripted.front();
    scripted.pop_front();
    return true;
  }
};

void test_a_failure_without_a_number_in_the_event_names_the_number_dialled() {
  CoreFakes f;
  AnonymousModem anonymous(f.clock);
  anonymous.begin();
  anonymous.setNetwork("Example Mobile", 4);
  Calls calls(f.clock, anonymous, f.power, f.store, f.profile, f.school, f.notices, f.haptic, f.logger, f.drops);
  TEST_ASSERT_TRUE(calls.call(kParentOne));
  CallEvent busy;
  busy.type = CallEventType::Busy;
  anonymous.scripted.push_back(busy);                                   // no phone in the event
  calls.poll();
  TEST_ASSERT_TRUE(f.notices.current().kind == NoticeKind::CallBusy);
  TEST_ASSERT_EQUAL_STRING(kParentOne, f.notices.current().peer.c_str());
  TEST_ASSERT_EQUAL_STRING(kParentOne, f.store.at(0).peer);
}

void test_a_ring_at_uptime_zero_is_timed_from_zero() {
  Rig r(ProfileBuilder().build(), 0);                                                 // the device has just started
  r.modem.injectIncomingCall("");
  r.poll();
  TEST_ASSERT_TRUE(r.modem.state() == ModemState::RingingIn);           // not rejected at once
  r.advance(Calls::kQuietRejectSeconds - 1);
  r.poll();
  TEST_ASSERT_TRUE(r.modem.state() == ModemState::RingingIn);
  r.advance(1);
  r.poll();
  TEST_ASSERT_TRUE(r.modem.state() == ModemState::Ready);               // rejected exactly after three seconds
}

void test_a_ringing_call_without_a_number_does_not_vibrate_even_when_school_time_would_allow_it() {
  Rig r(ProfileBuilder().schoolTime("tue", "14:00", "15:00").quietCallVibration(true).build());
  r.modem.injectIncomingCall("");
  r.poll();
  TEST_ASSERT_EQUAL(0, r.haptic.played().size());                       // nothing rings before the caller is known
}

void test_two_calls_in_a_row_in_school_time_are_both_rejected() {
  Rig r(inSchool());
  for (const char* who : {kParentOne, kParentTwo}) {
    r.modem.injectIncomingCall(who);
    r.poll();
    TEST_ASSERT_TRUE(r.calls.state() == ModemState::Ready);
    TEST_ASSERT_TRUE(r.noticeIs(NoticeKind::CallerInSchoolTime));
    TEST_ASSERT_EQUAL_STRING(who, r.notices.current().peer.c_str());
    r.notices.clear();
    r.poll();
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_only_allowed_numbers_are_dialled);
  RUN_TEST(test_a_number_in_another_spelling_and_a_listed_non_parent_may_be_called);
  RUN_TEST(test_dialling_is_refused_in_school_time);
  RUN_TEST(test_a_switched_off_window_does_not_stop_dialling_or_ringing);
  RUN_TEST(test_dialling_works_outside_school_time);
  RUN_TEST(test_dialling_is_refused_with_cellular_off);
  RUN_TEST(test_dialling_is_refused_without_a_network);
  RUN_TEST(test_the_rules_are_applied_in_this_order);
  RUN_TEST(test_a_refused_dial_leaves_no_call);
  RUN_TEST(test_a_dial_without_ims_is_refused_with_the_reason_and_starts_nothing);
  RUN_TEST(test_a_dial_on_an_sms_only_network_is_refused_with_that_reason);
  RUN_TEST(test_a_dial_failure_that_the_modem_calls_not_ready_says_so);
  RUN_TEST(test_a_dial_failure_without_a_code_while_the_ims_is_missing_says_not_ready);
  RUN_TEST(test_a_modem_restart_while_dialling_is_reported_as_such_and_logged);
  RUN_TEST(test_a_modem_restart_in_a_connected_call_is_an_end_not_a_failure);
  RUN_TEST(test_a_refused_dial_with_a_code_of_refused_says_not_connected);
  RUN_TEST(test_an_incoming_call_names_the_peer);
  RUN_TEST(test_a_missed_call_is_recorded);
  RUN_TEST(test_answer_and_hang_up);
  RUN_TEST(test_ringing_vibrates_until_it_stops);
  RUN_TEST(test_call_vibration_can_be_switched_off);
  RUN_TEST(test_every_call_in_school_time_is_rejected_silently_and_named);
  RUN_TEST(test_a_rejected_call_is_in_the_history_as_missed);
    RUN_TEST(test_call_vibration_in_school_time_is_a_setting);
  RUN_TEST(test_a_call_without_a_number_yet_is_neither_rung_nor_rejected);
  RUN_TEST(test_a_call_without_a_number_is_rejected_after_three_seconds);
  RUN_TEST(test_an_incoming_call_is_rejected_once);
  RUN_TEST(test_a_call_after_school_time_rings_and_is_not_rejected);
  RUN_TEST(test_a_stranger_is_hung_up_at_once_without_a_trace);
  RUN_TEST(test_the_rejection_of_a_stranger_is_one_log_line_with_the_number);
  RUN_TEST(test_a_stranger_is_rejected_in_school_time_too_and_without_a_notice);
  RUN_TEST(test_a_stranger_does_not_vibrate_even_when_school_time_vibration_is_on);
  RUN_TEST(test_a_stranger_who_is_announced_late_is_rejected_when_the_number_arrives);
  RUN_TEST(test_a_hidden_number_is_rejected_after_three_seconds_at_any_time_of_day);
  RUN_TEST(test_a_hidden_number_is_rejected_once_and_leaves_no_missed_call);
  RUN_TEST(test_a_contact_whose_number_arrives_late_rings_from_then_on);
  RUN_TEST(test_the_target_of_a_call_action_may_call);
  RUN_TEST(test_a_stranger_calling_again_is_rejected_again);
  RUN_TEST(test_a_flood_of_calls_from_a_stranger_is_one_line_and_one_summary);
  RUN_TEST(test_a_stranger_does_not_disturb_the_calls_of_contacts);
  RUN_TEST(test_a_call_we_dialled_is_not_hidden_by_the_rule);
  RUN_TEST(test_a_busy_line_is_reported);
  RUN_TEST(test_no_answer_is_reported);
  RUN_TEST(test_a_dial_failure_on_an_sms_only_network_says_so);
  RUN_TEST(test_a_dial_failure_without_a_network_says_so);
  RUN_TEST(test_a_dial_failure_is_still_reported);
  RUN_TEST(test_an_unconnected_end_is_a_failure);
  RUN_TEST(test_the_modems_reason_is_logged);
  RUN_TEST(test_calls_are_logged);
  RUN_TEST(test_the_state_after_a_failed_call_is_clean);
  RUN_TEST(test_a_failure_without_a_number_in_the_event_names_the_number_dialled);
  RUN_TEST(test_a_ring_at_uptime_zero_is_timed_from_zero);
  RUN_TEST(test_a_ringing_call_without_a_number_does_not_vibrate_even_when_school_time_would_allow_it);
  RUN_TEST(test_two_calls_in_a_row_in_school_time_are_both_rejected);
  RUN_TEST(test_volume_only_during_a_call_and_within_range);
  RUN_TEST(test_a_modem_without_volume_support);
  RUN_TEST(test_the_default_volume_is_set);
  RUN_TEST(test_a_call_is_active_while_dialling_ringing_or_connected);
  return UNITY_END();
}

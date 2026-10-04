#pragma once

// The contract of Modem. Rig (hooks return false when the rig cannot create the
// situation, and the test is then ignored):
//   Modem& modem()
//   void advanceSeconds(uint32_t)
//   bool setNetwork(operatorName, bars, voice)    empty name = no network
//   bool setSim(SimStatus)
//   bool injectSms(phone, body, timestamp)
//   bool injectIncomingCall(phone)
//   bool remoteAnswer() / remoteBusy() / remoteNoAnswer() / failDial(reason) / remoteHangup()
//   bool failNextSms(reason)
//   bool setVolumeSupported(bool)
// A fresh rig has a modem that is not begun yet, with a network and a usable SIM.

#include <string>
#include <vector>

#include <hal/Modem.h>

#include "Contract.h"

// Checks a stream of call events against the legal sequences of the Modem header:
//   incoming   Incoming, then Connected and Ended, or Missed
//   outgoing   Connected and Ended, or one of Busy, NoAnswer, DialFailed
//   cancelled dial   Ended without a Connected (duration 0)
// and that nothing follows a final event for the same call. `accept()` returns false
// for an event that is not legal at this point.
class CallSequenceChecker {
public:
  enum class Phase { Idle, Ringing, Connected };

  bool accept(const CallEvent& e) {
    switch (phase_) {
      case Phase::Idle:
        switch (e.type) {
          case CallEventType::Incoming: phase_ = Phase::Ringing; return true;
          case CallEventType::Connected: phase_ = Phase::Connected; return true;
          case CallEventType::Busy:
          case CallEventType::NoAnswer:
          case CallEventType::DialFailed: return true;
          case CallEventType::Ended: return e.duration == 0;  // a dial we cancelled
          case CallEventType::Missed: return false;
        }
        return false;
      case Phase::Ringing:
        if (e.type == CallEventType::Connected) {
          phase_ = Phase::Connected;
          return true;
        }
        if (e.type == CallEventType::Missed) {
          phase_ = Phase::Idle;
          return true;
        }
        return false;
      case Phase::Connected:
        if (e.type == CallEventType::Ended) {
          phase_ = Phase::Idle;
          return true;
        }
        return false;
    }
    return false;
  }
  Phase phase() const { return phase_; }

private:
  Phase phase_ = Phase::Idle;
};

template <class Rig>
struct ModemContract {
  static std::vector<CallEvent> events(Rig& rig) {
    std::vector<CallEvent> out;
    CallEvent e;
    while (rig.modem().pollCallEvent(e)) out.push_back(e);
    return out;
  }
  static void ready(Rig& rig) {
    rig.modem().begin();
    TEST_ASSERT_TRUE(rig.modem().state() == ModemState::Ready);
  }
  static void expect(const std::vector<CallEvent>& got, std::initializer_list<CallEventType> want) {
    TEST_ASSERT_EQUAL(want.size(), got.size());
    size_t i = 0;
    for (CallEventType t : want) TEST_ASSERT_TRUE(got[i++].type == t);
  }

  // ---- state, network, SIM ---------------------------------------------------------

  static void a_modem_that_was_not_started_is_off_and_registered_nowhere() {
    Rig rig;
    TEST_ASSERT_TRUE(rig.modem().state() == ModemState::Off);
    TEST_ASSERT_EQUAL(0, rig.modem().signalBars());
    TEST_ASSERT_EQUAL_STRING("", rig.modem().operatorName().c_str());
  }

  static void with_a_network_and_a_sim_it_becomes_ready_with_signal_and_operator() {
    Rig rig;
    ready(rig);
    const int bars = rig.modem().signalBars();
    TEST_ASSERT_TRUE(bars >= 1 && bars <= 5);
    TEST_ASSERT_TRUE(!rig.modem().operatorName().empty());
  }

  static void starting_it_twice_is_harmless() {
    Rig rig;
    ready(rig);
    rig.modem().begin();
    TEST_ASSERT_TRUE(rig.modem().state() == ModemState::Ready);
  }

  static void without_a_network_it_keeps_registering_with_no_signal_and_no_operator() {
    Rig rig;
    CONTRACT_NEEDS(rig.setNetwork("", 0, true));
    rig.modem().begin();
    TEST_ASSERT_TRUE(rig.modem().state() == ModemState::Registering);
    TEST_ASSERT_EQUAL(0, rig.modem().signalBars());
    TEST_ASSERT_EQUAL_STRING("", rig.modem().operatorName().c_str());
    TEST_ASSERT_FALSE(rig.modem().sendSms("+491701111111", "text"));
  }

  static void a_network_that_appears_later_makes_it_ready() {
    Rig rig;
    CONTRACT_NEEDS(rig.setNetwork("", 0, true));
    rig.modem().begin();
    CONTRACT_NEEDS(rig.setNetwork("Provider", 3, true));
    TEST_ASSERT_TRUE(rig.modem().state() == ModemState::Ready);
  }

  // Not "no network": a SIM with a PIN looks different from a missing network.
  static void a_sim_that_wants_its_pin_is_an_error_and_says_so() {
    Rig rig;
    CONTRACT_NEEDS(rig.setSim(SimStatus::PinRequired));
    rig.modem().begin();
    TEST_ASSERT_TRUE(rig.modem().state() == ModemState::Error);
    TEST_ASSERT_TRUE(rig.modem().simStatus() == SimStatus::PinRequired);
    TEST_ASSERT_FALSE(rig.modem().sendSms("+491701111111", "text"));
    CONTRACT_NEEDS(rig.setSim(SimStatus::Ready));
    TEST_ASSERT_TRUE(rig.modem().state() == ModemState::Ready);
  }

  static void a_missing_sim_is_an_error_too() {
    Rig rig;
    CONTRACT_NEEDS(rig.setSim(SimStatus::Missing));
    rig.modem().begin();
    TEST_ASSERT_TRUE(rig.modem().state() == ModemState::Error);
    TEST_ASSERT_TRUE(rig.modem().simStatus() == SimStatus::Missing);
  }

  // Writing works where calls do not, and the caller can tell.
  static void a_network_for_sms_only_allows_writing_but_no_calls() {
    Rig rig;
    CONTRACT_NEEDS(rig.setNetwork("Provider", 3, false));
    ready(rig);
    TEST_ASSERT_FALSE(rig.modem().voiceAvailable());
    TEST_ASSERT_TRUE(rig.modem().sendSms("+491701111111", "text"));
    TEST_ASSERT_FALSE(rig.modem().dial("+491701111111"));
    TEST_ASSERT_TRUE(rig.modem().state() == ModemState::Ready);
  }

  static void shutting_down_makes_it_off_and_it_can_be_started_again() {
    Rig rig;
    ready(rig);
    rig.modem().shutdown();
    TEST_ASSERT_TRUE(rig.modem().state() == ModemState::Off);
    TEST_ASSERT_EQUAL(0, rig.modem().signalBars());
    TEST_ASSERT_FALSE(rig.modem().sendSms("+491701111111", "text"));
    ready(rig);
  }

  static void sleeping_and_waking_do_not_change_the_state_or_lose_messages() {
    Rig rig;
    ready(rig);
    rig.modem().sleep();
    TEST_ASSERT_TRUE(rig.modem().state() == ModemState::Ready);
    CONTRACT_NEEDS(rig.injectSms("+491701111111", "while asleep", 1790000000u));
    rig.modem().wake();
    TEST_ASSERT_TRUE(rig.modem().state() == ModemState::Ready);
    SmsMessage m;
    TEST_ASSERT_TRUE(rig.modem().pollSms(m));
    TEST_ASSERT_EQUAL_STRING("while asleep", m.body.c_str());
  }

  // ---- sending messages --------------------------------------------------------------

  static void a_message_is_accepted_when_the_modem_is_ready() {
    Rig rig;
    ready(rig);
    TEST_ASSERT_TRUE(rig.modem().sendSms("+491701111111", "Hello from Alex"));
  }

  static void an_empty_number_or_text_or_one_that_is_too_long_is_refused() {
    Rig rig;
    ready(rig);
    TEST_ASSERT_FALSE(rig.modem().sendSms("", "text"));
    TEST_ASSERT_FALSE(rig.modem().sendSms("+491701111111", ""));
    TEST_ASSERT_TRUE(rig.modem().sendSms("+491701111111", std::string(kMaxSmsBytes, 'x')));
    TEST_ASSERT_FALSE(rig.modem().sendSms("+491701111111", std::string(kMaxSmsBytes + 1, 'x')));
  }

  static void a_refused_message_leaves_no_failure_report() {
    Rig rig;
    ready(rig);
    rig.modem().sendSms("", "text");
    SmsFailure f;
    TEST_ASSERT_FALSE(rig.modem().pollSendFailure(f));
  }

  static void a_sent_message_that_goes_through_leaves_no_failure_report() {
    Rig rig;
    ready(rig);
    rig.modem().sendSms("+491701111111", "fine");
    SmsFailure f;
    TEST_ASSERT_FALSE(rig.modem().pollSendFailure(f));
  }

  // The modem accepts the message and later refuses it: exactly one report, with the
  // number and the modem's own reason.
  static void a_failed_sending_is_reported_exactly_once_with_the_reason() {
    Rig rig;
    ready(rig);
    CONTRACT_NEEDS(rig.failNextSms("+CMS ERROR: 304"));
    TEST_ASSERT_TRUE(rig.modem().sendSms("+491702222222", "Emergency"));
    SmsFailure f;
    TEST_ASSERT_TRUE(rig.modem().pollSendFailure(f));
    TEST_ASSERT_EQUAL_STRING("+491702222222", f.phone.c_str());
    TEST_ASSERT_EQUAL_STRING("+CMS ERROR: 304", f.reason.c_str());
    TEST_ASSERT_FALSE(rig.modem().pollSendFailure(f));
    TEST_ASSERT_FALSE(rig.modem().pollSendFailure(f));
  }

  static void only_the_failed_message_is_reported_and_later_ones_are_not() {
    Rig rig;
    ready(rig);
    CONTRACT_NEEDS(rig.failNextSms("+CMS ERROR: 500"));
    rig.modem().sendSms("+491701111111", "one");
    rig.modem().sendSms("+491702222222", "two");
    SmsFailure f;
    TEST_ASSERT_TRUE(rig.modem().pollSendFailure(f));
    TEST_ASSERT_EQUAL_STRING("+491701111111", f.phone.c_str());
    TEST_ASSERT_FALSE(rig.modem().pollSendFailure(f));
  }

  // ---- receiving messages -------------------------------------------------------------

  static void received_messages_arrive_oldest_first_each_once_with_all_fields() {
    Rig rig;
    ready(rig);
    CONTRACT_NEEDS(rig.injectSms("+491701111111", "first", 1790000001u));
    CONTRACT_NEEDS(rig.injectSms("+491702222222", "second", 1790000002u));
    SmsMessage m;
    TEST_ASSERT_TRUE(rig.modem().pollSms(m));
    TEST_ASSERT_EQUAL_STRING("+491701111111", m.phone.c_str());
    TEST_ASSERT_EQUAL_STRING("first", m.body.c_str());
    TEST_ASSERT_EQUAL_UINT32(1790000001u, m.timestamp);
    TEST_ASSERT_TRUE(rig.modem().pollSms(m));
    TEST_ASSERT_EQUAL_STRING("second", m.body.c_str());
    TEST_ASSERT_FALSE(rig.modem().pollSms(m));
  }

  static void nothing_to_poll_gives_false() {
    Rig rig;
    ready(rig);
    SmsMessage m;
    CallEvent e;
    SmsFailure f;
    std::string answer;
    TEST_ASSERT_FALSE(rig.modem().pollSms(m));
    TEST_ASSERT_FALSE(rig.modem().pollCallEvent(e));
    TEST_ASSERT_FALSE(rig.modem().pollSendFailure(f));
    TEST_ASSERT_FALSE(rig.modem().pollQueryResult(answer));
  }

  // ---- calls ---------------------------------------------------------------------------

  static void an_incoming_call_that_is_answered_and_ended() {
    Rig rig;
    ready(rig);
    CONTRACT_NEEDS(rig.injectIncomingCall("+491701111111"));
    TEST_ASSERT_TRUE(rig.modem().state() == ModemState::RingingIn);
    TEST_ASSERT_TRUE(rig.modem().answer());
    TEST_ASSERT_TRUE(rig.modem().state() == ModemState::InCall);
    rig.advanceSeconds(30);
    CONTRACT_NEEDS(rig.remoteHangup());
    TEST_ASSERT_TRUE(rig.modem().state() == ModemState::Ready);
    const auto got = events(rig);
    expect(got, {CallEventType::Incoming, CallEventType::Connected, CallEventType::Ended});
    TEST_ASSERT_EQUAL_STRING("+491701111111", got[0].phone.c_str());
    TEST_ASSERT_TRUE(got[2].duration >= 30);
  }

  static void an_incoming_call_we_refuse_is_missed_and_never_connected() {
    Rig rig;
    ready(rig);
    CONTRACT_NEEDS(rig.injectIncomingCall("+491701111111"));
    TEST_ASSERT_TRUE(rig.modem().hangup());
    TEST_ASSERT_TRUE(rig.modem().state() == ModemState::Ready);
    expect(events(rig), {CallEventType::Incoming, CallEventType::Missed});
  }

  static void an_incoming_call_the_caller_gives_up_is_missed() {
    Rig rig;
    ready(rig);
    CONTRACT_NEEDS(rig.injectIncomingCall("+491701111111"));
    CONTRACT_NEEDS(rig.remoteHangup());
    TEST_ASSERT_TRUE(rig.modem().state() == ModemState::Ready);
    expect(events(rig), {CallEventType::Incoming, CallEventType::Missed});
  }

  static void a_dialled_call_that_is_answered_and_hung_up_by_us() {
    Rig rig;
    ready(rig);
    TEST_ASSERT_TRUE(rig.modem().dial("+491701111111"));
    TEST_ASSERT_TRUE(rig.modem().state() == ModemState::Dialing);
    CONTRACT_NEEDS(rig.remoteAnswer());
    TEST_ASSERT_TRUE(rig.modem().state() == ModemState::InCall);
    rig.advanceSeconds(12);
    TEST_ASSERT_TRUE(rig.modem().hangup());
    TEST_ASSERT_TRUE(rig.modem().state() == ModemState::Ready);
    const auto got = events(rig);
    expect(got, {CallEventType::Connected, CallEventType::Ended});
    TEST_ASSERT_TRUE(got[1].duration >= 12);
  }

  static void a_dialled_call_can_end_busy_unanswered_or_failed_each_with_one_final_event() {
    {
      Rig rig;
      ready(rig);
      rig.modem().dial("+491701111111");
      CONTRACT_NEEDS(rig.remoteBusy());
      TEST_ASSERT_TRUE(rig.modem().state() == ModemState::Ready);
      expect(events(rig), {CallEventType::Busy});
    }
    {
      Rig rig;
      ready(rig);
      rig.modem().dial("+491701111111");
      CONTRACT_NEEDS(rig.remoteNoAnswer());
      TEST_ASSERT_TRUE(rig.modem().state() == ModemState::Ready);
      expect(events(rig), {CallEventType::NoAnswer});
    }
    {
      Rig rig;
      ready(rig);
      rig.modem().dial("+491701111111");
      CONTRACT_NEEDS(rig.failDial("+CME ERROR: 30"));
      TEST_ASSERT_TRUE(rig.modem().state() == ModemState::Ready);
      const auto got = events(rig);
      expect(got, {CallEventType::DialFailed});
      TEST_ASSERT_EQUAL_STRING("+CME ERROR: 30", got[0].reason.c_str());
    }
  }

  static void a_dial_we_cancel_ends_with_ended_and_no_duration() {
    Rig rig;
    ready(rig);
    rig.modem().dial("+491701111111");
    TEST_ASSERT_TRUE(rig.modem().hangup());
    const auto got = events(rig);
    expect(got, {CallEventType::Ended});
    TEST_ASSERT_EQUAL_UINT32(0, got[0].duration);
    TEST_ASSERT_TRUE(rig.modem().state() == ModemState::Ready);
  }

  // However many sides hang up, Ended comes once.
  static void ended_is_delivered_once_whoever_hangs_up() {
    Rig rig;
    ready(rig);
    rig.modem().dial("+491701111111");
    CONTRACT_NEEDS(rig.remoteAnswer());
    TEST_ASSERT_TRUE(rig.modem().hangup());
    TEST_ASSERT_FALSE(rig.modem().hangup());
    rig.remoteHangup();
    expect(events(rig), {CallEventType::Connected, CallEventType::Ended});
  }

  static void call_commands_without_a_matching_call_are_refused() {
    Rig rig;
    ready(rig);
    TEST_ASSERT_FALSE(rig.modem().answer());
    TEST_ASSERT_FALSE(rig.modem().hangup());
    TEST_ASSERT_FALSE(rig.modem().dial(""));
    TEST_ASSERT_TRUE(rig.modem().state() == ModemState::Ready);
    TEST_ASSERT_TRUE(rig.modem().dial("+491701111111"));
    TEST_ASSERT_FALSE(rig.modem().dial("+491702222222"));  // already dialling
    TEST_ASSERT_FALSE(rig.modem().answer());                // nothing is ringing
  }

  static void a_call_cannot_be_dialled_while_the_modem_is_not_ready() {
    Rig rig;
    TEST_ASSERT_FALSE(rig.modem().dial("+491701111111"));  // off
    CONTRACT_NEEDS(rig.setNetwork("", 0, true));
    rig.modem().begin();
    TEST_ASSERT_FALSE(rig.modem().dial("+491701111111"));  // still registering
  }

  static void switching_off_in_a_call_ends_it_with_one_final_event() {
    Rig rig;
    ready(rig);
    rig.modem().dial("+491701111111");
    CONTRACT_NEEDS(rig.remoteAnswer());
    rig.modem().shutdown();
    const auto got = events(rig);
    CallSequenceChecker checker;
    for (const CallEvent& e : got) TEST_ASSERT_TRUE(checker.accept(e));
    TEST_ASSERT_TRUE(checker.phase() == CallSequenceChecker::Phase::Idle);
    TEST_ASSERT_TRUE(got.back().type == CallEventType::Ended);
  }

  // Whatever is done, in whatever order, the events stay legal and the state agrees
  // with them. A seeded sequence of operations, every one tried on every seed.
  static void random_operations_never_produce_an_illegal_event_sequence() {
    uint32_t state = 99;
    auto next = [&state](uint32_t n) {
      state = state * 1664525u + 1013904223u;
      return (state >> 8) % n;
    };
    for (int seed = 0; seed < 150; ++seed) {
      Rig rig;
      ready(rig);
      CallSequenceChecker checker;
      for (int step = 0; step < 40; ++step) {
        switch (next(10)) {
          case 0: rig.injectIncomingCall("+491701111111"); break;
          case 1: rig.modem().dial("+491702222222"); break;
          case 2: rig.modem().answer(); break;
          case 3: rig.modem().hangup(); break;
          case 4: rig.remoteAnswer(); break;
          case 5: rig.remoteBusy(); break;
          case 6: rig.remoteNoAnswer(); break;
          case 7: rig.failDial("+CME ERROR: 30"); break;
          case 8: rig.remoteHangup(); break;
          default: rig.advanceSeconds(next(20)); break;
        }
        CallEvent e;
        while (rig.modem().pollCallEvent(e)) TEST_ASSERT_TRUE(checker.accept(e));
        const ModemState s = rig.modem().state();
        // The state agrees with what the events say.
        TEST_ASSERT_EQUAL(s == ModemState::RingingIn, checker.phase() == CallSequenceChecker::Phase::Ringing);
        TEST_ASSERT_EQUAL(s == ModemState::InCall, checker.phase() == CallSequenceChecker::Phase::Connected);
        if (checker.phase() == CallSequenceChecker::Phase::Idle) {
          TEST_ASSERT_TRUE(s == ModemState::Ready || s == ModemState::Dialing);
        }
      }
    }
  }

  // ---- volume and queries --------------------------------------------------------------

  static void the_call_volume_is_clamped_to_0_to_7() {
    Rig rig;
    ready(rig);
    TEST_ASSERT_TRUE(rig.modem().setCallVolume(3));
    TEST_ASSERT_EQUAL(3, rig.modem().callVolume());
    TEST_ASSERT_TRUE(rig.modem().setCallVolume(99));
    TEST_ASSERT_EQUAL(7, rig.modem().callVolume());
    TEST_ASSERT_TRUE(rig.modem().setCallVolume(-4));
    TEST_ASSERT_EQUAL(0, rig.modem().callVolume());
  }

  static void a_modem_without_volume_control_says_so() {
    Rig rig;
    ready(rig);
    CONTRACT_NEEDS(rig.setVolumeSupported(false));
    TEST_ASSERT_FALSE(rig.modem().setCallVolume(3));
    TEST_ASSERT_EQUAL(-1, rig.modem().callVolume());
  }

  static void a_read_query_gives_its_answer_once() {
    Rig rig;
    ready(rig);
    std::string answer;
    TEST_ASSERT_FALSE(rig.modem().pollQueryResult(answer));
    TEST_ASSERT_TRUE(rig.modem().query("AT+CSCLK?"));
    TEST_ASSERT_TRUE(rig.modem().pollQueryResult(answer));
    TEST_ASSERT_FALSE(answer.empty());
    TEST_ASSERT_FALSE(rig.modem().pollQueryResult(answer));
  }

  // A query must never change anything, so anything but a read form is refused and not sent.
  static void only_read_forms_are_accepted_as_queries() {
    Rig rig;
    ready(rig);
    const char* bad[] = {"", "AT", "AT+", "AT+CFUN=0", "AT+CSCLK=1", "ATD+491701111111;", "AT+CGMR", "at+csclk?",
                         "AT+CSCLK?;AT+CFUN=0", "+CSCLK?", "AT+CMGS=\"+491701111111\"?"};
    for (const char* command : bad) {
      TEST_ASSERT_FALSE_MESSAGE(rig.modem().query(command), command);
      std::string answer;
      TEST_ASSERT_FALSE_MESSAGE(rig.modem().pollQueryResult(answer), command);
    }
    TEST_ASSERT_TRUE(rig.modem().state() == ModemState::Ready);
  }
};

#define RUN_MODEM_CONTRACT(Rig)                                                                                     \
  RUN_TEST(ModemContract<Rig>::a_modem_that_was_not_started_is_off_and_registered_nowhere);                         \
  RUN_TEST(ModemContract<Rig>::with_a_network_and_a_sim_it_becomes_ready_with_signal_and_operator);                 \
  RUN_TEST(ModemContract<Rig>::starting_it_twice_is_harmless);                                                      \
  RUN_TEST(ModemContract<Rig>::without_a_network_it_keeps_registering_with_no_signal_and_no_operator);              \
  RUN_TEST(ModemContract<Rig>::a_network_that_appears_later_makes_it_ready);                                        \
  RUN_TEST(ModemContract<Rig>::a_sim_that_wants_its_pin_is_an_error_and_says_so);                                   \
  RUN_TEST(ModemContract<Rig>::a_missing_sim_is_an_error_too);                                                      \
  RUN_TEST(ModemContract<Rig>::a_network_for_sms_only_allows_writing_but_no_calls);                                 \
  RUN_TEST(ModemContract<Rig>::shutting_down_makes_it_off_and_it_can_be_started_again);                             \
  RUN_TEST(ModemContract<Rig>::sleeping_and_waking_do_not_change_the_state_or_lose_messages);                       \
  RUN_TEST(ModemContract<Rig>::a_message_is_accepted_when_the_modem_is_ready);                                      \
  RUN_TEST(ModemContract<Rig>::an_empty_number_or_text_or_one_that_is_too_long_is_refused);                         \
  RUN_TEST(ModemContract<Rig>::a_refused_message_leaves_no_failure_report);                                         \
  RUN_TEST(ModemContract<Rig>::a_sent_message_that_goes_through_leaves_no_failure_report);                          \
  RUN_TEST(ModemContract<Rig>::a_failed_sending_is_reported_exactly_once_with_the_reason);                          \
  RUN_TEST(ModemContract<Rig>::only_the_failed_message_is_reported_and_later_ones_are_not);                         \
  RUN_TEST(ModemContract<Rig>::received_messages_arrive_oldest_first_each_once_with_all_fields);                    \
  RUN_TEST(ModemContract<Rig>::nothing_to_poll_gives_false);                                                        \
  RUN_TEST(ModemContract<Rig>::an_incoming_call_that_is_answered_and_ended);                                        \
  RUN_TEST(ModemContract<Rig>::an_incoming_call_we_refuse_is_missed_and_never_connected);                           \
  RUN_TEST(ModemContract<Rig>::an_incoming_call_the_caller_gives_up_is_missed);                                     \
  RUN_TEST(ModemContract<Rig>::a_dialled_call_that_is_answered_and_hung_up_by_us);                                  \
  RUN_TEST(ModemContract<Rig>::a_dialled_call_can_end_busy_unanswered_or_failed_each_with_one_final_event);         \
  RUN_TEST(ModemContract<Rig>::a_dial_we_cancel_ends_with_ended_and_no_duration);                                   \
  RUN_TEST(ModemContract<Rig>::ended_is_delivered_once_whoever_hangs_up);                                           \
  RUN_TEST(ModemContract<Rig>::call_commands_without_a_matching_call_are_refused);                                  \
  RUN_TEST(ModemContract<Rig>::a_call_cannot_be_dialled_while_the_modem_is_not_ready);                              \
  RUN_TEST(ModemContract<Rig>::switching_off_in_a_call_ends_it_with_one_final_event);                               \
  RUN_TEST(ModemContract<Rig>::random_operations_never_produce_an_illegal_event_sequence);                          \
  RUN_TEST(ModemContract<Rig>::the_call_volume_is_clamped_to_0_to_7);                                               \
  RUN_TEST(ModemContract<Rig>::a_modem_without_volume_control_says_so);                                             \
  RUN_TEST(ModemContract<Rig>::a_read_query_gives_its_answer_once);                                                 \
  RUN_TEST(ModemContract<Rig>::only_read_forms_are_accepted_as_queries)

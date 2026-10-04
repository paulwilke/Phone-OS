// Host tests for the SOS ping. The button that sends the child's position to the
// emergency contact; what is tested above all is the waiting: a position sent after
// three seconds is off by hundreds of metres and worth nothing.
// allow-non-english: the shipped German and French messages are checked too.

#include <unity.h>

#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include <app/SosPing.h>
#include <i18n/Translator.h>

namespace {

SosPing::Wording english() {
  SosPing::Wording w;
  w.emergency = "{name} sends SOS";
  w.emergencyAnonymous = "SOS, I need help";
  w.pickup = "{name} asks: please pick me up";
  w.pickupAnonymous = "Please pick me up";
  w.location = "{name} is here";
  w.locationAnonymous = "Location of the device";
  w.position = "{head}. Coordinates here:\n{map}\n{coordinates}, accurate to {accuracy} m";
  w.positionNoMap = "{head}. Coordinates here:\n{coordinates}\naccurate to {accuracy} m";
  w.noGps = "{head}.\nNo GPS signal after {seconds} s.";
  return w;
}

struct TestHost : SosPing::Host {
  GnssFix fix;
  bool fixReadable = false;
  bool networkUp = true;
  std::vector<std::string> sentTo, sent;

  bool readFix(GnssFix& out) override {
    if (!fixReadable) return false;
    out = fix;
    return true;
  }
  bool sendText(const char* peer, const std::string& text) override {
    if (!networkUp) return false;
    sentTo.push_back(peer);
    sent.push_back(text);
    return true;
  }

  void setFix(double lat, double lon, float hdop, uint8_t sats = 7) {
    fix.valid = true;
    fix.lat = lat;
    fix.lon = lon;
    fix.hdop = hdop;
    fix.sats = sats;
    fixReadable = true;
  }
};

struct Setup {
  TestHost host;
  SosPing::Config cfg;
  uint32_t now = 1000;
  // The configuration is copied when the ping is made, so a test that wants another
  // one sets cfg before using ping.
  SosPing ping{host, cfg};

  Setup() { ping.setWording(english()); }

  void run(uint32_t seconds) {
    for (uint32_t i = 0; i < seconds; ++i) {
      ++now;
      ping.poll(now);
    }
  }
};

bool has(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }

}  // namespace

void setUp() {}
void tearDown() {}

// ---------------------------------------------------------------------------
// Waiting
// ---------------------------------------------------------------------------

// **The point of it all:** even a good position does not go out at once. The first
// fix of a cold start is usually the worst, and sending it sends the wrong street.
void test_a_good_fix_is_not_sent_immediately() {
  Setup a;
  a.ping.start("+491701111111", a.now);
  a.host.setFix(48.137, 11.575, 1.0f);  // accurate at once
  a.run(5);
  TEST_ASSERT_TRUE(a.ping.active());
  TEST_ASSERT_EQUAL(0u, a.host.sent.size());

  a.run(19);  // 24 s: one short of the minimum
  TEST_ASSERT_TRUE(a.ping.active());
  a.run(1);  // 25 s
  TEST_ASSERT_FALSE(a.ping.active());
  TEST_ASSERT_EQUAL(1u, a.host.sent.size());
}

// Of two positions the more accurate wins, not the newer.
void test_the_best_fix_wins_not_the_last() {
  Setup a;
  a.ping.start("+491701111111", a.now);
  a.host.setFix(48.100, 11.500, 1.2f);  // good
  a.run(3);
  a.host.setFix(48.900, 11.900, 8.0f);  // worse afterwards
  a.run(40);

  TEST_ASSERT_EQUAL(1u, a.host.sent.size());
  TEST_ASSERT_TRUE(has(a.host.sent[0], "48.10000"));
  TEST_ASSERT_FALSE(has(a.host.sent[0], "48.90000"));
}

void test_a_better_fix_replaces_a_worse_one_and_a_tie_keeps_the_first() {
  Setup a;
  a.ping.start("+491701111111", a.now);
  a.host.setFix(48.100, 11.500, 4.0f);
  a.run(2);
  a.host.setFix(48.200, 11.600, 3.0f);  // better
  a.run(2);
  TEST_ASSERT_TRUE(48.200 == a.ping.best().lat);
  a.host.setFix(48.300, 11.700, 3.0f);  // equal: the earlier stays
  a.run(2);
  TEST_ASSERT_TRUE(48.200 == a.ping.best().lat);
}

// A poor position alone triggers nothing; only the time limit does.
void test_a_poor_fix_waits_for_the_timeout() {
  Setup a;
  a.ping.start("+491701111111", a.now);
  a.host.setFix(48.1, 11.5, 9.0f);  // far above goodHdop
  a.run(60);
  TEST_ASSERT_TRUE(a.ping.active());
  TEST_ASSERT_EQUAL(0u, a.host.sent.size());

  a.run(59);  // 119 s
  TEST_ASSERT_TRUE(a.ping.active());
  a.run(1);   // 120 s
  TEST_ASSERT_FALSE(a.ping.active());
  TEST_ASSERT_EQUAL(1u, a.host.sent.size());
  // The message says how inaccurate it is: HDOP 9.0 is about 45 metres.
  TEST_ASSERT_TRUE(has(a.host.sent[0], "accurate to 45 m"));
}

void test_a_fix_exactly_at_the_good_limit_is_not_good() {
  Setup a;
  a.ping.start("+491701111111", a.now);
  a.host.setFix(48.1, 11.5, 2.5f);  // goodHdop is exclusive
  a.run(100);
  TEST_ASSERT_TRUE(a.ping.active());
}

// If the minimum is longer than the timeout, the timeout wins: the child must not wait longer than the limit.
void test_the_timeout_wins_over_the_minimum() {
  Setup a;
  a.cfg.minSeconds = 300;
  SosPing ping(a.host, a.cfg);
  ping.setWording(english());
  ping.start("+491701111111", a.now);
  a.host.setFix(48.1, 11.5, 1.0f);
  for (uint32_t i = 0; i < 120; ++i) ping.poll(++a.now);
  TEST_ASSERT_FALSE(ping.active());
  TEST_ASSERT_EQUAL(1u, a.host.sent.size());
}

// A position that is valid in name but holds nonsense must not become "the best":
// comparisons with NaN are false, so it would never be replaced.
void test_nonsense_fixes_are_ignored() {
  Setup a;
  a.ping.start("+491701111111", a.now);
  const double nan = std::numeric_limits<double>::quiet_NaN();
  a.host.setFix(48.1, 11.5, std::numeric_limits<float>::quiet_NaN());
  a.run(2);
  TEST_ASSERT_FALSE(a.ping.best().valid);
  a.host.setFix(nan, 11.5, 1.0f);
  a.run(2);
  TEST_ASSERT_FALSE(a.ping.best().valid);
  a.host.setFix(95.0, 11.5, 1.0f);
  a.run(2);
  TEST_ASSERT_FALSE(a.ping.best().valid);
  a.host.setFix(48.1, 200.0, 1.0f);
  a.run(2);
  TEST_ASSERT_FALSE(a.ping.best().valid);
  a.host.setFix(48.1, 11.5, -1.0f);
  a.run(2);
  TEST_ASSERT_FALSE(a.ping.best().valid);
  TEST_ASSERT_EQUAL((int)SosPing::Stage::Search, (int)a.ping.stage());

  a.host.setFix(48.5, 11.5, 3.0f);  // a sound one is taken
  a.run(1);
  TEST_ASSERT_TRUE(48.5 == a.ping.best().lat);
  a.host.fix.valid = false;  // the receiver loses its fix: the best so far stays
  a.run(1);
  TEST_ASSERT_TRUE(a.ping.best().valid);
}

// The uptime clock does not run backwards, but if the caller's does, nothing may be sent at once.
void test_a_clock_that_goes_back_does_not_send_early() {
  Setup a;
  a.ping.start("+491701111111", 5000);
  a.host.setFix(48.1, 11.5, 1.0f);
  a.ping.poll(4000);
  TEST_ASSERT_TRUE(a.ping.active());
  TEST_ASSERT_EQUAL(0u, a.ping.elapsed(4000));
}

// ---------------------------------------------------------------------------
// Without any position
// ---------------------------------------------------------------------------

// A message goes out even with no reception: the press itself is the message.
void test_without_any_fix_a_message_still_goes_out() {
  Setup a;
  a.ping.start("+491701111111", a.now);
  a.run(119);
  TEST_ASSERT_EQUAL(0u, a.host.sent.size());
  a.run(1);
  TEST_ASSERT_EQUAL(1u, a.host.sent.size());
  TEST_ASSERT_EQUAL_STRING("SOS, I need help.\nNo GPS signal after 120 s.", a.host.sent[0].c_str());
  TEST_ASSERT_FALSE(a.ping.best().valid);
}

// ---------------------------------------------------------------------------
// The text
// ---------------------------------------------------------------------------

// Name first, then the map address from the template, then the plain coordinates.
// The numbers are always there: an address that a phone cannot open must not make
// the whole message worthless (measured: a geo: address arrived but did not open).
void test_the_message_carries_name_link_and_plain_coordinates() {
  Setup a;
  a.ping.setWho("Alex");
  a.ping.setMapTemplate("https://maps.google.com/?q={lat},{lon}");
  a.ping.start("+491701111111", a.now);
  a.host.setFix(48.137452, 11.575500, 1.0f);
  a.run(30);
  TEST_ASSERT_EQUAL_STRING(
      "Alex sends SOS. Coordinates here:\nhttps://maps.google.com/?q=48.13745,11.57550\n"
      "48.13745,11.57550, accurate to 5 m",
      a.host.sent[0].c_str());
  TEST_ASSERT_EQUAL_STRING("+491701111111", a.host.sentTo[0].c_str());
}

// Whoever wants another map enters a template, and whoever wants none an empty one;
// the coordinates remain.
void test_the_map_address_comes_from_the_template() {
  Setup a;
  a.ping.setWho("Alex");
  a.ping.setMapTemplate("geo:{lat},{lon}");
  a.ping.start("+491701111111", a.now);
  a.host.setFix(48.137452, 11.575500, 1.0f);
  a.run(30);
  TEST_ASSERT_TRUE(has(a.host.sent[0], "geo:48.13745,11.57550"));

  Setup b;
  b.ping.setWho("Alex");
  b.ping.setMapTemplate("");
  b.ping.start("+491701111111", b.now);
  b.host.setFix(48.137452, 11.575500, 1.0f);
  b.run(30);
  TEST_ASSERT_EQUAL_STRING("Alex sends SOS. Coordinates here:\n48.13745,11.57550\naccurate to 5 m",
                           b.host.sent[0].c_str());
}

void test_without_a_name_the_message_still_works() {
  Setup a;
  a.ping.start("+491701111111", a.now);
  a.host.setFix(48.1, 11.5, 1.0f);
  a.run(30);
  TEST_ASSERT_TRUE(has(a.host.sent[0], "SOS, I need help."));
  TEST_ASSERT_FALSE(has(a.host.sent[0], "sends SOS"));
  TEST_ASSERT_FALSE(has(a.host.sent[0], "{"));
}

void test_the_three_kinds_use_their_own_wording() {
  struct Case { SosPing::Kind kind; const char* named; const char* anonymous; };
  const Case cases[] = {
      {SosPing::Kind::Emergency, "Alex sends SOS.", "SOS, I need help."},
      {SosPing::Kind::Pickup, "Alex asks: please pick me up.", "Please pick me up."},
      {SosPing::Kind::Query, "Alex is here.", "Location of the device."},
  };
  for (const Case& c : cases) {
    Setup a;
    a.ping.setWho("Alex");
    a.ping.start("+491701111111", a.now, c.kind);
    a.run(120);
    TEST_ASSERT_TRUE_MESSAGE(has(a.host.sent[0], c.named), c.named);
    Setup b;
    b.ping.start("+491701111111", b.now, c.kind);
    b.run(120);
    TEST_ASSERT_TRUE_MESSAGE(has(b.host.sent[0], c.anonymous), c.anonymous);
  }
}

// The reply to a parent's question is no alarm: it must not say SOS.
void test_a_query_reply_is_not_an_alarm() {
  Setup a;
  a.ping.setWho("Alex");
  a.ping.start("+491701111111", a.now, SosPing::Kind::Query);
  a.host.setFix(48.1, 11.5, 1.0f);
  a.run(30);
  TEST_ASSERT_FALSE(has(a.host.sent[0], "SOS"));
  TEST_ASSERT_TRUE(a.ping.isQuery());
}

void test_accuracy_is_derived_from_hdop() {
  TEST_ASSERT_EQUAL_UINT(5, SosPing::accuracyMeters(1.0f));
  TEST_ASSERT_EQUAL_UINT(13, SosPing::accuracyMeters(2.5f));
  TEST_ASSERT_EQUAL_UINT(1, SosPing::accuracyMeters(0.0f));
  TEST_ASSERT_EQUAL_UINT(9999, SosPing::accuracyMeters(1e9f));
  TEST_ASSERT_EQUAL_UINT(1, SosPing::accuracyMeters(std::numeric_limits<float>::quiet_NaN()));
}

// The wording is an input: nothing to say means no ping, never a blank message.
void test_an_incomplete_wording_refuses_to_start() {
  TestHost host;
  SosPing::Config cfg;
  SosPing ping(host, cfg);
  TEST_ASSERT_FALSE(ping.start("+491701111111", 0));  // none set
  SosPing::Wording w = english();
  w.noGps.clear();
  ping.setWording(w);
  TEST_ASSERT_FALSE(ping.start("+491701111111", 0));
  ping.setWording(english());
  TEST_ASSERT_TRUE(ping.start("+491701111111", 0));
}

// ---------------------------------------------------------------------------
// Steps, cancelling, failing
// ---------------------------------------------------------------------------

void test_three_steps_in_order() {
  Setup a;
  TEST_ASSERT_EQUAL(0, a.ping.progress());
  a.ping.start("+491701111111", a.now);
  a.run(2);
  TEST_ASSERT_EQUAL(1, a.ping.progress());  // searching
  a.host.setFix(48.1, 11.5, 1.0f);
  a.run(2);
  TEST_ASSERT_EQUAL(2, a.ping.progress());  // position found, getting better
  a.run(30);
  TEST_ASSERT_EQUAL(3, a.ping.progress());  // sent
  TEST_ASSERT_EQUAL((int)SosPing::Stage::Done, (int)a.ping.stage());
}

void test_elapsed_counts_while_active_only() {
  Setup a;
  TEST_ASSERT_EQUAL(0u, a.ping.elapsed(a.now));
  a.ping.start("+491701111111", a.now);
  a.run(7);
  TEST_ASSERT_EQUAL(7u, a.ping.elapsed(a.now));
  a.run(120);
  TEST_ASSERT_EQUAL(0u, a.ping.elapsed(a.now));
}

// Cancelling means nothing goes out.
void test_cancel_sends_nothing() {
  Setup a;
  a.ping.start("+491701111111", a.now);
  a.host.setFix(48.1, 11.5, 1.0f);
  a.run(5);
  a.ping.cancel();
  a.run(200);
  TEST_ASSERT_FALSE(a.ping.active());
  TEST_ASSERT_EQUAL((int)SosPing::Stage::Idle, (int)a.ping.stage());
  TEST_ASSERT_EQUAL(0u, a.host.sent.size());
}

void test_cancel_does_not_undo_a_sent_ping() {
  Setup a;
  a.ping.start("+491701111111", a.now);
  a.run(120);
  a.ping.cancel();
  TEST_ASSERT_EQUAL((int)SosPing::Stage::Done, (int)a.ping.stage());
}

// When nothing goes out, the screen must be able to say so.
void test_failure_is_visible() {
  Setup a;
  a.host.networkUp = false;
  a.ping.start("+491701111111", a.now);
  a.run(120);
  TEST_ASSERT_EQUAL((int)SosPing::Stage::Failed, (int)a.ping.stage());
  TEST_ASSERT_EQUAL(3, a.ping.progress());
}

// The modem may refuse the message after it was handed over.
void test_a_rejection_after_sending_turns_done_into_failed() {
  Setup a;
  a.ping.start("+491701111111", a.now);
  a.run(120);
  TEST_ASSERT_EQUAL((int)SosPing::Stage::Done, (int)a.ping.stage());
  a.ping.noteSendFailed("+491709999999");  // another recipient's message: not ours
  TEST_ASSERT_EQUAL((int)SosPing::Stage::Done, (int)a.ping.stage());
  a.ping.noteSendFailed(nullptr);
  TEST_ASSERT_EQUAL((int)SosPing::Stage::Done, (int)a.ping.stage());
  a.ping.noteSendFailed("+491701111111");
  TEST_ASSERT_EQUAL((int)SosPing::Stage::Failed, (int)a.ping.stage());
}

void test_a_rejection_while_searching_is_not_ours() {
  Setup a;
  a.ping.start("+491701111111", a.now);
  a.ping.noteSendFailed("+491701111111");
  TEST_ASSERT_EQUAL((int)SosPing::Stage::Search, (int)a.ping.stage());
}

// ---------------------------------------------------------------------------
// Starting
// ---------------------------------------------------------------------------

void test_without_a_recipient_nothing_starts() {
  Setup a;
  TEST_ASSERT_FALSE(a.ping.start("", a.now));
  TEST_ASSERT_FALSE(a.ping.start(nullptr, a.now));
  TEST_ASSERT_FALSE(a.ping.active());
}

void test_a_second_press_does_not_restart() {
  Setup a;
  TEST_ASSERT_TRUE(a.ping.start("+491701111111", a.now));
  a.run(3);
  TEST_ASSERT_FALSE(a.ping.start("+491702222222", a.now));
  TEST_ASSERT_FALSE(a.ping.start("+491701111111", a.now, SosPing::Kind::Pickup));
  TEST_ASSERT_EQUAL_STRING("+491701111111", a.ping.peer().c_str());
  TEST_ASSERT_TRUE(a.ping.kind() == SosPing::Kind::Emergency);
}

// A parent's location query that is still searching gives way to the child's call
// for help; without this the emergency would be lost whenever a parent had just asked.
void test_an_emergency_replaces_a_running_query_reply() {
  Setup a;
  TEST_ASSERT_TRUE(a.ping.start("+491702222222", a.now, SosPing::Kind::Query));
  a.host.setFix(48.1, 11.5, 1.0f);
  a.run(10);
  TEST_ASSERT_TRUE(a.ping.start("+491701111111", a.now, SosPing::Kind::Emergency));
  TEST_ASSERT_TRUE(a.ping.kind() == SosPing::Kind::Emergency);
  TEST_ASSERT_EQUAL_STRING("+491701111111", a.ping.peer().c_str());
  TEST_ASSERT_FALSE(a.ping.best().valid);  // starts afresh
  a.run(30);
  TEST_ASSERT_EQUAL(1u, a.host.sent.size());  // only the emergency went out
  TEST_ASSERT_EQUAL_STRING("+491701111111", a.host.sentTo[0].c_str());
  TEST_ASSERT_TRUE(has(a.host.sent[0], "SOS"));
}

void test_a_pickup_also_replaces_a_query_but_not_the_other_way_round() {
  Setup a;
  a.ping.start("+491702222222", a.now, SosPing::Kind::Query);
  TEST_ASSERT_TRUE(a.ping.start("+491701111111", a.now, SosPing::Kind::Pickup));
  TEST_ASSERT_FALSE(a.ping.start("+491702222222", a.now, SosPing::Kind::Query));
  TEST_ASSERT_TRUE(a.ping.kind() == SosPing::Kind::Pickup);

  Setup b;
  b.ping.start("+491702222222", b.now, SosPing::Kind::Query);
  TEST_ASSERT_FALSE(b.ping.start("+491702222222", b.now, SosPing::Kind::Query));  // a second query: no
}

void test_a_new_ping_can_follow_a_finished_one() {
  Setup a;
  a.ping.start("+491701111111", a.now);
  a.run(120);
  TEST_ASSERT_EQUAL((int)SosPing::Stage::Done, (int)a.ping.stage());
  TEST_ASSERT_TRUE(a.ping.start("+491702222222", a.now, SosPing::Kind::Pickup));
  TEST_ASSERT_EQUAL(1, a.ping.progress());
  TEST_ASSERT_EQUAL_STRING("", a.ping.text().c_str());
  a.run(120);
  TEST_ASSERT_EQUAL(2u, a.host.sent.size());
}

// ---------------------------------------------------------------------------
// The shipped catalogs
// ---------------------------------------------------------------------------

namespace {

std::string readFile(const std::string& name) {
  std::ifstream in("lang/" + name);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

SosPing::Wording wordingOf(i18n::Translator& t, const std::string& language) {
  TEST_ASSERT_TRUE(t.select(language));
  SosPing::Wording w;
  w.emergency = t.text("sms.sos.emergency");
  w.emergencyAnonymous = t.text("sms.sos.emergency-anonymous");
  w.pickup = t.text("sms.sos.pickup");
  w.pickupAnonymous = t.text("sms.sos.pickup-anonymous");
  w.location = t.text("sms.sos.location");
  w.locationAnonymous = t.text("sms.sos.location-anonymous");
  w.position = t.text("sms.sos.position");
  w.positionNoMap = t.text("sms.sos.position-no-map");
  w.noGps = t.text("sms.sos.no-gps");
  return w;
}

}  // namespace

// Every shipped language fills every template completely, and the longest message
// still fits in one SMS (160 characters of the GSM alphabet, which check_i18n.py
// enforces for these keys).
void test_shipped_languages_build_complete_messages_that_fit_one_sms() {
  i18n::Translator t;
  for (const char* file : {"en.toml", "de.toml", "fr.toml"}) {
    i18n::Catalog c;
    const std::string text = readFile(file);
    TEST_ASSERT_TRUE_MESSAGE(!text.empty() && c.parse(text.data(), text.size()), file);
    t.add(c);
  }
  GnssFix fix;
  fix.valid = true;
  fix.lat = -48.13745;  // the widest rendering: a minus sign in both numbers
  fix.lon = -111.57550;
  fix.hdop = 2000.0f;   // 9999 m, the widest accuracy
  const std::string longName(23, 'W');

  for (const char* language : {"en", "de", "fr"}) {
    const SosPing::Wording w = wordingOf(t, language);
    TEST_ASSERT_TRUE_MESSAGE(w.complete(), language);
    for (SosPing::Kind kind : {SosPing::Kind::Emergency, SosPing::Kind::Pickup, SosPing::Kind::Query}) {
      for (const std::string& who : {std::string(), longName}) {
        for (int shape = 0; shape < 3; ++shape) {
          const std::string map = shape == 1 ? "" : "https://maps.google.com/?q={lat},{lon}";
          const std::string text = SosPing::buildText(w, who, map, fix, shape != 2, 120, kind);
          const std::string label = std::string(language) + ": " + text;
          TEST_ASSERT_TRUE_MESSAGE(!text.empty(), label.c_str());
          TEST_ASSERT_TRUE_MESSAGE(text.find('{') == std::string::npos, label.c_str());  // no unfilled placeholder
          TEST_ASSERT_TRUE_MESSAGE(text.size() <= 160, label.c_str());
          if (!who.empty()) TEST_ASSERT_TRUE_MESSAGE(text.find(who) != std::string::npos, label.c_str());
        }
      }
    }
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_a_good_fix_is_not_sent_immediately);
  RUN_TEST(test_the_best_fix_wins_not_the_last);
  RUN_TEST(test_a_better_fix_replaces_a_worse_one_and_a_tie_keeps_the_first);
  RUN_TEST(test_a_poor_fix_waits_for_the_timeout);
  RUN_TEST(test_a_fix_exactly_at_the_good_limit_is_not_good);
  RUN_TEST(test_the_timeout_wins_over_the_minimum);
  RUN_TEST(test_nonsense_fixes_are_ignored);
  RUN_TEST(test_a_clock_that_goes_back_does_not_send_early);
  RUN_TEST(test_without_any_fix_a_message_still_goes_out);
  RUN_TEST(test_the_message_carries_name_link_and_plain_coordinates);
  RUN_TEST(test_the_map_address_comes_from_the_template);
  RUN_TEST(test_without_a_name_the_message_still_works);
  RUN_TEST(test_the_three_kinds_use_their_own_wording);
  RUN_TEST(test_a_query_reply_is_not_an_alarm);
  RUN_TEST(test_accuracy_is_derived_from_hdop);
  RUN_TEST(test_an_incomplete_wording_refuses_to_start);
  RUN_TEST(test_three_steps_in_order);
  RUN_TEST(test_elapsed_counts_while_active_only);
  RUN_TEST(test_cancel_sends_nothing);
  RUN_TEST(test_cancel_does_not_undo_a_sent_ping);
  RUN_TEST(test_failure_is_visible);
  RUN_TEST(test_a_rejection_after_sending_turns_done_into_failed);
  RUN_TEST(test_a_rejection_while_searching_is_not_ours);
  RUN_TEST(test_without_a_recipient_nothing_starts);
  RUN_TEST(test_a_second_press_does_not_restart);
  RUN_TEST(test_an_emergency_replaces_a_running_query_reply);
  RUN_TEST(test_a_pickup_also_replaces_a_query_but_not_the_other_way_round);
  RUN_TEST(test_a_new_ping_can_follow_a_finished_one);
  RUN_TEST(test_shipped_languages_build_complete_messages_that_fit_one_sms);
  return UNITY_END();
}

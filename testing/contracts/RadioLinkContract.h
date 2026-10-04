#pragma once

// The contract of RadioLink. Rig: `RadioLink& link()` and
//   bool setUp(bool)                         the radio runs or not
//   bool addPeer(name, direct, lastSeen)     a contact appears
//   bool deliver(peer, text, timestamp)      a message arrives
//   bool confirm(bool arrived)               the radio answers for the oldest open message

#include <string>

#include <hal/RadioLink.h>

#include "Contract.h"

template <class Rig>
struct RadioLinkContract {
  static void ready_rig(Rig& rig) {
    CONTRACT_NEEDS(rig.setUp(true));
    CONTRACT_NEEDS(rig.addPeer("Node One", true, 1790000000u));
    CONTRACT_NEEDS(rig.addPeer("Node Two", false, 1790000000u));
  }

  static void a_radio_that_is_down_is_not_ready_and_sends_nothing() {
    Rig rig;
    CONTRACT_NEEDS(rig.setUp(false));
    TEST_ASSERT_FALSE(rig.link().ready());
    TEST_ASSERT_FALSE(rig.link().send("Node One", "hello"));
    TEST_ASSERT_EQUAL(0, rig.link().peerCount());
    TEST_ASSERT_EQUAL(0, rig.link().lastRssi());
  }

  static void a_radio_without_a_contact_is_not_ready() {
    Rig rig;
    CONTRACT_NEEDS(rig.setUp(true));
    TEST_ASSERT_FALSE(rig.link().ready());
    TEST_ASSERT_FALSE(rig.link().send("", "hello"));
  }

  static void with_a_contact_it_is_ready_and_has_a_default_contact() {
    Rig rig;
    ready_rig(rig);
    TEST_ASSERT_TRUE(rig.link().ready());
    TEST_ASSERT_TRUE(!rig.link().defaultPeer().empty());
  }

  static void an_empty_contact_means_the_default_contact() {
    Rig rig;
    ready_rig(rig);
    TEST_ASSERT_TRUE(rig.link().send("", "hello"));
    TEST_ASSERT_TRUE(rig.confirm(true));
    RadioDelivery d;
    TEST_ASSERT_TRUE(rig.link().pollDelivery(d));
    TEST_ASSERT_EQUAL_STRING(rig.link().defaultPeer().c_str(), d.peer.c_str());
  }

  static void an_unknown_contact_and_an_empty_text_are_refused() {
    Rig rig;
    ready_rig(rig);
    TEST_ASSERT_FALSE(rig.link().send("Nobody", "hello"));
    TEST_ASSERT_FALSE(rig.link().send("Node One", ""));
  }

  // A refused message gets no delivery report: nothing is queued for one.
  static void a_refused_message_leaves_nothing_open_for_a_delivery_report() {
    Rig rig;
    ready_rig(rig);
    rig.link().send("Nobody", "hello");
    rig.link().send("Node One", "");
    TEST_ASSERT_FALSE(rig.confirm(true));
    RadioDelivery d;
    TEST_ASSERT_FALSE(rig.link().pollDelivery(d));
  }

  static void there_is_no_delivery_report_before_the_radio_answers() {
    Rig rig;
    ready_rig(rig);
    rig.link().send("Node One", "hello");
    RadioDelivery d;
    TEST_ASSERT_FALSE(rig.link().pollDelivery(d));
  }

  // One report per sent message, in the order of sending, each once.
  static void delivery_reports_come_one_per_message_in_the_order_of_sending() {
    Rig rig;
    ready_rig(rig);
    rig.link().send("Node One", "a");
    rig.link().send("Node Two", "b");
    rig.link().send("Node Two", "c");
    TEST_ASSERT_TRUE(rig.confirm(true));
    TEST_ASSERT_TRUE(rig.confirm(false));
    TEST_ASSERT_TRUE(rig.confirm(true));
    TEST_ASSERT_FALSE(rig.confirm(true));  // nothing left open
    RadioDelivery d;
    TEST_ASSERT_TRUE(rig.link().pollDelivery(d));
    TEST_ASSERT_EQUAL_STRING("Node One", d.peer.c_str());
    TEST_ASSERT_TRUE(d.confirmed);
    TEST_ASSERT_TRUE(rig.link().pollDelivery(d));
    TEST_ASSERT_EQUAL_STRING("Node Two", d.peer.c_str());
    TEST_ASSERT_FALSE(d.confirmed);  // not lost, only not confirmed
    TEST_ASSERT_TRUE(rig.link().pollDelivery(d));
    TEST_ASSERT_EQUAL_STRING("Node Two", d.peer.c_str());
    TEST_ASSERT_TRUE(d.confirmed);
    TEST_ASSERT_FALSE(rig.link().pollDelivery(d));
  }

  static void received_messages_arrive_oldest_first_each_once_with_all_fields() {
    Rig rig;
    ready_rig(rig);
    CONTRACT_NEEDS(rig.deliver("Node One", "first", 1790000001u));
    CONTRACT_NEEDS(rig.deliver("Node Two", "second", 1790000002u));
    RadioMessage m;
    TEST_ASSERT_TRUE(rig.link().poll(m));
    TEST_ASSERT_EQUAL_STRING("Node One", m.peer.c_str());
    TEST_ASSERT_EQUAL_STRING("first", m.text.c_str());
    TEST_ASSERT_EQUAL_UINT32(1790000001u, m.timestamp);
    TEST_ASSERT_TRUE(rig.link().poll(m));
    TEST_ASSERT_EQUAL_STRING("second", m.text.c_str());
    TEST_ASSERT_FALSE(rig.link().poll(m));
  }

  static void the_contact_list_matches_the_count_and_ends_at_it() {
    Rig rig;
    ready_rig(rig);
    TEST_ASSERT_EQUAL(2, rig.link().peerCount());
    RadioPeer p;
    TEST_ASSERT_TRUE(rig.link().peerAt(0, p));
    TEST_ASSERT_EQUAL_STRING("Node One", p.name.c_str());
    TEST_ASSERT_TRUE(p.direct);
    TEST_ASSERT_TRUE(rig.link().peerAt(1, p));
    TEST_ASSERT_FALSE(p.direct);
    TEST_ASSERT_FALSE(rig.link().peerAt(2, p));
  }

  // An advertisement from a known device updates the contact; it does not add one.
  static void a_known_contact_seen_again_is_not_listed_twice() {
    Rig rig;
    ready_rig(rig);
    rig.addPeer("Node One", false, 1790000500u);
    TEST_ASSERT_EQUAL(2, rig.link().peerCount());
    RadioPeer p;
    TEST_ASSERT_TRUE(rig.link().peerAt(0, p));
    TEST_ASSERT_EQUAL_UINT32(1790000500u, p.lastSeen);
  }
};

#define RUN_RADIO_LINK_CONTRACT(Rig)                                                                   \
  RUN_TEST(RadioLinkContract<Rig>::a_radio_that_is_down_is_not_ready_and_sends_nothing);                \
  RUN_TEST(RadioLinkContract<Rig>::a_radio_without_a_contact_is_not_ready);                             \
  RUN_TEST(RadioLinkContract<Rig>::with_a_contact_it_is_ready_and_has_a_default_contact);               \
  RUN_TEST(RadioLinkContract<Rig>::an_empty_contact_means_the_default_contact);                         \
  RUN_TEST(RadioLinkContract<Rig>::an_unknown_contact_and_an_empty_text_are_refused);                   \
  RUN_TEST(RadioLinkContract<Rig>::a_refused_message_leaves_nothing_open_for_a_delivery_report);        \
  RUN_TEST(RadioLinkContract<Rig>::there_is_no_delivery_report_before_the_radio_answers);               \
  RUN_TEST(RadioLinkContract<Rig>::delivery_reports_come_one_per_message_in_the_order_of_sending);      \
  RUN_TEST(RadioLinkContract<Rig>::received_messages_arrive_oldest_first_each_once_with_all_fields);    \
  RUN_TEST(RadioLinkContract<Rig>::the_contact_list_matches_the_count_and_ends_at_it);                  \
  RUN_TEST(RadioLinkContract<Rig>::a_known_contact_seen_again_is_not_listed_twice)

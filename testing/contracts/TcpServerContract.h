#pragma once

// The contract of TcpServer and TcpConnection. Rig: `TcpServer& server()` and
//   std::unique_ptr<TcpPeer> connect()   a client connects to the listening server;
//                                        null if the server refuses the connection
// A TcpPeer is the client side: it sends bytes, goes away, and sees what the software wrote.

#include <cstring>
#include <memory>
#include <string>

#include <hal/TcpServer.h>

#include "Contract.h"

class TcpPeer {
public:
  virtual ~TcpPeer() = default;
  virtual void send(const std::string& bytes) = 0;
  virtual void close() = 0;
  virtual std::string received() = 0;
};

template <class Rig>
struct TcpServerContract {
  static std::string readAll(TcpConnection& c) {
    std::string out;
    uint8_t buf[16];
    int n;
    while ((n = c.read(buf, sizeof(buf))) > 0) out.append((const char*)buf, (size_t)n);
    return out;
  }

  static void it_accepts_nothing_before_it_listens() {
    Rig rig;
    TEST_ASSERT_NULL(rig.server().accept());
  }

  static void port_zero_cannot_be_opened() {
    Rig rig;
    TEST_ASSERT_FALSE(rig.server().begin(0));
    TEST_ASSERT_NULL(rig.server().accept());
  }

  static void a_listening_server_with_no_client_accepts_nothing() {
    Rig rig;
    TEST_ASSERT_TRUE(rig.server().begin(8080));
    TEST_ASSERT_NULL(rig.server().accept());
  }

  static void a_client_is_accepted_and_nothing_is_there_to_read_yet() {
    Rig rig;
    rig.server().begin(8080);
    auto peer = rig.connect();
    CONTRACT_NEEDS(peer != nullptr);
    TcpConnection* c = rig.server().accept();
    TEST_ASSERT_NOT_NULL(c);
    uint8_t buf[8];
    TEST_ASSERT_EQUAL(0, c->read(buf, sizeof(buf)));  // not -1: the client is still there
  }

  static void what_the_client_sends_is_read_in_pieces_and_in_order() {
    Rig rig;
    rig.server().begin(8080);
    auto peer = rig.connect();
    CONTRACT_NEEDS(peer != nullptr);
    peer->send("GET / HTTP/1.1\r\nHost: phone\r\n\r\n");
    TcpConnection* c = rig.server().accept();
    TEST_ASSERT_NOT_NULL(c);
    uint8_t small[5];
    TEST_ASSERT_EQUAL(5, c->read(small, sizeof(small)));
    TEST_ASSERT_EQUAL_MEMORY("GET /", small, 5);
    TEST_ASSERT_EQUAL_STRING(" HTTP/1.1\r\nHost: phone\r\n\r\n", readAll(*c).c_str());
  }

  static void what_the_software_writes_reaches_the_client() {
    Rig rig;
    rig.server().begin(8080);
    auto peer = rig.connect();
    CONTRACT_NEEDS(peer != nullptr);
    TcpConnection* c = rig.server().accept();
    TEST_ASSERT_NOT_NULL(c);
    const char* reply = "HTTP/1.1 200 OK\r\n\r\n";
    const size_t taken = c->write((const uint8_t*)reply, strlen(reply));
    TEST_ASSERT_TRUE(taken > 0 && taken <= strlen(reply));
    TEST_ASSERT_EQUAL_STRING(std::string(reply).substr(0, taken).c_str(), peer->received().c_str());
    TEST_ASSERT_EQUAL(0, c->write((const uint8_t*)reply, 0));
  }

  // The client sends and leaves: its bytes are still read, then -1.
  static void data_sent_before_the_client_left_is_read_before_the_end_is_reported() {
    Rig rig;
    rig.server().begin(8080);
    auto peer = rig.connect();
    CONTRACT_NEEDS(peer != nullptr);
    peer->send("bye");
    peer->close();
    TcpConnection* c = rig.server().accept();
    TEST_ASSERT_NOT_NULL(c);
    uint8_t buf[8];
    TEST_ASSERT_EQUAL(3, c->read(buf, sizeof(buf)));
    TEST_ASSERT_EQUAL(-1, c->read(buf, sizeof(buf)));
    TEST_ASSERT_EQUAL(-1, c->read(buf, sizeof(buf)));
  }

  static void a_client_that_left_cannot_be_written_to() {
    Rig rig;
    rig.server().begin(8080);
    auto peer = rig.connect();
    CONTRACT_NEEDS(peer != nullptr);
    TcpConnection* c = rig.server().accept();
    peer->close();
    TEST_ASSERT_EQUAL(0, c->write((const uint8_t*)"x", 1));
  }

  static void after_close_the_connection_reads_minus_one_and_writes_nothing() {
    Rig rig;
    rig.server().begin(8080);
    auto peer = rig.connect();
    CONTRACT_NEEDS(peer != nullptr);
    peer->send("data");
    TcpConnection* c = rig.server().accept();
    c->close();
    uint8_t buf[8];
    TEST_ASSERT_EQUAL(-1, c->read(buf, sizeof(buf)));
    TEST_ASSERT_EQUAL(0, c->write((const uint8_t*)"x", 1));
  }

  static void clients_are_accepted_one_after_the_other_in_order() {
    Rig rig;
    rig.server().begin(8080);
    auto first = rig.connect();
    auto second = rig.connect();
    CONTRACT_NEEDS(first != nullptr && second != nullptr);
    first->send("one");
    second->send("two");
    TcpConnection* a = rig.server().accept();
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_EQUAL_STRING("one", readAll(*a).c_str());
    a->close();
    TcpConnection* b = rig.server().accept();
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_EQUAL_STRING("two", readAll(*b).c_str());
    TEST_ASSERT_NULL(rig.server().accept());
  }

  static void stopping_ends_the_listening_and_it_can_begin_again() {
    Rig rig;
    rig.server().begin(8080);
    rig.server().stop();
    TEST_ASSERT_NULL(rig.server().accept());
    auto refused = rig.connect();
    TEST_ASSERT_TRUE(refused == nullptr);  // nobody listens
    TEST_ASSERT_TRUE(rig.server().begin(8081));
    auto peer = rig.connect();
    CONTRACT_NEEDS(peer != nullptr);
    TEST_ASSERT_NOT_NULL(rig.server().accept());
  }

  static void stopping_closes_the_connection() {
    Rig rig;
    rig.server().begin(8080);
    auto peer = rig.connect();
    CONTRACT_NEEDS(peer != nullptr);
    TcpConnection* c = rig.server().accept();
    TEST_ASSERT_NOT_NULL(c);
    rig.server().stop();
    TEST_ASSERT_NULL(rig.server().accept());
  }

  static void beginning_again_while_listening_restarts_it() {
    Rig rig;
    TEST_ASSERT_TRUE(rig.server().begin(8080));
    TEST_ASSERT_TRUE(rig.server().begin(8080));
    auto peer = rig.connect();
    CONTRACT_NEEDS(peer != nullptr);
    TEST_ASSERT_NOT_NULL(rig.server().accept());
  }
};

#define RUN_TCP_SERVER_CONTRACT(Rig)                                                                    \
  RUN_TEST(TcpServerContract<Rig>::it_accepts_nothing_before_it_listens);                                \
  RUN_TEST(TcpServerContract<Rig>::port_zero_cannot_be_opened);                                          \
  RUN_TEST(TcpServerContract<Rig>::a_listening_server_with_no_client_accepts_nothing);                   \
  RUN_TEST(TcpServerContract<Rig>::a_client_is_accepted_and_nothing_is_there_to_read_yet);               \
  RUN_TEST(TcpServerContract<Rig>::what_the_client_sends_is_read_in_pieces_and_in_order);                \
  RUN_TEST(TcpServerContract<Rig>::what_the_software_writes_reaches_the_client);                         \
  RUN_TEST(TcpServerContract<Rig>::data_sent_before_the_client_left_is_read_before_the_end_is_reported); \
  RUN_TEST(TcpServerContract<Rig>::a_client_that_left_cannot_be_written_to);                             \
  RUN_TEST(TcpServerContract<Rig>::after_close_the_connection_reads_minus_one_and_writes_nothing);       \
  RUN_TEST(TcpServerContract<Rig>::clients_are_accepted_one_after_the_other_in_order);                   \
  RUN_TEST(TcpServerContract<Rig>::stopping_ends_the_listening_and_it_can_begin_again);                  \
  RUN_TEST(TcpServerContract<Rig>::stopping_closes_the_connection);                                      \
  RUN_TEST(TcpServerContract<Rig>::beginning_again_while_listening_restarts_it)

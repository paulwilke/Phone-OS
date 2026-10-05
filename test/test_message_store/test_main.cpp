// Host tests for the message store. The log is an in-memory fake that outlives the
// store, so a "restart" is a second store reading the same lines.
// allow-non-english: one message contains an umlaut.

#include <unity.h>

#include <cstring>
#include <string>
#include <vector>

#include <app/MessageStore.h>
#include <app/SystemEntry.h>

namespace {

struct FakeLog : MessageLog {
  std::vector<std::string> lines;
  bool failAppend = false;
  bool failRead = false;

  bool append(const char* line) override {
    if (failAppend) return false;
    lines.push_back(line);
    return true;
  }
  bool readAll(Visitor& v) override {
    if (failRead) return false;
    for (auto& l : lines) v.line(l.c_str());
    return true;
  }
};

Message msg(const char* peer, const char* text, MsgDir dir = MsgDir::In, MsgSource src = MsgSource::Sms,
            uint32_t time = 1789475520) {
  Message m;
  m.time = time;
  m.source = src;
  m.dir = dir;
  strncpy(m.peer, peer, sizeof(m.peer) - 1);
  strncpy(m.text, text, sizeof(m.text) - 1);
  return m;
}

}  // namespace

void setUp() {}
void tearDown() {}

// ---------------------------------------------------------------------------
// In memory
// ---------------------------------------------------------------------------

void test_add_and_read_back() {
  MessageStore s;
  s.add(msg("+491701111111", "Pick us up"));
  s.add(msg("+491701111111", "Yes, I will be there at 12", MsgDir::Out));

  TEST_ASSERT_EQUAL(2, s.count());
  TEST_ASSERT_EQUAL_STRING("Pick us up", s.at(0).text);
  TEST_ASSERT_TRUE(s.at(1).dir == MsgDir::Out);
  TEST_ASSERT_EQUAL(1, s.at(0).seq);
  TEST_ASSERT_EQUAL(2, s.at(1).seq);
}

void test_unread_counts_only_incoming() {
  MessageStore s;
  s.add(msg("+491701111111", "one"));
  s.add(msg("+491701111111", "two"));
  s.add(msg("+491701111111", "out", MsgDir::Out));
  TEST_ASSERT_EQUAL(2, s.unread());

  s.markAllRead();
  TEST_ASSERT_EQUAL(0, s.unread());

  s.add(msg("+491701111111", "new"));
  TEST_ASSERT_EQUAL(1, s.unread());
}

void test_remove_deletes_one_and_fixes_the_unread_count() {
  MessageStore s;
  s.add(msg("+491701111111", "one"));
  const uint32_t two = s.add(msg("+491701111111", "two"));
  s.add(msg("+491701111111", "three"));
  TEST_ASSERT_EQUAL(3, s.unread());
  TEST_ASSERT_TRUE(s.remove(two));
  TEST_ASSERT_EQUAL(2, s.count());
  TEST_ASSERT_EQUAL(2, s.unread());
  TEST_ASSERT_EQUAL_STRING("one", s.at(0).text);
  TEST_ASSERT_EQUAL_STRING("three", s.at(1).text);
  TEST_ASSERT_FALSE(s.remove(two));  // not twice
  TEST_ASSERT_FALSE(s.remove(999));
}

void test_the_ring_keeps_the_newest() {
  MessageStore s;
  for (size_t i = 0; i < MessageStore::kCapacity + 10; ++i) s.add(msg("+491701111111", std::to_string(i).c_str()));
  TEST_ASSERT_EQUAL(MessageStore::kCapacity, s.count());
  TEST_ASSERT_EQUAL_STRING("10", s.at(0).text);  // the first ten are gone
  TEST_ASSERT_EQUAL_STRING("59", s.at(MessageStore::kCapacity - 1).text);
  TEST_ASSERT_EQUAL(11, s.at(0).seq);
}

void test_removing_from_a_full_ring_keeps_the_order() {
  MessageStore s;
  for (size_t i = 0; i < MessageStore::kCapacity + 5; ++i) s.add(msg("+491701111111", std::to_string(i).c_str()));
  TEST_ASSERT_TRUE(s.remove(s.at(10).seq));  // "15"
  TEST_ASSERT_EQUAL(MessageStore::kCapacity - 1, s.count());
  TEST_ASSERT_EQUAL_STRING("14", s.at(9).text);
  TEST_ASSERT_EQUAL_STRING("16", s.at(10).text);
  TEST_ASSERT_EQUAL_STRING("54", s.at(s.count() - 1).text);
}

void test_works_without_a_log() {
  MessageStore s;  // not attached
  s.add(msg("+491701111111", "memory only"));
  s.markAllRead();
  TEST_ASSERT_EQUAL(1, s.count());
  TEST_ASSERT_FALSE(s.restore());
  TEST_ASSERT_EQUAL(0, s.writeErrors());
}

// ---------------------------------------------------------------------------
// Waiting, sent, delivered
// ---------------------------------------------------------------------------

void test_a_waiting_message_becomes_sent() {
  FakeLog log;
  MessageStore s;
  s.attach(&log);
  Message m = msg("+491701111111", "queued", MsgDir::Out);
  m.pending = true;
  const uint32_t seq = s.add(m);
  TEST_ASSERT_TRUE(s.at(0).pending);
  TEST_ASSERT_TRUE(s.at(0).read);  // outgoing is never unread

  TEST_ASSERT_TRUE(s.markSent(seq, MsgSource::Radio));
  TEST_ASSERT_FALSE(s.at(0).pending);
  TEST_ASSERT_TRUE(s.at(0).source == MsgSource::Radio);
  TEST_ASSERT_FALSE(s.markSent(seq, MsgSource::Sms));  // it is not waiting any more
  TEST_ASSERT_FALSE(s.markSent(999, MsgSource::Sms));

  MessageStore again;
  again.attach(&log);
  TEST_ASSERT_TRUE(again.restore());
  TEST_ASSERT_FALSE(again.at(0).pending);
  TEST_ASSERT_TRUE(again.at(0).source == MsgSource::Radio);
}

// A message that was still waiting at the restart stays waiting.
void test_a_waiting_message_survives_a_restart() {
  FakeLog log;
  MessageStore s;
  s.attach(&log);
  Message m = msg("+491701111111", "queued", MsgDir::Out);
  m.pending = true;
  s.add(m);

  MessageStore again;
  again.attach(&log);
  TEST_ASSERT_TRUE(again.restore());
  TEST_ASSERT_TRUE(again.at(0).pending);
  TEST_ASSERT_TRUE(again.at(0).dir == MsgDir::Out);
}

// The radio's confirmation survives the restart: otherwise "sent" would show again
// for a message known to have arrived.
void test_delivery_state_survives_a_restart() {
  FakeLog log;
  MessageStore store;
  store.attach(&log);

  Message m = msg("Node One", "I am at the sports field", MsgDir::Out, MsgSource::Radio);
  const uint32_t seq = store.add(m);
  TEST_ASSERT_TRUE(store.markDelivered(seq, true));
  TEST_ASSERT_TRUE(store.at(0).delivery == MsgDelivery::Confirmed);

  MessageStore again;
  again.attach(&log);
  TEST_ASSERT_TRUE(again.restore());
  TEST_ASSERT_EQUAL(1, again.count());
  TEST_ASSERT_TRUE(again.at(0).delivery == MsgDelivery::Confirmed);

  const uint32_t second = store.add(msg("Node One", "second", MsgDir::Out, MsgSource::Radio));
  store.markDelivered(second, false);
  MessageStore third;
  third.attach(&log);
  third.restore();
  TEST_ASSERT_TRUE(third.at(1).delivery == MsgDelivery::Unconfirmed);
}

void test_delivery_only_concerns_outgoing_messages() {
  MessageStore s;
  const uint32_t in = s.add(msg("Node One", "hello"));
  TEST_ASSERT_FALSE(s.markDelivered(in, true));
  TEST_ASSERT_TRUE(s.at(0).delivery == MsgDelivery::Unknown);
  TEST_ASSERT_FALSE(s.markDelivered(999, true));
}

// ---------------------------------------------------------------------------
// The log
// ---------------------------------------------------------------------------

void test_persistence_across_a_restart() {
  FakeLog log;
  {
    MessageStore s;
    s.attach(&log);
    s.add(msg("+491701111111", "School is cancelled"));
    s.add(msg("+491701111111", "Back at 12", MsgDir::Out));
    s.add(msg("Node One", "over radio", MsgDir::In, MsgSource::Radio, 1789475999));
  }

  MessageStore s2;
  s2.attach(&log);
  TEST_ASSERT_TRUE(s2.restore());
  TEST_ASSERT_EQUAL(3, s2.count());
  TEST_ASSERT_EQUAL_STRING("School is cancelled", s2.at(0).text);
  TEST_ASSERT_EQUAL_STRING("+491701111111", s2.at(0).peer);
  TEST_ASSERT_TRUE(s2.at(2).source == MsgSource::Radio);
  TEST_ASSERT_EQUAL_UINT32(1789475999, s2.at(2).time);
  TEST_ASSERT_EQUAL(2, s2.unread());

  // a new message gets the next free number
  s2.add(msg("+491701111111", "afterwards"));
  TEST_ASSERT_EQUAL(4, s2.at(3).seq);
}

void test_the_log_line_format_is_stable() {
  FakeLog log;
  MessageStore s;
  s.attach(&log);
  s.add(msg("+491701111111", "hello"));
  Message out = msg("Node One", "bye", MsgDir::Out, MsgSource::Radio, 7);
  out.pending = true;
  const uint32_t seq = s.add(out);
  s.markSent(seq, MsgSource::Sms);
  s.markDelivered(seq, true);
  s.markAllRead();
  s.remove(1);
  TEST_ASSERT_EQUAL(6, log.lines.size());
  TEST_ASSERT_EQUAL_STRING("M\t1\t1789475520\ts\ti\t+491701111111\thello", log.lines[0].c_str());
  TEST_ASSERT_EQUAL_STRING("M\t2\t7\tm\tq\tNode One\tbye", log.lines[1].c_str());
  TEST_ASSERT_EQUAL_STRING("S\t2\ts", log.lines[2].c_str());
  TEST_ASSERT_EQUAL_STRING("D\t2\t1", log.lines[3].c_str());
  TEST_ASSERT_EQUAL_STRING("R\t2", log.lines[4].c_str());
  TEST_ASSERT_EQUAL_STRING("X\t1", log.lines[5].c_str());
}

void test_the_read_state_survives_a_restart() {
  FakeLog log;
  {
    MessageStore s;
    s.attach(&log);
    s.add(msg("+491701111111", "one"));
    s.add(msg("+491701111111", "two"));
    s.markAllRead();
    s.add(msg("+491701111111", "three"));
  }
  MessageStore s2;
  s2.attach(&log);
  TEST_ASSERT_TRUE(s2.restore());
  TEST_ASSERT_EQUAL(3, s2.count());
  TEST_ASSERT_TRUE(s2.at(0).read);
  TEST_ASSERT_TRUE(s2.at(1).read);
  TEST_ASSERT_FALSE(s2.at(2).read);
  TEST_ASSERT_EQUAL(1, s2.unread());
}

// Marking as read twice writes the mark once.
void test_a_read_mark_is_only_written_when_something_changed() {
  FakeLog log;
  MessageStore s;
  s.attach(&log);
  s.add(msg("+491701111111", "one"));
  s.markAllRead();
  s.markAllRead();
  TEST_ASSERT_EQUAL(2, log.lines.size());
}

// A deleted message stays in the file and is skipped on reading, read or not.
void test_a_deletion_survives_a_restart_without_rewriting_the_file() {
  FakeLog log;
  {
    MessageStore s;
    s.attach(&log);
    s.add(msg("+491701111111", "stays"));
    const uint32_t gone = s.add(msg("+491701111111", "gone"));
    s.add(msg("+491701111111", "also there"));
    s.markAllRead();
    TEST_ASSERT_TRUE(s.remove(gone));
  }
  TEST_ASSERT_EQUAL(5, log.lines.size());  // three messages, the read mark, the deletion
  TEST_ASSERT_NOT_NULL(strstr(log.lines[1].c_str(), "gone"));  // the message line is still there

  MessageStore s2;
  s2.attach(&log);
  TEST_ASSERT_TRUE(s2.restore());
  TEST_ASSERT_EQUAL(2, s2.count());
  TEST_ASSERT_EQUAL_STRING("stays", s2.at(0).text);
  TEST_ASSERT_EQUAL_STRING("also there", s2.at(1).text);
  TEST_ASSERT_EQUAL(0, s2.unread());
}

// Deleting the newest message must not free its number: the old deletion line
// would otherwise delete the next message that gets it.
void test_a_deleted_number_is_not_reused_after_a_restart() {
  FakeLog log;
  {
    MessageStore s;
    s.attach(&log);
    s.add(msg("+491701111111", "one"));
    const uint32_t newest = s.add(msg("+491701111111", "two"));
    s.remove(newest);
  }
  MessageStore s2;
  s2.attach(&log);
  TEST_ASSERT_TRUE(s2.restore());
  const uint32_t next = s2.add(msg("+491701111111", "three"));
  TEST_ASSERT_EQUAL(3, next);

  MessageStore s3;
  s3.attach(&log);
  TEST_ASSERT_TRUE(s3.restore());
  TEST_ASSERT_EQUAL(2, s3.count());
  TEST_ASSERT_EQUAL_STRING("three", s3.at(1).text);
}

// The log keeps everything; memory keeps the newest.
void test_a_long_history_restores_into_the_ring() {
  FakeLog log;
  MessageStore s;
  s.attach(&log);
  for (int i = 0; i < 120; ++i) s.add(msg("+491701111111", std::to_string(i).c_str()));
  MessageStore s2;
  s2.attach(&log);
  TEST_ASSERT_TRUE(s2.restore());
  TEST_ASSERT_EQUAL(MessageStore::kCapacity, s2.count());
  TEST_ASSERT_EQUAL_STRING("70", s2.at(0).text);
  TEST_ASSERT_EQUAL_STRING("119", s2.at(MessageStore::kCapacity - 1).text);
  TEST_ASSERT_EQUAL(121, s2.add(msg("+491701111111", "next")) );
}

// Marks that refer to a message which has dropped out of memory are harmless.
void test_marks_for_messages_outside_the_ring_are_ignored() {
  FakeLog log;
  log.lines.push_back("M\t1\t7\ts\to\t+491701111111\told");
  for (int i = 2; i < 60; ++i) log.lines.push_back("M\t" + std::to_string(i) + "\t7\ts\ti\t+491701111111\tx");
  log.lines.push_back("S\t1\tm");
  log.lines.push_back("D\t1\t1");
  log.lines.push_back("X\t1");
  MessageStore s;
  s.attach(&log);
  TEST_ASSERT_TRUE(s.restore());
  TEST_ASSERT_EQUAL(MessageStore::kCapacity, s.count());
  TEST_ASSERT_EQUAL(0, s.damagedLines());
}

void test_tabs_and_line_breaks_keep_a_message_on_one_line() {
  FakeLog log;
  MessageStore s;
  s.attach(&log);
  s.add(msg("+491701111111", "line one\nline\ttwo\r\nthree"));
  TEST_ASSERT_EQUAL(1, log.lines.size());
  TEST_ASSERT_NULL(strchr(log.lines[0].c_str(), '\n'));
  TEST_ASSERT_NULL(strchr(log.lines[0].c_str(), '\r'));

  MessageStore s2;
  s2.attach(&log);
  TEST_ASSERT_TRUE(s2.restore());
  TEST_ASSERT_EQUAL_STRING("line one|line two||three", s2.at(0).text);
}

// A name from a radio contact or a sender text can hold a tab too; it must not push
// the text into the wrong field.
void test_a_tab_in_the_peer_does_not_shift_the_fields() {
  FakeLog log;
  MessageStore s;
  s.attach(&log);
  s.add(msg("Sender\tName", "the text"));
  MessageStore s2;
  s2.attach(&log);
  TEST_ASSERT_TRUE(s2.restore());
  TEST_ASSERT_EQUAL_STRING("Sender Name", s2.at(0).peer);
  TEST_ASSERT_EQUAL_STRING("the text", s2.at(0).text);
}

void test_a_long_text_is_cut_to_its_field_on_a_character_boundary() {
  FakeLog log;
  MessageStore s;
  s.attach(&log);
  Message m = msg("+491701111111", "");
  memset(m.text, 'x', sizeof(m.text) - 1);
  s.add(m);

  MessageStore s2;
  s2.attach(&log);
  TEST_ASSERT_TRUE(s2.restore());
  TEST_ASSERT_EQUAL(sizeof(m.text) - 1, strlen(s2.at(0).text));

  // A hand-edited file with a longer text: the cut does not split the umlaut.
  FakeLog edited;
  edited.lines.push_back("M\t1\t7\ts\ti\t+491701111111\t" + std::string(159, 'x') + "\xC3\xBC" + "yy");
  MessageStore s3;
  s3.attach(&edited);
  TEST_ASSERT_TRUE(s3.restore());
  TEST_ASSERT_EQUAL(159, strlen(s3.at(0).text));
}

void test_windows_line_endings_in_the_file_are_tolerated() {
  FakeLog log;
  log.lines.push_back("M\t1\t7\ts\ti\t+491701111111\thello\r");
  log.lines.push_back("R\t1\r");
  MessageStore s;
  s.attach(&log);
  TEST_ASSERT_TRUE(s.restore());
  TEST_ASSERT_EQUAL_STRING("hello", s.at(0).text);
  TEST_ASSERT_EQUAL(0, s.unread());
}

void test_an_empty_text_and_an_empty_peer_are_messages() {
  FakeLog log;
  MessageStore s;
  s.attach(&log);
  s.add(msg("", "device notice", MsgDir::In, MsgSource::System));
  s.add(msg("+491701111111", ""));
  MessageStore s2;
  s2.attach(&log);
  TEST_ASSERT_TRUE(s2.restore());
  TEST_ASSERT_EQUAL(2, s2.count());
  TEST_ASSERT_EQUAL_STRING("", s2.at(0).peer);
  TEST_ASSERT_TRUE(s2.at(0).source == MsgSource::System);
  TEST_ASSERT_EQUAL_STRING("", s2.at(1).text);
}

// ---------------------------------------------------------------------------
// System entries: a kind and an argument, not a sentence
// ---------------------------------------------------------------------------

void test_a_system_entry_is_a_kind_with_an_optional_argument() {
  Message m = msg("+491701111111", "", MsgDir::In, MsgSource::Sms);
  TEST_ASSERT_TRUE(sysentry::make(m, sysentry::kMissedCall));
  TEST_ASSERT_EQUAL_STRING("@missed-call", m.text);
  TEST_ASSERT_TRUE(m.source == MsgSource::System);
  TEST_ASSERT_EQUAL_STRING("+491701111111", m.peer);   // the other party stays in the peer field

  sysentry::Entry e;
  TEST_ASSERT_TRUE(sysentry::parse(m, e));
  TEST_ASSERT_EQUAL_STRING("missed-call", e.kind.c_str());
  TEST_ASSERT_EQUAL_STRING("", e.argument.c_str());

  Message f = msg("+491701111111", "");
  TEST_ASSERT_TRUE(sysentry::make(f, sysentry::kCallFailed, sysentry::kNoAnswer));
  TEST_ASSERT_EQUAL_STRING("@call-failed no-answer", f.text);
  TEST_ASSERT_TRUE(sysentry::parse(f, e));
  TEST_ASSERT_EQUAL_STRING("call-failed", e.kind.c_str());
  TEST_ASSERT_EQUAL_STRING("no-answer", e.argument.c_str());
}

void test_a_system_entry_refuses_anything_that_is_not_a_catalog_word() {
  for (const char* kind : {"", "Missed", "missed call", "missed\tcall", "missed_call", "m\xC3\xBC"}) {
    Message m = msg("x", "unchanged");
    TEST_ASSERT_FALSE_MESSAGE(sysentry::make(m, kind), kind);
    TEST_ASSERT_EQUAL_STRING("unchanged", m.text);
    TEST_ASSERT_TRUE(m.source == MsgSource::Sms);
  }
  for (const char* arg : {"Busy", "no answer", "a\nb", "x@y"}) {
    Message m = msg("x", "unchanged");
    TEST_ASSERT_FALSE_MESSAGE(sysentry::make(m, "call-failed", arg), arg);
    TEST_ASSERT_EQUAL_STRING("unchanged", m.text);
  }
  Message m = msg("x", "unchanged");
  TEST_ASSERT_FALSE(sysentry::make(m, std::string(200, 'a')));   // longer than the text field
  TEST_ASSERT_EQUAL_STRING("unchanged", m.text);
  TEST_ASSERT_TRUE(sysentry::make(m, std::string(150, 'a')));
  // The text field holds 160 characters and a terminator: '@' and 159 letters fit, one more does not.
  TEST_ASSERT_TRUE(sysentry::make(m, std::string(159, 'a')));
  TEST_ASSERT_EQUAL(160, strlen(m.text));
  Message full = msg("x", "unchanged");
  TEST_ASSERT_FALSE(sysentry::make(full, std::string(160, 'a')));
  TEST_ASSERT_EQUAL_STRING("unchanged", full.text);
}

void test_only_a_system_message_in_the_encoding_is_parsed_as_an_entry() {
  sysentry::Entry e;
  Message sentence = msg("+491701111111", "Missed call from Grandma", MsgDir::In, MsgSource::System);
  TEST_ASSERT_FALSE(sysentry::parse(sentence, e));       // an entry from an earlier version holds a sentence
  Message sms = msg("+491701111111", "@missed-call", MsgDir::In, MsgSource::Sms);
  TEST_ASSERT_FALSE(sysentry::parse(sms, e));            // a person may type anything, even this
  for (const char* text : {"@", "@ x", "@Missed", "@a b c", "@a  b", "@a b\tc"}) {
    Message m = msg("x", text, MsgDir::In, MsgSource::System);
    TEST_ASSERT_FALSE_MESSAGE(sysentry::parse(m, e), text);
  }
}

void test_a_system_entry_goes_through_the_file_unchanged_and_old_entries_still_load() {
  FakeLog log;
  MessageStore s;
  s.attach(&log);
  Message m = msg("+493012345678", "", MsgDir::In, MsgSource::Sms);
  TEST_ASSERT_TRUE(sysentry::make(m, sysentry::kCallFailed, sysentry::kBusy));
  s.add(m);
  s.add(msg("", "Verpasster Anruf von Oma", MsgDir::In, MsgSource::System));   // as an earlier version wrote it
  TEST_ASSERT_EQUAL_STRING("M\t1\t1789475520\ty\ti\t+493012345678\t@call-failed busy", log.lines[0].c_str());

  MessageStore again;
  again.attach(&log);
  TEST_ASSERT_TRUE(again.restore());
  sysentry::Entry e;
  TEST_ASSERT_TRUE(sysentry::parse(again.at(0), e));
  TEST_ASSERT_EQUAL_STRING("busy", e.argument.c_str());
  TEST_ASSERT_FALSE(sysentry::parse(again.at(1), e));
  TEST_ASSERT_EQUAL_STRING("Verpasster Anruf von Oma", again.at(1).text);
}

// ---------------------------------------------------------------------------
// Damaged files
// ---------------------------------------------------------------------------

void test_a_broken_line_is_reported_but_the_rest_survives() {
  FakeLog log;
  log.lines.push_back("M\t1\t1789475520\ts\ti\t+491701111111\tgood");
  log.lines.push_back("broken");
  log.lines.push_back("");  // empty lines are ignored, not damaged
  log.lines.push_back("M\t2\t1789475521\ts\ti\t+491701111111\talso good");

  MessageStore s;
  s.attach(&log);
  TEST_ASSERT_FALSE(s.restore());  // reports the damage
  TEST_ASSERT_EQUAL(1, s.damagedLines());
  TEST_ASSERT_EQUAL(2, s.count());  // and loses nothing readable
  TEST_ASSERT_EQUAL_STRING("also good", s.at(1).text);
}

void test_every_kind_of_damage_is_counted() {
  const char* bad[] = {
      "M\t0\t7\ts\ti\t+491701111111\tseq zero",
      "M\tabc\t7\ts\ti\t+491701111111\tseq not a number",
      "M\t1\t7x\ts\ti\t+491701111111\ttime not a number",
      "M\t1\t7\tz\ti\t+491701111111\tunknown source",
      "M\t1\t7\ts\tz\t+491701111111\tunknown direction",
      "M\t1\t7\tsms\ti\t+491701111111\tlong source",
      "M\t1\t7\ts\ti",                       // cut off after the direction
      "M\t1\t7\ts\ti\t+491701111111",        // cut off before the text
      "M\t99999999999\t7\ts\ti\t+49\tnumber too large",
      "M",
      "Q\t1",
      "R\tabc",
      "R\t",
      "S\t1",
      "S\t1\tz",
      "D\t1\t2",
      "X\t",
      "X\t-1",
  };
  for (const char* line : bad) {
    FakeLog log;
    log.lines.push_back(line);
    MessageStore s;
    s.attach(&log);
    TEST_ASSERT_FALSE_MESSAGE(s.restore(), line);
    TEST_ASSERT_EQUAL_MESSAGE(1, s.damagedLines(), line);
    TEST_ASSERT_EQUAL_MESSAGE(0, s.count(), line);
  }
}

// Two message lines with one number (a file that was cleared and appended to again)
// would make every later mark ambiguous: the second is refused.
void test_a_repeated_number_is_damage() {
  FakeLog log;
  log.lines.push_back("M\t1\t7\ts\ti\t+491701111111\tfirst");
  log.lines.push_back("M\t1\t8\ts\ti\t+491701111111\tsecond");
  MessageStore s;
  s.attach(&log);
  TEST_ASSERT_FALSE(s.restore());
  TEST_ASSERT_EQUAL(1, s.count());
  TEST_ASSERT_EQUAL_STRING("first", s.at(0).text);
}

void test_an_unreadable_log_gives_an_empty_list_and_false() {
  FakeLog log;
  log.failRead = true;
  MessageStore s;
  s.attach(&log);
  s.add(msg("+491701111111", "in memory before"));
  TEST_ASSERT_FALSE(s.restore());
  TEST_ASSERT_EQUAL(0, s.count());
}

void test_restoring_twice_gives_the_same_list() {
  FakeLog log;
  MessageStore s;
  s.attach(&log);
  s.add(msg("+491701111111", "one"));
  s.add(msg("+491701111111", "two"));
  TEST_ASSERT_TRUE(s.restore());
  TEST_ASSERT_TRUE(s.restore());
  TEST_ASSERT_EQUAL(2, s.count());
  TEST_ASSERT_EQUAL(3, s.add(msg("+491701111111", "three")));
}

// A full card or a missing one must not lose the message on screen, and the failure
// must be visible to whoever shows the status.
void test_a_failed_write_keeps_the_message_and_is_counted() {
  FakeLog log;
  MessageStore s;
  s.attach(&log);
  log.failAppend = true;
  s.add(msg("+491701111111", "one"));
  s.markAllRead();
  TEST_ASSERT_EQUAL(1, s.count());
  TEST_ASSERT_EQUAL(2, s.writeErrors());
  TEST_ASSERT_EQUAL(0, log.lines.size());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_add_and_read_back);
  RUN_TEST(test_unread_counts_only_incoming);
  RUN_TEST(test_remove_deletes_one_and_fixes_the_unread_count);
  RUN_TEST(test_the_ring_keeps_the_newest);
  RUN_TEST(test_removing_from_a_full_ring_keeps_the_order);
  RUN_TEST(test_works_without_a_log);
  RUN_TEST(test_a_waiting_message_becomes_sent);
  RUN_TEST(test_a_waiting_message_survives_a_restart);
  RUN_TEST(test_delivery_state_survives_a_restart);
  RUN_TEST(test_delivery_only_concerns_outgoing_messages);
  RUN_TEST(test_persistence_across_a_restart);
  RUN_TEST(test_the_log_line_format_is_stable);
  RUN_TEST(test_the_read_state_survives_a_restart);
  RUN_TEST(test_a_read_mark_is_only_written_when_something_changed);
  RUN_TEST(test_a_deletion_survives_a_restart_without_rewriting_the_file);
  RUN_TEST(test_a_deleted_number_is_not_reused_after_a_restart);
  RUN_TEST(test_a_long_history_restores_into_the_ring);
  RUN_TEST(test_marks_for_messages_outside_the_ring_are_ignored);
  RUN_TEST(test_tabs_and_line_breaks_keep_a_message_on_one_line);
  RUN_TEST(test_a_tab_in_the_peer_does_not_shift_the_fields);
  RUN_TEST(test_a_long_text_is_cut_to_its_field_on_a_character_boundary);
  RUN_TEST(test_windows_line_endings_in_the_file_are_tolerated);
  RUN_TEST(test_an_empty_text_and_an_empty_peer_are_messages);
  RUN_TEST(test_a_system_entry_is_a_kind_with_an_optional_argument);
  RUN_TEST(test_a_system_entry_refuses_anything_that_is_not_a_catalog_word);
  RUN_TEST(test_only_a_system_message_in_the_encoding_is_parsed_as_an_entry);
  RUN_TEST(test_a_system_entry_goes_through_the_file_unchanged_and_old_entries_still_load);
  RUN_TEST(test_a_broken_line_is_reported_but_the_rest_survives);
  RUN_TEST(test_every_kind_of_damage_is_counted);
  RUN_TEST(test_a_repeated_number_is_damage);
  RUN_TEST(test_an_unreadable_log_gives_an_empty_list_and_false);
  RUN_TEST(test_restoring_twice_gives_the_same_list);
  RUN_TEST(test_a_failed_write_keeps_the_message_and_is_counted);
  return UNITY_END();
}

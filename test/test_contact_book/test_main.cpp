// Host tests for the contact book (the whitelist). allow-non-english: names with
// umlauts test that a name is never cut inside a UTF-8 character.

#include <unity.h>

#include <cstring>
#include <string>

#include <app/ContactBook.h>

using phone::ContactBook;
using Kind = phone::ContactBook::Problem::Kind;

void setUp() {}
void tearDown() {}

namespace {

const char* kWhitelist =
    "# Family\n"
    "+491701111111 admin Parent One\n"
    "0170 2222222  admin  Parent Two\n"
    "\n"
    "+493012345678 Grandma\n"
    "gibberish Nonsense\n";

size_t load(ContactBook& book, const std::string& text) { return book.load(text.data(), text.size()); }

}  // namespace

// ---------------------------------------------------------------------------
// Reading the file
// ---------------------------------------------------------------------------

void test_load_whitelist() {
  ContactBook book;
  TEST_ASSERT_EQUAL(3, load(book, kWhitelist));
  TEST_ASSERT_EQUAL_STRING("+491701111111", book.at(0).phone);
  TEST_ASSERT_EQUAL_STRING("Parent One", book.at(0).name);
  TEST_ASSERT_TRUE(book.at(0).admin);
  TEST_ASSERT_EQUAL_STRING("+491702222222", book.at(1).phone);  // written 0170 …
  TEST_ASSERT_EQUAL_STRING("Parent Two", book.at(1).name);
  TEST_ASSERT_TRUE(book.at(1).admin);
  TEST_ASSERT_EQUAL_STRING("Grandma", book.at(2).name);
  TEST_ASSERT_FALSE(book.at(2).admin);
}

void test_an_unreadable_line_is_reported_with_its_line_number() {
  ContactBook book;
  load(book, kWhitelist);
  TEST_ASSERT_EQUAL(1, book.problems().size());
  TEST_ASSERT_TRUE(book.problems()[0].kind == Kind::UnreadableNumber);
  TEST_ASSERT_EQUAL(6, book.problems()[0].line);
}

void test_name_defaults_to_the_number_and_the_word_admin_is_not_a_name() {
  ContactBook book;
  TEST_ASSERT_EQUAL(2, load(book, "+491701111111 admin\n+491702222222\n"));
  TEST_ASSERT_TRUE(book.at(0).admin);
  TEST_ASSERT_EQUAL_STRING("+491701111111", book.at(0).name);
  TEST_ASSERT_FALSE(book.at(1).admin);
  TEST_ASSERT_EQUAL_STRING("+491702222222", book.at(1).name);
}

// Only the exact word grants rights: a typo must not, and neither may a name that
// merely starts with it.
void test_admin_must_be_the_exact_lowercase_word() {
  ContactBook book;
  load(book, "+491701111111 Admin Parent One\n+491702222222 administrator\n+493012345678 ADMIN\n");
  TEST_ASSERT_EQUAL(3, book.count());
  for (size_t i = 0; i < book.count(); ++i) TEST_ASSERT_FALSE(book.at(i).admin);
  TEST_ASSERT_EQUAL_STRING("Admin Parent One", book.at(0).name);
}

void test_crlf_tabs_and_blank_lines() {
  ContactBook book;
  TEST_ASSERT_EQUAL(2, load(book, "+491701111111\tadmin\tParent One\r\n\r\n\t+491702222222   Parent Two  \r\n"));
  TEST_ASSERT_EQUAL_STRING("Parent One", book.at(0).name);
  TEST_ASSERT_EQUAL_STRING("Parent Two", book.at(1).name);  // no trailing blanks
  TEST_ASSERT_EQUAL(0, book.problems().size());
}

// "\r\n" is one line break; a lone "\r" (old editors) is another.
void test_line_numbers_follow_the_editor() {
  ContactBook book;
  load(book, "+491701111111 A\r\nnonsense\r\n\r\nmore nonsense\rtoo\n");
  TEST_ASSERT_EQUAL(3, book.problems().size());
  TEST_ASSERT_EQUAL(2, book.problems()[0].line);
  TEST_ASSERT_EQUAL(4, book.problems()[1].line);
  TEST_ASSERT_EQUAL(5, book.problems()[2].line);
}

// Notepad on Windows saves UTF-8 files with a byte order mark. The first contact
// is usually a parent.
void test_a_byte_order_mark_does_not_hide_the_first_contact() {
  ContactBook book;
  TEST_ASSERT_EQUAL(1, load(book, "\xEF\xBB\xBF+491701111111 admin Parent One\n"));
  TEST_ASSERT_TRUE(book.isAdmin("+491701111111"));
  TEST_ASSERT_EQUAL(0, book.problems().size());
}

void test_comments_are_whole_lines_only() {
  ContactBook book;
  load(book, "   # indented comment\n+491701111111 Dad # not a comment\n");
  TEST_ASSERT_EQUAL(1, book.count());
  TEST_ASSERT_EQUAL_STRING("Dad # not a comment", book.at(0).name);
}

// The words after the number are the name, even when the name starts with a digit
// or a bracket; only words made purely of number characters join the number.
void test_a_name_starting_with_a_digit_is_not_swallowed_by_the_number() {
  ContactBook book;
  load(book, "0170 2222222 3B Parents\n+491701111111 (Dad)\n");
  TEST_ASSERT_EQUAL(2, book.count());
  TEST_ASSERT_EQUAL_STRING("+491702222222", book.at(0).phone);
  TEST_ASSERT_EQUAL_STRING("3B Parents", book.at(0).name);
  TEST_ASSERT_EQUAL_STRING("(Dad)", book.at(1).name);
}

// The known limit of the rule above, pinned so that nobody relies on the opposite:
// a name that is only digits belongs to the number.
void test_a_name_made_only_of_digits_joins_the_number() {
  ContactBook book;
  load(book, "+49170 2222222 2\n");
  TEST_ASSERT_EQUAL(1, book.count());
  TEST_ASSERT_EQUAL_STRING("+4917022222222", book.at(0).phone);
}

void test_a_number_with_a_hidden_extra_digit_is_unreadable() {
  ContactBook book;
  const std::string padded = "+" + std::string(15, '1') + std::string(120, ' ') + "1 Name\n";
  TEST_ASSERT_EQUAL(0, load(book, padded));
  TEST_ASSERT_EQUAL(1, book.problems().size());
}

// A cut through the middle of "ü" would show as garbage on the display.
void test_a_long_name_is_cut_at_a_character_boundary() {
  ContactBook book;
  // 22 ASCII letters, then "ü" (two bytes) would end at byte 24 and not fit with the terminator.
  const std::string name = std::string(22, 'a') + "\xC3\xBC" + "xyz";
  load(book, "+491701111111 " + name + "\n");
  TEST_ASSERT_EQUAL_STRING(std::string(22, 'a').c_str(), book.at(0).name);
  // One byte earlier the whole character fits.
  load(book, "+491701111111 " + std::string(21, 'a') + "\xC3\xBC" + "xyz\n");
  TEST_ASSERT_EQUAL_STRING((std::string(21, 'a') + "\xC3\xBC").c_str(), book.at(0).name);
}

// ---------------------------------------------------------------------------
// Limits and duplicates
// ---------------------------------------------------------------------------

void test_the_list_is_capped_and_the_overflow_is_reported_once() {
  ContactBook book;
  std::string many;
  for (int i = 0; i < 20; ++i) many += "+4917011111" + std::to_string(10 + i) + " Contact\n";
  TEST_ASSERT_EQUAL(ContactBook::kMaxContacts, load(book, many));
  TEST_ASSERT_EQUAL(1, book.problems().size());
  TEST_ASSERT_TRUE(book.problems()[0].kind == Kind::TooManyContacts);
  TEST_ASSERT_EQUAL(ContactBook::kMaxContacts + 1, book.problems()[0].line);
}

void test_problems_are_capped() {
  ContactBook book;
  std::string junk;
  for (int i = 0; i < 100; ++i) junk += "nonsense\n";
  load(book, junk);
  TEST_ASSERT_EQUAL(ContactBook::kMaxProblems, book.problems().size());
}

// Two lines for one number: the first one counts, as it does when looking up. The
// second is reported so that the parents see why "admin" did not take effect.
void test_the_first_line_of_a_duplicate_number_wins() {
  ContactBook book;
  TEST_ASSERT_EQUAL(1, load(book, "+491701111111 Dad\n0170 1111111 admin Dad again\n"));
  TEST_ASSERT_FALSE(book.isAdmin("+491701111111"));
  TEST_ASSERT_EQUAL_STRING("Dad", book.displayName("+491701111111"));
  TEST_ASSERT_EQUAL(1, book.problems().size());
  TEST_ASSERT_TRUE(book.problems()[0].kind == Kind::DuplicateNumber);
  TEST_ASSERT_EQUAL(2, book.problems()[0].line);
}

void test_loading_again_replaces_everything() {
  ContactBook book;
  load(book, kWhitelist);
  TEST_ASSERT_EQUAL(1, load(book, "+491709999999 Someone\n"));
  TEST_ASSERT_FALSE(book.isKnown("+491701111111"));
  TEST_ASSERT_EQUAL(0, book.problems().size());
  TEST_ASSERT_EQUAL(0, book.load(nullptr, 0));
  TEST_ASSERT_EQUAL(0, book.count());
}

// ---------------------------------------------------------------------------
// Lookup
// ---------------------------------------------------------------------------

void test_lookup_is_independent_of_the_notation() {
  ContactBook book;
  load(book, kWhitelist);
  TEST_ASSERT_TRUE(book.isAdmin("0170 1111111"));
  TEST_ASSERT_TRUE(book.isAdmin("+491701111111"));
  TEST_ASSERT_TRUE(book.isAdmin("0049 170 1111111"));
  TEST_ASSERT_FALSE(book.isAdmin("+493012345678"));  // Grandma may call but not command
  TEST_ASSERT_TRUE(book.isKnown("0049 30 12345678"));
  TEST_ASSERT_FALSE(book.isKnown("+491709999999"));
}

// Callers that hide their number or send a name must never match anything.
void test_hidden_and_unreadable_callers_match_nobody() {
  ContactBook book;
  load(book, kWhitelist);
  TEST_ASSERT_FALSE(book.isKnown(""));
  TEST_ASSERT_FALSE(book.isKnown(nullptr));
  TEST_ASSERT_FALSE(book.isKnown("Provider"));
  TEST_ASSERT_FALSE(book.isAdmin(""));
  TEST_ASSERT_FALSE(book.isAdmin(nullptr));
}

void test_display_name_falls_back_to_the_number_then_to_nothing() {
  ContactBook book;
  load(book, kWhitelist);
  TEST_ASSERT_EQUAL_STRING("0030 12345678", book.displayName("0030 12345678"));  // another country: unknown, shown as given
  TEST_ASSERT_EQUAL_STRING("Grandma", book.displayName("+493012345678"));
  TEST_ASSERT_EQUAL_STRING("+491709999999", book.displayName("+491709999999"));
  TEST_ASSERT_EQUAL_STRING("", book.displayName(""));
  TEST_ASSERT_EQUAL_STRING("", book.displayName(nullptr));
}

void test_country_code_decides_how_national_numbers_are_read() {
  ContactBook book;
  TEST_ASSERT_TRUE(book.setCountryCode("+34"));
  load(book, "0600123456 admin Parent One\n");
  TEST_ASSERT_EQUAL_STRING("+34600123456", book.at(0).phone);
  TEST_ASSERT_TRUE(book.isAdmin("0600 123 456"));
}

void test_an_invalid_country_code_is_refused_and_the_old_one_kept() {
  ContactBook book;
  TEST_ASSERT_FALSE(book.setCountryCode("Germany"));
  TEST_ASSERT_FALSE(book.setCountryCode(""));
  TEST_ASSERT_FALSE(book.setCountryCode(nullptr));
  TEST_ASSERT_FALSE(book.setCountryCode("+0"));
  TEST_ASSERT_FALSE(book.setCountryCode("+12345"));
  load(book, "0170 1111111\n");
  TEST_ASSERT_EQUAL_STRING("+491701111111", book.at(0).phone);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_load_whitelist);
  RUN_TEST(test_an_unreadable_line_is_reported_with_its_line_number);
  RUN_TEST(test_name_defaults_to_the_number_and_the_word_admin_is_not_a_name);
  RUN_TEST(test_admin_must_be_the_exact_lowercase_word);
  RUN_TEST(test_crlf_tabs_and_blank_lines);
  RUN_TEST(test_line_numbers_follow_the_editor);
  RUN_TEST(test_a_byte_order_mark_does_not_hide_the_first_contact);
  RUN_TEST(test_comments_are_whole_lines_only);
  RUN_TEST(test_a_name_starting_with_a_digit_is_not_swallowed_by_the_number);
  RUN_TEST(test_a_name_made_only_of_digits_joins_the_number);
  RUN_TEST(test_a_number_with_a_hidden_extra_digit_is_unreadable);
  RUN_TEST(test_a_long_name_is_cut_at_a_character_boundary);
  RUN_TEST(test_the_list_is_capped_and_the_overflow_is_reported_once);
  RUN_TEST(test_problems_are_capped);
  RUN_TEST(test_the_first_line_of_a_duplicate_number_wins);
  RUN_TEST(test_loading_again_replaces_everything);
  RUN_TEST(test_lookup_is_independent_of_the_notation);
  RUN_TEST(test_hidden_and_unreadable_callers_match_nobody);
  RUN_TEST(test_display_name_falls_back_to_the_number_then_to_nothing);
  RUN_TEST(test_country_code_decides_how_national_numbers_are_read);
  RUN_TEST(test_an_invalid_country_code_is_refused_and_the_old_one_kept);
  return UNITY_END();
}

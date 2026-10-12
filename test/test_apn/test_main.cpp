// The operator list: the lookup by subscriber number, the shape of the data the generator wrote, and that every
// operator in it can be found again.

#include <unity.h>

#include <cstring>
#include <string>

#include <drivers/ApnTable.h>

namespace {
// An invented subscriber number: the operator code and nines (built, so that the repository holds no real-looking number).
std::string imsi(const std::string& code) { return code + std::string(15 - code.size(), '9'); }
const apn::Entry* findCode(const std::string& code) { return apn::find(imsi(code).c_str()); }
}  // namespace

void setUp() {}
void tearDown() {}

void test_germany_is_found_by_its_operator_codes() {
  // The subscriber numbers are invented: only the first digits mean anything.
  TEST_ASSERT_EQUAL_STRING("internet.v6.telekom", findCode("26201")->apn);
  // Written in two pieces: an access point name that begins with the name of a catalog section looks like a catalog key.
  TEST_ASSERT_EQUAL_STRING((std::string("web") + ".vodafone.de").c_str(), findCode("26202")->apn);
  TEST_ASSERT_EQUAL_STRING("internet", findCode("26203")->apn);
  TEST_ASSERT_EQUAL_STRING("internet", findCode("26207")->apn);
}

void test_something_that_is_not_a_subscriber_number_finds_nothing() {
  TEST_ASSERT_NULL(apn::find(nullptr));
  TEST_ASSERT_NULL(apn::find(""));
  TEST_ASSERT_NULL(apn::find("26201"));                // five digits: no operator code yet
  TEST_ASSERT_NULL(apn::find((imsi("26201") + "0").c_str()));     // sixteen
  TEST_ASSERT_NULL(apn::find((imsi("26201").substr(0, 14) + "x").c_str()));     // a letter
  TEST_ASSERT_NULL(apn::find((imsi("262").substr(0, 3) + " " + imsi("01").substr(0, 11)).c_str()));   // a blank
  TEST_ASSERT_NULL(apn::find(("+" + imsi("26201").substr(0, 14)).c_str()));   // a plus
}

void test_an_operator_the_list_does_not_know_finds_nothing() {
  TEST_ASSERT_NULL(findCode(""));
  TEST_ASSERT_NULL(findCode("262"));     // Germany, but not an operator of it
  TEST_ASSERT_NULL(findCode("00101"));     // the test networks are not in the list
}

// "050" and "05" are two operators. A three-digit code is looked for first, so the one that has it wins; the
// subscriber number does not say how long its code is, and the list is the only way to tell.
void test_a_three_digit_code_is_preferred_to_a_two_digit_one() {
  const apn::Entry* three = findCode("338050");
  TEST_ASSERT_NOT_NULL(three);
  TEST_ASSERT_EQUAL(3, three->mncDigits);
  TEST_ASSERT_EQUAL(50, three->mnc);
  const apn::Entry* two = findCode("338059");    // "059" is not an operator, "05" is
  TEST_ASSERT_NOT_NULL(two);
  TEST_ASSERT_EQUAL(2, two->mncDigits);
  TEST_ASSERT_EQUAL(5, two->mnc);
}

void test_the_data_is_sorted_unique_and_clean() {
  TEST_ASSERT_TRUE(apn::kTableSize > 1000);
  TEST_ASSERT_EQUAL(64, strlen(apn::kSourceSha256));
  for (size_t i = 0; i < apn::kTableSize; ++i) {
    const apn::Entry& e = apn::kTable[i];
    TEST_ASSERT_TRUE(e.mcc >= 100 && e.mcc <= 999 && e.mcc != 1);
    TEST_ASSERT_TRUE(e.mncDigits == 2 || e.mncDigits == 3);
    TEST_ASSERT_TRUE(e.mnc < (e.mncDigits == 2 ? 100 : 1000));
    const size_t n = strlen(e.apn);
    TEST_ASSERT_TRUE(n >= 1 && n <= 100);
    for (size_t k = 0; k < n; ++k) {
      const char c = e.apn[k];
      TEST_ASSERT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '.' || c == '-' || c == '_');
    }
    if (i > 0) {
      const apn::Entry& before = apn::kTable[i - 1];
      const bool ordered = before.mcc < e.mcc || (before.mcc == e.mcc && (before.mnc < e.mnc || (before.mnc == e.mnc && before.mncDigits < e.mncDigits)));
      TEST_ASSERT_TRUE_MESSAGE(ordered, e.apn);
    }
  }
}

// Every operator of the list is found by a subscriber number that starts with its code - or, when a three-digit
// code that starts with it exists too, by that one.
void test_every_operator_in_the_list_can_be_found() {
  for (size_t i = 0; i < apn::kTableSize; ++i) {
    const apn::Entry& e = apn::kTable[i];
    char imsi[16];
    snprintf(imsi, sizeof(imsi), "%03u%0*u%s", (unsigned)e.mcc, (int)e.mncDigits, (unsigned)e.mnc, "123456789012");
    imsi[15] = '\0';
    const apn::Entry* found = apn::find(imsi);
    TEST_ASSERT_NOT_NULL_MESSAGE(found, imsi);
    const bool same = found == &e;
    const bool shadowed = e.mncDigits == 2 && found->mncDigits == 3 && found->mcc == e.mcc && found->mnc / 10 == e.mnc;
    TEST_ASSERT_TRUE_MESSAGE(same || shadowed, imsi);
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_germany_is_found_by_its_operator_codes);
  RUN_TEST(test_something_that_is_not_a_subscriber_number_finds_nothing);
  RUN_TEST(test_an_operator_the_list_does_not_know_finds_nothing);
  RUN_TEST(test_a_three_digit_code_is_preferred_to_a_two_digit_one);
  RUN_TEST(test_the_data_is_sorted_unique_and_clean);
  RUN_TEST(test_every_operator_in_the_list_can_be_found);
  return UNITY_END();
}

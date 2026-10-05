#pragma once

// The whitelist: who may call and write to the child's phone, and which of them
// are parents. The source is a text file the parents edit, one contact per line,
// fields separated by blanks, `#` starting a comment line:
//
//   +491701111111 admin Parent One
//   0170 2222222  admin Parent Two
//   +493012345678 Grandma
//
// Without `admin` a number may call and write but not send remote commands.
// Numbers are normalised (PhoneNumber.h) before they are compared, so a contact
// written "0170 …" is found when "+49170 …" calls.
//
// The module returns data and problem codes, never text: the interface turns a
// Problem into a sentence in the parents' language.

#include <cstddef>
#include <vector>

#include "PhoneNumber.h"

namespace phone {

struct Contact {
  char phone[kNormalizedSize] = {};  // canonical form
  char name[24] = {};   // UTF-8, cut at a character boundary when too long
  bool admin = false;
};

class ContactBook {
public:
  // More lines than this are ignored: a child's phone needs a handful of numbers,
  // and a short list keeps the contact screen usable.
  static constexpr size_t kMaxContacts = 16;
  // Only the first problems are kept, so a damaged file cannot grow the list.
  static constexpr size_t kMaxProblems = 16;

  struct Problem {
    enum class Kind {
      UnreadableNumber,  // the line does not start with a phone number
      DuplicateNumber,   // the number is on file already; the first line counts
      TooManyContacts,   // the list is full; reported once, at the first line left out
    };
    Kind kind;
    size_t line;  // 1-based line number in the file
  };

  // Sets the country code used for numbers written with a national leading 0.
  // Returns false and keeps the old value when `cc` is not a country code. Call it
  // before load(): the numbers in the book are normalised when they are loaded.
  bool setCountryCode(const char* cc);

  // Replaces the contacts with those in `text`. Returns how many were taken over;
  // lines that cannot be used are skipped and listed in problems().
  size_t load(const char* text, size_t len);
  // The country code numbers are completed with (see setCountryCode).
  const char* countryCode() const { return country_; }

  size_t count() const { return count_; }
  // `i` must be below count().
  const Contact& at(size_t i) const { return contacts_[i]; }
  const std::vector<Problem>& problems() const { return problems_; }

  const Contact* find(const char* phone) const;
  bool isKnown(const char* phone) const { return find(phone) != nullptr; }
  bool isAdmin(const char* phone) const;

  // The contact's name; for an unknown caller the number as given; empty when there
  // is neither. Never null, so the caller can pass it on without a check.
  const char* displayName(const char* phone) const;

private:
  void report(Problem::Kind kind, size_t line);

  Contact contacts_[kMaxContacts];
  size_t count_ = 0;
  char country_[8] = "+49";
  std::vector<Problem> problems_;
};

}  // namespace phone

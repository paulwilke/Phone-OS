#include "ContactBook.h"

#include <cstring>

#include "PhoneNumber.h"
#include "Utf8.h"

namespace phone {
namespace {

bool isBlank(char c) { return c == ' ' || c == '\t'; }
bool isBreak(char c) { return c == '\n' || c == '\r'; }

bool isNumberChar(char c) {
  return (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '/' || c == '(' || c == ')' ||
         c == '.';
}

const char* skipBlanks(const char* p, const char* end) {
  while (p < end && isBlank(*p)) ++p;
  return p;
}

const char* tokenEnd(const char* p, const char* end) {
  while (p < end && !isBlank(*p)) ++p;
  return p;
}

bool allNumberChars(const char* begin, const char* end) {
  for (const char* p = begin; p < end; ++p) {
    if (!isNumberChar(*p)) return false;
  }
  return begin < end;
}

// A number may contain blanks ("0170 2222222") and so spans several words; it ends
// at the first word that has anything else in it, so a name such as "3B Parents"
// is not swallowed by the digits in front of it. A name made only of digits cannot
// be told from the rest of the number; the documentation tells people to avoid it.
const char* numberEnd(const char* p, const char* end) {
  const char* last = p;
  const char* word = p;
  while (word < end) {
    const char* wordEnd = tokenEnd(word, end);
    if (!allNumberChars(word, wordEnd)) break;
    last = wordEnd;
    word = skipBlanks(wordEnd, end);
  }
  return last;
}

}  // namespace

bool ContactBook::setCountryCode(const char* cc) {
  char probe[kNormalizedSize];
  if (!cc || !*cc || strlen(cc) >= sizeof(country_)) return false;
  // A code is valid when a national number can be completed with it.
  if (!normalize("01", cc, probe, sizeof(probe))) return false;
  memcpy(country_, cc, strlen(cc) + 1);
  return true;
}

void ContactBook::report(Problem::Kind kind, size_t line) {
  if (problems_.size() < kMaxProblems) problems_.push_back({kind, line});
}

size_t ContactBook::load(const char* text, size_t len) {
  count_ = 0;
  problems_.clear();
  if (!text) return 0;

  const char* p = text;
  const char* const end = text + len;
  // Editors on Windows put a byte order mark at the start of a file; left in
  // place it would make the first contact, usually a parent, unreadable.
  if (len >= 3 && memcmp(p, "\xEF\xBB\xBF", 3) == 0) p += 3;

  size_t line = 0;
  bool fullReported = false;
  while (p < end) {
    const char* lineEnd = p;
    while (lineEnd < end && !isBreak(*lineEnd)) ++lineEnd;
    ++line;

    const char* cursor = skipBlanks(p, lineEnd);
    if (cursor < lineEnd && *cursor != '#') {
      const char* numEnd = numberEnd(cursor, lineEnd);
      char raw[96];
      // Input that does not fit is unreadable; cutting it could hide extra digits.
      const bool fits = (size_t)(numEnd - cursor) < sizeof(raw);
      if (fits) utf8::copy(raw, sizeof(raw), cursor, numEnd);

      Contact c;
      if (!fits || !normalize(raw, country_, c.phone, sizeof(c.phone))) {
        report(Problem::Kind::UnreadableNumber, line);
      } else if (isKnown(c.phone)) {
        report(Problem::Kind::DuplicateNumber, line);
      } else if (count_ >= kMaxContacts) {
        if (!fullReported) report(Problem::Kind::TooManyContacts, line);
        fullReported = true;
      } else {
        cursor = skipBlanks(numEnd, lineEnd);
        const char* wordEnd = tokenEnd(cursor, lineEnd);
        // Case matters: a misspelt "Admin" leaves the contact without rights, which
        // is the safe way to fail.
        if (wordEnd - cursor == 5 && memcmp(cursor, "admin", 5) == 0) {
          c.admin = true;
          cursor = skipBlanks(wordEnd, lineEnd);
        }
        const char* nameEnd = lineEnd;
        while (nameEnd > cursor && isBlank(nameEnd[-1])) --nameEnd;
        utf8::copy(c.name, sizeof(c.name), cursor, nameEnd);
        if (c.name[0] == '\0') utf8::copy(c.name, sizeof(c.name), c.phone, c.phone + strlen(c.phone));
        contacts_[count_++] = c;
      }
    }

    // "\r\n" counts as one break, so line numbers match what an editor shows.
    if (lineEnd < end) {
      p = lineEnd + 1;
      if (*lineEnd == '\r' && p < end && *p == '\n') ++p;
    } else {
      p = lineEnd;
    }
  }
  return count_;
}

const Contact* ContactBook::find(const char* phone) const {
  if (!phone || !*phone) return nullptr;
  char normalized[kNormalizedSize];
  if (!normalize(phone, country_, normalized, sizeof(normalized))) return nullptr;
  for (size_t i = 0; i < count_; ++i) {
    if (strcmp(contacts_[i].phone, normalized) == 0) return &contacts_[i];
  }
  return nullptr;
}

bool ContactBook::isAdmin(const char* phone) const {
  const Contact* c = find(phone);
  return c && c->admin;
}

const char* ContactBook::displayName(const char* phone) const {
  const Contact* c = find(phone);
  if (c) return c->name;
  return phone ? phone : "";
}

}  // namespace phone

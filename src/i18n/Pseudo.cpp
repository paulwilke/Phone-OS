// allow-non-english: the accented letters it produces are named in comments.
#include "Pseudo.h"

namespace i18n {

namespace {

// The accented stand-in for a letter, or 0 when the letter stays as it is. Every result is one
// code point of Latin-1, written below as its UTF-8 bytes.
const char* accented(char c) {
  switch (c) {
    case 'a': return "\xC3\xA1";  // á
    case 'e': return "\xC3\xA9";  // é
    case 'i': return "\xC3\xAD";  // í
    case 'o': return "\xC3\xB6";  // ö
    case 'u': return "\xC3\xBC";  // ü
    case 'y': return "\xC3\xBD";  // ý
    case 'c': return "\xC3\xA7";  // ç
    case 'n': return "\xC3\xB1";  // ñ
    case 'A': return "\xC3\x85";  // Å
    case 'E': return "\xC3\x89";  // É
    case 'I': return "\xC3\x8C";  // Ì
    case 'O': return "\xC3\x96";  // Ö
    case 'U': return "\xC3\x9C";  // Ü
    default: return nullptr;
  }
}

// What is appended to a word: letters of the same kind the accented ones are, cycling.
const char* const kFiller[] = {"\xC3\xAB", "\xC3\xB6", "\xC3\xBC", "\xC3\xAF", "\xC3\xA4"};  // ë ö ü ï ä

bool isAsciiLetter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

// A letter for the purpose of growing words: an ASCII letter, or a two-byte UTF-8 sequence
// (Latin-1 and Latin Extended letters such as ü or œ). Longer sequences are symbols or the euro
// sign and do not make a word longer.
size_t sequenceLength(unsigned char lead) {
  if (lead < 0x80) return 1;
  if ((lead & 0xE0) == 0xC0) return 2;
  if ((lead & 0xF0) == 0xE0) return 3;
  if ((lead & 0xF8) == 0xF0) return 4;
  return 1;
}

}  // namespace

std::string pseudoLocalize(const std::string& message) {
  std::string out = "[";
  size_t letters = 0;       // letters in the word being written
  size_t filler = 0;        // how many filler letters were written so far, over the whole message
  bool trailingPlaceholder = false;

  // Writes the extra letters of the word that ends here: two for every five letters, rounded up.
  const auto endWord = [&]() {
    if (letters > 0 && !trailingPlaceholder) {
      const size_t extra = (letters * 2 + 4) / 5;
      for (size_t i = 0; i < extra; ++i) out += kFiller[filler++ % (sizeof kFiller / sizeof *kFiller)];
    }
    letters = 0;
    trailingPlaceholder = false;
  };

  for (size_t i = 0; i < message.size();) {
    const char c = message[i];
    if (c == '{' && i + 1 < message.size() && message[i + 1] == '{') {
      out.append(message, i, 2);  // an escaped brace: literal text, not the start of a placeholder
      i += 2;
      trailingPlaceholder = false;
    } else if (c == '{') {
      const size_t end = message.find('}', i);
      const size_t stop = end == std::string::npos ? message.size() : end + 1;
      out.append(message, i, stop - i);
      i = stop;
      trailingPlaceholder = true;
    } else if (c == ' ') {
      endWord();
      out += c;
      ++i;
    } else {
      trailingPlaceholder = false;
      const size_t n = sequenceLength((unsigned char)c);
      if (n == 1) {
        const char* replacement = isAsciiLetter(c) ? accented(c) : nullptr;
        if (isAsciiLetter(c)) ++letters;
        if (replacement) out += replacement;
        else out += c;
      } else {
        if (n == 2) ++letters;
        out.append(message, i, n);
      }
      i += n;
    }
  }
  endWord();
  out += ']';
  return out;
}

Catalog pseudoCatalog(const Catalog& reference) {
  Catalog out;
  out.code_ = kPseudoCode;
  out.name_ = "Pseudo-language";
  out.native_ = "[Pseudo]";
  out.plural_ = reference.plural_;
  for (const auto& [key, entry] : reference.entries_) {
    Entry e;
    for (const std::string& form : entry.forms) e.forms.push_back(pseudoLocalize(form));
    out.entries_[key] = e;
  }
  return out;
}

}  // namespace i18n

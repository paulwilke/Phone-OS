#include "Catalog.h"

#include <cctype>
#include <cstdio>

namespace i18n {
namespace {

bool validKey(const std::string& key) {
  if (key.empty() || key.front() == '.' || key.back() == '.') return false;
  for (char c : key) {
    if (!(islower((unsigned char)c) || isdigit((unsigned char)c) || c == '.' || c == '_' || c == '-')) return false;
  }
  return key.find("..") == std::string::npos;
}

void appendUtf8(std::string& out, uint32_t cp) {
  if (cp < 0x80) out += (char)cp;
  else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
  else if (cp < 0x10000) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
  else { out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
}

// Reads a double-quoted string starting at `p` (which points at the quote). On success
// `p` is left after the closing quote.
bool readString(const char*& p, const char* end, std::string& out, std::string& err) {
  ++p;
  while (p < end && *p != '"') {
    if (*p == '\n') { err = "string is not closed on this line"; return false; }
    if (*p != '\\') { out += *p++; continue; }
    if (++p >= end) break;
    switch (*p) {
      case '"': out += '"'; break;
      case '\\': out += '\\'; break;
      case 'n': out += '\n'; break;
      case 't': out += '\t'; break;
      case 'u': {
        uint32_t cp = 0;
        for (int i = 0; i < 4; ++i) {
          ++p;
          if (p >= end || !isxdigit((unsigned char)*p)) { err = "\\u needs four hex digits"; return false; }
          cp = cp * 16 + (uint32_t)(isdigit((unsigned char)*p) ? *p - '0' : (tolower(*p) - 'a' + 10));
        }
        appendUtf8(out, cp);
        break;
      }
      default: err = std::string("unknown escape \\") + *p; return false;
    }
    ++p;
  }
  if (p >= end || *p != '"') { err = "string is not closed"; return false; }
  ++p;
  return true;
}

void skipBlanks(const char*& p, const char* end) {
  while (p < end && (*p == ' ' || *p == '\t')) ++p;
}

}  // namespace

bool Catalog::parse(const char* text, size_t length) {
  code_.clear(); name_.clear(); native_.clear();
  plural_ = PluralRule::OneOther;
  entries_.clear();
  errors_.clear();

  enum class Section { None, Meta, Strings } section = Section::None;
  bool havePlural = false;
  std::set<std::string> seenMeta;
  const char* p = text;
  const char* const end = text + length;
  int lineNo = 0;

  while (p < end) {
    ++lineNo;
    const char* eol = p;
    while (eol < end && *eol != '\n') ++eol;
    const char* q = p;
    const auto fail = [&](const std::string& message) { errors_.push_back({lineNo, message}); };

    skipBlanks(q, eol);
    if (q < eol && *q == '\r') ++q;
    if (q >= eol || *q == '#' || *q == '\r') { p = eol + 1; continue; }

    if (*q == '[') {
      const char* close = (const char*)memchr(q, ']', (size_t)(eol - q));
      std::string name = close ? std::string(q + 1, close) : "";
      if (name == "meta") section = Section::Meta;
      else if (name == "strings") section = Section::Strings;
      else { fail("unknown section [" + name + "] (only [meta] and [strings] exist)"); section = Section::None; }
      p = eol + 1;
      continue;
    }

    // key = value
    const char* eq = (const char*)memchr(q, '=', (size_t)(eol - q));
    if (!eq) { fail("expected `key = value`"); p = eol + 1; continue; }
    const char* ke = eq;
    while (ke > q && (ke[-1] == ' ' || ke[-1] == '\t')) --ke;
    const std::string key(q, ke);
    const char* v = eq + 1;
    skipBlanks(v, eol);
    std::vector<std::string> values;
    std::string err;
    bool isArray = false;

    if (v < eol && *v == '"') {
      std::string s;
      if (!readString(v, eol, s, err)) { fail(err); p = eol + 1; continue; }
      values.push_back(s);
    } else if (v < eol && *v == '[') {
      isArray = true;
      ++v;
      bool ok = true;
      for (;;) {
        skipBlanks(v, eol);
        if (v < eol && *v == ']') { ++v; break; }
        if (v >= eol || *v != '"') { fail("array elements must be double-quoted strings, closed by ]"); ok = false; break; }
        std::string s;
        if (!readString(v, eol, s, err)) { fail(err); ok = false; break; }
        values.push_back(s);
        skipBlanks(v, eol);
        if (v < eol && *v == ',') ++v;
      }
      if (!ok) { p = eol + 1; continue; }
    } else {
      fail("the value must be a double-quoted string or an array of them");
      p = eol + 1;
      continue;
    }
    skipBlanks(v, eol);
    if (v < eol && *v != '#' && *v != '\r') { fail("unexpected text after the value"); p = eol + 1; continue; }

    if (section == Section::Meta) {
      if (isArray || values.size() != 1) fail("[meta] values are single strings");
      else if (!seenMeta.insert(key).second) fail("duplicate meta key `" + key + "`");
      else if (key == "code") code_ = values[0];
      else if (key == "name") name_ = values[0];
      else if (key == "native") native_ = values[0];
      else if (key == "plural") {
        if (!parsePluralRule(values[0].c_str(), &plural_)) fail("unknown plural rule `" + values[0] + "`");
        else havePlural = true;
      } else fail("unknown meta key `" + key + "`");
    } else if (section == Section::Strings) {
      if (!validKey(key)) fail("invalid key `" + key + "` (lowercase words, digits, `.`, `_`, `-`)");
      else if (entries_.count(key)) fail("duplicate key `" + key + "`");
      else if (values.empty()) fail("`" + key + "` has no text");
      else entries_[key].forms = values;
    } else {
      fail("`" + key + "` is outside any section");
    }
    p = eol + 1;
  }

  if (code_.empty()) errors_.push_back({0, "[meta] needs `code`"});
  if (name_.empty()) errors_.push_back({0, "[meta] needs `name`"});
  if (native_.empty()) errors_.push_back({0, "[meta] needs `native`"});
  if (!havePlural) errors_.push_back({0, "[meta] needs `plural`"});

  // The number of forms must match the language's plural rule — checked here so a
  // loaded catalog never has a message the rule can index out of range.
  if (havePlural) {
    for (auto& kv : entries_) {
      const size_t want = pluralFormCount(plural_);
      const size_t have = kv.second.forms.size();
      if (have != 1 && have != want) {
        char buf[96];
        snprintf(buf, sizeof(buf), "`%s` has %u forms, the plural rule needs %u (or one for a message without a count)",
                 kv.first.c_str(), (unsigned)have, (unsigned)want);
        errors_.push_back({0, buf});
      }
    }
  }
  if (!errors_.empty()) {
    entries_.clear();
    return false;
  }
  return true;
}

void Catalog::merge(const Catalog& other) {
  for (const auto& kv : other.entries_) entries_[kv.first] = kv.second;
}

std::vector<std::string> Catalog::placeholders(const std::string& form) {
  std::vector<std::string> names;
  for (size_t i = 0; i < form.size(); ++i) {
    if (form[i] == '{' && i + 1 < form.size() && form[i + 1] == '{') { ++i; continue; }
    if (form[i] == '}' && i + 1 < form.size() && form[i + 1] == '}') { ++i; continue; }
    if (form[i] != '{') continue;
    const size_t close = form.find('}', i);
    if (close == std::string::npos) break;
    const std::string name = form.substr(i + 1, close - i - 1);
    bool known = false;
    for (const auto& n : names) known = known || n == name;
    if (!known) names.push_back(name);
    i = close;
  }
  return names;
}

std::vector<Issue> Catalog::compare(const Catalog& reference) const {
  std::vector<Issue> issues;
  for (const auto& kv : reference.entries_) {
    const Entry* mine = find(kv.first);
    if (!mine) { issues.push_back({kv.first, "missing"}); continue; }
    // Placeholders: the same names in every form as in the reference's first form.
    std::set<std::string> want;
    for (const auto& name : placeholders(kv.second.forms[0])) want.insert(name);
    for (const auto& form : mine->forms) {
      std::set<std::string> got;
      for (const auto& name : placeholders(form)) got.insert(name);
      if (got != want) { issues.push_back({kv.first, "placeholders differ from the reference"}); break; }
    }
  }
  for (const auto& kv : entries_) {
    if (!reference.find(kv.first)) issues.push_back({kv.first, "not in the reference (renamed or removed key?)"});
  }
  return issues;
}

}  // namespace i18n

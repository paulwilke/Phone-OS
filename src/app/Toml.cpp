#include "Toml.h"

#include <cstdio>
#include <set>

namespace toml {

// ---- Value ----------------------------------------------------------------------

Value Value::string(std::string s) {
  Value v;
  v.type_ = Type::String;
  v.text_ = std::move(s);
  return v;
}

Value Value::integer(int64_t n) {
  Value v;
  v.type_ = Type::Integer;
  v.number_ = n;
  return v;
}

Value Value::boolean(bool b) {
  Value v;
  v.type_ = Type::Boolean;
  v.flag_ = b;
  return v;
}

Value Value::array() {
  Value v;
  v.type_ = Type::Array;
  return v;
}

Value Value::table() { return Value(); }

Value Value::tableList() {
  Value v;
  v.type_ = Type::TableList;
  return v;
}

const Value* Value::find(const std::string& key) const {
  if (type_ != Type::Table) return nullptr;
  for (size_t i = 0; i < keys_.size(); ++i) {
    if (keys_[i] == key) return &items_[i];
  }
  return nullptr;
}

Value* Value::find(const std::string& key) {
  return const_cast<Value*>(static_cast<const Value*>(this)->find(key));
}

Value* Value::set(const std::string& key, Value value) {
  if (type_ != Type::Table) return nullptr;
  if (Value* existing = find(key)) {
    *existing = std::move(value);
    return existing;
  }
  keys_.push_back(key);
  items_.push_back(std::move(value));
  return &items_.back();
}

Value* Value::push(Value value) {
  const bool plain = value.type_ == Type::String || value.type_ == Type::Integer || value.type_ == Type::Boolean;
  if (type_ == Type::Array ? !plain : (type_ != Type::TableList || value.type_ != Type::Table)) return nullptr;
  items_.push_back(std::move(value));
  return &items_.back();
}

const char* codeName(Code code) {
  switch (code) {
    case Code::None: return "none";
    case Code::ExpectedKey: return "expected-key";
    case Code::ExpectedEquals: return "expected-equals";
    case Code::DottedKey: return "dotted-key";
    case Code::ExpectedValue: return "expected-value";
    case Code::UnreadableValue: return "unreadable-value";
    case Code::UnterminatedString: return "unterminated-string";
    case Code::MultilineString: return "multiline-string";
    case Code::UnknownEscape: return "unknown-escape";
    case Code::InvalidUnicode: return "invalid-unicode";
    case Code::ControlCharacter: return "control-character";
    case Code::InlineTable: return "inline-table";
    case Code::NestedArray: return "nested-array";
    case Code::ExpectedCommaOrEnd: return "expected-comma-or-end";
    case Code::UnterminatedArray: return "unterminated-array";
    case Code::UnterminatedHeader: return "unterminated-header";
    case Code::DuplicateKey: return "duplicate-key";
    case Code::DuplicateTable: return "duplicate-table";
    case Code::NameClash: return "name-clash";
    case Code::TooDeep: return "too-deep";
    case Code::TrailingGarbage: return "trailing-garbage";
  }
  return "unknown";
}

// ---- Parser ---------------------------------------------------------------------

namespace {

bool bareChar(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
}

void appendUtf8(std::string& s, unsigned cp) {
  if (cp < 0x80) {
    s += (char)cp;
  } else if (cp < 0x800) {
    s += (char)(0xC0 | (cp >> 6));
    s += (char)(0x80 | (cp & 0x3F));
  } else {
    s += (char)(0xE0 | (cp >> 12));
    s += (char)(0x80 | ((cp >> 6) & 0x3F));
    s += (char)(0x80 | (cp & 0x3F));
  }
}

int hexDigit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

struct Parser {
  std::string_view s;
  size_t p = 0;
  int line = 1;
  Error err;
  // Tables whose [header] has been written, by path; a second header for one of
  // them is an error, while a table that only exists as the parent of another one
  // may still be defined once. Elements of a list of tables are told apart by "#n".
  std::set<std::string> defined;

  bool fail(Code code) {
    if (!err.failed()) err = Error{code, line};
    return false;
  }

  bool atEnd() const { return p >= s.size(); }
  char cur() const { return s[p]; }
  bool lookingAt(char c) const { return p < s.size() && s[p] == c; }
  bool atLineBreak() const { return lookingAt('\n') || (lookingAt('\r') && p + 1 < s.size() && s[p + 1] == '\n'); }

  void skipSpaces() {
    while (p < s.size() && (s[p] == ' ' || s[p] == '\t')) ++p;
  }
  void skipComment() {
    if (lookingAt('#')) {
      while (p < s.size() && s[p] != '\n') ++p;
    }
  }
  // Blanks, comments and line breaks: between the entries and inside arrays.
  void skipBlank() {
    for (;;) {
      skipSpaces();
      skipComment();
      if (lookingAt('\r') && p + 1 < s.size() && s[p + 1] == '\n') {
        ++p;
      } else if (lookingAt('\n')) {
        ++p;
        ++line;
      } else {
        return;
      }
    }
  }

  bool readString(std::string& out) {
    out.clear();
    const char quote = s[p++];
    if (p + 1 < s.size() && s[p] == quote && s[p + 1] == quote) return fail(Code::MultilineString);
    for (;;) {
      if (atEnd() || atLineBreak()) return fail(Code::UnterminatedString);
      const char c = s[p];
      if (c == quote) {
        ++p;
        return true;
      }
      if ((unsigned char)c < 0x20 && c != '\t') return fail(Code::ControlCharacter);
      if (c == 0x7F) return fail(Code::ControlCharacter);
      if (quote == '"' && c == '\\') {
        ++p;
        if (atEnd()) return fail(Code::UnterminatedString);
        switch (s[p]) {
          case '"': out += '"'; break;
          case '\\': out += '\\'; break;
          case 'n': out += '\n'; break;
          case 't': out += '\t'; break;
          case 'r': out += '\r'; break;
          case 'u': {
            // Exactly four hex digits: a sign or a blank would be accepted by a
            // looser number reader.
            if (p + 4 >= s.size()) return fail(Code::InvalidUnicode);
            unsigned cp = 0;
            for (int i = 1; i <= 4; ++i) {
              const int d = hexDigit(s[p + i]);
              if (d < 0) return fail(Code::InvalidUnicode);
              cp = cp * 16 + (unsigned)d;
            }
            // Zero would end a C string early, and a surrogate half is no character.
            if (cp == 0 || (cp >= 0xD800 && cp <= 0xDFFF)) return fail(Code::InvalidUnicode);
            appendUtf8(out, cp);
            p += 4;
            break;
          }
          default:
            return fail(Code::UnknownEscape);
        }
        ++p;
      } else {
        out += c;
        ++p;
      }
    }
  }

  bool readKey(std::string& out) {
    out.clear();
    if (lookingAt('"') || lookingAt('\'')) return readString(out);
    while (p < s.size() && bareChar(s[p])) out += s[p++];
    return out.empty() ? fail(Code::ExpectedKey) : true;
  }

  bool readInteger(const std::string& token, Value& out) {
    size_t i = 0;
    bool negative = false;
    if (i < token.size() && (token[i] == '+' || token[i] == '-')) negative = token[i++] == '-';
    if (i == token.size()) return false;
    if (token[i] == '0' && i + 1 < token.size()) return false;  // no leading zeros
    uint64_t magnitude = 0;
    for (; i < token.size(); ++i) {
      if (token[i] < '0' || token[i] > '9') return false;
      const unsigned digit = (unsigned)(token[i] - '0');
      if (magnitude > (UINT64_MAX - digit) / 10) return false;
      magnitude = magnitude * 10 + digit;
    }
    const uint64_t limit = negative ? (uint64_t)INT64_MAX + 1 : (uint64_t)INT64_MAX;
    if (magnitude > limit) return false;
    out = Value::integer(negative ? (int64_t)(0 - magnitude) : (int64_t)magnitude);
    return true;
  }

  bool readScalar(Value& out) {
    if (atEnd()) return fail(Code::ExpectedValue);
    if (cur() == '"' || cur() == '\'') {
      std::string text;
      if (!readString(text)) return false;
      out = Value::string(std::move(text));
      return true;
    }
    if (cur() == '{') return fail(Code::InlineTable);
    if (cur() == '[') return fail(Code::NestedArray);
    const size_t start = p;
    while (p < s.size() && s[p] != ',' && s[p] != ']' && s[p] != '#' && s[p] != '\n' && s[p] != '\r' &&
           s[p] != ' ' && s[p] != '\t') {
      ++p;
    }
    const std::string token(s.substr(start, p - start));
    if (token.empty()) return fail(Code::ExpectedValue);
    if (token == "true") {
      out = Value::boolean(true);
      return true;
    }
    if (token == "false") {
      out = Value::boolean(false);
      return true;
    }
    return readInteger(token, out) ? true : fail(Code::UnreadableValue);
  }

  bool readValue(Value& out) {
    if (!lookingAt('[')) return readScalar(out);
    ++p;
    out = Value::array();
    for (;;) {
      skipBlank();
      if (atEnd()) return fail(Code::UnterminatedArray);
      if (cur() == ']') {
        ++p;
        return true;
      }
      Value item;
      if (!readScalar(item)) return false;
      out.push(std::move(item));
      skipBlank();
      if (atEnd()) return fail(Code::UnterminatedArray);
      if (cur() == ',') {
        ++p;
        continue;
      }
      if (cur() == ']') {
        ++p;
        return true;
      }
      return fail(Code::ExpectedCommaOrEnd);
    }
  }

  // The rest of a line after a value or a header: blanks and a comment only.
  bool endOfLine() {
    skipSpaces();
    skipComment();
    if (atEnd() || atLineBreak()) return true;
    return fail(Code::TrailingGarbage);
  }

  bool readPath(std::vector<std::string>& path) {
    path.clear();
    for (;;) {
      skipSpaces();
      std::string key;
      if (!readKey(key)) return false;
      path.push_back(key);
      if (path.size() > kMaxDepth) return fail(Code::TooDeep);
      skipSpaces();
      if (lookingAt('.')) {
        ++p;
        continue;
      }
      return true;
    }
  }

  // Walks down the path except for its last name and returns the table that holds
  // it. Names that do not exist yet become tables; a list of tables is entered at
  // its last element. `canon` receives the path with the element numbers.
  Value* descend(Value& root, const std::vector<std::string>& path, std::string& canon) {
    Value* table = &root;
    for (size_t i = 0; i + 1 < path.size(); ++i) {
      Value* next = table->find(path[i]);
      canon += path[i];
      if (!next) {
        next = table->set(path[i], Value::table());
      } else if (next->type() == Value::Type::TableList) {
        next = &next->at(next->size() - 1);
        canon += "#" + std::to_string(table->find(path[i])->size() - 1);
      } else if (next->type() != Value::Type::Table) {
        fail(Code::NameClash);
        return nullptr;
      }
      canon += '\x1f';
      table = next;
    }
    return table;
  }

  bool readHeader(Value& root, Value*& current) {
    ++p;  // [
    const bool listEntry = lookingAt('[');
    if (listEntry) ++p;
    std::vector<std::string> path;
    if (!readPath(path)) return false;
    if (!lookingAt(']')) return fail(Code::UnterminatedHeader);
    ++p;
    if (listEntry) {
      if (!lookingAt(']')) return fail(Code::UnterminatedHeader);
      ++p;
    }

    std::string canon;
    Value* parent = descend(root, path, canon);
    if (!parent) return false;
    const std::string& name = path.back();
    Value* existing = parent->find(name);
    if (listEntry) {
      if (!existing) existing = parent->set(name, Value::tableList());
      else if (existing->type() != Value::Type::TableList) return fail(Code::NameClash);
      current = existing->push(Value::table());
    } else {
      canon += name;
      if (!existing) {
        existing = parent->set(name, Value::table());
      } else if (existing->type() != Value::Type::Table) {
        return fail(Code::NameClash);
      } else if (defined.count(canon)) {
        return fail(Code::DuplicateTable);
      }
      defined.insert(canon);
      current = existing;
    }
    return endOfLine();
  }

  bool readEntry(Value& table) {
    std::string key;
    if (!readKey(key)) return false;
    skipSpaces();
    if (lookingAt('.')) return fail(Code::DottedKey);
    if (!lookingAt('=')) return fail(Code::ExpectedEquals);
    ++p;
    skipSpaces();
    if (const Value* existing = table.find(key)) {
      return fail(existing->type() == Value::Type::Table || existing->type() == Value::Type::TableList
                      ? Code::NameClash
                      : Code::DuplicateKey);
    }
    Value value;
    if (!readValue(value)) return false;
    table.set(key, std::move(value));
    return endOfLine();
  }

  bool run(Value& root) {
    if (s.size() >= 3 && s.substr(0, 3) == "\xEF\xBB\xBF") p = 3;
    Value* current = &root;
    for (;;) {
      skipBlank();
      if (atEnd()) return true;
      if (cur() == '[') {
        if (!readHeader(root, current)) return false;
      } else if (!readEntry(*current)) {
        return false;
      }
    }
  }
};

}  // namespace

Error parse(std::string_view text, Value& out) {
  Parser parser;
  parser.s = text;
  Value root = Value::table();
  if (!parser.run(root)) {
    out = Value::table();
    return parser.err;
  }
  out = std::move(root);
  return Error{};
}

// ---- Writer ---------------------------------------------------------------------

namespace {

void emitString(std::string& out, const std::string& text) {
  out += '"';
  for (unsigned char c : text) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\t': out += "\\t"; break;
      case '\r': out += "\\r"; break;
      default:
        if (c < 0x20 || c == 0x7F) {
          char buffer[8];
          snprintf(buffer, sizeof(buffer), "\\u%04X", c);
          out += buffer;
        } else {
          out += (char)c;
        }
    }
  }
  out += '"';
}

void emitKey(std::string& out, const std::string& key) {
  bool bare = !key.empty();
  for (char c : key) bare = bare && bareChar(c);
  if (bare) out += key;
  else emitString(out, key);
}

void emitScalar(std::string& out, const Value& v) {
  switch (v.type()) {
    case Value::Type::String: emitString(out, v.str()); break;
    case Value::Type::Integer: out += std::to_string(v.integer()); break;
    case Value::Type::Boolean: out += v.boolean() ? "true" : "false"; break;
    default: break;
  }
}

void emitTable(std::string& out, const Value& table, const std::string& path, size_t depth) {
  if (depth > kMaxDepth) return;
  for (size_t i = 0; i < table.size(); ++i) {
    const Value& v = table.at(i);
    if (v.type() == Value::Type::Table || v.type() == Value::Type::TableList) continue;
    emitKey(out, table.keyAt(i));
    out += " = ";
    if (v.type() == Value::Type::Array) {
      out += '[';
      for (size_t k = 0; k < v.size(); ++k) {
        if (k) out += ", ";
        emitScalar(out, v.at(k));
      }
      out += ']';
    } else {
      emitScalar(out, v);
    }
    out += '\n';
  }
  for (size_t i = 0; i < table.size(); ++i) {
    if (table.at(i).type() != Value::Type::Table) continue;
    std::string sub = path;
    if (!sub.empty()) sub += '.';
    emitKey(sub, table.keyAt(i));
    out += "\n[" + sub + "]\n";
    emitTable(out, table.at(i), sub, depth + 1);
  }
  for (size_t i = 0; i < table.size(); ++i) {
    const Value& list = table.at(i);
    if (list.type() != Value::Type::TableList) continue;
    std::string sub = path;
    if (!sub.empty()) sub += '.';
    emitKey(sub, table.keyAt(i));
    for (size_t k = 0; k < list.size(); ++k) {
      out += "\n[[" + sub + "]]\n";
      emitTable(out, list.at(k), sub, depth + 1);
    }
  }
}

}  // namespace

std::string emit(const Value& root) {
  std::string out;
  if (root.type() != Value::Type::Table) return out;
  emitTable(out, root, "", 1);
  // The file starts with its first entry, not with a blank line.
  size_t lead = 0;
  while (lead < out.size() && out[lead] == '\n') ++lead;
  return out.substr(lead);
}

}  // namespace toml

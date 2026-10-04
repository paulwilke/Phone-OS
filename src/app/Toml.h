#pragma once

// The small subset of TOML the device's configuration is written in, as a plain
// document tree: a parser and a writer, independent of any JSON library.
//
// Why TOML: it is made for people. Comments, sections, no commas and braces as in
// JSON, and widely known, which matters because parents edit these files. Why only
// a subset: a complete parser would be bigger than everything it has to read, and a
// variant that never occurs cannot go wrong.
//
// Supported:
//   # comment            on its own line or after a value
//   key = "text"         basic strings with \" \\ \n \t \r \uXXXX, and 'literal strings'
//   key = 42             integers, also negative (no leading zeros)
//   key = true | false
//   key = ["a", "b"]     arrays of plain values, also across several lines
//   [table]  [a.b]       tables, also nested
//   [[list]] [[a.b]]     lists of tables (several contacts, tiles, …)
// Refused with an error and the line number, never swallowed silently:
//   floating-point numbers, dates, inline tables { }, multi-line strings, dotted keys
//   (a.b = 1; sections do that), arrays inside arrays, a table defined twice, a name
//   used for two different things, nesting deeper than kMaxDepth.
//
// A UTF-8 byte order mark at the start is skipped. The parser holds the whole
// document in memory: callers limit the size of the file they hand over.
//
// The module returns an error code and the line, never a sentence; the interface
// words the problem for the parents from the catalogs.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace toml {

// Tables nested deeper than this are refused. The writer recurses along the
// nesting, and a hostile file must not be able to exhaust the stack.
constexpr size_t kMaxDepth = 8;

class Value {
public:
  enum class Type {
    String,
    Integer,
    Boolean,
    Array,      // plain values in a row: ["a", "b"] (may be empty)
    Table,      // named values, in the order they were written
    TableList,  // [[name]]: a list of tables
  };

  Value() : type_(Type::Table) {}

  static Value string(std::string s);
  static Value integer(int64_t n);
  static Value boolean(bool b);
  static Value array();
  static Value table();
  static Value tableList();

  Type type() const { return type_; }

  // Valid for the matching type only.
  const std::string& str() const { return text_; }
  int64_t integer() const { return number_; }
  bool boolean() const { return flag_; }

  // Arrays and table lists: the elements. Tables: the entries (see keyAt()).
  size_t size() const { return items_.size(); }
  const Value& at(size_t i) const { return items_[i]; }
  Value& at(size_t i) { return items_[i]; }

  // Tables only.
  const std::string& keyAt(size_t i) const { return keys_[i]; }
  const Value* find(const std::string& key) const;
  Value* find(const std::string& key);
  // Sets an entry, replacing a value under the same key and keeping its place.
  // Returns the stored value, or null when this is not a table.
  Value* set(const std::string& key, Value value);

  // Appends an element and returns it, or null when it does not fit: an array
  // takes plain values (string, integer, boolean), a table list takes tables, and
  // nothing else takes elements.
  Value* push(Value value);

private:
  Type type_;
  std::string text_;
  int64_t number_ = 0;
  bool flag_ = false;
  std::vector<std::string> keys_;  // tables: the key of items_[i]
  std::vector<Value> items_;
};

enum class Code {
  None,
  ExpectedKey,          // a key (or a table name) was expected
  ExpectedEquals,       // "key" without "="
  DottedKey,            // a.b = 1: use a section instead
  ExpectedValue,        // "key =" with nothing after it
  UnreadableValue,      // not text, an integer or true/false (floats, dates, 007, 99999999999999999999)
  UnterminatedString,   // a string runs into the end of the line or the file
  MultilineString,      // """ or '''
  UnknownEscape,        // a backslash followed by something that is not allowed
  InvalidUnicode,       // \u with a bad number, a surrogate or zero
  ControlCharacter,     // a raw control character inside a string
  InlineTable,          // { }
  NestedArray,          // [[1]] inside a value
  ExpectedCommaOrEnd,   // inside an array
  UnterminatedArray,    // the file ended inside an array
  UnterminatedHeader,   // "[table" without "]"
  DuplicateKey,         // the same key twice in one table
  DuplicateTable,       // the same [table] written twice
  NameClash,            // a name used as a value and as a table, or as a list and a table
  TooDeep,              // more than kMaxDepth levels
  TrailingGarbage,      // something after a value or header on the same line
};

struct Error {
  Code code = Code::None;
  int line = 0;  // 1-based; 0 when there is no error
  bool failed() const { return code != Code::None; }
};

// A stable lower-case identifier of the code ("unterminated-string"), for log
// lines and as the end of a catalog key. Never shown to the user as it is.
const char* codeName(Code code);

// Parses `text` into `out`, which becomes a table. On an error `out` is an empty
// table and the error says what and where.
Error parse(std::string_view text, Value& out);

// Writes a table as TOML: first the plain values and arrays, then the sub-tables,
// then the lists of tables, each group in the order of the entries. Comments are
// not kept; the structure and the order are. An empty list of tables has no
// representation in the format and is not written; a string holding a zero byte
// is written as \u0000, which the parser refuses.
std::string emit(const Value& root);

}  // namespace toml

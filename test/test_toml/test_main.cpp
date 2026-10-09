// Host tests for the TOML subset: reading, writing, and what is refused.
// allow-non-english: some strings contain umlauts and escapes for them.

#include <unity.h>

#include <cstdint>
#include <cstring>
#include <string>

#include <app/Toml.h>

using toml::Code;
using toml::Value;

void setUp() {}
void tearDown() {}

namespace {

struct Doc {
  Value root;
  toml::Error err;
  explicit Doc(const std::string& text) { err = toml::parse(text, root); }
  const Value* operator[](const char* key) const { return root.find(key); }
};

// A string value at a path of keys, or "<missing>".
std::string str(const Value* v) { return v && v->type() == Value::Type::String ? v->str() : "<missing>"; }

bool same(const Value& a, const Value& b) {
  if (a.type() != b.type()) return false;
  switch (a.type()) {
    case Value::Type::String: return a.str() == b.str();
    case Value::Type::Integer: return a.integer() == b.integer();
    case Value::Type::Boolean: return a.boolean() == b.boolean();
    case Value::Type::Array:
    case Value::Type::TableList:
      if (a.size() != b.size()) return false;
      for (size_t i = 0; i < a.size(); ++i) {
        if (!same(a.at(i), b.at(i))) return false;
      }
      return true;
    case Value::Type::Table:
      // Entries of different kinds are written in groups, so their order may change.
      if (a.size() != b.size()) return false;
      for (size_t i = 0; i < a.size(); ++i) {
        const Value* other = b.find(a.keyAt(i));
        if (!other || !same(a.at(i), *other)) return false;
      }
      return true;
  }
  return false;
}

}  // namespace

// ---------------------------------------------------------------------------
// Reading
// ---------------------------------------------------------------------------

void test_scalars_and_comments() {
  Doc x("# header\nname = \"Alex\"  # trailing\nfont_scale = 2\nradio = false\nneg = -15\nplus = +7\n");
  TEST_ASSERT_FALSE(x.err.failed());
  TEST_ASSERT_EQUAL_STRING("Alex", str(x["name"]).c_str());
  TEST_ASSERT_EQUAL(2, x["font_scale"]->integer());
  TEST_ASSERT_FALSE(x["radio"]->boolean());
  TEST_ASSERT_EQUAL(-15, x["neg"]->integer());
  TEST_ASSERT_EQUAL(7, x["plus"]->integer());
  TEST_ASSERT_EQUAL(0, x.err.line);
}

void test_strings_escapes_and_literals() {
  Doc x("a = \"line\\ntwo \\\"quote\\\" \\\\ \\u00E4 \\t|\"\nb = 'C:\\raw\\n'\nc = \"\"\nd = ''\n");
  TEST_ASSERT_FALSE(x.err.failed());
  TEST_ASSERT_EQUAL_STRING("line\ntwo \"quote\" \\ \xC3\xA4 \t|", str(x["a"]).c_str());
  TEST_ASSERT_EQUAL_STRING("C:\\raw\\n", str(x["b"]).c_str());  // literal: no escapes
  TEST_ASSERT_EQUAL_STRING("", str(x["c"]).c_str());
  TEST_ASSERT_EQUAL_STRING("", str(x["d"]).c_str());
}

void test_unicode_escapes_cover_one_two_and_three_bytes() {
  Doc x("a = \"\\u0041\\u00e9\\u20AC\"\n");
  TEST_ASSERT_FALSE(x.err.failed());
  TEST_ASSERT_EQUAL_STRING("A\xC3\xA9\xE2\x82\xAC", str(x["a"]).c_str());
}

void test_a_hash_inside_a_string_is_not_a_comment() {
  Doc x("pw = \"abc#def\"\nlit = 'x # y'\n");
  TEST_ASSERT_EQUAL_STRING("abc#def", str(x["pw"]).c_str());
  TEST_ASSERT_EQUAL_STRING("x # y", str(x["lit"]).c_str());
}

void test_tables_and_nesting() {
  Doc x("[features]\nradio = true\n[wifi.access_point]\nssid = \"phone-os\"\n");
  TEST_ASSERT_FALSE(x.err.failed());
  TEST_ASSERT_TRUE(x["features"]->find("radio")->boolean());
  TEST_ASSERT_EQUAL_STRING("phone-os", str(x["wifi"]->find("access_point")->find("ssid")).c_str());
}

void test_spaces_around_header_names_are_fine() {
  Doc x("[ a . b ]\nk = 1\n");
  TEST_ASSERT_FALSE(x.err.failed());
  TEST_ASSERT_EQUAL(1, x["a"]->find("b")->find("k")->integer());
}

void test_quoted_keys() {
  Doc x("\"odd key\" = 1\n\"\" = 2\n['quoted table']\n\"x.y\" = 3\n");
  TEST_ASSERT_FALSE(x.err.failed());
  TEST_ASSERT_EQUAL(1, x["odd key"]->integer());
  TEST_ASSERT_EQUAL(2, x[""]->integer());
  TEST_ASSERT_EQUAL(3, x["quoted table"]->find("x.y")->integer());
}

void test_list_of_tables() {
  Doc x("[[contact]]\nnumber = \"+491701111111\"\nrole = \"parent\"\n\n[[contact]]\nnumber = \"+491702222222\"\n");
  TEST_ASSERT_FALSE(x.err.failed());
  const Value* list = x["contact"];
  TEST_ASSERT_TRUE(list->type() == Value::Type::TableList);
  TEST_ASSERT_EQUAL(2, list->size());
  TEST_ASSERT_EQUAL_STRING("+491701111111", str(list->at(0).find("number")).c_str());
  TEST_ASSERT_EQUAL_STRING("parent", str(list->at(0).find("role")).c_str());
  TEST_ASSERT_EQUAL_STRING("+491702222222", str(list->at(1).find("number")).c_str());
  TEST_ASSERT_NULL(list->at(1).find("role"));
}

void test_a_list_of_tables_below_a_table() {
  Doc x("[wifi]\nauto_off = 10\n[[wifi.network]]\nssid = \"A\"\n[[wifi.network]]\nssid = \"B\"\n");
  TEST_ASSERT_FALSE(x.err.failed());
  const Value* wifi = x["wifi"];
  TEST_ASSERT_EQUAL(10, wifi->find("auto_off")->integer());
  TEST_ASSERT_EQUAL(2, wifi->find("network")->size());
  TEST_ASSERT_EQUAL_STRING("B", str(wifi->find("network")->at(1).find("ssid")).c_str());
}

// A sub-table header after [[list]] belongs to the last element of the list.
void test_a_sub_table_of_a_list_element() {
  Doc x("[[tile]]\nname = \"one\"\n[tile.style]\nbold = true\n[[tile]]\nname = \"two\"\n[tile.style]\nbold = false\n");
  TEST_ASSERT_FALSE(x.err.failed());
  const Value* tiles = x["tile"];
  TEST_ASSERT_EQUAL(2, tiles->size());
  TEST_ASSERT_TRUE(tiles->at(0).find("style")->find("bold")->boolean());
  TEST_ASSERT_FALSE(tiles->at(1).find("style")->find("bold")->boolean());
}

void test_arrays_inline_and_across_lines() {
  Doc x("screens = [\"home\", \"call\"]\nmore = [\n  \"a\", # first\n  \"b\",\n]\nn = [1, 2, 3]\nnone = []\nmixed = [1, \"a\", true]\n");
  TEST_ASSERT_FALSE(x.err.failed());
  TEST_ASSERT_EQUAL(2, x["screens"]->size());
  TEST_ASSERT_EQUAL_STRING("call", str(&x["screens"]->at(1)).c_str());
  TEST_ASSERT_EQUAL(2, x["more"]->size());
  TEST_ASSERT_EQUAL(3, x["n"]->at(2).integer());
  TEST_ASSERT_TRUE(x["none"]->type() == Value::Type::Array);
  TEST_ASSERT_EQUAL(0, x["none"]->size());
  TEST_ASSERT_EQUAL(3, x["mixed"]->size());
}

void test_windows_line_endings() {
  Doc x("a = 1\r\n[t]\r\nb = \"x\"\r\nl = [\r\n1,\r\n2\r\n]\r\n");
  TEST_ASSERT_FALSE(x.err.failed());
  TEST_ASSERT_EQUAL_STRING("x", str(x["t"]->find("b")).c_str());
  TEST_ASSERT_EQUAL(2, x["t"]->find("l")->size());
}

// Windows editors put a byte order mark at the start of a UTF-8 file.
void test_a_byte_order_mark_is_skipped() {
  Doc x("\xEF\xBB\xBFname = \"Alex\"\n");
  TEST_ASSERT_FALSE(x.err.failed());
  TEST_ASSERT_EQUAL_STRING("Alex", str(x["name"]).c_str());
}

void test_the_last_line_needs_no_line_break() {
  Doc x("a = 1\nb = 2");
  TEST_ASSERT_FALSE(x.err.failed());
  TEST_ASSERT_EQUAL(2, x["b"]->integer());
  Doc y("[t]");
  TEST_ASSERT_FALSE(y.err.failed());
  TEST_ASSERT_NOT_NULL(y["t"]);
}

void test_empty_input_is_an_empty_document() {
  Doc x("\n# only a comment\n   \n");
  TEST_ASSERT_FALSE(x.err.failed());
  TEST_ASSERT_TRUE(x.root.type() == Value::Type::Table);
  TEST_ASSERT_EQUAL(0, x.root.size());
  Doc y("");
  TEST_ASSERT_FALSE(y.err.failed());
}

void test_integers_at_the_limits() {
  Doc x("max = 9223372036854775807\nmin = -9223372036854775808\nzero = 0\nnegzero = -0\n");
  TEST_ASSERT_FALSE(x.err.failed());
  TEST_ASSERT_EQUAL_INT64(INT64_MAX, x["max"]->integer());
  TEST_ASSERT_EQUAL_INT64(INT64_MIN, x["min"]->integer());
  TEST_ASSERT_EQUAL_INT64(0, x["zero"]->integer());
  TEST_ASSERT_EQUAL_INT64(0, x["negzero"]->integer());
}

// ---------------------------------------------------------------------------
// What is refused, with its code and line
// ---------------------------------------------------------------------------

void test_unsupported_and_malformed_input_is_refused() {
  struct Case { const char* text; Code code; };
  const Case cases[] = {
      {"a = 1.5\n", Code::UnreadableValue},
      {"a = 1979-05-27\n", Code::UnreadableValue},
      {"a = 007\n", Code::UnreadableValue},
      {"a = 99999999999999999999\n", Code::UnreadableValue},
      {"a = 9223372036854775808\n", Code::UnreadableValue},
      {"a = -9223372036854775809\n", Code::UnreadableValue},
      {"a = -\n", Code::UnreadableValue},
      {"a = 1_000\n", Code::UnreadableValue},
      {"a = TRUE\n", Code::UnreadableValue},
      {"a = {x = 1}\n", Code::InlineTable},
      {"a.b = 1\n", Code::DottedKey},
      {"a = \"\"\"x\"\"\"\n", Code::MultilineString},
      {"a = '''x'''\n", Code::MultilineString},
      {"a = [[1]]\n", Code::NestedArray},
      {"a = [1, [2]]\n", Code::NestedArray},
      {"a = \"open\n", Code::UnterminatedString},
      {"a = \"open", Code::UnterminatedString},
      {"a = 'open\n", Code::UnterminatedString},
      {"a = \"x\\", Code::UnterminatedString},
      {"= 5\n", Code::ExpectedKey},
      {"a 5\n", Code::ExpectedEquals},
      {"a =\n", Code::ExpectedValue},
      {"a = # comment\n", Code::ExpectedValue},
      {"a = [1,,2]\n", Code::ExpectedValue},
      {"[t\n", Code::UnterminatedHeader},
      {"[[t]\n", Code::UnterminatedHeader},
      {"[]\n", Code::ExpectedKey},
      {"[a..b]\n", Code::ExpectedKey},
      {"[a.]\n", Code::ExpectedKey},
      {"a = 1 b = 2\n", Code::TrailingGarbage},
      {"[t] x\n", Code::TrailingGarbage},
      {"a = \"x\" y\n", Code::TrailingGarbage},
      {"a = 1\na = 2\n", Code::DuplicateKey},
      {"a = [1 2]\n", Code::ExpectedCommaOrEnd},
      {"a = [1, 2\n", Code::UnterminatedArray},
      {"a = [1, 2\nb = 3\n", Code::ExpectedCommaOrEnd},
      {"a = [1, 2", Code::UnterminatedArray},
      {"a = \"\\x\"\n", Code::UnknownEscape},
      {"a = \"\\b\"\n", Code::UnknownEscape},
      {"a = \"\\u12\"\n", Code::InvalidUnicode},
      {"a = \"\\u12G4\"\n", Code::InvalidUnicode},
      {"a = \"\\u+123\"\n", Code::InvalidUnicode},
      {"a = \"\\u 123\"\n", Code::InvalidUnicode},
      {"a = \"\\u0000\"\n", Code::InvalidUnicode},
      {"a = \"\\uD800\"\n", Code::InvalidUnicode},
      {"a = \"\\uDFFF\"\n", Code::InvalidUnicode},
      {"a = \"tab\x01here\"\n", Code::ControlCharacter},
      {"a = \"del\x7F\"\n", Code::ControlCharacter},
      {"a = 1\n[a]\nb = 2\n", Code::NameClash},
      {"[[a]]\nx = 1\n[a]\ny = 2\n", Code::NameClash},
      {"[a]\nx = 1\n[[a]]\n", Code::NameClash},
      {"a = []\n[[a]]\n", Code::NameClash},
      {"a = 1\n[a.b]\n", Code::NameClash},
      {"a = []\n[a.b]\n", Code::NameClash},
      {"[a.b]\n[a]\nb = 1\n", Code::NameClash},
      {"[t]\nsub = 1\n[t.sub]\n", Code::NameClash},
      {"[a]\nx = 1\n[a]\ny = 2\n", Code::DuplicateTable},
      {"[a.b]\n[a]\n[a]\n", Code::DuplicateTable},
      {"[[l]]\n[l.t]\n[l.t]\n", Code::DuplicateTable},
      {"[a.b.c.d.e.f.g.h.i]\n", Code::TooDeep},
  };
  for (const Case& c : cases) {
    Doc x(c.text);
    TEST_ASSERT_TRUE_MESSAGE(x.err.failed(), c.text);
    TEST_ASSERT_EQUAL_MESSAGE((int)c.code, (int)x.err.code, c.text);
    TEST_ASSERT_TRUE_MESSAGE(x.err.line >= 1, c.text);
    TEST_ASSERT_EQUAL_MESSAGE(0, x.root.size(), c.text);  // nothing half-read is left over
  }
}

void test_the_error_names_the_line() {
  Doc a("ok = 1\n\nbroken\n");
  TEST_ASSERT_EQUAL((int)Code::ExpectedEquals, (int)a.err.code);
  TEST_ASSERT_EQUAL(3, a.err.line);

  Doc b("a = [\n  1,\n  2,\n  x\n]\n");
  TEST_ASSERT_EQUAL((int)Code::UnreadableValue, (int)b.err.code);
  TEST_ASSERT_EQUAL(4, b.err.line);

  Doc c("# one\r\n# two\r\na = 1\r\na = 2\r\n");
  TEST_ASSERT_EQUAL((int)Code::DuplicateKey, (int)c.err.code);
  TEST_ASSERT_EQUAL(4, c.err.line);

  Doc d("a = 1\n[t]\nx = 1\n[t]\n");
  TEST_ASSERT_EQUAL(4, d.err.line);
}

// A table that exists only as the parent of another may be defined once, later.
void test_a_parent_table_may_follow_its_child() {
  Doc x("[wifi.access_point]\nssid = \"a\"\n[wifi]\nauto_off = 5\n");
  TEST_ASSERT_FALSE(x.err.failed());
  TEST_ASSERT_EQUAL(5, x["wifi"]->find("auto_off")->integer());
  TEST_ASSERT_EQUAL_STRING("a", str(x["wifi"]->find("access_point")->find("ssid")).c_str());
}

// The same sub-table name inside different list elements is not a repeat.
void test_the_same_sub_table_in_two_list_elements_is_fine() {
  Doc x("[[l]]\n[l.t]\nk = 1\n[[l]]\n[l.t]\nk = 2\n");
  TEST_ASSERT_FALSE(x.err.failed());
  TEST_ASSERT_EQUAL(2, x["l"]->at(1).find("t")->find("k")->integer());
}

// The bookkeeping of defined tables is per list element: an implicit table in the
// second element must not count as defined because the first element defined one.
void test_defined_tables_are_tracked_per_list_element() {
  Doc x("[[l]]\n[l.t]\nk = 1\n[[l]]\n[l.t.u]\nk = 2\n[l.t]\nj = 3\n");
  TEST_ASSERT_FALSE(x.err.failed());
  TEST_ASSERT_EQUAL(3, x["l"]->at(1).find("t")->find("j")->integer());
  Doc y("[[l]]\n[l.t]\n[[l]]\n[l.t]\n[l.t]\n");  // twice in the same element is still an error
  TEST_ASSERT_TRUE(y.err.code == Code::DuplicateTable);
}

void test_every_code_has_a_distinct_name() {
  const Code all[] = {Code::None, Code::ExpectedKey, Code::ExpectedEquals, Code::DottedKey, Code::ExpectedValue,
                      Code::UnreadableValue, Code::UnterminatedString, Code::MultilineString, Code::UnknownEscape,
                      Code::InvalidUnicode, Code::ControlCharacter, Code::InlineTable, Code::NestedArray,
                      Code::ExpectedCommaOrEnd, Code::UnterminatedArray, Code::UnterminatedHeader,
                      Code::DuplicateKey, Code::DuplicateTable, Code::NameClash, Code::TooDeep, Code::TrailingGarbage};
  for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); ++i) {
    TEST_ASSERT_TRUE(strcmp(toml::codeName(all[i]), "unknown") != 0);
    for (size_t j = i + 1; j < sizeof(all) / sizeof(all[0]); ++j) {
      TEST_ASSERT_TRUE(strcmp(toml::codeName(all[i]), toml::codeName(all[j])) != 0);
    }
  }
}

// The writer recurses along the nesting; a file must not be able to make it deep.
void test_the_nesting_limit_is_exactly_kMaxDepth() {
  std::string ok = "[a";
  for (size_t i = 1; i < toml::kMaxDepth; ++i) ok += ".a";
  Doc fine(ok + "]\n");
  TEST_ASSERT_FALSE(fine.err.failed());
  Doc deep(ok + ".a]\n");
  TEST_ASSERT_TRUE(deep.err.code == Code::TooDeep);
  Doc hostile("[" + std::string(2000, 'a') + std::string(400, '.') + "]\n");
  TEST_ASSERT_TRUE(hostile.err.failed());
}

// ---------------------------------------------------------------------------
// Building and writing
// ---------------------------------------------------------------------------

void test_the_value_api_refuses_misuse() {
  Value table = Value::table();
  TEST_ASSERT_NOT_NULL(table.set("a", Value::integer(1)));
  TEST_ASSERT_NULL(table.push(Value::integer(1)));  // a table takes entries, not elements
  Value array = Value::array();
  TEST_ASSERT_NOT_NULL(array.push(Value::string("x")));
  TEST_ASSERT_NULL(array.push(Value::array()));  // no arrays in arrays
  TEST_ASSERT_NULL(array.push(Value::table()));
  TEST_ASSERT_NULL(array.set("k", Value::integer(1)));
  Value list = Value::tableList();
  TEST_ASSERT_NOT_NULL(list.push(Value::table()));
  TEST_ASSERT_NULL(list.push(Value::integer(1)));
  Value scalar = Value::integer(3);
  TEST_ASSERT_NULL(scalar.push(Value::integer(1)));
  TEST_ASSERT_NULL(scalar.find("a"));
}

void test_set_replaces_in_place_and_keeps_the_order() {
  Value t = Value::table();
  t.set("a", Value::integer(1));
  t.set("b", Value::integer(2));
  t.set("a", Value::string("new"));
  TEST_ASSERT_EQUAL(2, t.size());
  TEST_ASSERT_EQUAL_STRING("a", t.keyAt(0).c_str());
  TEST_ASSERT_EQUAL_STRING("new", t.at(0).str().c_str());
}

void test_emit_orders_plain_values_before_sections() {
  Value root = Value::table();
  root.set("features", Value::table())->set("radio", Value::boolean(true));
  root.set("name", Value::string("x"));
  const std::string out = toml::emit(root);
  TEST_ASSERT_TRUE(out.find("name") < out.find("[features]"));
  TEST_ASSERT_EQUAL(0, out.rfind("name = \"x\"", 0));  // no blank line at the start
  TEST_ASSERT_EQUAL_STRING("name = \"x\"\n\n[features]\nradio = true\n", out.c_str());
}

void test_emit_writes_lists_of_tables_with_dotted_headers() {
  Doc x("[wifi]\nauto_off = 10\n[[wifi.network]]\nssid = \"A\"\n[[wifi.network]]\nssid = \"B\"\n");
  TEST_ASSERT_EQUAL_STRING("[wifi]\nauto_off = 10\n\n[[wifi.network]]\nssid = \"A\"\n\n[[wifi.network]]\nssid = \"B\"\n",
                           toml::emit(x.root).c_str());
}

// Text from the web page can hold quotes, line breaks and control characters; the
// file must stay readable.
void test_emit_escapes_control_characters() {
  Value root = Value::table();
  root.set("t", Value::string(std::string("a\"b\nc\x01\x7F\\ \xC3\xA4")));
  const std::string out = toml::emit(root);
  Doc back(out);
  TEST_ASSERT_FALSE_MESSAGE(back.err.failed(), out.c_str());
  TEST_ASSERT_EQUAL_STRING("a\"b\nc\x01\x7F\\ \xC3\xA4", str(back["t"]).c_str());
}

void test_emit_quotes_keys_that_are_not_bare() {
  Value root = Value::table();
  root.set("odd key", Value::integer(1));
  root.set("", Value::integer(2));
  root.set("x.y", Value::table())->set("a b", Value::integer(3));
  const std::string out = toml::emit(root);
  Doc back(out);
  TEST_ASSERT_FALSE_MESSAGE(back.err.failed(), out.c_str());
  TEST_ASSERT_TRUE(same(root, back.root));
}

// Empty tables and empty arrays have a spelling and survive; an empty list of tables does not.
void test_empty_containers() {
  Value root = Value::table();
  root.set("empty_table", Value::table());
  root.set("empty_array", Value::array());
  root.set("no_contacts", Value::tableList());
  const std::string out = toml::emit(root);
  Doc back(out);
  TEST_ASSERT_FALSE_MESSAGE(back.err.failed(), out.c_str());
  TEST_ASSERT_TRUE(back["empty_table"]->type() == Value::Type::Table);
  TEST_ASSERT_TRUE(back["empty_array"]->type() == Value::Type::Array);
  TEST_ASSERT_NULL(back["no_contacts"]);
}

void test_a_non_table_root_writes_nothing() {
  TEST_ASSERT_EQUAL_STRING("", toml::emit(Value::integer(1)).c_str());
}

void test_round_trip_keeps_structure_and_odd_characters() {
  const char* src =
      "name = \"Alex \\\"A\\\"\"\nfont_scale = 2\nflag = true\nneg = -9223372036854775808\n"
      "screens = [\"home\", \"call\"]\nnumbers = [1, -2, 3]\n"
      "pw = \"with # and \\\\ and \\u00E4\"\n\n"
      "[features]\nradio = false\n\n"
      "[wifi.access_point]\nssid = \"x y\"\n\n"
      "[[contact]]\nnumber = \"+491701111111\"\nrole = \"parent\"\n\n"
      "[[contact]]\nnumber = \"+491702222222\"\n\n"
      "[[wifi.network]]\nssid = \"A\"\npassword = \"p\"\n\n"
      "[wifi.network.extra]\nk = 1\n";
  Doc a(src);
  TEST_ASSERT_FALSE(a.err.failed());
  const std::string out = toml::emit(a.root);
  Doc b(out);
  TEST_ASSERT_FALSE_MESSAGE(b.err.failed(), out.c_str());
  TEST_ASSERT_TRUE_MESSAGE(same(a.root, b.root), out.c_str());
  // And once more: writing what was read back gives the same text.
  TEST_ASSERT_EQUAL_STRING(out.c_str(), toml::emit(b.root).c_str());
}

// Random documents: whatever the writer writes, the parser reads back unchanged.
namespace {

struct Rng {
  uint32_t state;
  uint32_t next() {
    state = state * 1664525u + 1013904223u;
    return state >> 8;
  }
  size_t below(size_t n) { return (size_t)(next() % n); }
};

std::string randomText(Rng& r) {
  static const char* const pieces[] = {"a", "Z", "0", " ", "#", "=", "[", "]", ".", ",", "\"", "'", "\\", "\n", "\t",
                                       "\r", "\x01", "\x7F", "\xC3\xA4", "\xE2\x82\xAC", "{", "}", "-", "_"};
  std::string s;
  const size_t n = r.below(8);
  for (size_t i = 0; i < n; ++i) s += pieces[r.below(sizeof(pieces) / sizeof(pieces[0]))];
  return s;
}

Value randomScalar(Rng& r) {
  switch (r.below(4)) {
    case 0: return Value::string(randomText(r));
    case 1: return Value::integer((int64_t)((uint64_t)r.next() << 40 | (uint64_t)r.next()) >> r.below(60));
    case 2: return Value::integer(-(int64_t)r.next());
    default: return Value::boolean(r.below(2) == 0);
  }
}

Value randomTable(Rng& r, size_t depth) {
  Value t = Value::table();
  const size_t n = r.below(6);
  for (size_t i = 0; i < n; ++i) {
    const std::string key = r.below(3) == 0 ? randomText(r) : std::string(1, (char)('a' + r.below(26)));
    switch (r.below(depth >= 3 ? 2 : 5)) {
      case 0: t.set(key, randomScalar(r)); break;
      case 1: {
        Value a = Value::array();
        for (size_t k = r.below(4); k > 0; --k) a.push(randomScalar(r));
        t.set(key, std::move(a));
        break;
      }
      case 2: t.set(key, randomTable(r, depth + 1)); break;
      default: {
        Value list = Value::tableList();
        for (size_t k = 1 + r.below(3); k > 0; --k) list.push(randomTable(r, depth + 1));
        t.set(key, std::move(list));
      }
    }
  }
  return t;
}

}  // namespace

void test_random_documents_survive_a_round_trip() {
  for (uint32_t seed = 1; seed <= 300; ++seed) {
    Rng r{seed};
    const Value original = randomTable(r, 0);
    const std::string text = toml::emit(original);
    Doc back(text);
    char label[40];
    snprintf(label, sizeof(label), "seed %u", (unsigned)seed);
    TEST_ASSERT_FALSE_MESSAGE(back.err.failed(), (std::string(label) + ": " + text).c_str());
    TEST_ASSERT_TRUE_MESSAGE(same(original, back.root), (std::string(label) + ": " + text).c_str());
  }
}

// Whatever bytes arrive, the parser ends with an answer and never reads past the end.
void test_random_bytes_never_crash_the_parser() {
  Rng r{12345};
  for (int i = 0; i < 2000; ++i) {
    std::string text;
    const size_t n = r.below(60);
    for (size_t k = 0; k < n; ++k) {
      static const char alphabet[] = "ab =[]\"'#\\.\n\r\t,{}1-u0\xC3";
      text += alphabet[r.below(sizeof(alphabet) - 1)];
    }
    Doc x(text);
    TEST_ASSERT_TRUE(x.err.failed() ? x.root.size() == 0 : true);
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_scalars_and_comments);
  RUN_TEST(test_strings_escapes_and_literals);
  RUN_TEST(test_unicode_escapes_cover_one_two_and_three_bytes);
  RUN_TEST(test_a_hash_inside_a_string_is_not_a_comment);
  RUN_TEST(test_tables_and_nesting);
  RUN_TEST(test_spaces_around_header_names_are_fine);
  RUN_TEST(test_quoted_keys);
  RUN_TEST(test_list_of_tables);
  RUN_TEST(test_a_list_of_tables_below_a_table);
  RUN_TEST(test_a_sub_table_of_a_list_element);
  RUN_TEST(test_arrays_inline_and_across_lines);
  RUN_TEST(test_windows_line_endings);
  RUN_TEST(test_a_byte_order_mark_is_skipped);
  RUN_TEST(test_the_last_line_needs_no_line_break);
  RUN_TEST(test_empty_input_is_an_empty_document);
  RUN_TEST(test_integers_at_the_limits);
  RUN_TEST(test_unsupported_and_malformed_input_is_refused);
  RUN_TEST(test_the_error_names_the_line);
  RUN_TEST(test_a_parent_table_may_follow_its_child);
  RUN_TEST(test_the_same_sub_table_in_two_list_elements_is_fine);
  RUN_TEST(test_defined_tables_are_tracked_per_list_element);
  RUN_TEST(test_every_code_has_a_distinct_name);
  RUN_TEST(test_the_nesting_limit_is_exactly_kMaxDepth);
  RUN_TEST(test_the_value_api_refuses_misuse);
  RUN_TEST(test_set_replaces_in_place_and_keeps_the_order);
  RUN_TEST(test_emit_orders_plain_values_before_sections);
  RUN_TEST(test_emit_writes_lists_of_tables_with_dotted_headers);
  RUN_TEST(test_emit_escapes_control_characters);
  RUN_TEST(test_emit_quotes_keys_that_are_not_bare);
  RUN_TEST(test_empty_containers);
  RUN_TEST(test_a_non_table_root_writes_nothing);
  RUN_TEST(test_round_trip_keeps_structure_and_odd_characters);
  RUN_TEST(test_random_documents_survive_a_round_trip);
  RUN_TEST(test_random_bytes_never_crash_the_parser);
  return UNITY_END();
}

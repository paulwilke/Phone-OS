#include "Json.h"

#include <cstdlib>
#include <cstring>

namespace web::json {

namespace {

// ---------------------------------------------------------------------------
// fromToml helpers
// ---------------------------------------------------------------------------

struct Writer {
  char* buf;
  size_t cap;
  size_t pos = 0;

  bool put(char c) {
    if (pos + 1 >= cap) return false;
    buf[pos++] = c;
    return true;
  }
  bool puts(const char* s) {
    while (*s) if (!put(*s++)) return false;
    return true;
  }
  bool putString(const std::string& s) {
    if (!put('"')) return false;
    for (unsigned char c : s) {
      if (c == '"')       { if (!puts("\\\"")) return false; }
      else if (c == '\\') { if (!puts("\\\\")) return false; }
      else if (c == '\n') { if (!puts("\\n"))  return false; }
      else if (c == '\r') { if (!puts("\\r"))  return false; }
      else if (c == '\t') { if (!puts("\\t"))  return false; }
      else if (c < 0x20) {
        char esc[8];
        snprintf(esc, sizeof(esc), "\\u%04x", c);
        if (!puts(esc)) return false;
      } else {
        if (!put((char)c)) return false;
      }
    }
    return put('"');
  }
  bool putInt(int64_t n) {
    char tmp[24];
    snprintf(tmp, sizeof(tmp), "%lld", (long long)n);
    return puts(tmp);
  }
  void terminate() { buf[pos < cap ? pos : cap - 1] = '\0'; }
};

bool writeValue(Writer& w, const toml::Value& v);

bool writeTable(Writer& w, const toml::Value& v) {
  if (!w.put('{')) return false;
  bool first = true;
  for (size_t i = 0; i < v.size(); ++i) {
    if (!first && !w.put(',')) return false;
    first = false;
    if (!w.putString(v.keyAt(i))) return false;
    if (!w.put(':')) return false;
    if (!writeValue(w, v.at(i))) return false;
  }
  return w.put('}');
}

bool writeValue(Writer& w, const toml::Value& v) {
  switch (v.type()) {
    case toml::Value::Type::String:  return w.putString(v.str());
    case toml::Value::Type::Integer: return w.putInt(v.integer());
    case toml::Value::Type::Boolean: return w.puts(v.boolean() ? "true" : "false");
    case toml::Value::Type::Table:   return writeTable(w, v);
    case toml::Value::Type::Array: {
      if (!w.put('[')) return false;
      for (size_t i = 0; i < v.size(); ++i) {
        if (i > 0 && !w.put(',')) return false;
        if (!writeValue(w, v.at(i))) return false;
      }
      return w.put(']');
    }
    case toml::Value::Type::TableList: {
      if (!w.put('[')) return false;
      for (size_t i = 0; i < v.size(); ++i) {
        if (i > 0 && !w.put(',')) return false;
        if (!writeTable(w, v.at(i))) return false;
      }
      return w.put(']');
    }
  }
  return false;
}

// ---------------------------------------------------------------------------
// toToml helpers — minimal recursive-descent JSON parser
// ---------------------------------------------------------------------------

struct Parser {
  const char* p;
  const char* end;
  std::string error;
  size_t depth = 0;   // objects and arrays open at this point; bounded by kMaxDepth

  // Every object or array is one level and one stack frame: more than kMaxDepth of them is refused before
  // the frame is made, so a body of nothing but brackets cannot run the loop task out of stack.
  bool enter() {
    if (depth >= kMaxDepth) { error = "nested too deep"; return false; }
    ++depth;
    return true;
  }
  void leave() { --depth; }

  void ws() {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
  }

  bool expect(char c) {
    ws();
    if (p < end && *p == c) { ++p; return true; }
    error = std::string("expected '") + c + "'";
    return false;
  }

  bool peek(char c) {
    ws(); return p < end && *p == c;
  }

  // Parse a JSON string into out. Handles \", \\, \/, \n, \r, \t, \uXXXX.
  bool parseString(std::string& out) {
    out.clear();
    if (!expect('"')) return false;
    while (p < end && *p != '"') {
      if (*p == '\\') {
        if (++p >= end) { error = "unterminated string escape"; return false; }
        switch (*p) {
          case '"': out += '"'; break;
          case '\\': out += '\\'; break;
          case '/':  out += '/'; break;
          case 'n':  out += '\n'; break;
          case 'r':  out += '\r'; break;
          case 't':  out += '\t'; break;
          case 'b':  out += '\b'; break;
          case 'f':  out += '\f'; break;
          case 'u': {
            if (p + 4 >= end) { error = "\\u too short"; return false; }
            char hex[5] = {p[1], p[2], p[3], p[4], 0};
            char* ep = nullptr;
            long cp = strtol(hex, &ep, 16);
            if (ep != hex + 4) { error = "bad \\uXXXX"; return false; }
            p += 4;
            // Encode as UTF-8 (BMP only; surrogate pairs not supported)
            if (cp < 0x80) {
              out += (char)cp;
            } else if (cp < 0x800) {
              out += (char)(0xC0 | (cp >> 6));
              out += (char)(0x80 | (cp & 0x3F));
            } else {
              out += (char)(0xE0 | (cp >> 12));
              out += (char)(0x80 | ((cp >> 6) & 0x3F));
              out += (char)(0x80 | (cp & 0x3F));
            }
            break;
          }
          default: error = "unknown escape"; return false;
        }
      } else {
        out += *p;
      }
      ++p;
    }
    if (p >= end) { error = "unterminated string"; return false; }
    ++p;  // closing '"'
    return true;
  }

  bool parseValue(toml::Value& out) {
    ws();
    if (p >= end) { error = "unexpected end"; return false; }
    if (*p == '"') {
      std::string s;
      if (!parseString(s)) return false;
      out = toml::Value::string(s);
      return true;
    }
    if (*p == '{') return parseObject(out);
    if (*p == '[') return parseArray(out);
    if (strncmp(p, "true", 4) == 0)  { p += 4; out = toml::Value::boolean(true);  return true; }
    if (strncmp(p, "false", 5) == 0) { p += 5; out = toml::Value::boolean(false); return true; }
    if (strncmp(p, "null", 4) == 0)  {
      p += 4;
      out = toml::Value::table();
      return true;
    }
    bool neg = (*p == '-');
    if (neg) ++p;
    if (p >= end || *p < '0' || *p > '9') { error = "expected value"; return false; }
    int64_t n = 0;
    while (p < end && *p >= '0' && *p <= '9') {
      n = n * 10 + (*p++ - '0');
    }
    if (p < end && (*p == '.' || *p == 'e' || *p == 'E')) {
      error = "float values are not supported";
      return false;
    }
    out = toml::Value::integer(neg ? -n : n);
    return true;
  }

  bool parseObject(toml::Value& out) {
    if (!enter()) return false;
    const bool ok = parseObjectBody(out);
    leave();
    return ok;
  }

  bool parseArray(toml::Value& out) {
    if (!enter()) return false;
    const bool ok = parseArrayBody(out);
    leave();
    return ok;
  }

  bool parseObjectBody(toml::Value& out) {
    out = toml::Value::table();
    if (!expect('{')) return false;
    ws();
    if (peek('}')) { ++p; return true; }
    do {
      std::string key;
      if (!parseString(key)) return false;
      if (!expect(':')) return false;
      toml::Value val;
      if (!parseValue(val)) return false;
      // null → skip; otherwise set
      if (val.type() != toml::Value::Type::Table || val.size() > 0 ||
          key != "\x01null") {  // sentinel: skip only explicit null
        out.set(key, std::move(val));
      }
      ws();
    } while (peek(',') && ++p);
    return expect('}');
  }

  bool parseArrayBody(toml::Value& out) {
    if (!expect('[')) return false;
    ws();
    if (peek(']')) { ++p; out = toml::Value::array(); return true; }

    // Peek at the first element to decide: array-of-tables or plain array.
    const char* saved = p;
    ws();
    const bool firstIsObject = peek('{');
    p = saved;

    if (firstIsObject) {
      out = toml::Value::tableList();
      do {
        toml::Value elem;
        if (!parseObject(elem)) return false;
        out.push(std::move(elem));
        ws();
      } while (peek(',') && ++p);
    } else {
      out = toml::Value::array();
      do {
        toml::Value elem;
        if (!parseValue(elem)) return false;
        out.push(std::move(elem));
        ws();
      } while (peek(',') && ++p);
    }
    return expect(']');
  }

};

// ---------------------------------------------------------------------------
// getString helpers
// ---------------------------------------------------------------------------

// Skip JSON whitespace.
static const char* jsws(const char* p, const char* end) {
  while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
  return p;
}

// Read a JSON string starting at *p (which must point at '"').
// Returns the length of the string in `out`, or -1 on error.
static int readJsonString(const char* p, const char* end, char* out, size_t outSize) {
  if (p >= end || *p != '"') return -1;
  ++p;
  size_t n = 0;
  while (p < end && *p != '"') {
    if (*p == '\\') {
      if (++p >= end) return -1;
      char c = 0;
      switch (*p) {
        case '"': c = '"'; break; case '\\': c = '\\'; break;
        case '/': c = '/'; break; case 'n':  c = '\n'; break;
        case 'r': c = '\r'; break; case 't':  c = '\t'; break;
        case 'b': c = '\b'; break; case 'f':  c = '\f'; break;
        case 'u': {
          if (p + 4 >= end) return -1;
          char hex[5] = {p[1], p[2], p[3], p[4], 0};
          char* ep = nullptr;
          long cp = strtol(hex, &ep, 16);
          if (ep != hex + 4 || cp > 0x7F) return -1;  // only ASCII for simplicity
          c = (char)cp;
          p += 4;
          break;
        }
        default: return -1;
      }
      if (n + 1 < outSize) out[n++] = c;
    } else {
      if (n + 1 < outSize) out[n++] = (char)*p;
    }
    ++p;
  }
  if (p >= end) return -1;
  out[n] = '\0';
  return (int)n;
}

}  // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

size_t fromToml(const toml::Value& v, char* out, size_t size) {
  if (size == 0) return 0;
  Writer w{out, size};
  if (!writeValue(w, v)) { w.terminate(); return 0; }
  w.terminate();
  return w.pos;
}

bool toToml(const char* data, size_t len, toml::Value& out, std::string& error) {
  Parser p{data, data + len, {}, 0};
  if (!p.parseObject(out)) { error = p.error; return false; }
  p.ws();
  if (p.p != p.end) { error = "trailing garbage"; return false; }
  return true;
}

bool getString(const char* json, size_t len, const char* key, char* out, size_t outSize) {
  const char* p = json;
  const char* end = json + len;
  p = jsws(p, end);
  if (p >= end || *p != '{') return false;
  ++p;

  const size_t klen = strlen(key);

  while (true) {
    p = jsws(p, end);
    if (p >= end) return false;
    if (*p == '}') return false;
    if (*p == ',') { ++p; p = jsws(p, end); }
    if (p >= end || *p != '"') return false;

    // Read the JSON key.
    char kbuf[128];
    int kn = readJsonString(p, end, kbuf, sizeof(kbuf));
    if (kn < 0) return false;
    p += 2 + (size_t)kn;  // skip '...'

    // Skip colon.
    p = jsws(p, end);
    if (p >= end || *p != ':') return false;
    ++p;
    p = jsws(p, end);

    if ((size_t)kn == klen && memcmp(kbuf, key, klen) == 0) {
      // Found the key — read its string value.
      if (p >= end || *p != '"') return false;
      return readJsonString(p, end, out, outSize) >= 0;
    }

    // Skip the value to advance to the next pair.
    // Simple skip: count braces/brackets, handle strings.
    int depth = 0;
    bool inStr = false;
    while (p < end) {
      char c = *p++;
      if (inStr) {
        if (c == '\\') { if (p < end) ++p; }
        else if (c == '"') inStr = false;
      } else if (c == '"') {
        inStr = true;
      } else if (c == '{' || c == '[') {
        ++depth;
      } else if (c == '}' || c == ']') {
        if (depth == 0) { --p; break; }  // end of object
        --depth;
      } else if (c == ',' && depth == 0) {
        --p; break;
      }
    }
  }
}

}  // namespace web::json

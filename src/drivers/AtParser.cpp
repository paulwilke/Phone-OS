#include "AtParser.h"

#include <cstdio>
#include <cstring>

#include "Ucs2.h"

namespace at {
namespace {

bool startsWith(const char* s, const char* prefix) { return strncmp(s, prefix, strlen(prefix)) == 0; }

// A non-negative number of at most nine digits; anything else (a sign, a space, nothing) is
// refused, so that "12abc" is not read as 12 and a long digit string cannot overflow.
bool toInt(const char* s, int* out) {
  if (!*s) return false;
  int v = 0;
  size_t n = 0;
  for (; *s; ++s, ++n) {
    if (*s < '0' || *s > '9' || n >= 9) return false;
    v = v * 10 + (*s - '0');
  }
  *out = v;
  return true;
}

// The fields after "+XXXX:", split at commas. Quotes are removed; a comma inside quotes (a
// timestamp, an operator name) stays part of the field. Spaces around a field are dropped.
struct Fields {
  static constexpr int kMax = 16;
  char buf[LineReader::kMaxLine + 1];
  const char* f[kMax];
  bool quoted[kMax];
  int n = 0;
};

void splitFields(const char* line, Fields& out) {
  const char* colon = strchr(line, ':');
  const char* s = colon ? colon + 1 : line;
  while (*s == ' ') ++s;

  size_t len = strlen(s);
  if (len > LineReader::kMaxLine) len = LineReader::kMaxLine;
  memcpy(out.buf, s, len);
  out.buf[len] = '\0';
  out.n = 0;

  char* p = out.buf;
  while (out.n < Fields::kMax) {
    while (*p == ' ') ++p;
    if (*p == '"') {
      char* start = p + 1;
      char* end = strchr(start, '"');
      out.quoted[out.n] = true;
      if (!end) {  // unterminated: the rest is the field
        out.f[out.n++] = start;
        break;
      }
      *end = '\0';
      out.f[out.n++] = start;
      p = strchr(end + 1, ',');
      if (!p) break;
      ++p;
    } else {
      out.quoted[out.n] = false;
      out.f[out.n] = p;
      char* comma = strchr(p, ',');
      char* stop = comma ? comma : p + strlen(p);
      char* trim = stop;
      while (trim > p && trim[-1] == ' ') --trim;
      const bool more = comma != nullptr;
      *trim = '\0';
      ++out.n;
      if (!more) break;
      p = comma + 1;
    }
  }
}

// Takes a number out of an answer field. False when it is unreadable or does not fit: a cut
// number is somebody else's number.
bool decodeAddress(const char* field, Charset cs, char* out, size_t outSize) {
  if (outSize == 0) return false;
  out[0] = '\0';
  const size_t len = strlen(field);
  if (cs == Charset::Ucs2) {
    bool truncated = false;
    if (!text::ucs2HexToUtf8(field, len, out, outSize, &truncated) || truncated) {
      out[0] = '\0';
      return false;
    }
    return out[0] != '\0';
  }
  if (len == 0 || len >= outSize) return false;
  memcpy(out, field, len + 1);
  return true;
}

int64_t daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const int yoe = y - era * 400;
  const int doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
  const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (int64_t)era * 146097 + doe - 719468;
}

bool daysInMonthOk(int year, int month, int day) {
  static const int kDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int max = kDays[month - 1];
  if (month == 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))) max = 29;
  return day <= max;
}

// "yy/MM/dd,hh:mm:ss±zz" → UTC and the zone in minutes.
bool scts(const char* s, uint32_t* epochUtc, int* tzMinutes, int* year) {
  int yy, mo, dd, hh, mi, ss, tz;
  char sign;
  if (sscanf(s, "%2d/%2d/%2d,%2d:%2d:%2d%c%2d", &yy, &mo, &dd, &hh, &mi, &ss, &sign, &tz) != 8) return false;
  if ((sign != '+' && sign != '-') || yy < 0 || mo < 1 || mo > 12 || dd < 1 || hh < 0 || hh > 23 || mi < 0 ||
      mi > 59 || ss < 0 || ss > 59 || tz < 0 || tz > 96) {
    return false;
  }
  if (!daysInMonthOk(2000 + yy, mo, dd)) return false;
  int64_t t = daysFromCivil(2000 + yy, mo, dd) * 86400 + hh * 3600 + mi * 60 + ss;
  const int offsetMinutes = tz * 15;
  t += (sign == '+') ? -(int64_t)offsetMinutes * 60 : (int64_t)offsetMinutes * 60;
  if (t < 0 || t > 0xFFFFFFFFll) return false;
  *epochUtc = (uint32_t)t;
  if (tzMinutes) *tzMinutes = sign == '+' ? offsetMinutes : -offsetMinutes;
  if (year) *year = 2000 + yy;
  return true;
}

}  // namespace

// ---------------------------------------------------------------------------
// LineReader
// ---------------------------------------------------------------------------

bool LineReader::feed(char c) {
  if (c == '\0') return false;
  if (c == '\n') {
    while (len_ > 0 && buf_[len_ - 1] == '\r') --len_;
    buf_[len_] = '\0';
    lastTruncated_ = overflow_;
    len_ = 0;
    overflow_ = false;
    return true;
  }

  if (len_ < kMaxLine) buf_[len_++] = c;
  else overflow_ = true;

  if (promptMode_ && len_ == 2 && buf_[0] == '>' && buf_[1] == ' ') {
    buf_[1] = '\0';
    lastTruncated_ = false;
    len_ = 0;
    return true;
  }
  return false;
}

void LineReader::reset() {
  len_ = 0;
  buf_[0] = '\0';
  overflow_ = false;
  lastTruncated_ = false;
}

// ---------------------------------------------------------------------------
// Classification
// ---------------------------------------------------------------------------

LineType classify(const char* line) {
  if (!line || line[0] == '\0') return LineType::Empty;
  if (strcmp(line, "OK") == 0) return LineType::Ok;
  if (strcmp(line, "ERROR") == 0 || startsWith(line, "+CMS ERROR:") || startsWith(line, "+CME ERROR:")) {
    return LineType::Error;
  }
  if (strcmp(line, ">") == 0) return LineType::Prompt;
  if (strcmp(line, "RING") == 0) return LineType::Ring;
  if (strcmp(line, "NO CARRIER") == 0) return LineType::NoCarrier;
  if (strcmp(line, "BUSY") == 0) return LineType::Busy;
  if (strcmp(line, "NO ANSWER") == 0) return LineType::NoAnswer;
  if (startsWith(line, "VOICE CALL: BEGIN")) return LineType::VoiceCallBegin;
  if (startsWith(line, "VOICE CALL: END")) return LineType::VoiceCallEnd;
  if (startsWith(line, "*ATREADY:")) return LineType::AtReady;
  if (startsWith(line, "+CLIP:")) return LineType::Clip;
  if (startsWith(line, "+CMTI:")) return LineType::Cmti;
  if (startsWith(line, "+CMGL:")) return LineType::Cmgl;
  if (startsWith(line, "+CMGR:")) return LineType::Cmgr;
  if (startsWith(line, "+CMGS:")) return LineType::Cmgs;
  if (startsWith(line, "+CSQ:")) return LineType::Csq;
  if (startsWith(line, "+CREG:")) return LineType::Creg;
  if (startsWith(line, "+COPS:")) return LineType::Cops;
  if (startsWith(line, "+CCLK:")) return LineType::Cclk;
  if (startsWith(line, "+CPIN:")) return LineType::Cpin;
  if (startsWith(line, "+CIREG:")) return LineType::Cireg;
  if (startsWith(line, "+CGDCONT:")) return LineType::Cgdcont;
  if (startsWith(line, "+CLCC:")) return LineType::Clcc;
  return LineType::Other;
}

// ---------------------------------------------------------------------------
// One line each
// ---------------------------------------------------------------------------

bool parseClip(const char* line, Charset cs, char* phone, size_t phoneSize) {
  if (phoneSize > 0) phone[0] = '\0';
  if (classify(line) != LineType::Clip) return false;
  Fields f;
  splitFields(line, f);
  if (f.n < 1) return false;
  return decodeAddress(f.f[0], cs, phone, phoneSize);
}

bool parseCmti(const char* line, int* index) {
  if (classify(line) != LineType::Cmti) return false;
  Fields f;
  splitFields(line, f);
  int v;
  if (f.n < 2 || !toInt(f.f[1], &v)) return false;
  *index = v;
  return true;
}

bool parseVoiceCallEnd(const char* line, uint32_t* seconds) {
  if (classify(line) != LineType::VoiceCallEnd) return false;
  *seconds = 0;
  const char* p = line + strlen("VOICE CALL: END");
  if (*p != ':') return true;
  ++p;
  while (*p == ' ') ++p;

  // Groups of digits separated by colons: "m:ss", "h:mm:ss"; or six digits "hhmmss".
  int group[3] = {0, 0, 0};
  int groups = 0;
  size_t digits = 0, total = 0;
  for (const char* q = p;; ++q) {
    if (*q >= '0' && *q <= '9') {
      if (++digits > 6) return true;
      if (groups < 3) group[groups] = group[groups] * 10 + (*q - '0');
      ++total;
    } else if (*q == ':' || *q == '\0') {
      if (digits == 0) return true;
      ++groups;
      digits = 0;
      if (*q == '\0') break;
      if (groups >= 3) return true;
    } else {
      return true;
    }
  }
  if (groups == 2 && group[1] < 60) {
    *seconds = (uint32_t)(group[0] * 60 + group[1]);
  } else if (groups == 3 && group[1] < 60 && group[2] < 60) {
    *seconds = (uint32_t)(group[0] * 3600 + group[1] * 60 + group[2]);
  } else if (groups == 1 && total == 6) {
    const int v = group[0];  // hhmmss as one number
    const int h = v / 10000, m = (v / 100) % 100, s = v % 100;
    if (m < 60 && s < 60) *seconds = (uint32_t)(h * 3600 + m * 60 + s);
  }
  return true;
}

bool parseCsq(const char* line, int* rssi) {
  if (classify(line) != LineType::Csq) return false;
  Fields f;
  splitFields(line, f);
  int v;
  if (f.n < 1 || !toInt(f.f[0], &v)) return false;
  *rssi = v;
  return true;
}

int csqToBars(int rssi) {
  if (rssi == 99 || rssi <= 0) return 0;
  if (rssi <= 5) return 1;
  if (rssi <= 10) return 2;
  if (rssi <= 15) return 3;
  if (rssi <= 20) return 4;
  return 5;
}

bool parseCreg(const char* line, Registration* out) {
  if (classify(line) != LineType::Creg) return false;
  Fields f;
  splitFields(line, f);
  int stat;
  if (f.n < 2 || !toInt(f.f[1], &stat)) return false;
  out->stat = stat;
  return true;
}

bool parseCireg(const char* line, ImsRegistration* out) {
  if (classify(line) != LineType::Cireg) return false;
  Fields f;
  splitFields(line, f);
  int mode, info, ext = -1;
  if (f.n < 2 || !toInt(f.f[0], &mode) || !toInt(f.f[1], &info)) return false;
  if (f.n >= 3 && !toInt(f.f[2], &ext)) ext = -1;
  out->mode = mode;
  out->info = info;
  out->ext = ext;
  return true;
}

bool parseCgdcont(const char* line, PdpContext* out) {
  if (classify(line) != LineType::Cgdcont) return false;
  Fields f;
  splitFields(line, f);
  int cid;
  if (f.n < 3 || !toInt(f.f[0], &cid)) return false;
  if (strlen(f.f[1]) >= sizeof(out->type) || strlen(f.f[2]) >= sizeof(out->apn)) return false;
  out->cid = cid;
  strcpy(out->type, f.f[1]);
  strcpy(out->apn, f.f[2]);
  return true;
}

bool parseClcc(const char* line, Charset cs, CallListEntry* out) {
  if (classify(line) != LineType::Clcc) return false;
  Fields f;
  splitFields(line, f);
  int index, dir, state, mode, mpty;
  if (f.n < 5 || !toInt(f.f[0], &index) || !toInt(f.f[1], &dir) || !toInt(f.f[2], &state) ||
      !toInt(f.f[3], &mode) || !toInt(f.f[4], &mpty)) {
    return false;
  }
  out->index = index;
  out->direction = dir;
  out->state = state;
  out->phone[0] = '\0';
  if (f.n > 5 && f.f[5][0] != '\0') decodeAddress(f.f[5], cs, out->phone, sizeof(out->phone));
  return true;
}

bool parseCpin(const char* line, SimStatus* out) {
  if (classify(line) != LineType::Cpin) return false;
  const char* what = line + strlen("+CPIN:");
  while (*what == ' ') ++what;
  if (strcmp(what, "READY") == 0) *out = SimStatus::Ready;
  else if (strcmp(what, "SIM PIN") == 0) *out = SimStatus::PinRequired;
  else if (strcmp(what, "SIM PUK") == 0) *out = SimStatus::PukRequired;
  else return false;
  return true;
}

bool isSimNotInserted(const char* errorLine) {
  return classify(errorLine) == LineType::Error && strstr(errorLine, "not inserted") != nullptr;
}

bool parseCopsName(const char* line, char* name, size_t nameSize) {
  if (nameSize == 0) return false;
  name[0] = '\0';
  if (classify(line) != LineType::Cops) return false;
  const char* open = strchr(line, '"');
  if (!open) return false;
  const char* close = strchr(open + 1, '"');
  if (!close || close == open + 1) return false;
  size_t len = (size_t)(close - open - 1);
  if (len >= nameSize) len = nameSize - 1;
  memcpy(name, open + 1, len);
  name[len] = '\0';
  const char* mapped = operatorNameForCode(name);
  if (mapped != name) {
    strncpy(name, mapped, nameSize - 1);
    name[nameSize - 1] = '\0';
  }
  return true;
}

bool parseFirstOperatorCode(const char* line, char* out, size_t outSize) {
  if (!line || !out || outSize < 7) return false;
  for (const char* p = line; *p; ++p) {
    if (*p != '"') continue;
    const char* start = p + 1;
    const char* end = strchr(start, '"');
    if (!end) return false;
    const size_t len = (size_t)(end - start);
    if (len == 5 || len == 6) {
      bool digits = true;
      for (const char* q = start; q < end; ++q)
        if (*q < '0' || *q > '9') { digits = false; break; }
      if (digits) {
        memcpy(out, start, len);
        out[len] = '\0';
        return true;
      }
    }
    p = end;  // this field is dealt with
  }
  return false;
}

const char* operatorNameForCode(const char* code) {
  if (!code) return code;
  const size_t n = strlen(code);
  if (n != 5 && n != 6) return code;
  for (size_t i = 0; i < n; ++i)
    if (code[i] < '0' || code[i] > '9') return code;
  // Only the countries a device here is likely to meet (Germany, its neighbours and the usual
  // holiday countries); everything else stays a code.
  struct Net { const char* code; const char* name; };
  static const Net kNets[] = {
      {"26201", "Telekom"}, {"26206", "Telekom"},
      {"26202", "Vodafone"}, {"26204", "Vodafone"}, {"26209", "Vodafone"},
      {"26203", "O2"}, {"26205", "O2"}, {"26207", "O2"}, {"26208", "O2"}, {"26211", "O2"},
      {"20801", "Orange"}, {"20802", "Orange"}, {"20810", "SFR"}, {"20811", "SFR"},
      {"20815", "Free"}, {"20816", "Free"}, {"20820", "Bouygues"}, {"20821", "Bouygues"},
      {"21401", "Vodafone"}, {"21403", "Orange"}, {"21404", "Yoigo"}, {"21407", "Movistar"},
      {"23201", "A1"}, {"23203", "Magenta"}, {"23205", "Drei"}, {"23210", "Drei"},
      {"22801", "Swisscom"}, {"22802", "Sunrise"}, {"22803", "Salt"},
      {"20404", "Vodafone"}, {"20408", "KPN"}, {"20416", "T-Mobile"}, {"20420", "T-Mobile"},
      {"20601", "Proximus"}, {"20610", "Orange"}, {"20620", "Base"},
      {"22201", "TIM"}, {"22210", "Vodafone"}, {"22288", "Wind"}, {"22250", "Iliad"},
  };
  for (const Net& net : kNets)
    if (strncmp(code, net.code, 5) == 0) return net.name;
  return code;
}

bool parseScts(const char* s, uint32_t* epochUtc) { return scts(s, epochUtc, nullptr, nullptr); }

bool parseCclk(const char* line, NetworkTime* out) {
  if (classify(line) != LineType::Cclk) return false;
  const char* quote = strchr(line, '"');
  if (!quote) return false;
  uint32_t epoch;
  int tz, year;
  if (!scts(quote + 1, &epoch, &tz, &year) || year < 2020 || year > 2069) return false;
  out->epochUtc = epoch;
  out->tzOffsetMinutes = tz;
  return true;
}

// ---------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------

bool isValidDialString(const char* phone) {
  const size_t len = strlen(phone);
  if (len == 0 || len > kMaxPhoneChars) return false;
  for (size_t i = 0; i < len; ++i) {
    const char c = phone[i];
    const bool ok = (c >= '0' && c <= '9') || c == '*' || c == '#' || (c == '+' && i == 0);
    if (!ok) return false;
  }
  return !(len == 1 && phone[0] == '+');
}

bool buildCmgsCommand(const char* phone, Charset cs, char* out, size_t outSize) {
  if (outSize == 0) return false;
  out[0] = '\0';
  if (!isValidDialString(phone)) return false;

  char encoded[(kMaxPhoneChars + 1) * 4 + 1];
  const char* number = phone;
  if (cs == Charset::Ucs2) {
    if (text::utf8ToUcs2Hex(phone, encoded, sizeof(encoded)) < 0) return false;
    number = encoded;
  }
  const int n = snprintf(out, outSize, "AT+CMGS=\"%s\"", number);
  if (n < 0 || (size_t)n >= outSize) {
    out[0] = '\0';
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// SmsResponseParser
// ---------------------------------------------------------------------------

void SmsResponseParser::reset(Charset cs, ParsedSms* out, size_t capacity) {
  cs_ = cs;
  out_ = out;
  capacity_ = capacity;
  count_ = dropped_ = 0;
  finished_ = ok_ = inMessage_ = false;
  rawLen_ = 0;
  rawOverflow_ = false;
  firstBodyLine_ = true;
}

Feed SmsResponseParser::feedLine(const char* line) {
  if (finished_) return Feed::Unsolicited;

  const LineType t = classify(line);
  if (t == LineType::Cmgl || t == LineType::Cmgr) {
    if (inMessage_) finishMessage();
    startMessage(line, t == LineType::Cmgl);
    return Feed::Consumed;
  }
  if (t == LineType::Ok || t == LineType::Error) {
    if (inMessage_) finishMessage();
    finished_ = true;
    ok_ = (t == LineType::Ok);
    return Feed::Done;
  }
  if (!inMessage_) return t == LineType::Empty ? Feed::Consumed : Feed::Unsolicited;

  if (cs_ == Charset::Ucs2) {
    if (t == LineType::Empty) return Feed::Consumed;
    if (!text::isUcs2Hex(line, strlen(line))) return Feed::Unsolicited;
  }
  appendBody(line);
  return Feed::Consumed;
}

void SmsResponseParser::startMessage(const char* line, bool isCmgl) {
  cur_ = ParsedSms();
  rawLen_ = 0;
  rawOverflow_ = false;
  firstBodyLine_ = true;
  inMessage_ = true;

  // +CMGL: <index>,"<stat>","<oa>",["<alpha>"],["<scts>"]
  // +CMGR: "<stat>","<oa>",["<alpha>"],"<scts>"[,...]
  Fields f;
  splitFields(line, f);
  const int addr = isCmgl ? 2 : 1;
  const int stamp = isCmgl ? 4 : 3;

  int index;
  if (isCmgl && f.n > 0 && toInt(f.f[0], &index)) cur_.index = index;
  if (f.n > addr) {
    char phone[kPhoneSize];
    cur_.senderValid = decodeAddress(f.f[addr], cs_, phone, sizeof(phone));
    if (cur_.senderValid) cur_.msg.phone = phone;
  }
  if (f.n > stamp) parseScts(f.f[stamp], &cur_.msg.timestamp);
}

void SmsResponseParser::appendBody(const char* line) {
  // GSM: a line break inside the text arrives as a line of its own and is put back together.
  // UCS2: breaks are 000A inside the hex.
  if (cs_ == Charset::Gsm && !firstBodyLine_) {
    if (rawLen_ < kMaxRaw) raw_[rawLen_++] = '\n';
    else rawOverflow_ = true;
  }
  firstBodyLine_ = false;

  size_t len = strlen(line);
  const size_t room = kMaxRaw - rawLen_;
  if (len > room) {
    len = room;
    rawOverflow_ = true;
  }
  memcpy(raw_ + rawLen_, line, len);
  rawLen_ += len;
}

void SmsResponseParser::finishMessage() {
  inMessage_ = false;

  // GSM: the empty line before OK is no part of the text.
  while (rawLen_ > 0 && raw_[rawLen_ - 1] == '\n') --rawLen_;

  char body[kMaxSmsBytes + 1];
  bool truncated = rawOverflow_;

  if (cs_ == Charset::Ucs2) {
    rawLen_ -= rawLen_ % 4;  // a unit that was cut off is dropped
    bool t = false;
    if (!text::ucs2HexToUtf8(raw_, rawLen_, body, sizeof(body), &t)) body[0] = '\0';
    truncated = truncated || t;
  } else {
    size_t len = rawLen_;
    if (len > kMaxSmsBytes) {
      len = kMaxSmsBytes;
      truncated = true;
      // Never cut inside a multi-byte character.
      while (len > 0 && ((unsigned char)raw_[len] & 0xC0) == 0x80) --len;
    }
    memcpy(body, raw_, len);
    body[len] = '\0';
  }
  cur_.msg.body = body;
  cur_.bodyTruncated = truncated;

  if (count_ < capacity_) out_[count_++] = cur_;
  else ++dropped_;
}

}  // namespace at

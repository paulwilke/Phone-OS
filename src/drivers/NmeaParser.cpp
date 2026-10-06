#include "NmeaParser.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "../hal/Clock.h"
#include "CivilTime.h"

namespace {

int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

// Exactly `n` decimal digits at `s`, as a number; -1 when anything else is there.
int digits(const char* s, int n) {
  int v = 0;
  for (int i = 0; i < n; ++i) {
    if (s[i] < '0' || s[i] > '9') return -1;
    v = v * 10 + (s[i] - '0');
  }
  return v;
}

// "4834.5678" (degrees and minutes) -> 48.5761…, negative for S and W. `maxDegrees` is 90 or 180. False
// for anything that is not a position.
bool degreesFromNmea(const char* value, const char* hemisphere, int maxDegrees, double& out) {
  if (!value || !*value || !hemisphere) return false;
  const char* dot = strchr(value, '.');
  if (!dot || dot - value < 3 || dot - value > 5) return false;
  const int degreeDigits = (int)(dot - value) - 2;
  const int degrees = digits(value, degreeDigits);
  if (degrees < 0 || degrees > maxDegrees) return false;
  char* end = nullptr;
  const double minutes = strtod(value + degreeDigits, &end);
  if (end == value + degreeDigits || *end != '\0' || !std::isfinite(minutes) || minutes < 0 || minutes >= 60) return false;
  double result = degrees + minutes / 60.0;
  if (result > maxDegrees) return false;
  const char h = *hemisphere;
  if (h == 'S' || h == 'W') result = -result;
  else if (h != 'N' && h != 'E') return false;
  out = result;
  return true;
}

}  // namespace

void NmeaParser::reset() {
  len_ = 0;
  collecting_ = false;
  overflow_ = false;
  rmcCount_ = 0;
  fix_ = GnssFix();
  sats_ = 0;
  hdop_ = kUnknownHdop;
}

bool NmeaParser::feed(char c) {
  if (c == '$') {  // a new sentence; one that was begun was incomplete
    collecting_ = true;
    overflow_ = false;
    len_ = 0;
    return false;
  }
  if (!collecting_) return false;

  if (c == '\r' || c == '\n') {
    const bool had = len_ > 0 && !overflow_;
    collecting_ = false;
    if (overflow_) ++rejected_;
    if (!had) {
      len_ = 0;
      return false;
    }
    buf_[len_] = '\0';

    // The checksum is the XOR of everything between '$' and '*'.
    char* star = strchr(buf_, '*');
    if (!star || hexVal(star[1]) < 0 || hexVal(star[2]) < 0) {
      ++rejected_;
      len_ = 0;
      return false;
    }
    *star = '\0';
    const uint8_t expected = (uint8_t)((hexVal(star[1]) << 4) | hexVal(star[2]));
    uint8_t actual = 0;
    for (const char* p = buf_; *p; ++p) actual ^= (uint8_t)*p;
    if (actual != expected) {
      ++rejected_;
      len_ = 0;
      return false;
    }

    handleSentence(buf_);
    len_ = 0;
    return true;
  }

  if (len_ >= kMaxSentence) {
    overflow_ = true;  // too long: dropped, never written past the buffer
    return false;
  }
  buf_[len_++] = c;
  return false;
}

void NmeaParser::handleSentence(char* sentence) {
  char* fields[24];
  int count = 0;
  fields[count++] = sentence;
  for (char* p = sentence; *p && count < 24; ++p) {
    if (*p == ',') {
      *p = '\0';
      fields[count++] = p + 1;
    }
  }
  if (count < 2) return;

  // The kind follows the two letters that name the constellation.
  const char* type = fields[0] + (strlen(fields[0]) > 2 ? 2 : 0);
  if (strcmp(type, "RMC") == 0) handleRmc(fields, count);
  else if (strcmp(type, "GGA") == 0) handleGga(fields, count);
}

// $xxRMC,hhmmss.ss,A,llll.ll,a,yyyyy.yy,a,x.x,x.x,ddmmyy,...
void NmeaParser::handleRmc(char* fields[], int count) {
  if (count < 10) return;
  ++rmcCount_;

  if (fields[2][0] != 'A') {
    fix_ = GnssFix();  // no position: nothing of the old one is left to misuse
    return;
  }

  const char* time = fields[1];
  const char* date = fields[9];
  if (strlen(time) < 6 || strlen(date) != 6) {
    ++rejected_;
    return;
  }
  const int hour = digits(time, 2), minute = digits(time + 2, 2), second = digits(time + 4, 2);
  const int day = digits(date, 2), month = digits(date + 2, 2), year = digits(date + 4, 2);
  if (hour < 0 || minute < 0 || second < 0 || day < 0 || month < 0 || year < 0) {
    ++rejected_;
    return;
  }

  double lat = 0, lon = 0;
  if (!degreesFromNmea(fields[3], fields[4], 90, lat) || !degreesFromNmea(fields[5], fields[6], 180, lon)) {
    // The receiver says "valid" and gives a position that cannot exist: believe neither.
    fix_ = GnssFix();
    ++rejected_;
    return;
  }

  const uint32_t epoch = civil::toEpoch(2000 + year, month, day, hour, minute, second);
  if (epoch == 0) {  // a day or a month that does not exist
    fix_ = GnssFix();
    ++rejected_;
    return;
  }
  fix_.lat = lat;
  fix_.lon = lon;
  // A date that is not plausible (a receiver after a week-number rollover, or one that has not learned
  // the date) is no time: the position stays, the time of it is unknown.
  fix_.utc = epoch >= Clock::kEarliestEpoch && epoch < Clock::kLatestEpoch ? epoch : 0;
  fix_.sats = sats_;
  fix_.hdop = hdop_;
  fix_.valid = true;
}

// $xxGGA,hhmmss.ss,llll.ll,a,yyyyy.yy,a,q,ss,h.h,alt,M,...
void NmeaParser::handleGga(char* fields[], int count) {
  if (count < 9) return;
  const int quality = atoi(fields[6]);

  // The number of satellites in use: up to three digits, never more than a byte.
  const long sats = atol(fields[7]);
  sats_ = (uint8_t)(sats < 0 ? 0 : sats > 255 ? 255 : sats);

  // The HDOP must be a number that can be compared. "nan", "inf", an empty field, zero and a negative
  // number all mean "not known".
  char* end = nullptr;
  const double hdop = fields[8][0] ? strtod(fields[8], &end) : 0.0;
  const bool usable = fields[8][0] && end && *end == '\0' && std::isfinite(hdop) && hdop > 0;
  hdop_ = usable ? (float)hdop : kUnknownHdop;

  if (quality == 0) {
    fix_ = GnssFix();
    return;
  }
  // The quality counts at once, even when the next RMC comes later.
  if (fix_.valid) {
    fix_.sats = sats_;
    fix_.hdop = hdop_;
  }
}

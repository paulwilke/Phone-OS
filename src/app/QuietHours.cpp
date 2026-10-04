#include "QuietHours.h"

#include <cstring>

#include "Calendar.h"

namespace {

const char* const kDayNames[] = {"mon", "tue", "wed", "thu", "fri", "sat", "sun"};

char lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c; }
bool isBlank(char c) { return c == ' ' || c == '\t'; }

// Index of the day named by [begin, end), ignoring blanks around it, or -1.
int dayIndex(const char* begin, const char* end) {
  while (begin < end && isBlank(*begin)) ++begin;
  while (end > begin && isBlank(end[-1])) --end;
  if (end - begin != 3) return -1;
  char name[4] = {lower(begin[0]), lower(begin[1]), lower(begin[2]), '\0'};
  for (int i = 0; i < 7; ++i) {
    if (strcmp(name, kDayNames[i]) == 0) return i;
  }
  return -1;
}

bool equalsIgnoreCase(const char* text, const char* word) {
  for (; *word; ++text, ++word) {
    if (lower(*text) != *word) return false;
  }
  return *text == '\0';
}

}  // namespace

bool QuietHours::parseDays(const char* text, uint8_t& mask) {
  if (!text) return false;
  while (isBlank(*text)) ++text;
  if (!*text) return false;

  const char* trimmedEnd = text + strlen(text);
  while (trimmedEnd > text && isBlank(trimmedEnd[-1])) --trimmedEnd;
  char whole[8] = {};
  if (trimmedEnd - text < (long)sizeof(whole)) {
    memcpy(whole, text, (size_t)(trimmedEnd - text));
    if (equalsIgnoreCase(whole, "daily")) {
      mask = 0x7F;
      return true;
    }
  }

  uint8_t m = 0;
  const char* p = text;
  for (;;) {
    const char* end = p;
    while (*end && *end != ',') ++end;
    const char* dash = (const char*)memchr(p, '-', (size_t)(end - p));
    if (dash) {
      const int a = dayIndex(p, dash);
      const int b = dayIndex(dash + 1, end);
      if (a < 0 || b < 0) return false;
      // Count forwards, also across the weekend ("fri-mon").
      for (int d = a;; d = (d + 1) % 7) {
        m |= (uint8_t)(1u << d);
        if (d == b) break;
      }
    } else {
      const int a = dayIndex(p, end);
      if (a < 0) return false;  // also an empty entry: "mon,,tue", "mon,"
      m |= (uint8_t)(1u << a);
    }
    if (!*end) break;
    p = end + 1;
  }
  mask = m;
  return true;
}

bool QuietHours::parseTime(const char* text, int& minuteOfDay) {
  if (!text) return false;
  // Strict "H:MM" or "HH:MM": a loose parser accepts "+8:00" or "8:5", which nobody
  // means, and a time that is silently misread locks the phone at the wrong hour.
  int hour = 0;
  size_t i = 0;
  while (i < 2 && text[i] >= '0' && text[i] <= '9') hour = hour * 10 + (text[i++] - '0');
  if (i == 0 || text[i] != ':') return false;
  ++i;
  if (!(text[i] >= '0' && text[i] <= '9') || !(text[i + 1] >= '0' && text[i + 1] <= '9')) return false;
  const int minute = (text[i] - '0') * 10 + (text[i + 1] - '0');
  if (text[i + 2] != '\0' || hour > 23 || minute > 59) return false;
  minuteOfDay = hour * 60 + minute;
  return true;
}

QuietHours::AddResult QuietHours::add(const char* days, const char* from, const char* to) {
  uint8_t mask;
  int a, b;
  if (!parseDays(days, mask)) return AddResult::BadDays;
  if (!parseTime(from, a) || !parseTime(to, b)) return AddResult::BadTime;
  if (a == b) return AddResult::EmptyWindow;

  for (int day = 0; day < 7; ++day) {
    if (!(mask & (1u << day))) continue;
    size_t onThisDay = 0;
    for (size_t i = 0; i < count_; ++i) {
      if (windows_[i].days & (1u << day)) ++onThisDay;
    }
    if (onThisDay >= kMaxPerDay) return AddResult::DayFull;
  }
  windows_[count_++] = Window{mask, (uint16_t)a, (uint16_t)b};
  return AddResult::Added;
}

bool QuietHours::active(uint32_t epochUtc, int tzOffsetMinutes, int* endMinute) const {
  if (epochUtc == 0 || count_ == 0) return false;
  const calendar::LocalTime now = calendar::toLocal((int64_t)epochUtc, tzOffsetMinutes);
  const int today = now.weekday;
  const int yesterday = (today + 6) % 7;

  bool any = false;
  int longestRemaining = 0;  // minutes until the window that lasts longest ends
  int endsAt = 0;
  for (size_t i = 0; i < count_; ++i) {
    const Window& w = windows_[i];
    int remaining = 0;
    if (w.from < w.to) {
      if ((w.days & (1u << today)) && now.minuteOfDay >= w.from && now.minuteOfDay < w.to) {
        remaining = w.to - now.minuteOfDay;
      }
    } else if ((w.days & (1u << today)) && now.minuteOfDay >= w.from) {
      remaining = 24 * 60 - now.minuteOfDay + w.to;  // evening: runs into the next day
    } else if ((w.days & (1u << yesterday)) && now.minuteOfDay < w.to) {
      remaining = w.to - now.minuteOfDay;  // morning: the tail of last night's window
    }
    if (remaining > longestRemaining) {
      longestRemaining = remaining;
      endsAt = w.to;
      any = true;
    }
  }
  if (any && endMinute) *endMinute = endsAt;
  return any;
}

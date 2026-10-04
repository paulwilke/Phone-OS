#pragma once

// School time: the child must not use the phone during lessons, and the parents
// say when. This is pure arithmetic on weekday and time of day; what is allowed
// or blocked during a quiet period is decided elsewhere.
//
// A window is written with days, a start and an end:
//   days  "mon-fri", a list "mon,wed,fri", a range across the weekend "fri-mon",
//         or "daily" (English three-letter names, any case, blanks ignored)
//   from  "08:00"      to  "13:30"      (24-hour clock; "8:00" is fine, "24:00" is not)
// The end minute itself is free again. A window may cross midnight ("22:00" to
// "06:00"): the morning part belongs to the day on which the window began. An end
// of "00:00" means the end of that day.
//
// Each day can have at most two windows (morning lessons, afternoon lessons); a
// window counts for the day it begins on.
//
// **Without a known time of day there is no lock.** The board has no clock of its
// own, so after a restart without network the time is unknown, and without
// network the child cannot call anyway. Guessing the lock would be wrong in both
// directions.

#include <cstddef>
#include <cstdint>

class QuietHours {
public:
  static constexpr size_t kMaxPerDay = 2;
  static constexpr size_t kMaxWindows = 7 * kMaxPerDay;

  // Day bits: bit 0 = Monday … bit 6 = Sunday.
  struct Window {
    uint8_t days = 0;
    uint16_t from = 0;  // minute of the day
    uint16_t to = 0;
  };

  enum class AddResult {
    Added,
    BadDays,      // the day text cannot be read
    BadTime,      // a time cannot be read
    EmptyWindow,  // start equals end: neither "nothing" nor "all day" is guessed
    DayFull,      // one of the days already has kMaxPerDay windows; nothing was added
  };

  AddResult add(const char* days, const char* from, const char* to);

  size_t count() const { return count_; }
  // `i` must be below count(). The settings page shows the windows from this.
  const Window& window(size_t i) const { return windows_[i]; }

  // Is a quiet period running at this instant? `epochUtc` 0 means the time is not
  // known and never locks. When true and `endMinute` is given, it receives the
  // minute of the day at which the period ends (13:30 = 810); with overlapping
  // windows that is the end of the one that lasts longest. Two windows that merely
  // touch (10:00 end, 10:00 start) are reported one at a time.
  bool active(uint32_t epochUtc, int tzOffsetMinutes, int* endMinute = nullptr) const;

  // Public because the parsing is exactly what has to be tested.
  static bool parseDays(const char* text, uint8_t& mask);       // bit 0 = Monday
  static bool parseTime(const char* text, int& minuteOfDay);

private:
  Window windows_[kMaxWindows];
  size_t count_ = 0;
};

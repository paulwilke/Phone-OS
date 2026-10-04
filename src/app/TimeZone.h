#pragma once

// Time zones with summer and winter time. A parent picks a zone by name
// ("Europe/Berlin") and the clock then follows the changes of the clocks on its
// own, so a school time of 08:00 stays 08:00 on the wall all year.
//
// A zone is a name, a fixed offset from UTC for winter time, and a rule for
// summer time:
//   EU    from the last Sunday of March 01:00 UTC to the last Sunday of October
//         01:00 UTC, +1 hour. The same instant in every zone that follows it
//         (European Union, United Kingdom, Switzerland), whatever its offset.
//   US    from the second Sunday of March 02:00 local standard time to the first
//         Sunday of November 02:00 local summer time, +1 hour.
//   None  no changes.
// Other rules (southern hemisphere, Australia) do not exist on purpose: somebody
// who lives there enters a fixed offset instead.

#include <cstddef>
#include <cstdint>

namespace tz {

enum class Rule : uint8_t { None, EU, US };

struct Zone {
  const char* name;
  int16_t baseMinutes;  // winter time against UTC
  Rule rule;
};

size_t zoneCount();
// `i` must be below zoneCount().
const Zone& zoneAt(size_t i);

// The zone with exactly this name, or null.
const Zone* find(const char* name);

// Offset from UTC in minutes at this instant, summer time included. A zone with no
// rule has the same offset all year.
int offsetMinutes(const Zone& zone, uint32_t epochUtc);

}  // namespace tz

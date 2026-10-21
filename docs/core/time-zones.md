# Time zones and calendar arithmetic

Code: `src/app/TimeZone.{h,cpp}` and `src/app/Calendar.{h,cpp}`, tests in `test/test_time_zone/` and `test/test_calendar/`. Pure C++17, no hardware, no global state.

A parent picks a zone by name and the clock follows the changes between winter and summer time on its own, so a school time of 08:00 stays 08:00 on the wall all year.

## Zones

`tz::find("Europe/Berlin")` returns a `Zone`: the exact name (case matters), the **base offset** from UTC in winter time in minutes, and a **rule** for summer time. `tz::zoneCount()` and `tz::zoneAt(i)` list the zones for a picker. `tz::offsetMinutes(zone, epochUtc)` is the offset at an instant, summer time included.

| Rule | Summer time | Zones |
|---|---|---|
| `EU` | from the last Sunday of March, 01:00 UTC, to the last Sunday of October, 01:00 UTC; +1 hour | Berlin, Vienna, Zurich, Paris, Madrid, Rome, Amsterdam, Brussels, Prague, Warsaw, Copenhagen, Stockholm (+01:00); London, Dublin, Lisbon, Canary (+00:00); Athens, Helsinki, Kyiv (+02:00) |
| `US` | from the second Sunday of March, 02:00 local standard time, to the first Sunday of November, 02:00 local summer time; +1 hour | New York (−05:00), Chicago (−06:00), Denver (−07:00), Los Angeles (−08:00) |
| `None` | never | Istanbul and Moscow (+03:00), UTC |

All EU zones change at the same instant, whatever their offset: 01:00 UTC is 02:00 in Berlin but 01:00 in London. The US zones each change at 02:00 on their own wall clock, which is a different UTC instant per zone.

There is no rule for the southern hemisphere or for Australia on purpose. Somebody who lives there enters a fixed offset instead.

### The transitions 2026–2028

`test_time_zone` pins every transition of 2026, 2027 and 2028 for Berlin, London, Athens and the four US zones, one second before and at the instant. The expected values come from the IANA time zone database, not from the code. The dates for the EU rule are:

| Year | Spring (to summer time) | Autumn (to winter time) |
|---|---|---|
| 2026 | 29 March | 25 October |
| 2027 | 28 March | 31 October |
| 2028 (leap year) | 26 March | 29 October |

and for the US rule 8 March / 1 November (2026), 14 March / 7 November (2027), 12 March / 5 November (2028). Two more tests step through every year hour by hour and require that every zone with a rule changes exactly twice and every zone without one never, and that every zone in the list has its base offset in winter and one hour more in summer.

## Calendar arithmetic

`calendar::daysFromCivil(year, month, day)` counts days from 1970-01-01 in the Gregorian calendar; `calendar::yearOf(epochSeconds)` is its inverse for the year; `calendar::weekday(days)` gives 0 = Monday … 6 = Sunday; `calendar::toLocal(epochUtc, offsetMinutes)` splits a Unix time into local day, minute of the day and weekday.

- Leap years follow from the arithmetic (2028 and 2000 have a 29 February, 2027 and 2100 do not); there is no table of month lengths. The last Sunday of a month is found from the day before the first of the next month.
- Divisions round down. A time shortly after 1970-01-01 with a negative offset is therefore the previous evening and not a negative minute of the day.
- The time is a 32-bit unsigned Unix time, which works until 2106.

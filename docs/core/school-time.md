# School time on the clock

Code: `src/app/SchoolTime.{h,cpp}`; tests in `test/test_school_time/`. The windows themselves are [quiet-hours.md](../quiet-hours.md); this is the profile's windows on the clock, and the one exception.

`SchoolTime::active(&end)` asks the profile whether school time is running now, with the minute it ends. A window the parents switched off (`enabled = false`) is not school time and there is nothing for an emergency to lift ([quiet-hours.md](../quiet-hours.md)). Without a known time of day there is no school time ([hardware-capabilities.md](../hardware/hardware-capabilities.md)); the end minute itself is free.

## The emergency exception

A child who sends an emergency from a locked phone could neither be called back nor call. So the last tap of the SOS override ([sos-flow.md](sos-flow.md)) calls `lift()`, and the lock is **lifted until the end of the school time that is running** — and only that: the next window locks again.

- The end is counted on the wall clock **from the second**, so the lift ends exactly at the end minute and not up to a minute later. A window across midnight is lifted until its end after midnight.
- Lifting when no school time runs changes nothing and returns false.
- The lift is logged (`Quiet hours lifted by emergency until 15:00`), on every log level: it must be provable afterwards.
- While the lift holds, `active()` is false and every consumer — dialling, messages, the SOS flow itself — sees no school time. If the parents change the windows while the device runs, the next question sees the new windows.

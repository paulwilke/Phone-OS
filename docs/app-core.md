# The application core

Code: `src/app/AppCore.{h,cpp}`; tests in `test/test_app_core/`. The facade over the components: it owns them, runs them in one fixed order per `poll()`, and gives the interface two things — **entry points** to call and a **read-only state** to draw from. It decides nothing itself; every rule lives in the component that owns it ([app-behaviour.md](app-behaviour.md) lists who owns what, and which test pins it).

| Component | Page |
|---|---|
| Profile | [profile.md](profile.md) |
| Notices | [notices.md](notices.md) |
| Positioning | [positioning.md](positioning.md) |
| Messaging, SchoolTime | [messaging.md](messaging.md), [school-time.md](school-time.md) |
| Calls | [calls.md](calls.md) |
| Emergency | [emergency.md](emergency.md) |
| Connectivity, WifiController | [connectivity.md](connectivity.md), [wifi-controller.md](wifi-controller.md) |
| PowerCoordination | [power-coordination.md](power-coordination.md) |

## No sentence

The core produces no text for the screen. Notices and system entries are codes with arguments, the network label is data, Wi-Fi reasons are enums. The interface words them from the catalogs when it draws, so a change of language also changes the old history entries. The only text the core builds is the body of an SMS, from the catalogs in the profile's SMS language.

## The host

The host (the firmware, or the simulator) provides the capabilities and calls:

1. `setRadio`, `setWifi`, `attachMessageLog` before `start()` for what it has; the radio may also come later, once it has started it on request;
2. `start()` once: restores the history, reads the trigger words, asks for the start-up fix;
3. `poll(displayOn, nextRenderInMs)` in the main loop. It returns the decision on sleeping, which the host carries out;
4. `leaveSos()` when the SOS screen closes.

The host does what only hardware can do: sleeps the processor, starts the radio when `takeRadioStartRequest()` says so, restarts when `restartRequested()` says so, switches the device off when `powerOffRequested()` says so (and tells `notePowerOffFailed()` when it could not).

## One pass

`poll()` runs, in this order:

1. the **time zone**: the profile's zone sets the clock's offset from the epoch, so school time stays on the wall clock across the change of the clocks; without a known time, or without a zone, nothing changes;
2. **messaging** (SMS, radio messages, send failures, delivery reports, the queue) and **calls** (call events, the ring); anything the modem reported proves it is awake;
3. **positioning** (remember fixes, end the start-up fix), then **emergency** (the countdown, the ping, the outcome, the receiver's reasons), then **connectivity** (the cell probe, Wi-Fi);
4. **power**: what is going on elsewhere (a call, queued work that could leave or a running search, a usable radio, Wi-Fi) goes to PowerCoordination as `Needs`, which applies the policy.

Messaging asks the core two things through its `Hooks`, and the core forwards them to the emergency component: a parent's query starts a location reply, and a text the network refused is told to the ping.

## The state

`state()` returns a plain struct: the wall clock as numbers (month, day, weekday counted from Sunday, minute of the day, in the clock's zone; the interface has no calendar of its own, and the numbers are absent while the time is unknown), the network label (data), registration, signal, SIM status, cellular, save mode and probe seconds, Wi-Fi phase and reason and what to join with (network, access point password, address, seconds left), unread count, queued messages, the notice, the call state and its other party, the volume, the SOS phase and countdown and taps, the ping stage and progress, school time and the minute it ends, whether the receiver is on, the battery, the charge state, whether the modem is asleep, the front light. The unread count includes system entries (a missed call, a refused query).

## Settings that change while the device runs

The core keeps its own copy of the profile. The child may change message vibration, how visible a remote query is and the language of the screen (`setInterfaceLanguage`, which a code without a catalog does not change; the SMS language is not touched); all apply at once and last until the next start, because writing the configuration back is the configuration's job. Nothing else of the profile changes while the device runs. `start()` applies the interface language of the profile.

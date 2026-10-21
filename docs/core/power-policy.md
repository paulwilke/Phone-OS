# Power policy

Code: `src/app/PowerPolicy.{h,cpp}`, tests in `test/test_power_policy/`. Pure decision logic: `PowerPolicy::decide(inputs)` returns what may sleep; putting things to sleep is the caller's job.

The trade-off: a modem that stays registered costs about a day of battery life, a modem that is switched off costs reachability. The compromise is a modem that stays registered but sleeps between events. It is woken by the ring-indicator line (an incoming call or SMS) or by the device when there is something to do.

## Inputs and outputs

`PowerInputs`: the time and the last key press, `callActive`, `pendingWork` (a queue, a running location search, an open reply), `modemSleepAllowed` (the setting), `cellularEnabled` (the modem is powered), `displayOn`, `nextRenderInMs`, `radioRunning`, `externalPower`, `modemRegistered`, `voiceSettled` (the IMS is registered, or the network carries SMS only), `registeredSeconds` (since the registration), `wifiActive`.

`PowerCoordination` fills the last three from the modem: registered is `Ready` or a call state; `voiceSettled` is a `voiceStatus()` of `Ready` or `SmsOnly` (`Unknown`, an IMS question that was not answered, waits like `ImsMissing`); `registeredSeconds` counts from the pass in which it first saw the modem registered, and every new registration (after a lost network, a restart of the modem, switching it on) starts it again at 0.

`PowerDecision`: `modemShouldSleep`, `cpuMaySleep` with `cpuSleepMs` (at most that long, then something is due again), and `radioShouldBePowered`.

## The rules

| Rule | Effect | Why |
|---|---|---|
| 5. radio | `radioShouldBePowered` is true exactly while `radioRunning` | The boot state of the port expander powers the radio chip; with no radio software running nobody read it, and it received around the clock. Cutting power while it is in use confuses the radio library |
| 6. external power | nothing sleeps | saving matters only on battery; light sleep also takes the USB console (the ESP32-S3 switches its USB bridge off) |
| 1. call or pending work | nothing sleeps | |
| 2. setting | modem sleep only if `modemSleepAllowed` | |
| 3. quiet time | modem may sleep after `kIdleSeconds` (60) without a key press; the boundary is inclusive | shorter saves more but wakes more often |
| 7. network | the modem must be registered to sleep | asleep it no longer answers AT commands and registration never happens; on the device it went to sleep during the operator search |
| 9. IMS | a registered modem sleeps only once `voiceSettled`, or `kImsWaitSeconds` (120) after the registration without it; the boundary is inclusive | asleep the modem is not asked about the IMS again. On the device (4.2.5) it went to sleep 14 s after registering, before its IMS, and the screens said "connecting" and a call was refused for 40 minutes, until something woke it. The IMS came about 30 s after the network; 120 s are four of the driver's 30-second questions, and the bound keeps a network without VoLTE from keeping the modem awake for good: it then sleeps as before, and the driver asks again at the next wake ([modem-driver.md](../modem/modem-driver.md)) |
| 4. processor | may sleep only when quiet, the display is off, and the modem sleeps or is switched off; for at most `kMaxCpuSleepMs` (2000) and not beyond the next render | in light sleep the serial clock stops, and a modem that is awake could send a message that is then lost; a sleeping modem wakes the device through the ring line first. A switched-off modem cannot lose anything |
| 8. WLAN | the processor does not sleep while WLAN is active | the connection drops in light sleep and the web server stops answering |

Rule 9 is pinned three times: the rule alone (`test_a_registered_modem_waits_awake_for_its_ims_up_to_the_bound`), the coordination with a fake modem (`test/test_power_coordination/`: the wait, the bound, a new registration that waits again), and the real driver on the scripted modem under the coordination (`test/test_modem_driver/`: no sleep while the IMS is missing, sleep once `+CIREG` says registered, sleep after the bound, the IMS asked at once after a wake).

A render that is due already (`nextRenderInMs` 0) gives the full `kMaxCpuSleepMs` and never a sleep of zero length. A last input in the future (the clock jumped back) counts as no quiet time: when in doubt, stay awake.

## Order matters

The rule for the radio is answered **first**, before the early returns for external power and calls. A version with the radio rule below the return for a call once cut the radio's power in the middle of a call; `test_the_radio_survives_a_call_and_pending_work` pins that, and `test_the_radio_follows_only_its_own_input_in_every_combination` runs all 1024 combinations of the other inputs. A second exhaustive test runs 2048 combinations and checks that sleeping implies every condition above and that, when all conditions hold, the processor sleeps.

The decision says nothing about energy; whether the numbers add up shows only a measurement over 24 hours on the device.

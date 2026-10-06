# Voice over LTE: what is proven, what is not

Code: `src/drivers/A7682EDriver.{h,cpp}` ([modem-driver.md](modem-driver.md)); tests in `test/test_modem_driver/`, `test/test_modem_calls/` and `test/test_hardware_contracts/`; the measurements are in [hardware-lessons.md](hardware-lessons.md). **Nothing in this document has been verified by the software on the device.** The host tests prove that the driver does what is written here against a script; they cannot prove that the modem answers as the script does. The four steps in the last section are the owner's checklist, and their date fields are empty on purpose.

## What was measured (the owner, 2026-10-06)

- Outgoing calls hung the modem two seconds after `ATD` (no AT answer for about seven seconds, then `*ATREADY`) whenever the IMS was not registered (`+CIREG: 1,0,0`). In that state the modem sometimes reports `VOICE CALL: BEGIN` at once instead: the "phantom call", where the product showed "in a call" while nothing was dialled.
- After one change the IMS registered: **PDP context 1 defined as `IPV4V6`** with the operator's APN, then the radio restarted (`AT+CFUN=0`, `AT+CFUN=1`). The IMS bearer (context 8) then appeared as `IPV4V6, APN "ims"`, `+CIREG: 1,1,15`; calls rang and connected. It survived a cold start of the modem and worked from the product firmware.
- The previous initialisation contained no `AT+CGDCONT` and never looked at `+CIREG`: everything depended on what the modem happened to have stored.

This was measured with **one SIM and one operator**, with an explicit APN.

## What the driver does about it

| Requirement | How | Where |
|---|---|---|
| A. Set up the default bearer itself, idempotently | the job `DataBearer` between the SIM check and registration: read `AT+CGDCONT?`, write context 1 as `IPV4V6` with the APN only when it differs, `AT+CFUN=0`, `AT+CFUN=1`; context 8 is never touched | [modem-driver.md](modem-driver.md), "The data bearer" |
| B. The APN comes from the configuration | `[modem] apn` in `settings.toml` through the schema; length (100) and characters (letters, digits, dots, hyphens, underscores) are checked by the loader and again by the driver before anything is sent | [config-loader.md](config-loader.md) |
| C. Never dial without the IMS | `voiceStatus()` is `Ready` only with a registered IMS (`SmsOnly` for registration 6 and 7, `ImsMissing` for a missing IMS); `dial()` refuses otherwise and sends nothing; the IMS is asked once more right before `ATD` | modem-driver.md, "Calls" |
| D. Do not believe `VOICE CALL: BEGIN` blindly | BEGIN counts only with a call that waits for it (ATD went out, which needs the fresh IMS answer, or an answer is on its way); `AT+CLCC` state 0 connects as well; BEGIN without a call is ignored and logged | modem-driver.md, "Calls" |
| E. Survive and report a restart during a call | `*ATREADY` after the modem answered ends the call once (`DialFailed` with `ModemRestarted`, `Ended`, or `Missed`), counts it (`restartCount()`) and starts again | modem-driver.md, "A restart of the modem" |
| F. Log and show the IMS | one line per registration (`Cellular: IMS registered (PDN8 IPV4V6)` or `IMS not registered, calls cannot be made (PDN8 IP)`), one line when it changes; a failed call carries the last lines of the modem and the IMS facts; the network page shows "IMS: yes/no" and the status bar and lock screen say "No calls yet" | [screen-net.md](screen-net.md), [ui-screens.md](ui-screens.md) |

The six test cases of the proposal exist, with counter-checks (the bug put back, the test failing):

1. context 1 missing → write, `CFUN=0`, `CFUN=1` in this order, once: `test_a_missing_context_1_is_written_and_the_radio_cycled_in_order_once`;
2. context 1 right → no write, no `CFUN`: `test_a_context_1_that_is_right_is_left_alone`;
3. `+CIREG: 1,0,0` → `dial()` refused, no `ATD`: `test_a_dial_without_ims_is_refused_and_no_atd_is_sent`;
4. `+CIREG: 1,1,15` → `ATD`; `+CLCC` 2, 3, 0 gives dialling, ringing, connected: `test_a_dial_with_ims_goes_through_dialling_ringing_and_connected`;
5. a `BEGIN` with no IMS → failed call, not connected: `test_a_dial_after_the_ims_was_lost_is_a_failed_call_and_a_phantom_begin_is_no_call`;
6. `*ATREADY` while dialling → failed call "modem restarted", then the initialisation again: `test_a_restart_while_dialling_fails_the_call_and_starts_over`.

## What is not proven, and what is not built

- **Other SIMs and operators.** The APN differs from operator to operator, and a wrong APN is written as faithfully as a right one. Only the explicit APN of one SIM was tested.
- **Roaming SIMs** are a separate problem: a roaming foreign SIM was rejected at call setup (`NW ACT 8,10` followed by `NW DEACT 8,10`), regardless of this change. A voice-only plan without a data package cannot have the IMS bearer at all (2026-09-30). The driver reports both as "calls not ready" or "SMS only" and does not pretend otherwise.
- **An empty APN** (the default) is option 1 of the proposal: the modem's own contexts are left alone, nothing is read for them, nothing is written, the radio is not cycled, and the log says so once per start. Whether a modem that was never configured then registers the IMS is **not tested**.
- **Not built, and the owner's decision:** option 2, a built-in table by MCC and MNC, and option 3, a context 1 as `IPV4V6` with an empty APN. A table would be correct for the operators in it and wrong, silently, for the others; an empty APN was not tried on the device.
- **Wear of the modem's memory.** `AT+CGDCONT` appears to be stored (it survived a cold start). The check "write only if different" keeps the writes rare: once per SIM, plus the extra ten seconds of the radio cycle.
- **`AT+CFUN=0/1` while registered drops a message in flight.** The job therefore runs before registration and before the first sending; a message queued before a restart goes out after the cycle (`test_a_message_queued_before_a_restart_goes_out_after_the_radio_cycle`). A message that was in flight when the modem restarted is reported as failed and not sent again.
- **Not understood:** why the IMS registered in one earlier good run without this setting.
- **The modem firmware** (`A7682M7_B15V01_241119`) may have a fault in the call setup path: it hangs without the IMS. No update package is published; that is a question for the vendor, not for this change. The driver's answer is to never dial without the IMS and to recognise the restart when it comes.
- **The registration of the IMS takes time.** After the radio cycle the IMS may need a minute; until then the voice status is `ImsMissing` and the screens say "calls not ready". The text on the network page names no duration, because none was measured.

## The owner's checklist on the device

Done with a tool on the real modem, not by the software. Date each step and write the result into [hardware-lessons.md](hardware-lessons.md).

| # | Step | Expected | Date | Result |
|---|---|---|---|---|
| 1 | Reproduce the broken state: `AT+CGDCONT=1`, `AT+CFUN=0`, `AT+CFUN=1` | `+CIREG: 1,0,0`; context 8 as `"IP"` | | |
| 2 | Flash the new firmware | context 1 `IPV4V6`; `+CIREG: 1,1,15` | | |
| 3 | Dial from the device | `+CLCC` 2, 3, 0; the call rings and connects | | |
| 4 | Cold start of the modem, repeat 2 and 3 | the same | | |

Until these four fields are filled, the honest sentence about calls is: **the driver is written and tested against a script; the device has not confirmed it.**

# The radio's emergency

Code: `src/app/EmergencyRadio.{h,cpp}`, with `Connectivity` (save mode, the start and stop requests), `Messaging` (who may write) and `Emergency` (the start); tests in `test/test_emergency_radio/`. Decisions: D-59 (the radio is an emergency path, hidden in normal use), D-61 (the SOS sends nothing on it), D-62 (when it ends), D-67 (the modem is off and searches on demand and by a timer).

## The antenna

The owner's note (2026-11-04): the emergency radio ideally uses a **LoRa antenna screwed on** for it; the radio is not a normal function but for the case that the mobile network has failed. The radio page says so under the Europe note ("Ideally a LoRa antenna is screwed on for the radio.", `radio.antenna`, English, German and French); [hardware/device.md](../hardware/device.md) has the hardware side.

## The rules

1. **It begins** when an emergency or a pickup is started (not the reply to a parent's query) while the device is not registered in a mobile network, and the profile allows the radio (`features.radio`). With the switch off nothing of this happens.
2. **While it lasts** the application
   * asks the host to start the radio (`takeRadioStartRequest()`; the host starts it and attaches it with `setRadio()`),
   * switches **save mode** on: Wi-Fi off, the modem **off** (D-67, [connectivity.md](connectivity.md)): without a network it only costs battery. A **search** switches it on for at most 90 seconds and off again if it found no network; it runs when the radio page's control "Check mobile network now" is used and every `[radio] cellular_retry` minutes (default 0 = never, D-68; on the parents' page under Advanced). **Once registered it stays on** as long as it keeps the network. Switching cellular on or off by hand is refused while the emergency lasts (logged; the control is the way),
   * accepts a radio message from **anybody** and shows it ([messaging.md](messaging.md)); the child may answer whoever wrote, whatever the free-text switch says.
3. **It lasts until the SOS is ended on the device.** The SOS page shows the key "End SOS" while it lasts. The mobile network coming back does not end it, and neither does a time: a child must not lose the radio in the middle of a conversation (D-62). Ending it asks the host to stop the radio (`takeRadioStopRequest()`), puts cellular back as it was before (on if it was on, off if it was off; no timer is left) and closes the door to strangers again: only `[radio] peer` may write, as in normal use.
4. **The SOS itself is not sent on the radio** (D-61). The text of the SOS is sent cellular-only: the modem is off between the searches, so the text waits in the queue until a search finds a network and then goes out by SMS, **while the radio keeps running** until the SOS is ended (D-67). The radio is for a conversation with home or with others. A queued cellular-only text is work for the power policy only while the modem is on (`Messaging::hasWork()` is unchanged): with the modem off between searches the processor may sleep; during a search the device is awake, as for any modem without a network.
5. **A radio that does not start ends the emergency** (`radioStartFailed()`), so that the device does not sit in save mode without a radio; cellular is put back as it was.

The state the interface reads is `AppCore::State::emergencyRadio`; the entry point of the key is `AppCore::endSos()`.

## What the host does

The host (`device/main.cpp`, `sim/Simulator.h`) answers `takeRadioStartRequest()` by starting the radio and `takeRadioStopRequest()` by detaching it and letting go of the chip. The radio chip is powered while a radio is attached (`PowerCoordination` is told by `radioAttached`, not by "knows a contact": a radio that has just started knows nobody yet). The device also announces itself with an advertisement at the start and every ten minutes, because MeshCore delivers a message only from a node it knows ([hardware/device.md](../hardware/device.md)).

## Not done

No vibration or sound for a radio message, also during the emergency; no notice for save mode; the radio page's "Switch radio on" still starts the radio outside an emergency where the profile allows it. See open point 9 and D-63.

## Tests

`test_emergency_radio` (RAD-01 to RAD-18: with and without a network, a pickup, the switch off, the query reply, save mode switching the modem off, a stranger dropped and heard, answered, the SOS text kept off the radio, the SOS text leaving by SMS when a search finds the network, the timer and a lost network, timer 0, the control outside the emergency, sleep while the modem is off, ending it, cellular that was off staying off, a radio that does not start, the chip's power), `test_connectivity` (CON-20 to CON-27: the modem off, the search on demand, the timer, timer 0, a network found kept, a network lost searched again, leaving), `test_messaging` (MSG-65, MSG-66), `test_screen_sos` and `test_screen_radio` (the key, the control "Check mobile network now", the Europe and antenna notes, writing), `test_config_loader` (`cellular_retry`), the scenario `radio-emergency` and the changed `sos-page`.

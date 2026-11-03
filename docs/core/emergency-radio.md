# The radio's emergency

Code: `src/app/EmergencyRadio.{h,cpp}`, with `Connectivity` (save mode, the start and stop requests), `Messaging` (who may write) and `Emergency` (the start); tests in `test/test_emergency_radio/`. Decisions: D-59 (the radio is an emergency path, hidden in normal use), D-61 (the SOS sends nothing on it), D-62 (when it ends), D-64 (the modem stays on).

## The rules

1. **It begins** when an emergency or a pickup is started (not the reply to a parent's query) while the device is not registered in a mobile network, and the profile allows the radio (`features.radio`). With the switch off nothing of this happens.
2. **While it lasts** the application
   * asks the host to start the radio (`takeRadioStartRequest()`; the host starts it and attaches it with `setRadio()`),
   * switches **save mode** on: Wi-Fi off, the modem **on and searching** (switched on if it was off, and switching it off is refused while the emergency lasts), so that the SOS text leaves at the first chance (D-64, [connectivity.md](connectivity.md)),
   * accepts a radio message from **anybody** and shows it ([messaging.md](messaging.md)); the child may answer whoever wrote, whatever the free-text switch says.
3. **It lasts until the SOS is ended on the device.** The SOS page shows the key "End SOS" while it lasts. The mobile network coming back does not end it, and neither does a time: a child must not lose the radio in the middle of a conversation (D-62). Ending it asks the host to stop the radio (`takeRadioStopRequest()`), puts cellular back as it was before (off again if it was off) and closes the door to strangers again: only `[radio] peer` may write, as in normal use.
4. **The SOS itself is not sent on the radio** (D-61). The text of the SOS is sent cellular-only: the modem keeps searching during the emergency, so it waits in the queue until the modem is registered and then goes out by SMS, **while the radio keeps running** until the SOS is ended (D-64). The radio is for a conversation with home or with others. A queued text is work for the power policy while the modem is on (`Messaging::hasWork()` is unchanged), so the device stays awake until the text has left; without a network the modem never sleeps anyway.
5. **A radio that does not start ends the emergency** (`radioStartFailed()`), so that the device does not sit in save mode without a radio; cellular is put back as it was.

The state the interface reads is `AppCore::State::emergencyRadio`; the entry point of the key is `AppCore::endSos()`.

## What the host does

The host (`device/main.cpp`, `sim/Simulator.h`) answers `takeRadioStartRequest()` by starting the radio and `takeRadioStopRequest()` by detaching it and letting go of the chip. The radio chip is powered while a radio is attached (`PowerCoordination` is told by `radioAttached`, not by "knows a contact": a radio that has just started knows nobody yet). The device also announces itself with an advertisement at the start and every ten minutes, because MeshCore delivers a message only from a node it knows ([hardware/device.md](../hardware/device.md)).

## Not done

No vibration or sound for a radio message, also during the emergency; no timed probe of the mobile network and no notice for save mode (the modem's search current during the emergency is the price the owner accepted, D-64); the radio page's "Switch radio on" still starts the radio outside an emergency where the profile allows it. See open point 9 and D-63.

## Tests

`test_emergency_radio` (RAD-01 to RAD-14: with and without a network, a pickup, the switch off, the query reply, save mode keeping the modem on, a stranger dropped and heard, answered, the SOS text kept off the radio, the SOS text leaving by SMS when the network returns during the emergency, ending it, cellular that was off switched on and off again, a radio that does not start, the chip's power), `test_connectivity` (CON-20 to CON-22), `test_messaging` (MSG-65, MSG-66), `test_screen_sos` and `test_screen_radio` (the key, the note, writing), the scenario `radio-emergency` and the changed `sos-page`.

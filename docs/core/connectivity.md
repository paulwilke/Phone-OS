# Connectivity

Code: `src/app/Connectivity.{h,cpp}`; tests in `test/test_connectivity/`. How the device reaches the world, and the switches that decide it. Uses power, modem and clock ([hardware-capabilities.md](../hardware/hardware-capabilities.md)), the profile ([profile.md](profile.md)), notices ([notices.md](notices.md)), [power-coordination.md](power-coordination.md) and [wifi-controller.md](../web/wifi-controller.md).

## Cellular

`setCellularEnabled(on)` switches the modem's power. It is logged and shown as a notice; asking for the state it is in does nothing, and a switch the hardware refuses is logged and returns false (no notice: nothing happened). Either way the modem is **not asleep** afterwards — off or just started — which is told to PowerCoordination.

"Registered" is one definition in `Cellular.h`: powered, in a state past the search for a network, with an operator named.

## The radio

Starting the radio software belongs to the host. This component keeps the request: `requestRadioStart()` is refused when the profile does not allow the radio or it runs already, and the host reads and clears it with `takeRadioStartRequest()` (true once per request), then attaches the radio with `setRadio()`. The radio never starts by itself.

`requestRestart()` only records the wish; the host restarts.

`requestRadioStop()` is the other direction: it asks the host to stop the radio when it runs (`takeRadioStopRequest()`, once per request), and the host detaches it with `setRadio(nullptr)`.

**Save mode** is not something a person switches. It comes back with the radio (D-1, D-62) as the battery saving of the radio's emergency, and since D-67 it is **Wi-Fi off, modem off except for searches**: without a network the modem only costs battery, but the queued SOS text must not be lost. `EmergencyRadio` calls `enterSaveMode()` when the emergency begins and `leaveSaveMode()` when the SOS is ended (or the radio does not start). Entering remembers whether cellular was on and switches the modem **off at once**. A **search** (`searchNow()` from the radio page's control, and `poll()` by the timer `[radio] cellular_retry`, minutes, default 0 = never (D-68), at most 240) switches the modem on for at most `kSaveSearchWindowSeconds` (90); if it is not registered by then it is switched off again and the timer counts from that moment. **Once registered it stays on** for as long as it keeps the network: the queued SOS text leaves by SMS, and calls and texts work. A network lost again starts a new window of 90 seconds (the modem is on already), then off again. The searches switch the modem without a notice; the choice is that **switching cellular by hand is refused in both directions** while save mode holds (`setCellularEnabled` logs `Cellular on refused` / `Cellular off refused`), Wi-Fi on likewise; the radio page's control is the way. Leaving puts cellular back as it was before (a cellular the child had switched off is off again) and leaves no timer. **Power:** with the modem off between searches `PowerPolicy` lets the processor sleep (rule 4: a switched-off modem cannot lose a message; `Messaging::hasWork()` is false for a queued cellular-only text while the modem is off); during a search the modem has no network and so stays awake (rule 7). Time is the application's uptime in `poll()`, nothing sleeps for real. The old timed probe of the mobile network and the save-mode notices are not part of it. Tests: CON-20, CON-21, CON-23 to CON-27.

## Wi-Fi

`setWifiMode(mode)` goes to the Wi-Fi controller, with one rule in front of it: **not on a nearly empty battery** — below 20 percent, unless a cable feeds the device (`WifiBatteryLow`, logged). An unknown battery level does not refuse.

Switching it **off** always works. Without a Wi-Fi capability nothing happens. Every request is logged ("Wi-Fi home network", "Wi-Fi access point failed", ...). `poll()` keeps the controller going.

## The network label

`networkLabel()` is data, not text: `Operator` (the name shortened for the status line, [operator-label.md](operator-label.md); it can be empty for a name that shortens to nothing, and the interface then says a generic word), `Radio` (no operator, the radio runs), `Connecting` or `None`. The interface words the last three. **`Connecting`** is a modem on its way: cellular on, the modem neither off nor in error, no operator, and either no network named since the modem was last switched on or the modem registered while the name has not come yet. A network lost after the first one is `None`, so that the child sees that nobody can reach the device (owner, 2026-10-08: after switching on, the status line said "Network" and "No calls yet" before the operator's name). `test_the_label_says_connecting_until_the_first_network_since_switching_on`.

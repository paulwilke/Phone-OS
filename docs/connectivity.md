# Connectivity

Code: `src/app/Connectivity.{h,cpp}`; tests in `test/test_connectivity/`. How the device reaches the world, and the switches that decide it. Uses power, modem and clock ([hardware-capabilities.md](hardware-capabilities.md)), the profile ([profile.md](profile.md)), notices ([notices.md](notices.md)), [power-coordination.md](power-coordination.md) and [wifi-controller.md](wifi-controller.md).

## Cellular

`setCellularEnabled(on)` switches the modem's power. It is logged and shown as a notice; asking for the state it is in does nothing, and a switch the hardware refuses is logged and returns false (no notice: nothing happened). Either way the modem is **not asleep** afterwards — off or just started — which is told to PowerCoordination.

"Registered" is one definition in `Cellular.h`: powered, in a state past the search for a network, with an operator named.

## The radio

Starting the radio software belongs to the host. This component keeps the request: `requestRadioStart()` is refused when the profile does not allow the radio or it runs already, and the host reads and clears it with `takeRadioStartRequest()` (true once per request), then attaches the radio with `setRadio()`. The radio never starts by itself.

`requestRestart()` only records the wish; the host restarts.

**Save mode** ("radio only": cellular and Wi-Fi off, with a timed probe of the mobile network) is not part of version 4.0. It needs the LoRa radio, which 4.0 does not have, and comes back with the radio project, where an emergency radio mode switches battery saving on implicitly.

## Wi-Fi

`setWifiMode(mode)` goes to the Wi-Fi controller, with one rule in front of it: **not on a nearly empty battery** — below 20 percent, unless a cable feeds the device (`WifiBatteryLow`, logged). An unknown battery level does not refuse.

Switching it **off** always works. Without a Wi-Fi capability nothing happens. Every request is logged ("Wi-Fi home network", "Wi-Fi access point failed", ...). `poll()` keeps the controller going.

## The network label

`networkLabel()` is data, not text: `Operator` (the name shortened for the status line, [operator-label.md](operator-label.md); it can be empty for a name that shortens to nothing, and the interface then says a generic word), `Radio` (no operator, the radio runs), `Connecting` or `None`. The interface words the last three. **`Connecting`** is a modem on its way: cellular on, the modem neither off nor in error, no operator, and either no network named since the modem was last switched on or the modem registered while the name has not come yet. A network lost after the first one is `None`, so that the child sees that nobody can reach the device (owner, 2026-10-08: after switching on, the status line said "Network" and "No calls yet" before the operator's name). `test_the_label_says_connecting_until_the_first_network_since_switching_on`.

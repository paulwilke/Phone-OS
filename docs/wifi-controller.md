# Wi-Fi

Code: `src/app/WifiSettings.{h,cpp}`, `src/app/WifiController.{h,cpp}`; tests in `test/test_wifi_controller/`. Wi-Fi is off unless somebody asks for it, and it switches itself off after a while. After the modem it is the biggest consumer; a device that transmits on the side does not last a day. Everything runs through the `Wifi` capability ([hardware-capabilities.md](hardware-capabilities.md)); the rules in front of a request (save mode, battery) are in [connectivity.md](connectivity.md).

## There is one

The device has **one** Wi-Fi controller: the application core's (`AppCore::setWifi` makes it, `AppCore::wifiController()` hands it out). The device's own screens switch it through the core (`setWifiMode`, with the rules of [connectivity.md](connectivity.md) in front: save mode, battery), the web service tells it about use and reads its state, and the server of the parents' page runs exactly while it says Wi-Fi is up. What the child's screens show and what the page shows are therefore the same state (`AppCore::State::wifiPhase`, `/api/state`), and the idle time cannot run out on one side while the other believes Wi-Fi is on. The child's device does not need Wi-Fi in daily use (calls are VoLTE, messages are SMS); it is switched on from the device when somebody wants the page.

The idle time is `auto_off_minutes` (default 10). The page has a button "keep Wi-Fi on" (`POST /api/wifi-extend`) that restarts it on purpose.

## Settings

`WifiSettings` is plain data with the rules that make an entry usable; reading it from the configuration is a later package.

| Setting | Rule |
|---|---|
| networks | at most 4; name 1 … 32 bytes; password empty (an open network) or 8 … 63 characters |
| access point name | 1 … 32 bytes, default `phoneplus` |
| access point password | empty or 8 … 63 characters; empty means a new random one at every start |
| idle minutes | clamped to 1 … 120, default 10 |

An entry that breaks a rule is refused and changes nothing. Passwords are kept in clear on the card like every other secret there, and are never shown on the web page.

## Modes

- **Station**: signs in to the stored networks in turn, each for at most `kAttemptSeconds` (20). A failure or a timeout moves on to the next; when the last one fails the chip is switched off and the reason is `HomeNetworkUnreachable`. Without any stored network: `NoHomeNetwork`. A connection that is lost is retried from the first network.
- **AccessPoint**: the device's own network, so that it can be set up where there is no home network. The password is the stored one, or a random eight-digit one (padded with zeros, typeable from the screen) that **only the device's screen reveals** and that is forgotten when Wi-Fi goes off. An access point that will not start, or stops while running, is `AccessPointFailed` / `AccessPointLost`.

Switch-off counts from the **last use** (`noteActivity`), not from switching on: somebody who is editing the contacts must not be thrown out in the middle. The web service calls it for **every authenticated request** of the parents' page ([web-interface.md](web-interface.md)); a request without the right password, from a client that is locked out, or for the login page does not, or anybody could keep Wi-Fi on. A station's idle time starts when it has connected. The reason of a switch-off is a code (`WifiReason`), not a sentence; the interface words it.

While Wi-Fi runs the processor does not sleep (power rule 8, [power-policy.md](power-policy.md)): `active()` is what PowerCoordination is told.

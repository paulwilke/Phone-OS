# Wi-Fi

Code: `src/app/WifiSettings.{h,cpp}`, `src/app/WifiController.{h,cpp}`; tests in `test/test_wifi_controller/`. Wi-Fi is off unless somebody asks for it, and it switches itself off after a while. After the modem it is the biggest consumer; a device that transmits on the side does not last a day. Everything runs through the `Wifi` capability ([hardware-capabilities.md](hardware-capabilities.md)); the rules in front of a request (save mode, battery) are in [connectivity.md](connectivity.md).

## There is one

The device has **one** Wi-Fi controller: the application core's (`AppCore::setWifi` makes it, `AppCore::wifiController()` hands it out). The device's own screens switch it through the core (`setWifiMode`, with the rules of [connectivity.md](connectivity.md) in front: save mode, battery), the web service tells it about use and reads its state, and the server of the parents' page runs exactly while it says Wi-Fi is up. What the child's screens show and what the page shows are therefore the same state (`AppCore::State::wifiPhase`, `/api/state`), and the idle time cannot run out on one side while the other believes Wi-Fi is on. The child's device does not need Wi-Fi in daily use (calls are VoLTE, messages are SMS); it is switched on from the device when somebody wants the page.

The idle time is `auto_off_minutes` (default 10). The page has a button "keep Wi-Fi on" (`POST /api/wifi-extend`) that restarts it on purpose.

## Settings

`WifiSettings` is plain data with the rules that make an entry usable; `ConfigLoader` reads it from the configuration ([config-loader.md](config-loader.md)).

| Setting | Rule |
|---|---|
| networks | at most 4; name 1 … 32 bytes; password empty (an open network) or 8 … 63 characters |
| access point name | 1 … 32 bytes, default `phoneplus` |
| access point password | empty or 8 … 63 characters; empty means a new random one at every start |
| idle minutes | clamped to 1 … 120, default 10 |

An entry that breaks a rule is refused and changes nothing. Passwords are kept in clear on the card like every other secret there, and are never shown on the web page.

## Modes

- **Station**: signs in to the stored networks in turn, each for at most `kAttemptSeconds` (20). A failure or a timeout moves on to the next; when the last one fails the chip is switched off and the reason is `HomeNetworkUnreachable`. Without any stored network: `NoHomeNetwork`. A connection that is lost is retried from the first network, but only when it stays lost (see "A lost link is a link that stays lost").
- **AccessPoint**: the device's own network, so that it can be set up where there is no home network. The password is the stored one, or a random eight-digit one (padded with zeros, typeable from the screen) that **only the device's screen reveals** and that is forgotten when Wi-Fi goes off. An access point that will not start, or stops while running, is `AccessPointFailed` / `AccessPointLost`.

Switch-off counts from the **last use** (`noteActivity`), not from switching on: somebody who is editing the contacts must not be thrown out in the middle. The web service calls it for **every authenticated request** of the parents' page ([web-interface.md](web-interface.md)); a request without the right password, from a client that is locked out, or for the login page does not, or anybody could keep Wi-Fi on. A station's idle time starts when it has connected. The reason of a switch-off is a code (`WifiReason`), not a sentence; the interface words it.

While Wi-Fi runs the processor does not sleep (power rule 8, [power-policy.md](power-policy.md)): `active()` is what PowerCoordination is told.

## A lost link is a link that stays lost

Seen on the device 2026-10-07, in station mode: a few seconds after the parent logged in, `system.log` had `WEB: GET /api/lang/de -> 200, aborted (stopped), sent 165 of 4603 bytes` (earlier the same with `/api/languages`, 164 of 315 bytes). "stopped" means `WebServer::stop()` ran in the middle of the answer. The chain: `Esp32WifiRadio::stationLink()` maps every `WiFi.status()` except connected, connect-failed and no-SSID to "connecting"; `WifiController::poll` took the first such reading in the Connected phase for a lost link and started the whole connection again (`startNetwork(0, ...)`, phase Connecting); `WebService::poll` stops the server whenever the phase is not Connected or AccessPoint. The cause of the reading itself is not established. The first API request after login is when the translator parses the web catalogs, so memory pressure is the main suspect; USB (no TCP) and `curl` of the page (no catalogs) never showed it.

The rule now: in the Connected phase a reading other than Connected starts a grace period of `kLinkGraceSeconds` (5). The phase stays Connected, so the server keeps its answer going and the radio is not touched. A Connected reading inside the period ends it. Only a link that is still not connected when the period is over is lost, and the connection starts again from the first network. The poll counts whole seconds, so the wait is 4 to 5 s of real time. Five seconds is long enough for the stack's transient states, short enough that a real loss (router restarted, out of range) is noticed at once by a person; the cost of a real loss is those five seconds. This is a decision on the owner's list of limits only in the sense that it delays a reconnect; it weakens no check.

The access point has no such pattern: `BoardWifi::state()` answers AccessPoint for as long as the mode is on and never reads the chip, so there is no transient to wait out, and it is left as it was.

The log says why (`Wi-Fi:` lines are kept on every log level, [log-level.md](log-level.md)): when the period starts, `Wi-Fi: link not connected (status 6, heap 41236 free, largest 20480); waiting 5 s before reconnecting` (the text in brackets is `Wifi::linkDetail()`: on the device the raw `wl_status_t` number, the free heap and the largest free block; empty in the simulator), then either `Wi-Fi: link back after 3 s` or `Wi-Fi: link still not connected after 5 s (...); reconnecting`. At most three opening lines a minute; the next one that is written says how many were left out, and the closing line is written only for an opening line that was. Times are whole seconds (the controller works on the seconds of the main loop), so a link that came back at once reads "within 1 s".

Tests: `test/test_wifi_controller/` (WIF-17 to WIF-21), `test/test_runtime/` (WSV-04), `test/test_board_network/` (the radio's words reach the capability). Status: seen on the device; fix host-tested, not yet verified on the device. What only the device can show: whether the readings still happen, what status they carry, and what the heap was.

## The idle time never runs backwards

Seen on the device 2026-10-08, after the 5 s grace period was in: Wi-Fi still switched off by itself, each time at the first request after the login, and the answer in flight was cut (`WEB: GET /api/state -> 200, aborted (stopped)`); no `Wi-Fi: link …` line, so the link had not been lost. The page notes its activity with the time of the host's loop (`millis()/1000`), the controller is polled with the clock's uptime, which starts when the clock object is made, a few hundred milliseconds after boot. For part of every second the page's time stood one second ahead, and `now - lastActivity_` in unsigned arithmetic read as the whole range: idle far beyond `auto_off_minutes`, so the controller switched Wi-Fi off. The controller now counts an activity "in the future" as an activity now (`idleSeconds`). Test: `test_an_activity_a_second_ahead_of_the_controllers_clock_keeps_wifi_on`; counter-check: the plain subtraction makes it fail. Host-tested; not yet verified on the device.

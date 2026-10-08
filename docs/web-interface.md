# Web interface

Code: `src/web/`; tests in `test/test_web_server/`, `test/test_http_fuzz/`, `test/test_web_api/`, `test/test_web_page/`. A small HTTP/1.1 server that the parent opens in a browser over the device's Wi-Fi. The interface lets the parent read the device state, edit every configuration file, browse the card, view the log, and export or restore a backup. The page is a single HTML file (`web/index.html`) that the server answers; language switching is live.

## HTTP layer

`src/web/Http.{h,cpp}` — a bounded, stateless parser in the `http` namespace.

| Constant | Value | Meaning |
|---|---|---|
| `kMaxHead` | 1024 bytes | Maximum header section |
| `kMaxBody` | 8192 bytes | Maximum body |

`Parser::feed()` takes raw bytes in any chunk size and returns one of four states: `Head` (still reading headers), `Body` (reading body), `Done` (complete), `Error`. An error sets a status code — 400 (garbage), 411 (no Content-Length), 413 (body too large), 414 (URI too long), 431 (headers too large), 501 (chunked encoding). Once the parser reaches `Done` or `Error` it ignores further input. The fuzz suite (`test/test_http_fuzz/`) feeds random bytes and truncated valid requests under AddressSanitizer to check that no input causes a crash or an out-of-bounds read.

`formatHead()` writes the response status line and headers into a caller-supplied buffer; the body is a separate pointer so the firmware can point directly at flash.

`basicPassword()` extracts the password from an `Authorization: Basic …` header without allocating. `constantTimeEquals()` compares two strings in constant time to prevent timing attacks on the password check.

## Server

`src/web/WebServer.{h,cpp}` — one TCP connection at a time; port 80. The server owns nothing about content: it takes a `RequestHandler` and calls `handle()` on a complete request or `error()` on a parse error. `WebAssets::indexHtml()` supplies the embedded page; `GET /` returns it.

| Constant | Value | Meaning |
|---|---|---|
| `kFirstByteSeconds` | 3 | a connection that has not sent one byte is let go after this (browsers keep spare connections that never send) |
| `kIdleSeconds` | 10 | silence once a request has begun, and a response that makes no progress |
| `kLingerSeconds` | 2 | after an error answer that was sent before the request was read to the end, the rest of the input is read and thrown away for at most this long |
| `kLogLinesPerMinute` | 20 | at most this many lines about bad requests per minute; the rest is counted and reported by one line |

A connection is never closed with input unread. The stack answers such a close with a reset, and a reset can make the browser throw away the response it has already received; so input that is waiting is read before the close, and a parse error (a body that is too large, a bad head) is answered at once and followed by the short lingering close.

**Writes.** `write()` may take fewer bytes than offered; the server sends the rest on the next call and counts every byte. A write that takes nothing is not simply tried again: the server reads once, and a read that says "gone" ends the connection at once, because on the device a dead connection answers every write with 0 and would otherwise hold the one slot for the whole idle time.

**The log.** Every request that ends with an error status (400 and above), and every response that did not go out completely, and every connection that closed without a complete request, leaves one line in the log (`WEB:` lines are kept on every log level, [log-level.md](log-level.md)): `WEB: GET /api/file?path -> 404, sent 33 of 33 bytes`, `WEB: GET /api/contacts -> 200, aborted (stalled), sent 1460 of 5120 bytes`, `WEB: connection closed without a complete request (silent, 3 s, 0 bytes received)`. Method, path, the *names* of the query parameters (never their values), the status and the bytes sent of the total: nothing from a body, the password or a query value, and characters outside printable ASCII are shown as `?`. At most `kLogLinesPerMinute` lines a minute; the lines left out are counted and one line says how many at the next one after the minute.

### Seen on the device 2026-10-07 and what the code path does

On the device the contacts tab and the log tab did not load, in the simulator they did. **Not yet verified on the device**: the cause was not found by looking at the device, only by reading the path below; the changes remove the weaknesses that reading found and the log lines are there so that the next try shows the cause.

The path: browser, lwIP, `WiFiServer`/`WiFiClient` of the Arduino core (`framework-arduinoespressif32` 2.0.17), `device/Esp32TcpListener.h`, `board::BoardTcpServer`, `web::WebServer`.

- **A second connection while one is served.** `WebServer::poll` takes the next connection only when it has finished the one before. `WiFiServer` was built with the core's default backlog of 4 (`max_clients`); further connections wait in lwIP's accept queue, completed at the TCP level, and their request waits unread. A browser opens up to six connections to one host and keeps spare ones. Before the change a spare connection that sent nothing kept the slot for `kIdleSeconds` (10 s), and every request behind it waited that long: three spare connections are half a minute during which the page looks dead. Now such a connection is let go after `kFirstByteSeconds`. The simulator serves from the same `WebServer`; what differs there is the network below it (loopback, kernel buffers, no radio), which is why the host tests now put the device's behaviour into their fakes.
- **`WiFiClient::write`** (WiFiClient.cpp in the core's WiFi library). It is not non-blocking: it loops, each pass waits up to one second in `select()` for room and then `send(MSG_DONTWAIT)`s; progress renews ten tries; it returns the bytes taken, which is all of them unless there was no progress for ten tries (ten seconds), and it returns 0 as well when `send` fails with anything but `EAGAIN`, in which case it also stops the client for good. So a full lwIP send buffer (`CONFIG_LWIP_TCP_SND_BUF_DEFAULT` 5760 bytes in the core's build for the ESP32-S3) does not give short writes: it makes the call wait, and the main loop with it. A 0 therefore means "dead or stuck for ten seconds", never "try again in a moment". `WebServer` used to take a 0 for "no progress" and wait for the idle time on top; on a dead connection (reset by the browser that gave up) every later write returned 0 at once and the slot stayed taken for the rest of the ten seconds. Now a write that takes nothing is followed by a read, and "gone" ends the connection at once. The host fakes accept short writes, give 0 when the send buffer is full and take bytes only when the peer reads (`test_web_server`).
- **The idle timeout.** `lastProgress_` is the time of the last byte read or written, in whole seconds from the main loop (`now` is read once at the start of the poll, so a `write` that blocked for ten seconds shows up at the next one). Ten seconds for a request that has begun and for a response without progress; three seconds before the first byte.
- **`Connection: close`.** Every response carries it (`Http.cpp`, `formatHead`) and the connection is closed after the last byte; there is no keep-alive, no pipelining. What was missing is the lingering close above: `WiFiClient::stop()` closes the socket, and lwIP answers a close with unread data in the receive queue with a reset instead of a FIN. A request that is read completely leaves nothing unread, but an error answer to a request still being sent (413 for a body that is too large) and a second request sent down the same connection do. The page's own requests are all complete, so this is not the main suspect for the two tabs, but it is a way to lose an answer that the host fakes never showed (their close never resets).
- **`BoardTcpConnection::read` answering -1 while a request is still arriving.** It does not: it reads what is available first and says -1 only when nothing is available and `socket_->connected()` is false (`test_board_network`: half a request is handed over, a quiet client is 0, -1 only after the last byte). The weakness was in `Esp32TcpSocket::connected()`, which called `WiFiClient::connected()`: that asks `recv()` for zero bytes and decides from `errno`, which `recv` leaves untouched when the peer has closed its side, so a closed connection was reported as alive (until the idle time) or, with an `errno` left over from an earlier failure on another socket, a live one as gone. The device glue now peeks one byte itself: data or "would block" is alive, end of stream or an error is gone. Not host-testable; to be verified on the device. A client that half-closes right after its request is therefore treated as gone (no browser does that; the simulator's socket treats it the same way).
- **The listener's one socket object.** `Esp32TcpListener::nextClient` puts every new client into the same `Esp32TcpSocket`; `BoardTcpServer::accept` used to close "the one before" after taking the new client, which stops the new client. `WebServer` never accepts while a connection is open, so this did not bite; the order is fixed and tested with a listener that behaves like the device's (`test_board_network`).
- **Sizes.** The largest answers are the log tail (the end of `system.log`, at most 12287 bytes: the log on the device was 56 KB) and the catalogs; the head is a separate 512-byte buffer and the body is sent from its own storage. A request head may be 1024 bytes; a Chrome head with the password, `Referer` and the client hints is roughly 700 bytes (estimated, not measured), so it should fit, but a long cookie from another application on the same host name would not (431, now logged).

**The first API request after login stopped the server (2026-10-07, home network).** `WEB: GET /api/lang/de -> 200, aborted (stopped), sent 165 of 4603 bytes`: the answer was cut by `WebServer::stop()`, which `WebService::poll` calls whenever the controller's phase is not Connected or AccessPoint. The controller had taken a single `WiFi.status()` reading other than connected for a lost link and restarted the connection. The fix is in the controller, not the server: a lost link now has to stay lost for five seconds before the phase changes ([wifi-controller.md](wifi-controller.md), "A lost link is a link that stays lost"), so the server keeps running through a glitch and an answer in flight is finished (`test_runtime`: `test_the_page_keeps_an_answer_going_through_a_link_glitch`). Seen on the device; fix host-tested, not yet verified on the device. The first API request is also when the translator parses the web catalogs (`Translator::ensureWeb`); the device now logs `Web catalogs loaded (before: heap N free, largest M; after: ...)` once per load (`device/ShippedCatalogs.h`), which shows whether that squeezes memory.

What only the device can show: whether the log and contacts tabs load now, how many connections the browser leaves idle, and the lines of the new kind in `system.log` (`WEB: ... -> status`, `aborted (...)`, `connection closed without a complete request`).

## API

`src/web/WebApi.{h,cpp}` implements `RequestHandler`. All routes require a matching password from `/config/secrets.toml`; without a stored password the API is entirely locked.

### Routes

| Method | Path | Action |
|---|---|---|
| GET | `/` | The web page (index.html) |
| GET | `/api/state` | Device state as JSON |
| GET | `/api/settings` | `/config/settings.toml` as JSON |
| PUT | `/api/settings` | Write `/config/settings.toml` |
| GET | `/api/contacts` | `/config/contacts.toml` as JSON |
| PUT | `/api/contacts` | Write `/config/contacts.toml`: this file is also the list of who may write to the device and call it, nobody else ([contacts.md](contacts.md)) |
| GET | `/api/secrets` | `/config/secrets.toml` as JSON |
| PUT | `/api/secrets` | Write `/config/secrets.toml` |
| GET | `/api/files` | One directory (query: `path=`, default `/`) as `{"path":"/config","entries":[{"name":"contacts.toml","size":210,"dir":false}, …]}`; names of the device's own scratch files are left out, everything else is listed (macOS `._*` leftovers too) |
| GET | `/api/file` | Download a file (query: `path=`) |
| PUT | `/api/file` | Upload or overwrite a file (query: `path=`) |
| DELETE | `/api/file` | Delete a file (query: `path=`) |
| GET | `/api/backup` | All config files as a JSON bundle |
| PUT | `/api/restore` | Restore a backup bundle |
| PUT | `/api/restart` | Ask the device to restart |
| PUT, POST | `/api/wifi-off` | Ask Wi-Fi to switch off (done after the answer has gone out) |
| PUT, POST | `/api/wifi-extend` | Keep Wi-Fi on: restarts the idle time on purpose; answers with `seconds_left`; logged like every change |
| GET | `/api/languages` | Installed language catalog list |
| GET | `/api/lang/<code>` | The `web.*` keys, from the web set (`lang/web/<code>.toml` layered with `/lang/web/<code>.toml` of the card; English where a language has no web catalog) |
| GET | `/api/lang/<code>/labels` | The `config.*.label` keys of the form (the words of an enum's values too, see below) |
| GET | `/api/lang/<code>/help` | The `config.*.help` keys of the form. A language comes in three answers because all of it does not fit one response buffer with room to spare (`kResponseMax` 12288 bytes; the shipped French set is 11.9 KB: words 2.4, labels 3.7, help 5.8; each answer must stay under 6000 bytes, `test_sim_web`). Two answers were enough until 4.1 brought the tabs, the weekday boxes and the tiles of `[home]`. The page asks for the labels and the help right after the words; when the labels fail a field is named by its key in the file (`timezone`), when the help fails it has no hint; the page never shows a catalog key as text |

### Security model

1. **Password required.** The web password is read from `/config/secrets.toml` on every request (no caching): a parent who changes the password locks out any session that was using the old one immediately. A password shorter than four characters (`kMinWebPassword`) is not accepted as a credential — the API stays locked until a long-enough password is stored. Without any stored password the API is permanently locked.

2. **Lockout after repeated failures.** Five consecutive wrong passwords (`kMaxFailures`) lock the API for sixty seconds (`kLockSeconds`). The counter resets on a correct password.

3. **CSRF protection.** Mutating requests (PUT, POST, DELETE) must carry the header `X-Phoneplus: 1`. A cross-origin page cannot set a custom header, so the browser's same-origin policy blocks any site that tries to use a logged-in parent's session without their knowledge.

4. **`/config/secrets.toml` cannot be deleted.** The `DELETE /api/file` route refuses any path that points to the secrets file. Deleting it would remove the password and unlock the API.

5. **Config files are validated before writing.** Every PUT to a config route runs the file through `ConfigLoader` first. A file that the device would reject at startup for a syntax error or a value of the wrong type (a string where the schema says a list) is never stored; an unknown key is stored with a warning. The page writes a list field (`sms.extra_trigger_words`) as an array, words separated by commas in the field, an empty field as `[]`, and an empty list section (`quiet_hours`, `tiles`) as an empty top-level array, which the loader reads as none, without a diagnostic.

6. **The log can hold the text of dropped messages, and the page shows the log.** The log tab reads the log file of the card through the file route, with the web password. What the device turns away is logged with the start of its text (the first 120 characters, the first drop of each sender in ten minutes: [log-level.md](log-level.md)), so that the parents can read a balance, a tariff or a PIN notice there; whoever has the password can read it. The log never holds the content of what the page itself writes (next point).
7. **Every change is logged.** All writes go through the application log (`AppCore::log()`): path and byte count only, never the content. Deleting the log itself is allowed but the deletion is logged before the file is removed.

The trade-off: the API returns `secrets.toml` on `GET /api/secrets` and the raw editor shows it, because the parents asked for real access to every file; the secrets are stored as plain text, there is no hash. Whoever has the web password therefore sees every other secret too, and because the server speaks plain HTTP (there is no HTTPS on this device) the password and the secrets cross the Wi-Fi in clear text. Use the interface on a network you trust and keep the session short; it ends by itself when Wi-Fi switches off. Without the password nothing comes out, and `GET /api/state` only says *whether* a secret is set. The decision was taken on 2026-10-01 and is intentional.

## The page

`web/index.html` is generated from `config/form_generated.json` and `lang/web/en.toml` (the page's own catalog, apart from the device's) by `scripts/gen_web.py`. Run `scripts/gen_web.py --check` to verify the file is up to date.

The page is a single-file SPA with no external resources. On load it shows a password prompt; after a correct password it stores a `Basic` credential in memory and attaches it to every API call. The language shown is: the last choice the parent made in the page's language menu (`pp_lang` in `localStorage`), or the device's own `[interface] language` (`GET /api/state` carries it as `language`), or the browser's preferred language, or English; a language the device does not have (404) is English.

**The language comes after the login.** Every `/api/` route needs the password, the catalogs (`/api/languages`, `/api/lang/<code>`) included, and the server has no exception for them. So the page makes no request before the login; the few words of the login screen and its messages (wrong password, locked, error) are a small English fallback built into the page, and the catalog is fetched once the password has been accepted, after the login form and after the stored-credential check alike. An answer that is not ok is never taken as a catalog. (Fetching the catalog at page load once left every tab showing its raw key on a device with a password; the simulator tests missed it because they ran without one.) `test/test_web_page/test.mjs` runs the page's script against a fake device that enforces the password.

Every string the page shows is looked up through a `t(key)` function that reads from a `web.*` / `config.*` translation object fetched from `/api/lang/<code>`. The device reads these catalogs only when the page asks for them and lets them go when Wi-Fi goes off ([i18n.md](i18n.md)). Device-supplied content — file names, log lines, configuration values — is always set via `.textContent`, never `.innerHTML`, to prevent XSS.

**Enum fields show words, not values.** A select for an enum key (`contact.role`, `remote_queries.notice`, `log.level`, `tiles.action`) shows one catalog word per value, e.g. German "Elternteil" / "Familie" for `parent` / `family`; the option's value, and so what is saved, stays the raw one. The key of a word is the key's label key without `.label`, then the value, then `.label` (`config.contact.role.parent.label`): it ends in `.label`, so the device delivers it with the labels in the first `/api/lang/<code>` answer, and `config/form_generated.json` carries it as `option_labels`. A new enum value needs its word in all three web catalogs (`check_i18n` knows the keys from `config/schema.toml`); a word that is missing shows the raw value, never a key.

**The Files tab** shows one directory at a time, the one the answer of `/api/files?path=` names. A directory is a button that opens it; a `..` row leads one level up (there is none in the root). A file shows its name and size with Download, Edit and Delete, each built from the directory and the name (`/config` + `contacts.toml` → `/config/contacts.toml`); a directory has none of them. An upload goes into the directory on show. When the device refuses (the secrets file cannot be deleted: 403; a directory it does not have: 404) the tab shows the device's own error text above the table. `test/test_web_page/test.mjs` feeds the page the real shape; `test/test_sim_web` pins that the real server sends it.

Form fields come from the embedded `SCHEMA` (a copy of `form_generated.json`). Field labels and help texts are catalog keys; the page fetches the translations and applies them when the language is set or changed. List sections (contacts, home networks, school times, call buttons) support adding and removing entries inline. A key the schema marks `widget = "checkbox"` (a bool) is a checkbox: each school-time window has "active" (`enabled`), ticked when the key is missing from the file; a section with a `default_entry` (the school times) shows that entry when the card has no entry, so a card without windows shows Monday to Friday, 08:00 to 12:30, unticked, and saving writes it to the card (`enabled = false`). If the parent removes it and saves, the card holds an empty list and the page shows the default window again at the next load.

**A school-time window is compact, and its days are ticked.** A section with a set of weekdays (`weekday_set`, the school times) shows each entry in two rows: seven boxes Mo to Su (`web.day.*`), then from, to (the browser's time fields), "active" and remove; the section's help stands once under its heading instead of a hint under every field. The page reads what the file says (`mon-fri`, `fri-mon` across the weekend, `mon,wed`, `daily`, in any case) into the boxes and writes back `daily` for all seven, `mon-fri` for Monday to Friday and a list otherwise. A text it does not understand (`monday`) stays a text field and is saved unchanged, so the device reports it rather than the page losing it; a time the time field would not show (`8:00`) stays a text field too. The seven boxes are not inside one `<label>`, where a click on any day's word would tick Monday. `test/test_web_page/test.mjs` (n).

**`settings.toml` on three tabs.** The tab bar is **Status**, **Basics**, **School time**, **Advanced**, **Contacts**, **Secrets**, **Files**, **Log**, **Backup**. Basics holds the child's name, the languages of the interface and of the texts, the features, the remote queries and the call buttons; School time the windows; Advanced the time zone, the log level, the Wi-Fi idle time, the battery capacity, the modem's APN, the map link and the radio, under a line that most parents leave them as they are. Which key is on which tab is data, not a list in the page: every key of `settings.toml` has a `group` in `config/schema.toml`, the tabs are its `[[group]]` entries in the order of the tab bar, and `scripts/gen_config.py` refuses a key without a group, a group nobody uses, a key of another file with one and a section split over two tabs (a section is one table of the file and one box of the form). `scripts/gen_web.py` makes a tab button and a panel per group.

The device stores whole files, so a tab never sends only its part: on **Save** the page reads `settings.toml` again, puts the tab's sections into it and sends the whole file. What another tab saved in the meantime stays, and when the file cannot be read at that moment nothing is sent and the tab shows the error. A field is read back from its own section's box: two sections have a key `language` (`[interface]`, `[sms]`) and two a key `name` (`[child]`, `[radio]`), and the single form of 4.0 saved the first of each name for both, so the texts' language became the interface language at every save. A form with a change that is not saved says "Unsaved changes" (also after adding or removing an entry) and is left as it is when the parent opens another tab and comes back; a saved form is read from the card again when its tab is opened. Choosing another language builds the open tab again. `test/test_web_page/test.mjs` (m) pins the tabs, the merge, the two `language` keys and the unsaved changes; `test/test_schema_gen/test_gen.py` the groups.

The page can be tried without a device in two ways: the simulator serves the same page and API on `http://127.0.0.1:8080/` after `wifi ap` ([simulator.md](simulator.md)), and `scripts/webbridge.py` shows the real device's page over the USB cable ([device-console.md](device-console.md)). `test/test_web_page/test.mjs` checks key coverage, data attribute validity, and that no device content goes through `innerHTML`.

## Service

`src/web/WebService.{h,cpp}` has no Wi-Fi controller of its own: it uses the application core's, the only one ([wifi-controller.md](wifi-controller.md)). It starts the HTTP server when that controller says Wi-Fi is up and stops it when it goes off, whoever switched it off, and wires the API to the rest of the application.

**Idle time.** Wi-Fi switches itself off after `auto_off_minutes` (default 10) without use. **Every authenticated request restarts that time**, so a parent who is still editing is never cut off; the status tab has a button "Keep Wi-Fi on" and shows how many minutes are left. A request that is not the parent's — no password, a wrong one, a client that is locked out, the login page — does not restart it. The tests pin each of these (`test/test_web_api`, `test/test_runtime`) and the scenario `sim/scripts/wifi-admin-page.txt` runs them through the simulator. `handleRaw()` lets a USB bridge inject a raw HTTP request and get a raw response back — the same code path as the network, for testing without Wi-Fi.

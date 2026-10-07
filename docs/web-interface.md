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

`src/web/WebServer.{h,cpp}` — one TCP connection at a time; port 80; idle timeout 10 seconds (`kIdleSeconds`). The server owns nothing about content: it takes a `RequestHandler` and calls `handle()` on a complete request or `error()` on a parse error. `WebAssets::indexHtml()` supplies the embedded page; `GET /` returns it.

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
| GET | `/api/files` | Directory listing (query: `path=`) |
| GET | `/api/file` | Download a file (query: `path=`) |
| PUT | `/api/file` | Upload or overwrite a file (query: `path=`) |
| DELETE | `/api/file` | Delete a file (query: `path=`) |
| GET | `/api/backup` | All config files as a JSON bundle |
| PUT | `/api/restore` | Restore a backup bundle |
| PUT | `/api/restart` | Ask the device to restart |
| PUT, POST | `/api/wifi-off` | Ask Wi-Fi to switch off (done after the answer has gone out) |
| PUT, POST | `/api/wifi-extend` | Keep Wi-Fi on: restarts the idle time on purpose; answers with `seconds_left`; logged like every change |
| GET | `/api/languages` | Installed language catalog list |
| GET | `/api/lang/<code>` | `web.*` and `config.*` translation keys for the page, from the web set (`lang/web/<code>.toml` layered with `/lang/web/<code>.toml` of the card; English where a language has no web catalog) |

### Security model

1. **Password required.** The web password is read from `/config/secrets.toml` on every request (no caching): a parent who changes the password locks out any session that was using the old one immediately. A password shorter than four characters (`kMinWebPassword`) is not accepted as a credential — the API stays locked until a long-enough password is stored. Without any stored password the API is permanently locked.

2. **Lockout after repeated failures.** Five consecutive wrong passwords (`kMaxFailures`) lock the API for sixty seconds (`kLockSeconds`). The counter resets on a correct password.

3. **CSRF protection.** Mutating requests (PUT, POST, DELETE) must carry the header `X-Phoneplus: 1`. A cross-origin page cannot set a custom header, so the browser's same-origin policy blocks any site that tries to use a logged-in parent's session without their knowledge.

4. **`/config/secrets.toml` cannot be deleted.** The `DELETE /api/file` route refuses any path that points to the secrets file. Deleting it would remove the password and unlock the API.

5. **Config files are validated before writing.** Every PUT to a config route runs the file through `ConfigLoader` first. A file that the device would reject at startup is never stored.

6. **The log can hold the text of dropped messages, and the page shows the log.** The log tab reads the log file of the card through the file route, with the web password. What the device turns away is logged with the start of its text (the first 120 characters, the first drop of each sender in ten minutes: [log-level.md](log-level.md)), so that the parents can read a balance, a tariff or a PIN notice there; whoever has the password can read it. The log never holds the content of what the page itself writes (next point).
7. **Every change is logged.** All writes go through the application log (`AppCore::log()`): path and byte count only, never the content. Deleting the log itself is allowed but the deletion is logged before the file is removed.

The trade-off: the API returns `secrets.toml` on `GET /api/secrets` and the raw editor shows it, because the parents asked for real access to every file; the secrets are stored as plain text, there is no hash. Whoever has the web password therefore sees every other secret too, and because the server speaks plain HTTP (there is no HTTPS on this device) the password and the secrets cross the Wi-Fi in clear text. Use the interface on a network you trust and keep the session short; it ends by itself when Wi-Fi switches off. Without the password nothing comes out, and `GET /api/state` only says *whether* a secret is set. The decision was taken on 2026-10-01 and is intentional.

## The page

`web/index.html` is generated from `config/form_generated.json` and `lang/web/en.toml` (the page's own catalog, apart from the device's) by `scripts/gen_web.py`. Run `scripts/gen_web.py --check` to verify the file is up to date.

The page is a single-file SPA with no external resources. On load it shows a password prompt; after a correct password it stores a `Basic` credential in memory and attaches it to every API call. The language shown is: the last choice in `localStorage`, or the browser's preferred language, or English; a language the device does not have (404) is English.

**The language comes after the login.** Every `/api/` route needs the password, the catalogs (`/api/languages`, `/api/lang/<code>`) included, and the server has no exception for them. So the page makes no request before the login; the few words of the login screen and its messages (wrong password, locked, error) are a small English fallback built into the page, and the catalog is fetched once the password has been accepted, after the login form and after the stored-credential check alike. An answer that is not ok is never taken as a catalog. (Fetching the catalog at page load once left every tab showing its raw key on a device with a password; the simulator tests missed it because they ran without one.) `test/test_web_page/test.mjs` runs the page's script against a fake device that enforces the password.

Every string the page shows is looked up through a `t(key)` function that reads from a `web.*` / `config.*` translation object fetched from `/api/lang/<code>`. The device reads these catalogs only when the page asks for them and lets them go when Wi-Fi goes off ([i18n.md](i18n.md)). Device-supplied content — file names, log lines, configuration values — is always set via `.textContent`, never `.innerHTML`, to prevent XSS.

Form fields come from the embedded `FORM_SCHEMA` (a copy of `form_generated.json`). Field labels and help texts are catalog keys; the page fetches the translations and applies them when the language is set or changed. List sections (contacts, home networks, school times, call buttons) support adding and removing entries inline.

The page can be tried without a device in two ways: the simulator serves the same page and API on `http://127.0.0.1:8080/` after `wifi ap` ([simulator.md](simulator.md)), and `scripts/webbridge.py` shows the real device's page over the USB cable ([device-console.md](device-console.md)). `test/test_web_page/test.mjs` checks key coverage, data attribute validity, and that no device content goes through `innerHTML`.

## Service

`src/web/WebService.{h,cpp}` has no Wi-Fi controller of its own: it uses the application core's, the only one ([wifi-controller.md](wifi-controller.md)). It starts the HTTP server when that controller says Wi-Fi is up and stops it when it goes off, whoever switched it off, and wires the API to the rest of the application.

**Idle time.** Wi-Fi switches itself off after `auto_off_minutes` (default 10) without use. **Every authenticated request restarts that time**, so a parent who is still editing is never cut off; the status tab has a button "Keep Wi-Fi on" and shows how many minutes are left. A request that is not the parent's — no password, a wrong one, a client that is locked out, the login page — does not restart it. The tests pin each of these (`test/test_web_api`, `test/test_runtime`) and the scenario `sim/scripts/wifi-admin-page.txt` runs them through the simulator. `handleRaw()` lets a USB bridge inject a raw HTTP request and get a raw response back — the same code path as the network, for testing without Wi-Fi.

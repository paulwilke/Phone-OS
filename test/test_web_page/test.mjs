/**
 * Tests for the generated web page (web/index.html).
 *
 * Checks:
 *  1. web/index.html exists and has content.
 *  2. Every data-t attribute references a known web.* key (full key name).
 *  3. The page uses textContent (not innerHTML) for device-supplied content.
 *  4. The WEB_KEYS list in gen_web.py matches the web.* keys in lang/web/en.toml.
 *  5. Every web.* key in en.toml also exists in de.toml and fr.toml.
 *  6. Translations are non-empty for all languages.
 */

import { readFileSync, existsSync } from 'fs';
import { resolve, dirname } from 'path';
import vm from 'vm';
import { fileURLToPath } from 'url';

const __dir = dirname(fileURLToPath(import.meta.url));
const ROOT = resolve(__dir, '..', '..');

let pass = 0;
let fail = 0;

function ok(msg) { console.log('  PASS:', msg); pass++; }
function err(msg) { console.error('  FAIL:', msg); fail++; }

function assert(cond, msg) { if (cond) ok(msg); else err(msg); }

// ---------- helpers ----------

function parseTOML(path) {
  // Minimal flat TOML parser: handles key = "value" and [section] lines only.
  const text = readFileSync(path, 'utf8');
  const out = {};
  for (const rawLine of text.split('\n')) {
    const line = rawLine.trim();
    if (!line || line.startsWith('#') || line.startsWith('[')) continue;
    const eq = line.indexOf('=');
    if (eq === -1) continue;
    const key = line.slice(0, eq).trim();
    const valRaw = line.slice(eq + 1).trim();
    let value = valRaw;
    if (valRaw.startsWith('"') && valRaw.endsWith('"')) {
      value = valRaw.slice(1, -1);
    } else if (valRaw.startsWith('[')) {
      // array value — just record as non-empty
      value = valRaw;
    }
    out[key] = value;
  }
  return out;
}

function webKeys(catalog) {
  return Object.keys(catalog).filter(k => k.startsWith('web.'));
}

// ---------- load files ----------

const htmlPath = resolve(ROOT, 'web', 'index.html');
const enPath   = resolve(ROOT, 'lang', 'web', 'en.toml');
const dePath   = resolve(ROOT, 'lang', 'web', 'de.toml');
const frPath   = resolve(ROOT, 'lang', 'web', 'fr.toml');
const genPath  = resolve(ROOT, 'scripts', 'gen_web.py');

// ---------- test 1: file existence ----------
console.log('\nTest: file existence');
assert(existsSync(htmlPath), 'web/index.html exists');
assert(existsSync(enPath), 'lang/web/en.toml exists');
assert(existsSync(dePath), 'lang/web/de.toml exists');
assert(existsSync(frPath), 'lang/web/fr.toml exists');
assert(existsSync(genPath), 'scripts/gen_web.py exists');

const html    = existsSync(htmlPath) ? readFileSync(htmlPath, 'utf8') : '';
const en      = existsSync(enPath)   ? parseTOML(enPath)  : {};
const de      = existsSync(dePath)   ? parseTOML(dePath)  : {};
const fr      = existsSync(frPath)   ? parseTOML(frPath)  : {};
const genText = existsSync(genPath)  ? readFileSync(genPath, 'utf8') : '';

assert(html.length > 1000, 'web/index.html is non-trivial (>1000 bytes)');

// ---------- test 2: data-t attributes reference known keys ----------
console.log('\nTest: data-t attributes reference known web.* catalog keys');
const enWebKeys = new Set(webKeys(en));
const dataTRe = /data-t="([^"]+)"/g;
let dataTMatch;
const unknownDataT = [];
const foundDataT = new Set();
while ((dataTMatch = dataTRe.exec(html)) !== null) {
  const key = dataTMatch[1];
  foundDataT.add(key);
  if (!enWebKeys.has(key)) unknownDataT.push(key);
}
assert(foundDataT.size > 0, 'page has at least one data-t attribute');
if (unknownDataT.length > 0) {
  err(`data-t attributes reference unknown keys: ${unknownDataT.slice(0, 5).join(', ')}`);
} else {
  ok(`all ${foundDataT.size} data-t values are known web.* keys`);
}

// ---------- test 3: XSS — no .innerHTML used for device content ----------
console.log('\nTest: no innerHTML used for device-supplied dynamic content');
// The JS may use innerHTML = '' to clear containers, but must not use it to
// inject data fetched from the API (file names, log lines, etc.).
const innerHTMLRe = /\.innerHTML\s*=/g;
const innerHTMLMatches = [...html.matchAll(innerHTMLRe)];
// Only allow innerHTML = '' (clearing) or innerHTML = '<...' (static HTML in JS, not device data).
// Extract context for each match to check.
let badInnerHTML = 0;
for (const m of innerHTMLMatches) {
  const context = html.slice(m.index, m.index + 60);
  // Allow only: .innerHTML = '' (clear) or .innerHTML = '<...' (static literal HTML)
  if (!/\.innerHTML\s*=\s*(?:''|""|``)/.test(context) &&
      !/\.innerHTML\s*=\s*'</.test(context) &&
      !/\.innerHTML\s*=\s*"</.test(context) &&
      !/\.innerHTML\s*=\s*`</.test(context)) {
    badInnerHTML++;
  }
}
assert(badInnerHTML === 0,
  `no non-static innerHTML assignments (found ${badInnerHTML} suspicious uses)`);

// ---------- test 4: WEB_KEYS in gen_web.py matches en.toml web.* keys ----------
console.log('\nTest: WEB_KEYS in gen_web.py matches lang/web/en.toml web.* keys');
// Extract WEB_KEYS list from gen_web.py
const webKeyRe = /"(web\.[a-z][a-z0-9.-]*)"/g;
const genWebKeys = new Set();
let m;
while ((m = webKeyRe.exec(genText)) !== null) genWebKeys.add(m[1]);

const missing_in_gen = [...enWebKeys].filter(k => !genWebKeys.has(k));
const extra_in_gen   = [...genWebKeys].filter(k => !enWebKeys.has(k));
if (missing_in_gen.length > 0) {
  err(`web.* keys in en.toml not referenced in gen_web.py: ${missing_in_gen.join(', ')}`);
} else {
  ok('all en.toml web.* keys appear in gen_web.py');
}
if (extra_in_gen.length > 0) {
  err(`web.* keys in gen_web.py not in en.toml: ${extra_in_gen.join(', ')}`);
} else {
  ok('gen_web.py references no unknown web.* keys');
}

// ---------- test 5: all web.* keys exist in all languages ----------
console.log('\nTest: all web.* keys in en.toml exist in de.toml and fr.toml');
const enWebKeyList = [...enWebKeys];
const missingDe = enWebKeyList.filter(k => !(k in de));
const missingFr = enWebKeyList.filter(k => !(k in fr));
if (missingDe.length > 0) {
  err(`de.toml missing web.* keys: ${missingDe.join(', ')}`);
} else {
  ok(`de.toml has all ${enWebKeyList.length} web.* keys`);
}
if (missingFr.length > 0) {
  err(`fr.toml missing web.* keys: ${missingFr.join(', ')}`);
} else {
  ok(`fr.toml has all ${enWebKeyList.length} web.* keys`);
}

// ---------- test 6: translations are non-empty ----------
console.log('\nTest: translations are non-empty for all languages');
let emptyDe = 0, emptyFr = 0;
for (const k of enWebKeyList) {
  if (!de[k] || de[k].trim() === '') emptyDe++;
  if (!fr[k] || fr[k].trim() === '') emptyFr++;
}
assert(emptyDe === 0, `de.toml: all web.* translations non-empty (${emptyDe} empty)`);
assert(emptyFr === 0, `fr.toml: all web.* translations non-empty (${emptyFr} empty)`);

// ---------- test 7: hostile strings shown as text not HTML ----------
console.log('\nTest: no string concatenation into innerHTML');
// Verify that the page never does something like:
//   element.innerHTML = someApiData + ...
// The specific patterns we guard against: innerHTML = variable or innerHTML = d.*
const dangerousInner = /\.innerHTML\s*=\s*[a-zA-Z_$]/.test(html);
assert(!dangerousInner, 'no innerHTML = <variable> patterns (device data via textContent only)');

// ---------- test 8: the page talks to routes that exist, and Wi-Fi is the one controller's ----------
console.log('\nTest: every route the page calls is a route of the API; the Wi-Fi status and buttons');
const api = readFileSync(resolve(ROOT, 'src', 'web', 'WebApi.cpp'), 'utf8');
const routes = [...new Set([...html.matchAll(/['"`]\/api\/([a-z-]+)/g)].map((m) => m[1]))];
const unknown = routes.filter((r) => !api.includes(`"/${r}"`) && !api.includes(`"/${r}/"`));
assert(routes.length >= 8, `the page calls ${routes.length} routes`);
assert(unknown.length === 0, 'no route of the page is missing in WebApi.cpp' + (unknown.length ? ': ' + unknown.join(', ') : ''));
assert(/id="wifi-extend-btn"[^>]*\s+data-t="web\.status\.wifi-extend-btn"/.test(html), 'the button "keep Wi-Fi on" is in the page, worded from the catalog');
assert(html.includes("api('POST', '/api/wifi-extend'") && html.includes("api('POST', '/api/wifi-off'"), 'the two buttons call their routes');
assert(html.includes("LOG_PATH = '/system.log'") && html.includes("'/api/file?path=' + LOG_PATH") && !html.includes('/api/log'), 'the log tab reads and clears /system.log through the file route');
assert(html.includes('w.seconds_left') && html.includes('w.state') && !html.includes('d.wifi_mode'), 'the status reads the fields the API sends');

// ---------- test 9: the page runs against a device with a web password ----------
// Every /api/ route needs the password, the catalogs too, so the page must fetch its language only
// after the login. The old page fetched it at load, got 401, stored the error object as its catalog
// and showed raw keys ("web.tab.settings") for the whole session.
console.log('\nTest: with a web password the login screen is English and the language comes after the login');
const PASSWORD = 'test-password-1';
const script = html.match(/<script>([\s\S]*)<\/script>/)[1];

function fakeElement(id) {
  const el = {
    id, textContent: '', value: '', hidden: false, style: {}, children: [],
    classList: { add() {}, remove() {}, toggle() {}, contains: () => false },
    files: [], listeners: {}, attrs: {},
    addEventListener(type, fn) { this.listeners[type] = fn; },
    getAttribute(n) { return this.attrs[n] ?? null; },
    setAttribute(n, v) { this.attrs[n] = v; },
    appendChild(c) { this.children.push(c); return c; },
    querySelector: () => fakeElement('q'), querySelectorAll: () => [],
    focus() {}, click() {}, remove() {},
  };
  return el;
}

async function runPage({ storedAuth, storedLang, browserLang, catalogs, wrongFirst }) {
  const store = {};
  if (storedAuth) store.pp_auth = storedAuth;
  if (storedLang) store.pp_lang = storedLang;
  const els = {};
  const dataT = [];
  // data-t elements of the markup, with the id they carry (for the tabs and the overlay)
  for (const m of html.matchAll(/<(\w+)([^>]*?)data-t="([^"]+)"([^>]*)>/g)) {
    const el = fakeElement((m[2] + m[4]).match(/id="([^"]+)"/)?.[1] || '');
    el.attrs['data-t'] = m[3];
    el.attrs['data-tab'] = (m[2] + m[4]).match(/data-tab="([^"]+)"/)?.[1];
    dataT.push(el);
  }
  const document = {
    getElementById(id) { return els[id] ??= fakeElement(id); },
    querySelectorAll(sel) { return sel === '[data-t]' ? dataT : []; },
    createElement: (tag) => fakeElement(tag),
    title: '', body: fakeElement('body'),
  };
  const calls = [];
  const fetchFn = async (path, opts = {}) => {
    const authed = opts.headers && opts.headers.Authorization === 'Basic ' + Buffer.from(':' + PASSWORD).toString('base64');
    calls.push({ path, authed });
    const reply = (status, body) => ({ ok: status < 400, status, json: async () => body });
    if (!authed) return reply(401, { error: 'Unauthorized' });
    if (path === '/api/state') return reply(200, {});
    if (path === '/api/languages') return reply(200, Object.keys(catalogs).map((c) => ({ code: c, name: c, native: c })));
    const l = path.match(/^\/api\/lang\/(.+)$/);
    if (l) return catalogs[l[1]] ? reply(200, catalogs[l[1]]) : reply(404, { error: 'Unknown language' });
    return reply(200, {});
  };
  const context = {
    document, fetch: fetchFn, btoa: (x) => Buffer.from(x).toString('base64'), console,
    navigator: { language: browserLang || 'en-US' },
    localStorage: { getItem: (k) => store[k] ?? null, setItem: (k, v) => { store[k] = String(v); }, removeItem: (k) => { delete store[k]; } },
    setTimeout, clearTimeout, setInterval: () => 0, clearInterval() {}, FileReader: function () {}, Blob: function () {},
    URL: { createObjectURL: () => '' }, confirm: () => true, alert() {},
  };
  context.window = context;
  vm.createContext(context);
  vm.runInContext(script, context);
  const settle = () => new Promise((r) => setTimeout(r, 30));
  await settle();
  const shown = () => dataT.map((e) => e.textContent);
  return { els, dataT, calls, store, settle, shown, document, context };
}

const catalogs = { en, de };
const rawKey = (arr) => arr.filter((x) => /^web\./.test(x));
// what the parent sees on the login screen: the overlay's words (the panels behind it are hidden)
const overlayWords = (pg) => pg.dataT.filter((e) => e.attrs['data-t'].startsWith('web.auth.')).map((e) => e.textContent);

// (a) not logged in: English words on the login screen, no key, and no request that would fail
{
  const pg = await runPage({ browserLang: 'de-DE', catalogs });
  const overlay = pg.dataT.filter((e) => e.attrs['data-t'].startsWith('web.auth.')).map((e) => e.textContent);
  assert(overlay.join('|') === 'Password|Web password|Enter', 'login overlay shows the English fallback words: ' + overlay.join('|'));
  assert(rawKey(overlayWords(pg)).length === 0, 'no raw web.* key is shown on the login screen');
  assert(pg.calls.length === 0, 'the page makes no request before the login');

  // (b) wrong password: English fallback message
  pg.els['auth-pw'] = Object.assign(fakeElement('auth-pw'), { value: 'wrong-password' });
  await pg.els['auth-form'].listeners.submit({ preventDefault() {} });
  await pg.settle();
  assert(pg.els['auth-error'].textContent === 'Wrong password.' && pg.els['auth-error'].hidden === false,
    'a wrong password shows the English fallback message: ' + pg.els['auth-error'].textContent);

  // (c) right password: the browser language (German) is loaded after the login
  pg.els['auth-pw'].value = PASSWORD;
  await pg.els['auth-form'].listeners.submit({ preventDefault() {} });
  await pg.settle();
  const tabs = pg.dataT.filter((e) => e.attrs['data-tab']).map((e) => e.textContent);
  assert(tabs.includes('Einstellungen'), 'after the login the tabs are German: ' + tabs.join('|'));
  assert(rawKey(pg.shown()).length === 0, 'no raw web.* key is shown after the login');
  assert(pg.calls.filter((c) => c.path !== '/api/state' && !c.authed).length === 0, 'the catalog and every later request carry the password');
}

// (d) stored credential and stored language: no login, German at once
{
  const auth = 'Basic ' + Buffer.from(':' + PASSWORD).toString('base64');
  const pg = await runPage({ storedAuth: auth, storedLang: 'de', browserLang: 'en-US', catalogs });
  const tabs = pg.dataT.filter((e) => e.attrs['data-tab']).map((e) => e.textContent);
  assert(tabs.includes('Einstellungen') && rawKey(pg.shown()).length === 0, 'stored credential: the stored language is shown: ' + tabs.join('|'));
}

// (e) a language the device does not have falls back to English
{
  const auth = 'Basic ' + Buffer.from(':' + PASSWORD).toString('base64');
  const pg = await runPage({ storedAuth: auth, browserLang: 'xx-XX', catalogs });
  const tabs = pg.dataT.filter((e) => e.attrs['data-tab']).map((e) => e.textContent);
  assert(tabs.includes('Settings') && rawKey(pg.shown()).length === 0, 'unknown language: English words: ' + tabs.join('|'));
}

// (f) a stored credential that is no longer right: back to the login screen, English words
{
  const pg = await runPage({ storedAuth: 'Basic ' + Buffer.from(':changed').toString('base64'), storedLang: 'de', catalogs });
  assert(pg.store.pp_auth === undefined && rawKey(overlayWords(pg)).length === 0, 'a stale credential is dropped and the login screen stays English');
}

// ---------- summary ----------
console.log(`\n${pass + fail} checks: ${pass} passed, ${fail} failed.`);
if (fail > 0) process.exit(1);

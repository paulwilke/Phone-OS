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

// what every input of a fake form holds when the page reads it back
let fakeQueryValue = '';

function fakeElement(id) {
  const el = {
    id, textContent: '', value: '', hidden: false, style: {}, children: [],
    classList: { add() {}, remove() {}, toggle() {}, contains: () => false },
    files: [], listeners: {}, attrs: {},
    addEventListener(type, fn) { this.listeners[type] = fn; },
    getAttribute(n) { return this.attrs[n] ?? null; },
    setAttribute(n, v) { this.attrs[n] = v; },
    appendChild(c) { this.children.push(c); return c; },
    querySelector: () => Object.assign(fakeElement('q'), { value: fakeQueryValue }), querySelectorAll: () => [],
    focus() {}, click() {}, remove() {},
  };
  return el;
}

// An element that keeps its children and answers the three selectors the form code uses, for the tests
// that read a built form back (the plain fakeElement answers every query with one empty input).
function richElement(tag) {
  const matches = (el, sel) => {
    let m;
    if ((m = sel.match(/^\.([\w-]+)$/))) return String(el.className || '').split(/\s+/).includes(m[1]);
    if ((m = sel.match(/^\[name="([^"]+)"\]$/))) return el.name === m[1];
    if ((m = sel.match(/^\[data-list-section="([^"]+)"\]$/))) return el.attrs['data-list-section'] === m[1];
    if ((m = sel.match(/^\[data-section="([^"]*)"\]$/))) return el.attrs['data-section'] === m[1];
    throw new Error('selector the test double does not know: ' + sel);
  };
  const el = {
    tag, id: tag, children: [], attrs: {}, className: '', textContent: '', value: '', checked: false, hidden: false, style: {}, listeners: {},
    classList: { add() {}, remove() {}, toggle() {}, contains: () => false },
    // innerHTML = '' is how the page empties a container
    set innerHTML(_v) { this.children = []; },
    appendChild(c) { this.children.push(c); return c; },
    setAttribute(n, v) { this.attrs[n] = v; }, getAttribute(n) { return this.attrs[n] ?? null; },
    addEventListener(t, f) { this.listeners[t] = f; }, remove() {}, focus() {}, click() {},
    querySelectorAll(sel) {
      const out = [];
      const walkDown = (n) => { for (const c of n.children) { if (matches(c, sel)) out.push(c); walkDown(c); } };
      walkDown(this);
      return out;
    },
    querySelector(sel) { return this.querySelectorAll(sel)[0] ?? null; },
  };
  if (tag === 'select') {
    // a select holds the value of its selected option (the first one when none is), as in a browser
    let own;
    Object.defineProperty(el, 'value', {
      get() { return own ?? (this.children.find((o) => o.selected) ?? this.children[0])?.value ?? ''; },
      set(v) { own = v; },
    });
  }
  return el;
}

async function runPage({ storedAuth, storedLang, browserLang, catalogs, deviceLang, helps = {}, helpFails = false, labelsFail = false, rich = false, settings = {}, onFetch = null, fileReader = null }) {
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
    getElementById(id) { return els[id] ??= (rich && (id.startsWith('form-') || id === 'files-tbody') ? richElement(id) : fakeElement(id)); },
    querySelectorAll(sel) { return sel === '[data-t]' ? dataT : sel === '#tabs button' ? dataT.filter((e) => e.attrs['data-tab']) : []; },
    createElement: (tag) => (rich ? richElement(tag) : fakeElement(tag)), querySelector: () => null,
    title: '', body: fakeElement('body'),
  };
  const calls = [];
  const fetchFn = async (path, opts = {}) => {
    const authed = opts.headers && opts.headers.Authorization === 'Basic ' + Buffer.from(':' + PASSWORD).toString('base64');
    calls.push({ path, authed, method: opts.method, body: opts.body });
    const reply = (status, body, text) => ({ ok: status < 400, status, json: async () => structuredClone(body), text: async () => text ?? JSON.stringify(body) });
    if (!authed) return reply(401, { error: 'Unauthorized' });
    if (onFetch) { const r = onFetch(path, opts, reply); if (r) return r; }
    if (path === '/api/state') return reply(200, deviceLang ? { language: deviceLang } : {});
    if (path === '/api/languages') return reply(200, Object.keys(catalogs).map((c) => ({ code: c, name: c, native: c })));
    // The device's three answers of a language (WebApi::langCatalog): the words of the page, the labels of the
    // form, the help texts. A test's catalog is split the same way; its help texts are `helps` when it has them.
    const pick = (cat, keep) => Object.fromEntries(Object.entries(cat).filter(([k]) => keep(k)));
    const isLabel = (k) => k.startsWith('config.') && k.endsWith('.label');
    const lb = path.match(/^\/api\/lang\/(.+)\/labels$/);
    const h = path.match(/^\/api\/lang\/(.+)\/help$/);
    const l = !lb && !h && path.match(/^\/api\/lang\/(.+)$/);
    if (lb && labelsFail) return reply(500, { error: 'Response too large' });
    if (lb) return catalogs[lb[1]] ? reply(200, pick(catalogs[lb[1]], isLabel)) : reply(404, { error: 'Unknown language' });
    if (h && helpFails) return reply(500, { error: 'Response too large' });
    if (h) return catalogs[h[1]] ? reply(200, helps[h[1]] || {}) : reply(404, { error: 'Unknown language' });
    if (l) return catalogs[l[1]] ? reply(200, pick(catalogs[l[1]], (k) => !k.startsWith('config.'))) : reply(404, { error: 'Unknown language' });
    if (path === '/api/settings' && opts.method !== 'PUT') return reply(200, settings);
    if (path === '/api/contacts') return reply(200, { contact: [{ number: '+491701111111', name: 'Parent One', role: 'parent' }] });
    return reply(200, {});
  };
  const context = {
    document, fetch: fetchFn, btoa: (x) => Buffer.from(x).toString('base64'), console,
    navigator: { language: browserLang || 'en-US' },
    localStorage: { getItem: (k) => store[k] ?? null, setItem: (k, v) => { store[k] = String(v); }, removeItem: (k) => { delete store[k]; } },
    setTimeout, clearTimeout, setInterval: () => 0, clearInterval() {}, FileReader: fileReader || function () {}, Blob: function () {},
    URL: { createObjectURL: () => '' }, confirm: () => true, alert() {},
  };
  context.window = context;
  vm.createContext(context);
  vm.runInContext(script, context);
  const settle = () => new Promise((r) => setTimeout(r, 30));
  await settle();
  const shown = () => dataT.map((e) => e.textContent);
  return { els, dataT, calls, store, settle, shown, document, context, T: () => vm.runInContext('T', context) };
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
  assert(tabs.includes('Allgemein'), 'after the login the tabs are German: ' + tabs.join('|'));
  assert(rawKey(pg.shown()).length === 0, 'no raw web.* key is shown after the login');
  assert(pg.calls.filter((c) => c.path !== '/api/state' && !c.authed).length === 0, 'the catalog and every later request carry the password');
}

// (d) stored credential and stored language: no login, German at once
{
  const auth = 'Basic ' + Buffer.from(':' + PASSWORD).toString('base64');
  const pg = await runPage({ storedAuth: auth, storedLang: 'de', browserLang: 'en-US', catalogs });
  const tabs = pg.dataT.filter((e) => e.attrs['data-tab']).map((e) => e.textContent);
  assert(tabs.includes('Allgemein') && rawKey(pg.shown()).length === 0, 'stored credential: the stored language is shown: ' + tabs.join('|'));
}

// (e) a language the device does not have falls back to English
{
  const auth = 'Basic ' + Buffer.from(':' + PASSWORD).toString('base64');
  const pg = await runPage({ storedAuth: auth, browserLang: 'xx-XX', catalogs });
  const tabs = pg.dataT.filter((e) => e.attrs['data-tab']).map((e) => e.textContent);
  assert(tabs.includes('Basics') && rawKey(pg.shown()).length === 0, 'unknown language: English words: ' + tabs.join('|'));
}

// (f) a stored credential that is no longer right: back to the login screen, English words
{
  const pg = await runPage({ storedAuth: 'Basic ' + Buffer.from(':changed').toString('base64'), storedLang: 'de', catalogs });
  assert(pg.store.pp_auth === undefined && rawKey(overlayWords(pg)).length === 0, 'a stale credential is dropped and the login screen stays English');
}

// (g) the device's own language wins over the browser's when the page has no choice of its own
{
  const auth = 'Basic ' + Buffer.from(':' + PASSWORD).toString('base64');
  const tabsOf = (pg) => pg.dataT.filter((e) => e.attrs['data-tab']).map((e) => e.textContent);
  let pg = await runPage({ storedAuth: auth, browserLang: 'en-US', deviceLang: 'de', catalogs });
  assert(tabsOf(pg).includes('Allgemein'), 'device language de, browser en, no choice: German: ' + tabsOf(pg).join('|'));

  // after the login form too
  pg = await runPage({ browserLang: 'en-US', deviceLang: 'de', catalogs });
  pg.els['auth-pw'] = Object.assign(fakeElement('auth-pw'), { value: PASSWORD });
  await pg.els['auth-form'].listeners.submit({ preventDefault() {} });
  await pg.settle();
  assert(tabsOf(pg).includes('Allgemein'), 'the login form takes the device language too');

  // the parent's choice on the page is remembered and wins
  pg = await runPage({ storedAuth: auth, storedLang: 'en', browserLang: 'de-DE', deviceLang: 'de', catalogs });
  assert(tabsOf(pg).includes('Basics'), 'a stored choice (en) beats the device language (de): ' + tabsOf(pg).join('|'));

  // choosing a language in the menu stores it
  pg = await runPage({ storedAuth: auth, browserLang: 'en-US', deviceLang: 'en', catalogs });
  pg.els['lang-select'].value = 'de';
  await pg.els['lang-select'].listeners.change.call(pg.els['lang-select']);
  await pg.settle();
  assert(pg.store.pp_lang === 'de' && tabsOf(pg).includes('Allgemein'), 'the language menu stores the choice');

  // no device language (old firmware): the browser's
  pg = await runPage({ storedAuth: auth, browserLang: 'de-DE', catalogs });
  assert(tabsOf(pg).includes('Allgemein'), 'no device language: the browser language');

  // a device language the page has no catalog for: English
  pg = await runPage({ storedAuth: auth, browserLang: 'de-DE', deviceLang: 'xx', catalogs });
  assert(tabsOf(pg).includes('Basics'), 'device language without a catalog: English');
}

// (h) the help texts under the form fields are a second request; a raw key is never shown
{
  const auth = 'Basic ' + Buffer.from(':' + PASSWORD).toString('base64');
  // the device sends the words and labels in one answer and the help texts in another
  const split = (cat) => ({
    main: Object.fromEntries(Object.entries(cat).filter(([k]) => !k.endsWith('.help') || !k.startsWith('config.'))),
    help: Object.fromEntries(Object.entries(cat).filter(([k]) => k.startsWith('config.') && k.endsWith('.help'))),
  });
  const sEn = split(en), sDe = split(de);
  const catalogsH = { en: sEn.main, de: sDe.main };
  const helps = { en: sEn.help, de: sDe.help };
  assert(Object.keys(helps.de).length >= 3, 'the German catalog has help texts to test with');
  // open the contacts tab the way the parent does and collect what the form shows under its fields
  const walk = (el, out) => { for (const c of el.children || []) { out.push(c); walk(c, out); } return out; };
  const hintsOf = async (pg) => {
    pg.dataT.find((e) => e.attrs['data-tab'] === 'contacts').listeners.click();
    await pg.settle();
    return walk(pg.els['form-contacts'], []).filter((c) => c.className === 'hint').map((c) => c.textContent);
  };
  let pg = await runPage({ storedAuth: auth, storedLang: 'de', catalogs: catalogsH, helps });
  let hints = await hintsOf(pg);
  assert(hints.length === 3 && hints.includes(de['config.contact.number.help'].replace(/^"|"$/g, '')),
    'every field of a contact has its German help text under it: ' + hints.join(' | '));
  assert(!hints.some((x) => /^config\./.test(x)), 'no raw help key under a field');
  assert(pg.calls.some((c) => c.path === '/api/lang/de/help' && c.authed), 'the help texts are asked for after the login, with the password');

  // the secrets tab: its first group has no section and so no heading; it used to be headed by the key "config..label"
  pg.dataT.find((e) => e.attrs['data-tab'] === 'secrets').listeners.click();
  await pg.settle();
  const secretsText = walk(pg.els['form-secrets'], []).map((c) => c.textContent).filter((x) => typeof x === 'string' && x);
  assert(secretsText.length > 0 && !secretsText.some((x) => /^config\./.test(x)), 'the secrets form shows no raw key: ' + secretsText.join(' | '));

  // the help request fails (older firmware, answer too large): the words still load and the hints are left out, not shown as keys
  pg = await runPage({ storedAuth: auth, storedLang: 'de', catalogs: catalogsH, helps, helpFails: true });
  assert(pg.dataT.filter((e) => e.attrs['data-tab']).map((e) => e.textContent).includes('Allgemein'), 'a failing help request leaves the language loaded');
  hints = await hintsOf(pg);
  assert(hints.length === 0, 'a help text that did not arrive is left out, not shown as its key: ' + hints.join(' | '));

  // a language chosen in the menu brings its help texts too
  pg = await runPage({ storedAuth: auth, storedLang: 'en', catalogs: catalogsH, helps });
  pg.els['lang-select'].value = 'de';
  await pg.els['lang-select'].listeners.change.call(pg.els['lang-select']);
  await pg.settle();
  hints = await hintsOf(pg);
  assert(hints.length === 3 && hints.includes(de['config.contact.number.help'].replace(/^"|"$/g, '')), 'after a change in the menu the hints are in the new language');
}

// (i) a list field is written as an array: an empty one is [], never "" (the device reported
// "wrong-type settings sms.extra_trigger_words string-list" at every start after the page wrote a string)
{
  const auth = 'Basic ' + Buffer.from(':' + PASSWORD).toString('base64');
  const schema = JSON.parse(readFileSync(resolve(ROOT, 'config', 'form_generated.json'), 'utf8'));
  const listFields = [];
  for (const file of Object.values(schema.files))
    for (const sec of file.sections)
      for (const k of sec.keys) if (k.type === 'string_list') listFields.push({ path: (sec.id ? sec.id + '.' : '') + k.name, sec, k });
  assert(listFields.length >= 1, 'the schema has list fields to test: ' + listFields.map((f) => f.path).join(', '));
  const walk2 = (el, out) => { for (const c of el.children || []) { out.push(c); walk2(c, out); } return out; };
  const nested = (obj, path) => path.split('.').reduce((o, p) => (o == null ? o : o[p]), obj);
  // every list field of settings.toml is on the tab of its section's group
  const tabOf = listFields[0].sec.group;
  assert(listFields.every((f) => f.sec.group === tabOf), 'the list fields are on one tab: ' + tabOf);
  const pg = await runPage({ storedAuth: auth, storedLang: 'en', catalogs });
  pg.dataT.find((e) => e.attrs['data-tab'] === tabOf).listeners.click();
  await pg.settle();
  const save = walk2(pg.els['form-' + tabOf], []).find((c) => c.className === 'btn-primary');
  save.listeners.click();
  await pg.settle();
  const put = pg.calls.find((c) => c.method === 'PUT' && c.path === '/api/settings');
  assert(!!put, 'the settings form saves with a PUT');
  const sent = JSON.parse(put.body);
  for (const f of listFields)
    assert(Array.isArray(nested(sent, f.path)) && nested(sent, f.path).length === 0, 'an empty list field is sent as []: ' + f.path + ' = ' + JSON.stringify(nested(sent, f.path)));

  // typed words, separated by commas, are an array of trimmed words
  fakeQueryValue = ' #WoIstDu ,, Ping,  ';
  const pg2 = await runPage({ storedAuth: auth, storedLang: 'en', catalogs });
  pg2.dataT.find((e) => e.attrs['data-tab'] === tabOf).listeners.click();
  await pg2.settle();
  walk2(pg2.els['form-' + tabOf], []).find((c) => c.className === 'btn-primary').listeners.click();
  await pg2.settle();
  const sent2 = JSON.parse(pg2.calls.find((c) => c.method === 'PUT' && c.path === '/api/settings').body);
  fakeQueryValue = '';
  assert(JSON.stringify(sent2.sms.extra_trigger_words) === '["#WoIstDu","Ping"]', 'typed words become an array of trimmed words: ' + JSON.stringify(sent2.sms.extra_trigger_words));
}

// (j) a school-time window has a checkbox "active"; a card without windows shows the schema's one window,
// unticked; saving writes `enabled` (the owner's decision of 2026-10-07)
{
  const auth = 'Basic ' + Buffer.from(':' + PASSWORD).toString('base64');
  const schema = JSON.parse(readFileSync(resolve(ROOT, 'config', 'form_generated.json'), 'utf8'));
  const section = schema.files.settings.sections.find((x) => x.id === 'quiet_hours');
  const enabledKey = section.keys.find((k) => k.name === 'enabled');
  assert(enabledKey && enabledKey.type === 'bool' && enabledKey.widget === 'checkbox' && enabledKey.default === 'true',
    'the schema gives a school-time window the key enabled: a checkbox, on unless the file says otherwise');
  assert(JSON.stringify(section.default_entry) === JSON.stringify({ days: 'mon-fri', from: '08:00', to: '12:30', enabled: false }),
    'the schema carries the default window: Monday to Friday, 08:00 to 12:30, switched off');
  for (const [name, cat, word] of [['en', en, 'Active'], ['de', de, 'aktiv'], ['fr', fr, 'actif']])
    assert(cat['config.quiet_hours.enabled.label'] === word, 'the catalog ' + name + ' words the checkbox: ' + cat['config.quiet_hours.enabled.label']);

  const openSettings = async (settings, lang = 'en') => {
    const pg = await runPage({ storedAuth: auth, storedLang: lang, catalogs: { en, de }, rich: true, settings });
    pg.dataT.find((e) => e.attrs['data-tab'] === 'school_time').listeners.click();
    await pg.settle();
    return pg;
  };
  const windowsOf = (pg) => pg.els['form-school_time'].querySelector('[data-list-section="quiet_hours"]').querySelectorAll('.list-entry');
  const daysOf = (entry) => {
    const on = entry.querySelector('[name="days"]').querySelectorAll('.day-box').filter((b) => b.checked).map((b) => b.attrs['data-day']);
    return on.join(',') === 'mon,tue,wed,thu,fri' ? 'mon-fri' : on.join(',');
  };
  const fieldsOf = (entry) => ({
    days: daysOf(entry), from: entry.querySelector('[name="from"]').value,
    to: entry.querySelector('[name="to"]').value, box: entry.querySelector('[name="enabled"]'),
  });
  const save = async (pg) => {
    pg.els['form-school_time'].querySelectorAll('.btn-primary')[0].listeners.click();
    await pg.settle();
    return JSON.parse(pg.calls.find((c) => c.method === 'PUT' && c.path === '/api/settings').body);
  };

  // no key at all, and the empty array the page writes today: the default window, unticked
  for (const settings of [{}, { quiet_hours: [] }]) {
    const pg = await openSettings(settings);
    const entries = windowsOf(pg);
    assert(entries.length === 1, 'a card without windows shows one: ' + JSON.stringify(settings));
    const f = fieldsOf(entries[0]);
    assert(f.days === 'mon-fri' && f.from === '08:00' && f.to === '12:30', 'the default window is Monday to Friday, 08:00 to 12:30');
    assert(f.box.type === 'checkbox' && f.box.checked === false, 'the default window is not ticked');
    const sent = await save(pg);
    assert(JSON.stringify(sent.quiet_hours) === '[{"days":"mon-fri","from":"08:00","to":"12:30","enabled":false}]',
      'saving writes the default window to the card, switched off: ' + JSON.stringify(sent.quiet_hours));
  }

  // ticking it writes enabled = true
  {
    const pg = await openSettings({});
    fieldsOf(windowsOf(pg)[0]).box.checked = true;
    const sent = await save(pg);
    assert(sent.quiet_hours.length === 1 && sent.quiet_hours[0].enabled === true, 'a ticked window is written enabled = true');
  }

  // a card that lists windows gets exactly those: no default added; a window without the key is on
  {
    const pg = await openSettings({ quiet_hours: [{ days: 'sat', from: '10:00', to: '11:00' }, { days: 'sun', from: '10:00', to: '11:00', enabled: false }] });
    const entries = windowsOf(pg);
    assert(entries.length === 2, 'a card with two windows shows two, no default');
    assert(fieldsOf(entries[0]).box.checked === true, 'a window without the key is ticked');
    assert(fieldsOf(entries[1]).box.checked === false, 'enabled = false is unticked');
    const sent = await save(pg);
    assert(JSON.stringify(sent.quiet_hours.map((w) => w.enabled)) === '[true,false]', 'both are written with their state: ' + JSON.stringify(sent.quiet_hours));
  }

  // the checkbox carries the catalog's word in the page's language
  {
    const pg = await openSettings({}, 'de');
    const box = fieldsOf(windowsOf(pg)[0]).box;
    const all = (el, out = []) => { for (const c of el.children || []) { out.push(c); all(c, out); } return out; };
    const label = all(windowsOf(pg)[0]).find((c) => c.children.includes(box));
    assert(label && label.textContent === 'aktiv', 'in German the checkbox is called "aktiv": ' + (label && label.textContent));
  }
}


// (k) the Files tab reads the shape the device sends: {path, entries:[{name,size,dir}]}, one directory at a time.
// The first page read a bare array of {path,size}, so every real answer looked like an empty list.
{
  const auth = 'Basic ' + Buffer.from(':' + PASSWORD).toString('base64');
  const tree = {
    '/': [{ name: 'config', size: 0, dir: true }, { name: 'system.log', size: 68412, dir: false }],
    '/config': [{ name: 'contacts.toml', size: 210, dir: false }, { name: '._contacts.toml', size: 4096, dir: false },
                { name: 'secrets.toml', size: 90, dir: false }, { name: 'sub', size: 0, dir: true }],
    '/config/sub': [],
  };
  const texts = { '/config/contacts.toml': '[[contact]]\nnumber = "+491700000000"\n' };
  const onFetch = (path, opts, reply) => {
    const url = new URL(path, 'http://device');
    if (url.pathname === '/api/files') {
      const dir = url.searchParams.get('path') ?? '/';
      return tree[dir] ? reply(200, { path: dir, entries: tree[dir] }) : reply(404, { error: 'Directory not found' });
    }
    if (url.pathname === '/api/file') {
      const f = url.searchParams.get('path');
      if (opts.method === 'DELETE' && f === '/config/secrets.toml') return reply(403, { error: 'The secrets file cannot be deleted' });
      if (opts.method === 'DELETE' || opts.method === 'PUT') return reply(200, { ok: true });
      return reply(200, {}, texts[f] ?? '');
    }
    return null;
  };
  const walkF = (el, out = []) => { for (const c of el.children || []) { out.push(c); walkF(c, out); } return out; };
  const open = async (extra = {}) => {
    const pg = await runPage({ storedAuth: auth, storedLang: 'en', catalogs: { en, de }, rich: true, onFetch, ...extra });
    pg.dataT.find((e) => e.attrs['data-tab'] === 'files').listeners.click();
    await pg.settle();
    return pg;
  };
  // what the table shows: one entry per row, the name cell and its buttons
  const rows = (pg) => pg.els['files-tbody'].children.map((tr) => {
    const [name, size, actions] = tr.children;
    const buttons = walkF(name).concat(walkF(actions)).filter((c) => c.tag === 'button');
    return { name: name.textContent || buttons.map((b) => b.textContent)[0] || '', size: size.textContent, buttons, name_buttons: walkF(name).filter((c) => c.tag === 'button') };
  });
  const btn = (row, cls) => row.buttons.find((b) => b.className.includes(cls));
  const listCalls = (pg) => pg.calls.filter((c) => c.path.startsWith('/api/files')).map((c) => decodeURIComponent(c.path));

  // the root lists its entries (the real answer of the device), a directory has no file buttons
  let pg = await open();
  let r = rows(pg);
  assert(r.length === 2 && r[0].name === 'config/' && r[1].name === 'system.log', 'the root lists its two entries: ' + r.map((x) => x.name).join(', '));
  assert(r[1].size === 68412 || r[1].size === '68412', 'a file shows its size');
  assert(pg.els['files-empty'].hidden === true, 'a listing with entries hides the empty text');
  assert(r[0].buttons.length === 1 && r[0].name_buttons.length === 1, 'a directory has one button (open) and no Download, Edit or Delete');
  assert(r[1].buttons.map((b) => b.textContent).join('|') === 'Download|Edit|Delete', 'a file has Download, Edit and Delete');
  assert(pg.els['files-path'].textContent === '/', 'the current directory is shown');

  // a file's actions use the full path: directory and name
  btn(r[1], 'btn-edit').listeners.click();
  await pg.settle();
  assert(pg.calls.some((c) => c.path === '/api/file?path=%2Fsystem.log'), 'edit in the root asks for /system.log');

  // a directory opens
  btn(r[0], 'btn-open').listeners.click();
  await pg.settle();
  r = rows(pg);
  assert(listCalls(pg).at(-1) === '/api/files?path=/config', 'opening a directory asks for ?path=/config: ' + listCalls(pg).join(' '));
  assert(r.map((x) => x.name).join('|') === '..|contacts.toml|._contacts.toml|secrets.toml|sub/', 'the directory lists its entries behind a ".." step, macOS leftovers included: ' + r.map((x) => x.name).join('|'));
  assert(pg.els['files-path'].textContent === '/config', 'the path shows /config');

  // the actions of a file in that directory use /config/<name>
  const contacts = r.find((x) => x.name === 'contacts.toml');
  btn(contacts, 'btn-edit').listeners.click();
  await pg.settle();
  assert(pg.calls.some((c) => c.path === '/api/file?path=%2Fconfig%2Fcontacts.toml'), 'edit asks for /config/contacts.toml');
  assert(pg.els['file-editor-text'].value === texts['/config/contacts.toml'], 'the editor holds the file text');
  btn(contacts, 'btn-dl').listeners.click();
  await pg.settle();
  assert(pg.calls.filter((c) => c.path === '/api/file?path=%2Fconfig%2Fcontacts.toml').length === 2, 'download asks for the full path too');
  const leftover = r.find((x) => x.name === '._contacts.toml');
  btn(leftover, 'btn-del').listeners.click();
  await pg.settle();
  assert(pg.calls.some((c) => c.method === 'DELETE' && c.path === '/api/file?path=%2Fconfig%2F._contacts.toml'), 'a macOS leftover can be deleted, by its full path');
  assert(pg.els['files-error'].hidden === true, 'a delete that worked shows no error');

  // saving in the editor writes the full path
  pg.els['file-editor-text'].value = 'x = 1\n';
  pg.els['file-editor-save'].onclick();
  await pg.settle();
  assert(pg.calls.some((c) => c.method === 'PUT' && c.path === '/api/file?path=%2Fconfig%2Fcontacts.toml' && c.body === 'x = 1\n'), 'saving writes /config/contacts.toml');

  // upload goes into the current directory
  const pgU = await open({ fileReader: function () { this.readAsText = () => this.onload({ target: { result: 'a = 1\n' } }); } });
  btn(rows(pgU)[0], 'btn-open').listeners.click();
  await pgU.settle();
  pgU.els['files-upload-input'].files = [{ name: 'new.toml' }];
  pgU.els['files-upload-input'].onchange();
  await pgU.settle();
  assert(pgU.calls.some((c) => c.method === 'PUT' && c.path === '/api/file?path=%2Fconfig%2Fnew.toml'), 'an upload in /config goes to /config/new.toml');

  // the secrets file cannot be deleted: the parent reads the device's reason
  btn(r.find((x) => x.name === 'secrets.toml'), 'btn-del').listeners.click();
  await pg.settle();
  assert(pg.els['files-error'].hidden === false && pg.els['files-error'].textContent.includes('The secrets file cannot be deleted'),
    'a refused delete shows the device\'s error text: ' + pg.els['files-error'].textContent);

  // an empty directory shows the empty text (and the way back)
  btn(r.find((x) => x.name === 'sub/'), 'btn-open').listeners.click();
  await pg.settle();
  r = rows(pg);
  assert(pg.els['files-empty'].hidden === false && r.length === 1 && r[0].name === '..', 'an empty directory shows the empty text and the ".." step');
  assert(pg.els['files-path'].textContent === '/config/sub', 'the path shows /config/sub');

  // back up, one step at a time
  btn(r[0], 'btn-up').listeners.click();
  await pg.settle();
  assert(listCalls(pg).at(-1) === '/api/files?path=/config' && pg.els['files-path'].textContent === '/config', 'the ".." step of /config/sub opens /config');
  btn(rows(pg)[0], 'btn-up').listeners.click();
  await pg.settle();
  r = rows(pg);
  assert(listCalls(pg).at(-1) === '/api/files?path=/' && r.map((x) => x.name).join('|') === 'config/|system.log', 'the ".." step of /config opens the root, which has no ".." step');

  // a directory the device does not have: its error text, not an empty list
  const pg404 = await open({ onFetch: (p, o, rep) => (p.startsWith('/api/files') ? rep(404, { error: 'Directory not found' }) : null) });
  assert(pg404.els['files-error'].textContent.includes('Directory not found'), 'a failed listing shows the device\'s error text');
}

// (l) the options of an enum select are catalog words, the saved value is the raw one (the first page
// showed "parent" and "family" as they are written in the file)
{
  const auth = 'Basic ' + Buffer.from(':' + PASSWORD).toString('base64');
  const schema = JSON.parse(readFileSync(resolve(ROOT, 'config', 'form_generated.json'), 'utf8'));
  const enumKeys = [];
  for (const file of Object.values(schema.files))
    for (const sec of file.sections)
      for (const k of sec.keys) if (k.type === 'enum') enumKeys.push({ sec, k });
  assert(enumKeys.length >= 4, 'the schema has enum keys to test: ' + enumKeys.map((e) => e.k.name).join(', '));
  // every value of every enum has a word in every shipped web catalog, and the schema names its key
  for (const { sec, k } of enumKeys) for (const v of k.enums.split('|')) {
    const key = k.option_labels && k.option_labels[v];
    assert(key === k.label.replace(/\.label$/, '') + '.' + v + '.label', 'the schema names the catalog key of ' + sec.id + '.' + k.name + '=' + v + ': ' + key);
    for (const [name, cat] of [['en', en], ['de', de], ['fr', fr]])
      assert(cat[key] && cat[key].trim() !== '', name + ' has a word for ' + key);
  }
  assert(de['config.contact.role.parent.label'] === 'Elternteil' && de['config.contact.role.family.label'] === 'Familie', 'German: Elternteil / Familie');
  assert(fr['config.contact.role.parent.label'] === 'Parent' && fr['config.contact.role.family.label'] === 'Famille', 'French: Parent / Famille');

  const walk3 = (el, out = []) => { for (const c of el.children || []) { out.push(c); walk3(c, out); } return out; };
  const openTab = async (tab, lang, settings = {}) => {
    const pg = await runPage({ storedAuth: auth, storedLang: lang, catalogs: { en, de }, rich: true, settings });
    pg.dataT.find((e) => e.attrs['data-tab'] === tab).listeners.click();
    await pg.settle();
    return pg;
  };
  const optionsOf = (pg, form, name) => walk3(pg.els[form]).find((c) => c.tag === 'select' && c.name === name).children.map((o) => [o.value, o.textContent]);

  // the role of a contact
  for (const [lang, words] of [['en', ['Parent', 'Family']], ['de', ['Elternteil', 'Familie']]]) {
    const pg = await openTab('contacts', lang);
    const opts = optionsOf(pg, 'form-contacts', 'role');
    assert(opts.map((o) => o[1]).join('|') === words.join('|'), lang + ': the role options read ' + words.join(' / ') + ': ' + opts.map((o) => o[1]).join('|'));
    assert(opts.map((o) => o[0]).join('|') === 'parent|family', lang + ': the option values stay parent|family');
  }
  // the saved value is the raw one, whatever language the page is in
  {
    const pg = await openTab('contacts', 'de');
    const role = walk3(pg.els['form-contacts']).find((c) => c.tag === 'select' && c.name === 'role');
    role.value = 'family';
    walk3(pg.els['form-contacts']).find((c) => c.className === 'btn-primary').listeners.click();
    await pg.settle();
    const sent = JSON.parse(pg.calls.find((c) => c.method === 'PUT' && c.path === '/api/contacts').body);
    assert(sent.contact[0].role === 'family', 'the German word "Familie" is saved as family: ' + JSON.stringify(sent.contact[0]));
    const pg2 = await openTab('contacts', 'de');
    walk3(pg2.els['form-contacts']).find((c) => c.className === 'btn-primary').listeners.click();
    await pg2.settle();
    const sent2 = JSON.parse(pg2.calls.find((c) => c.method === 'PUT' && c.path === '/api/contacts').body);
    assert(sent2.contact[0].role === 'parent', 'a contact that is a parent stays parent when saved: ' + sent2.contact[0].role);
  }
  // the other enum selects of the page are worded the same way
  {
    for (const [tab, name, key] of [['advanced', 'level', 'config.log.level'], ['basics', 'notice', 'config.remote_queries.notice']]) {
      const pg = await openTab(tab, 'de', { log: { level: 'events' }, remote_queries: { notice: 'history' } });
      const opts = optionsOf(pg, 'form-' + tab, name);
      const raw = opts.map((o) => o[0]);
      assert(opts.every(([v, text]) => text === de[key + '.' + v + '.label'] && text !== v), 'settings: the ' + name + ' options are catalog words: ' + opts.map((o) => o[1]).join('|'));
      assert(raw.length === 3, 'settings: ' + name + ' keeps its raw values: ' + raw.join('|'));
    }
  }
}

// (m) settings.toml on three tabs (owner's wish of 2026-10-08: one long form needed too much scrolling). Which
// section is on which tab is data of config/schema.toml; every tab saves into the one file and keeps the others.
{
  const auth = 'Basic ' + Buffer.from(':' + PASSWORD).toString('base64');
  const schema = JSON.parse(readFileSync(resolve(ROOT, 'config', 'form_generated.json'), 'utf8'));
  const groups = schema.files.settings.groups;
  const walkM = (el, out = []) => { for (const c of el.children || []) { out.push(c); walkM(c, out); } return out; };
  const tabIds = [...html.matchAll(/data-tab="([^"]+)"/g)].map((m) => m[1]);
  assert(tabIds.join('|') === 'status|basics|school_time|advanced|contacts|secrets|files|log|backup',
    'the tab bar: Status, the three settings tabs, then the rest: ' + tabIds.join('|'));
  for (const [name, cat, words] of [['en', en, 'Basics|School time|Advanced'], ['de', de, 'Allgemein|Schulzeit|Erweitert'], ['fr', fr, 'Général|Heures de classe|Avancé']])
    assert(groups.map((g) => cat[g.label]).join('|') === words, name + ' names the tabs ' + words);
  assert(/<div id="panel-advanced" class="panel">\s*<p class="note" data-t="web.settings.advanced-note"><\/p>/.test(html),
    'the Advanced tab starts with the line that most parents leave it as it is');

  // a full file, as a card that was set up by hand holds it
  const full = () => ({
    child: { name: 'Kim' }, display: { map_url: 'https://maps.example/?q={lat},{lon}' }, modem: { apn: 'internet' },
    battery: { capacity_mah: 1500 }, interface: { language: 'de' }, sms: { language: 'en', extra_trigger_words: ['Ping'] },
    features: { free_text: true, radio: false, parent_tools: true, message_vibration: true, call_vibration: true, quiet_call_vibration: false, modem_sleep: true },
    remote_queries: { notice: 'notify' }, time: { timezone: 'Europe/Berlin' }, log: { level: 'events' }, wifi: { auto_off_minutes: 20 },
    radio: { name: 'pp', peer: '' }, quiet_hours: [{ days: 'mon-fri', from: '08:00', to: '13:00', enabled: true }],
    tiles: [{ label: 'Mum', action: 'call', target: '+491701111111' }],
  });
  const open = async (settings) => {
    const state = { file: settings, puts: [] };
    const onFetch = (path, opts, reply) => {
      if (path !== '/api/settings') return null;
      if (opts.method === 'PUT') { state.file = JSON.parse(opts.body); state.puts.push(state.file); return reply(200, { ok: true, restart_required: true }); }
      return state.file === null ? reply(500, { error: 'Read failed' }) : reply(200, state.file);
    };
    const pg = await runPage({ storedAuth: auth, storedLang: 'en', catalogs: { en, de }, rich: true, onFetch });
    const tab = async (id) => { pg.dataT.find((e) => e.attrs['data-tab'] === id).listeners.click(); await pg.settle(); };
    const form = (id) => pg.els['form-' + id];
    const save = async (id) => { walkM(form(id)).find((c) => c.className === 'btn-primary').listeners.click(); await pg.settle(); };
    const field = (id, section, name) => form(id).querySelector('[data-section="' + section + '"]').querySelector('[name="' + name + '"]');
    const statusOf = (id) => walkM(form(id)).filter((c) => c.className === 'form-actions').flatMap((a) => a.children).filter((c) => c.tag === 'span').map((c) => c.textContent).join('');
    return { pg, state, tab, form, save, field, statusOf };
  };

  // each tab shows exactly the sections of its group, and every section is on one tab
  {
    const t = await open(full());
    const seen = [];
    for (const g of groups) {
      await t.tab(g.id);
      const shown = walkM(t.form(g.id)).filter((c) => c.tag === 'fieldset').map((c) => c.attrs['data-section']);
      const want = schema.files.settings.sections.filter((sec) => sec.group === g.id).map((sec) => sec.id);
      assert(shown.length > 0 && shown.join('|') === want.join('|'), 'the ' + g.id + ' tab shows its sections: ' + shown.join(', '));
      seen.push(...shown);
    }
    assert(seen.slice().sort().join('|') === schema.files.settings.sections.map((sec) => sec.id).sort().join('|') && new Set(seen).size === seen.length,
      'every section of settings.toml is on exactly one tab');
  }

  // saving one tab keeps what the others hold, and what was saved on them since this tab was opened
  {
    const t = await open(full());
    await t.tab('school_time');
    t.state.file.child.name = 'Alex';           // saved meanwhile on the Basics tab (or in the Files tab)
    walkM(t.form('school_time')).find((c) => c.name === 'enabled').checked = false;
    await t.save('school_time');
    const sent = t.state.puts.at(-1);
    const want = full(); want.child.name = 'Alex'; want.quiet_hours[0].enabled = false;
    assert(JSON.stringify(sent) === JSON.stringify(want), 'saving School time writes the whole file with only the window changed: ' + JSON.stringify(sent));
    assert(t.statusOf('school_time').startsWith('Saved.'), 'the tab says Saved.: ' + t.statusOf('school_time'));
  }
  {
    const t = await open(full());
    await t.tab('advanced');
    t.field('advanced', 'time', 'timezone').value = 'Europe/Paris';
    await t.save('advanced');
    const sent = t.state.puts.at(-1);
    assert(sent.time.timezone === 'Europe/Paris' && sent.child.name === 'Kim' && sent.quiet_hours.length === 1 && sent.tiles[0].target === '+491701111111' && sent.features.free_text === true,
      'saving Advanced keeps Basics and School time: ' + JSON.stringify(sent));
  }
  // the file cannot be read at the moment of saving: nothing is sent, the tab says so
  {
    const t = await open(full());
    await t.tab('basics');
    t.state.file = null;
    await t.save('basics');
    assert(t.state.puts.length === 0, 'without the current file no tab writes a part of it');
    assert(t.statusOf('basics').startsWith('Error'), 'the tab shows the error: ' + t.statusOf('basics'));
  }

  // two keys of the same name in two sections ([interface] language and [sms] language) are read back from their
  // own section: the single form saved the interface language as the SMS language too
  {
    const t = await open(full());
    await t.tab('basics');
    await t.save('basics');
    const sent = t.state.puts.at(-1);
    assert(sent.interface.language === 'de' && sent.sms.language === 'en',
      'interface de and SMS en stay as they are: ' + JSON.stringify([sent.interface, sent.sms]));
    assert(JSON.stringify(sent) === JSON.stringify(full()), 'saving a tab without a change writes the file unchanged: ' + JSON.stringify(sent));
  }

  // a change that is not saved: the tab says so, and looking at another tab does not lose it
  {
    const t = await open(full());
    await t.tab('basics');
    assert(t.statusOf('basics') === '', 'a fresh tab says nothing');
    t.field('basics', 'child', 'name').value = 'Robin';
    t.form('basics').oninput();
    assert(t.statusOf('basics') === 'Unsaved changes', 'a typed change: "Unsaved changes": ' + t.statusOf('basics'));
    await t.tab('advanced');
    await t.tab('basics');
    assert(t.field('basics', 'child', 'name').value === 'Robin' && t.statusOf('basics') === 'Unsaved changes',
      'back on the tab the change is still there, still unsaved');
    await t.save('basics');
    assert(t.state.puts.at(-1).child.name === 'Robin' && t.statusOf('basics').startsWith('Saved.'), 'saving it writes it and says Saved.');
    t.state.file.child.name = 'Sam';
    await t.tab('advanced');
    await t.tab('basics');
    assert(t.field('basics', 'child', 'name').value === 'Sam', 'a saved tab is read from the card again when it is opened');
    // adding a school-time window is a change too, though no input event says so
    await t.tab('school_time');
    walkM(t.form('school_time')).find((c) => c.className === 'action-btn btn-edit').listeners.click();
    assert(t.statusOf('school_time') === 'Unsaved changes', 'an added window is an unsaved change');
  }
}

// (n) the days of a school-time window are ticked, not typed, and a window is compact: the days in one row, from,
// to, active and remove in the row below, no hint under each field (owner, 2026-10-08)
{
  const auth = 'Basic ' + Buffer.from(':' + PASSWORD).toString('base64');
  const walkN = (el, out = []) => { for (const c of el.children || []) { out.push(c); walkN(c, out); } return out; };
  const open = async (windows, lang = 'en') => {
    const state = { file: { quiet_hours: windows }, puts: [] };
    const onFetch = (path, opts, reply) => {
      if (path !== '/api/settings') return null;
      if (opts.method === 'PUT') { state.file = JSON.parse(opts.body); state.puts.push(state.file); return reply(200, { ok: true }); }
      return reply(200, state.file);
    };
    const pg = await runPage({ storedAuth: auth, storedLang: lang, catalogs: { en, de }, rich: true, onFetch });
    pg.dataT.find((e) => e.attrs['data-tab'] === 'school_time').listeners.click();
    await pg.settle();
    const entries = () => pg.els['form-school_time'].querySelector('[data-list-section="quiet_hours"]').querySelectorAll('.list-entry');
    const boxes = (entry) => entry.querySelector('[name="days"]').querySelectorAll('.day-box');
    const ticked = (entry) => boxes(entry).filter((b) => b.checked).map((b) => b.attrs['data-day']).join(',');
    const save = async () => {
      walkN(pg.els['form-school_time']).find((c) => c.className === 'btn-primary').listeners.click();
      await pg.settle();
      return state.puts.at(-1).quiet_hours;
    };
    return { pg, entries, boxes, ticked, save };
  };

  // what the file says becomes ticked days, and the page writes it back in the file's own words
  const cases = [
    ['mon-fri', 'mon,tue,wed,thu,fri', 'mon-fri'], ['daily', 'mon,tue,wed,thu,fri,sat,sun', 'daily'],
    ['Mon, Wed ,FRI', 'mon,wed,fri', 'mon,wed,fri'], ['fri-mon', 'mon,fri,sat,sun', 'mon,fri,sat,sun'],
    ['sat-sun', 'sat,sun', 'sat,sun'], ['mon-sun', 'mon,tue,wed,thu,fri,sat,sun', 'daily'],
  ];
  for (const [file, shown, saved] of cases) {
    const t = await open([{ days: file, from: '13:00', to: '15:00', enabled: true }]);
    const e = t.entries()[0];
    assert(t.boxes(e).length === 7 && t.ticked(e) === shown, 'days "' + file + '" are the boxes ' + shown + ': ' + t.ticked(e));
    const sent = await t.save();
    assert(sent[0].days === saved, 'and are saved as "' + saved + '": ' + sent[0].days);
  }

  // ticking changes what is saved; an empty window gets no day ticked
  {
    const t = await open([{ days: 'mon-fri', from: '08:00', to: '12:30', enabled: true }]);
    const b = t.boxes(t.entries()[0]);
    b.find((x) => x.attrs['data-day'] === 'fri').checked = false;
    b.find((x) => x.attrs['data-day'] === 'sat').checked = true;
    let sent = await t.save();
    assert(sent[0].days === 'mon,tue,wed,thu,sat', 'Friday off, Saturday on: ' + sent[0].days);
    walkN(t.pg.els['form-school_time']).find((c) => c.className === 'action-btn btn-edit').listeners.click();
    assert(t.ticked(t.entries()[1]) === '', 'an added window has no day ticked yet');
    t.boxes(t.entries()[1]).find((x) => x.attrs['data-day'] === 'tue').checked = true;
    t.entries()[1].querySelector('[name="from"]').value = '13:00';
    t.entries()[1].querySelector('[name="to"]').value = '16:00';
    sent = await t.save();
    assert(JSON.stringify(sent[1]) === '{"days":"tue","from":"13:00","to":"16:00","enabled":true}', 'a second window, Tuesday afternoon: ' + JSON.stringify(sent[1]));
  }

  // a text the page does not understand stays a text field and is saved unchanged, never lost
  {
    const t = await open([{ days: 'monday', from: '08:00', to: '12:30', enabled: true }]);
    const f = t.entries()[0].querySelector('[name="days"]');
    assert(f.tag === 'input' && f.value === 'monday', 'days the page cannot read stay a text field: ' + f.tag);
    const sent = await t.save();
    assert(sent[0].days === 'monday', 'and are saved as they were (the device reports them): ' + sent[0].days);
  }

  // the window is compact: the days alone in the first row, from, to, active and remove together, no hints
  {
    const t = await open([{ days: 'mon-fri', from: '08:00', to: '12:30', enabled: false }], 'de');
    const e = t.entries()[0];
    assert(e.className === 'list-entry compact', 'a window is a compact entry');
    const row = e.children.find((c) => c.className === 'entry-row');
    const names = walkN(row).filter((c) => c.name).map((c) => c.name);
    assert(row && names.join('|') === 'from|to|enabled', 'from, to and active stand in one row: ' + names.join('|'));
    assert(walkN(row).some((c) => c.className === 'remove-btn'), 'the remove button is in that row');
    assert(walkN(e).filter((c) => c.className === 'hint').length === 0, 'no hint under the fields of a window');
    assert(e.querySelector('[name="from"]').type === 'time' && e.querySelector('[name="to"]').type === 'time', 'from and to are time fields');
    const words = walkN(e.querySelector('[name="days"]')).filter((c) => c.tag === 'span').map((c) => c.textContent);
    assert(words.join(' ') === 'Mo Di Mi Do Fr Sa So', 'the days carry the German short words: ' + words.join(' '));
    assert(walkN(e).filter((c) => c.tag === 'label' && c.children.some((x) => x.className === 'days')).length === 0,
      'the seven boxes are not inside one label (a click on a word would tick Monday)');
    for (const [name, cat, w] of [['en', en, 'Mo Tu We Th Fr Sa Su'], ['de', de, 'Mo Di Mi Do Fr Sa So'], ['fr', fr, 'lu ma me je ve sa di']])
      assert(['mon', 'tue', 'wed', 'thu', 'fri', 'sat', 'sun'].map((d) => cat['web.day.' + d]).join(' ') === w, name + ' has the day words ' + w);
  }
  // a time the time field would not show stays a text field
  {
    const t = await open([{ days: 'mon', from: '8:00', to: '12:30', enabled: true }]);
    assert(t.entries()[0].querySelector('[name="from"]').type === 'text', '"8:00" stays a text field, not an empty time field');
  }
}

// (o) a language comes in three answers (words, labels of the form, help texts): two did not fit the device's
// buffer with room to spare once 4.1 added the tabs, the weekday boxes and the tiles of [home]
{
  const auth = 'Basic ' + Buffer.from(':' + PASSWORD).toString('base64');
  const walkO = (el, out = []) => { for (const c of el.children || []) { out.push(c); walkO(c, out); } return out; };
  const openAdvanced = async (extra) => {
    const pg = await runPage({ storedAuth: auth, storedLang: 'de', catalogs: { en, de }, rich: true, ...extra });
    pg.dataT.find((e) => e.attrs['data-tab'] === 'advanced').listeners.click();
    await pg.settle();
    return pg;
  };
  let pg = await openAdvanced({});
  assert(pg.calls.some((c) => c.path === '/api/lang/de/labels' && c.authed), 'the labels are asked for after the login, with the password');
  let names = walkO(pg.els['form-advanced']).filter((c) => c.tag === 'label' || c.tag === 'legend').map((c) => c.textContent);
  assert(names.includes(de['config.time.timezone.label']) && names.includes(de['config.home.label']), 'the fields carry their German labels: ' + names.slice(0, 4).join(' | '));
  // the labels do not arrive: the tabs keep their words, a field is named by its key in the file, never a raw key
  pg = await openAdvanced({ labelsFail: true });
  const tabs = pg.dataT.filter((e) => e.attrs['data-tab']).map((e) => e.textContent);
  assert(tabs.includes('Erweitert'), 'without labels the tabs are still German: ' + tabs.join('|'));
  names = walkO(pg.els['form-advanced']).filter((c) => c.tag === 'label' || c.tag === 'legend').map((c) => c.textContent);
  assert(names.includes('timezone') && names.includes('home'), 'a field without its label is named by its key in the file: ' + names.slice(0, 6).join(' | '));
  assert(!names.some((x) => /^config\./.test(x)), 'no raw key names a field');
}

// ---------- summary ----------
console.log(`\n${pass + fail} checks: ${pass} passed, ${fail} failed.`);
if (fail > 0) process.exit(1);

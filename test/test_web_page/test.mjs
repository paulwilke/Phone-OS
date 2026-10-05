/**
 * Tests for the generated web page (web/index.html).
 *
 * Checks:
 *  1. web/index.html exists and has content.
 *  2. Every data-t attribute references a known web.* key (full key name).
 *  3. The page uses textContent (not innerHTML) for device-supplied content.
 *  4. The WEB_KEYS list in gen_web.py matches the web.* keys in lang/en.toml.
 *  5. Every web.* key in en.toml also exists in de.toml and fr.toml.
 *  6. Translations are non-empty for all languages.
 */

import { readFileSync, existsSync } from 'fs';
import { resolve, dirname } from 'path';
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
const enPath   = resolve(ROOT, 'lang', 'en.toml');
const dePath   = resolve(ROOT, 'lang', 'de.toml');
const frPath   = resolve(ROOT, 'lang', 'fr.toml');
const genPath  = resolve(ROOT, 'scripts', 'gen_web.py');

// ---------- test 1: file existence ----------
console.log('\nTest: file existence');
assert(existsSync(htmlPath), 'web/index.html exists');
assert(existsSync(enPath), 'lang/en.toml exists');
assert(existsSync(dePath), 'lang/de.toml exists');
assert(existsSync(frPath), 'lang/fr.toml exists');
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
console.log('\nTest: WEB_KEYS in gen_web.py matches lang/en.toml web.* keys');
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

// ---------- summary ----------
console.log(`\n${pass + fail} checks: ${pass} passed, ${fail} failed.`);
if (fail > 0) process.exit(1);

/**
 * Tests for the project's web page (site/index.html): it has no words of its own (scripts/make_site.py prerenders one
 * page per language with its words), and its script, run against a small fake of the parts of the DOM it uses, builds
 * the parts that depend on the language (the keys, the status line, the pictures, the links) in the language of the
 * page, which is the page's folder: the root is English, /de/ is German. A root page opened with ?lang=de goes to /de/,
 * and the selector goes to the other language's page.
 *
 *   node test/test_site_page/test.mjs
 */
import { readFileSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';
import vm from 'vm';

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '..', '..');
const html = readFileSync(resolve(ROOT, 'site', 'index.html'), 'utf8');

let pass = 0, fail = 0;
const assert = (cond, msg) => { if (cond) { pass++; console.log('  PASS:', msg); } else { fail++; console.error('  FAIL:', msg); } };

// Cuts every span from `open` to `close` out of the text (the page's own source from the repository, not anyone's input);
// with `keepOpen` the opening tag stays and only what is between the tags goes.
function without(text, open, close, keepOpen = false) {
  let out = text;
  let i = out.indexOf(open);
  while (i >= 0) {
    const j = out.indexOf(close, i + open.length);
    const k = keepOpen ? out.indexOf('>', i) + 1 : i;
    out = out.slice(0, k) + (j < 0 ? '' : out.slice(keepOpen ? j : j + close.length));
    i = out.indexOf(open, k);
  }
  return out;
}
const tags = (text) => without(text, '<', '>');

// ---- no words of its own ---------------------------------------------------------------------------------
const markup = without(without(without(html, '<script', '</script>'), '<style', '</style>'), '<!--', '-->');
// The texts the markup may hold: a path of the repository in <code>, and a command to type in <code class="cmd">; both are
// the same in every language.
const visible = tags(markup.replace(/<code>[a-z]+(?:\/[a-z]+)*\/<\/code>/g, '').replace(/<code class="cmd">[^<]*<\/code>/g, '')).trim();
assert(visible === '', 'the markup holds no text: every word comes from the catalogs' + (visible ? ': ' + visible.slice(0, 80) : ''));
const wordy = [...markup.matchAll(/\s(alt|aria-label|title|placeholder)="/g)].map((m) => m[1]);
assert(wordy.length === 0, 'no attribute of the markup holds words: they are keys (data-i18n-alt, data-i18n-aria-label)' + (wordy.length ? ': ' + wordy.join(', ') : ''));
const metas = [...markup.matchAll(/<meta\s[^>]*(?:name="description"|property="og:(?:title|description)")[^>]*>/g)].map((m) => m[0]);
assert(metas.length === 3 && metas.every((m) => /data-i18n-content="site\./.test(m) && !/\scontent="/.test(m)),
  'the description and the link preview name their keys and hold no words');
assert(/<html lang="en" data-root="">/.test(html) && html.split('<meta name="site-alternates">').length === 2,
  'the page declares its language and root once, and has one placeholder for its links (make_site.py writes them)');
// The page's one script: what follows the first plain <script> up to its end tag (the data script before it is typed).
const scriptStart = html.indexOf('<script>') + '<script>'.length;
const script = html.slice(scriptStart, html.indexOf('</script>', scriptStart));
// A sentence has spaces in it; the script's strings are keys, file names, addresses and DOM names.
const strings = [...script.matchAll(/"((?:[^"\\]|\\.)*)"/g)].map((m) => m[1]).filter((t) => /\s/.test(t) && t !== 'use strict');
assert(strings.length === 0, 'the script has no sentence in it' + (strings.length ? ': ' + strings.join(' | ') : ''));
assert(!/navigator\.language/.test(script), 'the script does not ask the browser for its language');
assert(!/replaceState/.test(script), 'the script does not rewrite the address: a language is a page, not a query');

// ---- a fake DOM for what the page uses --------------------------------------------------------------------------
class Element {
  constructor(tag, attrs = {}) {
    this.tag = tag; this.attrs = { ...attrs }; this.children = []; this.listeners = {}; this._text = ''; this.style = {};
    this.hidden = 'hidden' in attrs; this.disabled = false; this.value = ''; this.type = ''; this.className = '';
    const classes = new Set();
    this.classList = { add: (c) => classes.add(c), remove: (c) => classes.delete(c), contains: (c) => classes.has(c) };
  }
  getAttribute(name) { return name in this.attrs ? this.attrs[name] : null; }
  setAttribute(name, value) { this.attrs[name] = String(value); }
  removeAttribute(name) { delete this.attrs[name]; }
  hasAttribute(name) { return name in this.attrs; }
  set textContent(v) { this._text = String(v); this.children = []; }
  get textContent() { return this._text || this.children.map((c) => c.textContent).join(''); }
  appendChild(c) { this.children.push(c); return c; }
  addEventListener(type, fn) { this.listeners[type] = fn; }
  click() { if (!this.disabled && this.listeners.click) this.listeners.click(); }
  contains(other) { return other === this || this.children.some((c) => c.contains(other)); }
  focus() { if (this.doc) this.doc.activeElement = this; }
}

// Every element of the markup with its attributes; what the script creates hangs below them.
function parse() {
  const out = [];
  const source = without(without(without(html, '<script', '</script>', true), '<style', '</style>'), '<!--', '-->');
  for (const m of source.matchAll(/<([a-zA-Z][a-zA-Z0-9]*)\b([^>]*)>/g)) {
    const attrs = {};
    for (const a of m[2].matchAll(/([a-zA-Z:-]+)(?:="([^"]*)")?/g)) attrs[a[1]] = a[2] === undefined ? '' : a[2];
    out.push(new Element(m[1].toLowerCase(), attrs));
  }
  return out;
}

// The words: each key says its language and itself, so that a word in the wrong language or a key without a word shows.
const keys = new Set([...html.matchAll(/\sdata-i18n(?:-[a-z-]+)?="([^"]+)"/g)].map((m) => m[1]).concat([...script.matchAll(/"(site\.[a-z0-9.-]+)"/g)].map((m) => m[1])));
const LANGUAGES = [{ code: 'en', native: 'English' }, { code: 'de', native: 'Deutsch' }, { code: 'fr', native: 'Français' }];
const words = {};
for (const { code } of LANGUAGES) {
  words[code] = Object.fromEntries([...keys].map((k) => [k, code + ':' + k]));
  words[code]['site.device.status'] = code + ':{screen} {n}/{total}';
  words[code]['site.device.tap'] = code + ':tap {screen}';
}
// One graph over every language, as the walk makes it: each language has its start (3 states each: home, compose, sos),
// and a state is in the language of its picture. The SOS page of German (5) leads with Up to the English compose page
// (1), as the Language screen leads from one language into another; the key test of the self-test (9, 10) ends by itself.
const graph = { starts: { en: 0, de: 3, fr: 6 }, complete: true, nodes: [], edges: [] };
LANGUAGES.forEach(({ code }, i) => {
  const base = 3 * i;
  graph.nodes.push({ id: base, screen: 'home', lang: code }, { id: base + 1, screen: 'compose', lang: code }, { id: base + 2, screen: 'sos', lang: code });
  graph.edges.push({ from: base, key: 'select', to: base + 1 }, { from: base + 1, key: 'back', to: base }, { from: base + 1, key: 'down', to: base + 2 },
                   { from: base, tap: { x: 160, y: 48, w: 74, h: 69 }, to: base + 2 });
});
graph.nodes.push({ id: 9, screen: 'selftest', lang: 'en' }, { id: 10, screen: 'selftest', lang: 'en' });
graph.edges.push({ from: 5, key: 'up', to: 1 }, { from: 1, key: 'up', to: 9 }, { from: 9, key: 'down', to: 10 }, { from: 10, wait: 7, to: 9 }, { from: 10, key: 'down', to: 9 });

const settle = () => new Promise((r) => setTimeout(r, 0));

// A page of the site: `lang` and `root` are what make_site.py writes into <html> (the root page is English, a language's
// folder is one level down); the data script holds the words of that language only, as the built page does.
async function load({ lang = 'en', root = '', search = '', hash = '#try', withGraph = true, browser = 'de-DE' } = {}) {
  const elements = parse();
  const all = () => { const out = []; const walk = (e) => { out.push(e); e.children.forEach(walk); }; elements.forEach(walk); return out; };
  const byId = (id) => all().find((e) => e.getAttribute('id') === id);
  byId('data')._text = JSON.stringify({ languages: LANGUAGES, words: { [lang]: words[lang] } });
  const replaced = [];
  const timers = [];
  const location = { search, pathname: '/Phone-OS/', hash, href: 'https://example.test/Phone-OS/', replace: (url) => { replaced.push(url); } };
  const document = {
    activeElement: null,
    documentElement: { lang, getAttribute: (name) => (name === 'data-root' ? root : null) },
    getElementById: byId,
    createElement: (tag) => new Element(tag),
    querySelectorAll: (selector) => { const name = selector.match(/^\[([a-z0-9-]+)\]$/)[1]; return all().filter((e) => e.hasAttribute(name)); },
  };
  document.body = new Element('body');
  document.activeElement = document.body;
  for (const e of elements) e.doc = document;
  // The markup's elements are a flat list here: what lies inside the device is what the real markup nests in it.
  const parts = ['softkeys', 'pad', 'taps', 'picture'].map(byId);
  byId('device').contains = (other) => other === byId('device') || parts.some((part) => part === other || part.children.includes(other));
  // A clock the test turns by hand: the page asks for a time to pass, and the test lets it pass or not.
  const fakeSetTimeout = (fn, ms) => { timers.push({ fn, ms, live: true }); return timers.length; };
  const fakeClearTimeout = (id) => { if (timers[id - 1]) timers[id - 1].live = false; };
  const fetch = (url) => (url === root + 'demo/graph.json' && !withGraph ? Promise.reject(new Error('404'))
    : Promise.resolve({ ok: true, json: async () => graph, text: async () => '4.2.10\n' }));
  vm.runInNewContext(script, { document, location, fetch, URLSearchParams, Image: class { }, setTimeout: fakeSetTimeout, clearTimeout: fakeClearTimeout,
    navigator: { language: browser }, JSON, console });
  await settle();
  const live = () => timers.filter((t) => t.live && t.ms > 100);   // the flash of the screen lasts 90 ms and is not a clock of the device
  return { document, byId, all, replaced, location, lang, root, live,
    select: async (code) => { const s = byId('language'); s.value = code; s.listeners.change(); await settle(); } };
}

const picturesIn = (page) => page.all().filter((e) => e.hasAttribute('data-picture')).every((e) => e.getAttribute('src').startsWith(page.root + 'demo/' + page.lang + '/'));
const guide = (page) => page.all().find((e) => e.hasAttribute('data-guide') && e.getAttribute('data-guide') === '').getAttribute('href');
const demoLink = (page) => page.all().find((e) => e.hasAttribute('data-demo')).getAttribute('href');
// The guides of the other languages that are listed: the link of the page's own language is hidden.
const listedLanguages = (page) => page.all().filter((e) => e.hasAttribute('data-native') && !e.hidden).map((e) => e.getAttribute('data-native')).join();
const key = (page, name) => page.all().find((e) => e.getAttribute('aria-label') === page.lang + ':site.key.' + name);
const numberText = (page, value) => page.all().find((e) => e.getAttribute('data-number') === value).textContent;

// ---- the root page is English, and the browser's language does not change it -----------------------------------
let page = await load({ browser: 'de-DE' });
assert(page.document.documentElement.lang === 'en' && page.byId('status').textContent === 'en:en:site.screen.home 1/11',
  'the root page is English, and a browser set to German does not change it');
assert(page.replaced.length === 0 && page.location.href === 'https://example.test/Phone-OS/', 'opening the root page does not move it or touch the address');
assert(page.byId('language').value === 'en' && page.byId('language').children.map((o) => o.textContent).join() === 'English,Deutsch,Français',
  'the selector offers the three languages by their own names and shows English');
assert(picturesIn(page) && page.byId('picture').getAttribute('src') === 'demo/en/n0.png', 'the pictures are the English ones');
assert(page.byId('pic-compose').getAttribute('src') === 'demo/en/n1.png' && page.byId('pic-sos').getAttribute('src') === 'demo/en/n2.png',
  'the cards show the first picture of their screen');
assert(numberText(page, '2500+') === '2,500+' && page.byId('stat-pictures').textContent === '11',
  'numbers are written the English way, and the pictures are counted in all languages');
assert(guide(page).endsWith('/docs/user-guide.md') && listedLanguages(page) === 'de,fr', 'the guide in English, and links to the guides of the other languages');
assert(demoLink(page) === 'demo/index.html?lang=en', 'the link to the full demo names English too: without it the demo would follow the browser');

// ---- ?lang= on the root page goes to that language's page; an unknown code and English stay here -------------------
page = await load({ search: '?lang=de' });
assert(page.replaced.length === 1 && page.replaced[0] === './de/#try', 'a root page opened with ?lang=de goes to de/, keeping the hash');
page = await load({ search: '?lang=fr' });
assert(page.replaced.length === 1 && page.replaced[0] === './fr/#try', 'a root page opened with ?lang=fr goes to fr/');
page = await load({ search: '?lang=xx' });
assert(page.replaced.length === 0 && page.byId('status').textContent === 'en:en:site.screen.home 1/11', 'a code the page does not have is ignored: the page stays English');
page = await load({ search: '?lang=en' });
assert(page.replaced.length === 0, 'English is the root itself: ?lang=en does not move the page');
page = await load({ lang: 'de', root: '../', search: '?lang=fr' });
assert(page.replaced.length === 0, 'a language folder does not redirect again: the address of its own page is its language');

// ---- a language's page: its folder is one level down, and everything it builds is in its language ----------------
page = await load({ lang: 'de', root: '../' });
assert(page.byId('picture').getAttribute('src') === '../demo/de/n3.png' && picturesIn(page), 'the pictures of the German page are the German ones, one folder down, from the state the device starts in');
assert(page.byId('pic-compose').getAttribute('src') === '../demo/de/n4.png' && page.byId('pic-sos').getAttribute('src') === '../demo/de/n5.png', 'the cards too: the German states of their screens');
assert(page.byId('status').textContent === 'de:de:site.screen.home 4/11', 'the status line is in German');
assert(key(page, 'select') && key(page, 'select').getAttribute('aria-label') === 'de:site.key.select' && key(page, 'select').children[1].textContent === 'de:site.key.select',
  'the labels of the keys are in German, the name of a key and its label for a reader of the screen');
assert(numberText(page, '2500+') === '2.500+', 'numbers are written the German way');
assert(guide(page).endsWith('/docs/de/benutzerhandbuch.md') && listedLanguages(page) === 'en,fr', 'the guide button leads to the German guide');
assert(demoLink(page) === '../demo/index.html?lang=de', 'the link to the full demo asks for German from the German page');
assert(page.replaced.length === 0, 'the German page does not move');

// ---- the selector goes to the other language's page ----------------------------------------------------------------
page = await load();
await page.select('de');
assert(page.location.href === './de/#try', 'choosing German on the root page goes to de/, with the hash kept');
assert(page.replaced.length === 0, 'and the selector navigates instead of rewriting the address');
page = await load();
await page.select('en');
assert(page.location.href === 'https://example.test/Phone-OS/', 'choosing the page\'s own language does nothing');
page = await load({ lang: 'de', root: '../' });
await page.select('en');
assert(page.location.href === '../#try', 'choosing English on the German page goes back to the root, one level up');
page = await load({ lang: 'de', root: '../' });
await page.select('fr');
assert(page.location.href === '../fr/#try', 'choosing French on the German page goes to the French folder, beside it');

// ---- the keys and the taps of the device, in the language of the page -----------------------------------------------
page = await load({ lang: 'de', root: '../' });
assert(key(page, 'select').disabled === false && key(page, 'back').disabled === true, 'only the keys that lead somewhere are enabled');
key(page, 'select').click();
assert(page.byId('picture').getAttribute('src') === '../demo/de/n4.png' && page.byId('status').textContent === 'de:de:site.screen.compose 5/11',
  'a key follows its edge');
page.byId('start').click();
assert(page.byId('picture').getAttribute('src') === '../demo/de/n3.png', 'start over goes to the state the device starts in, in the language of the page');
const tap = page.byId('taps').children[0];
assert(tap && tap.getAttribute('aria-label') === 'de:tap de:site.screen.sos', 'a touch target is named in the language of the page');
tap.click();
assert(page.byId('picture').getAttribute('src') === '../demo/de/n5.png', 'a tap follows its edge');
key(page, 'up').click();
assert(page.byId('picture').getAttribute('src') === '../demo/en/n1.png' && page.byId('status').textContent === 'de:de:site.screen.compose 2/11',
  'choosing English on the device\'s own Language screen shows the English pictures, and the page keeps its words');

// ---- the device keeps the keyboard ------------------------------------------------------------------------------------------
// After a click with the mouse the target is gone, or the key is disabled, and the browser would give the focus to the page:
// the arrows and Enter would scroll it instead of driving the device.
page = await load();
const device = page.byId('device');
const held = (el) => { page.document.activeElement = el; };
held(page.byId('taps').children[0]);
page.byId('taps').children[0].click();
assert(page.document.activeElement === device, 'after a click on a touch target (which is replaced by the next picture\'s) the device has the focus');
page.byId('start').click();
held(key(page, 'select'));
key(page, 'select').click();
assert(key(page, 'select').disabled === true && page.document.activeElement === device, 'after a click on a key that the move disables, the device has the focus');
held(page.document.body);
key(page, 'up').click();
assert(page.document.activeElement === device, 'a click that left the focus on the page gives it to the device');
held(key(page, 'down'));
key(page, 'down').click();
assert(key(page, 'down').disabled === false && page.document.activeElement === key(page, 'down'), 'an enabled key of the device that has the focus keeps it (somebody who tabs to a key stays there)');
held(page.byId('start'));
page.byId('start').click();
assert(page.document.activeElement === device, 'start over gives the device the focus');

// ---- a screen that ends by itself ------------------------------------------------------------------------------------------
page = await load();
assert(page.live().length === 0, 'a state that does not end by itself starts no clock');
key(page, 'select').click();
key(page, 'up').click();
key(page, 'down').click();
assert(page.byId('picture').getAttribute('src') === 'demo/en/n10.png' && page.live().length === 1 && page.live()[0].ms === 7000,
  'the key test of the self-test page asks for seven seconds');
page.document.activeElement = page.byId('language');
page.live()[0].fn();
assert(page.byId('picture').getAttribute('src') === 'demo/en/n9.png' && page.live().length === 0, 'and ends when they have passed, as on the device');
assert(page.document.activeElement === page.byId('language'), 'the end of the time does not take the focus from where somebody has put it');
key(page, 'down').click();
page.byId('start').click();
assert(page.live().length === 0, 'leaving the screen stops the clock');

// ---- without the pictures ------------------------------------------------------------------------------------------
page = await load({ withGraph: false });
assert(page.byId('status').textContent === 'en:site.device.missing', 'a page without the demo says so');
page = await load({ lang: 'de', root: '../', withGraph: false });
assert(page.byId('status').textContent === 'de:site.device.missing', 'in the language of the page');

console.log(`\n${pass} checks: ${pass} passed, ${fail} failed.`);
process.exit(fail ? 1 : 0);

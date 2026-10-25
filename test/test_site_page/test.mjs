/**
 * Tests for the project's web page (site/index.html): it has no words of its own, and its script, run against a small
 * fake of the parts of the DOM it uses, fills every word and swaps every picture for the language of the address
 * (?lang=de), which is English unless the address says otherwise, and the selector changes it in both directions.
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

// ---- no words of its own ---------------------------------------------------------------------------------
const markup = html.replace(/<script[\s\S]*?<\/script>/g, '').replace(/<style[\s\S]*?<\/style>/g, '').replace(/<!--[\s\S]*?-->/g, '');
// The one text the markup may hold: a path of the repository in <code>, the same in every language.
const visible = markup.replace(/<code>[a-z]+(?:\/[a-z]+)*\/<\/code>/g, '').replace(/<[^>]*>/g, '').trim();
assert(visible === '', 'the markup holds no text: every word comes from the catalogs' + (visible ? ': ' + visible.slice(0, 80) : ''));
const wordy = [...markup.matchAll(/\s(alt|aria-label|title|placeholder)="/g)].map((m) => m[1]);
assert(wordy.length === 0, 'no attribute of the markup holds words: they are keys (data-i18n-alt, data-i18n-aria-label)' + (wordy.length ? ': ' + wordy.join(', ') : ''));
const metas = [...markup.matchAll(/<meta\s[^>]*(?:name="description"|property="og:(?:title|description)")[^>]*>/g)].map((m) => m[0]);
assert(metas.length === 3 && metas.every((m) => /data-i18n-content="site\./.test(m) && !/\scontent="/.test(m)),
  'the description and the link preview name their keys and hold no words');
const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];
// A sentence has spaces in it; the script's strings are keys, file names, addresses and DOM names.
const strings = [...script.matchAll(/"((?:[^"\\]|\\.)*)"/g)].map((m) => m[1]).filter((t) => /\s/.test(t) && t !== 'use strict');
assert(strings.length === 0, 'the script has no sentence in it' + (strings.length ? ': ' + strings.join(' | ') : ''));
assert(!/navigator\.language/.test(script), 'the script does not ask the browser for its language');

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
}

// Every element of the markup with its attributes; what the script creates hangs below them.
function parse() {
  const out = [];
  const tags = html.replace(/(<script[^>]*>)[\s\S]*?<\/script>/g, '$1</script>').replace(/<style[\s\S]*?<\/style>/g, '').replace(/<!--[\s\S]*?-->/g, '');
  for (const m of tags.matchAll(/<([a-zA-Z][a-zA-Z0-9]*)\b([^>]*)>/g)) {
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
const graph = {
  start: 0,
  nodes: [{ id: 0, screen: 'home' }, { id: 1, screen: 'compose' }, { id: 2, screen: 'sos' }],
  edges: [{ from: 0, key: 'select', to: 1 }, { from: 1, key: 'back', to: 0 }, { from: 1, key: 'down', to: 2 },
          { from: 0, tap: { x: 160, y: 48, w: 74, h: 69 }, to: 2 }],
};

const settle = () => new Promise((r) => setTimeout(r, 0));

async function load(search = '', { withGraph = true } = {}) {
  const elements = parse();
  const all = () => { const out = []; const walk = (e) => { out.push(e); e.children.forEach(walk); }; elements.forEach(walk); return out; };
  const byId = (id) => all().find((e) => e.getAttribute('id') === id);
  byId('data')._text = JSON.stringify({ languages: LANGUAGES, words });
  const location = { search, pathname: '/Phone-OS/', hash: '#try' };
  const urls = [];
  const document = {
    documentElement: { lang: 'en' }, title: '',
    getElementById: byId,
    createElement: (tag) => new Element(tag),
    querySelectorAll: (selector) => { const name = selector.match(/^\[([a-z0-9-]+)\]$/)[1]; return all().filter((e) => e.hasAttribute(name)); },
  };
  const history = { replaceState: (state, title, url) => { urls.push(url); location.search = url.includes('?') ? url.slice(url.indexOf('?'), url.indexOf('#') < 0 ? undefined : url.indexOf('#')) : ''; } };
  const fetch = (url) => (url === 'demo/graph.json' && !withGraph ? Promise.reject(new Error('404'))
    : Promise.resolve({ ok: true, json: async () => graph, text: async () => '4.2.10\n' }));
  vm.runInNewContext(script, { document, location, history, fetch, URLSearchParams, Image: class { }, setTimeout,
    navigator: { language: 'de-DE' }, JSON, console });
  await settle();
  return { document, byId, all, urls, location,
    select: async (code) => { const s = byId('language'); s.value = code; s.listeners.change(); await settle(); } };
}

const textsIn = (page, code) => page.all().filter((e) => e.hasAttribute('data-i18n')).every((e) => e.textContent === code + ':' + e.getAttribute('data-i18n'));
const altsIn = (page, code) => page.all().filter((e) => e.hasAttribute('data-i18n-alt')).every((e) => e.getAttribute('alt') === code + ':' + e.getAttribute('data-i18n-alt'));
const picturesIn = (page, code) => page.all().filter((e) => e.hasAttribute('data-picture')).every((e) => e.getAttribute('src').startsWith('demo/' + code + '/'));
const guide = (page) => page.all().find((e) => e.hasAttribute('data-guide') && e.getAttribute('data-guide') === '').getAttribute('href');
const demoLink = (page) => page.all().find((e) => e.hasAttribute('data-demo')).getAttribute('href');
const nativeLinks = (page) => page.all().filter((e) => e.hasAttribute('data-native')).filter((e) => !e.hidden).map((e) => e.textContent);

// ---- the default is English, whatever the browser says -------------------------------------------------------------
let page = await load();
assert(page.document.documentElement.lang === 'en' && page.document.title === 'en:site.meta.title', 'a browser set to German gets the page in English: the default');
assert(textsIn(page, 'en') && altsIn(page, 'en'), 'every word of the markup is filled in English');
assert(page.all().filter((e) => e.hasAttribute('data-i18n-content')).every((e) => e.getAttribute('content').startsWith('en:')), 'the description too');
assert(page.byId('language').value === 'en' && page.byId('language').children.map((o) => o.textContent).join() === 'English,Deutsch,Français',
  'the selector offers the three languages by their own names and shows English');
assert(picturesIn(page, 'en') && page.byId('picture').getAttribute('src') === 'demo/en/n0.png', 'the pictures are the English ones');
assert(page.byId('pic-compose').getAttribute('src') === 'demo/en/n1.png' && page.byId('pic-sos').getAttribute('src') === 'demo/en/n2.png',
  'the cards show the first picture of their screen');
assert(page.byId('status').textContent === 'en:en:site.screen.home 1/3', 'the status names the screen and counts the pictures');
assert(page.all().find((e) => e.getAttribute('data-number') === '2500+').textContent === '2,500+' && page.byId('stat-pictures').textContent === '9',
  'numbers are written the English way, and the pictures are counted in all languages');
assert(guide(page).endsWith('/docs/user-guide.md') && nativeLinks(page).join() === 'Deutsch,Français', 'the guide in English, and links to it in the other languages');
assert(demoLink(page) === 'demo/index.html', 'the link to the full demo carries no language in English, the default');
assert(page.urls.length === 0, 'opening the page does not touch the address');

page = await load('?lang=fr');
assert(page.document.documentElement.lang === 'fr' && textsIn(page, 'fr') && picturesIn(page, 'fr'), '?lang=fr opens the page in French');
assert(demoLink(page) === 'demo/index.html?lang=fr', 'and the link to the full demo asks it for French');
page = await load('?lang=xx');
assert(page.document.documentElement.lang === 'en' && textsIn(page, 'en'), 'a language the page does not have falls back to English');

// ---- the selector, both ways --------------------------------------------------------------------------------------
page = await load();
await page.select('de');
assert(page.document.documentElement.lang === 'de' && page.document.title === 'de:site.meta.title', 'choosing German sets the language and the title');
assert(textsIn(page, 'de') && altsIn(page, 'de'), 'every word changes to German');
assert(page.all().filter((e) => e.hasAttribute('data-i18n-aria-label')).every((e) => e.getAttribute('aria-label').startsWith('de:')), 'the labels of the keys and of the device too');
assert(picturesIn(page, 'de') && page.byId('picture').getAttribute('src') === 'demo/de/n0.png', 'every picture changes to German');
assert(page.urls.at(-1) === '/Phone-OS/?lang=de#try', 'the address carries the language, so that a reload and a link keep it');
assert(demoLink(page) === 'demo/index.html?lang=de', 'the link to the full demo follows the selector');
assert(page.all().find((e) => e.getAttribute('data-number') === '2500+').textContent === '2.500+', 'numbers are written the German way');
assert(guide(page).endsWith('/docs/de/benutzerhandbuch.md') && nativeLinks(page).join() === 'English,Français', 'the guide button leads to the German guide');
await page.select('en');
assert(page.byId('language').value === 'en' && page.document.documentElement.lang === 'en' && textsIn(page, 'en') && picturesIn(page, 'en'),
  'choosing English again brings back every word and picture in English');
assert(page.urls.at(-1) === '/Phone-OS/#try', 'and the address loses the language: English is the default');
assert(demoLink(page) === 'demo/index.html', 'and so does the link to the full demo');
page = await load('?lang=de');
await page.select('en');
assert(page.document.documentElement.lang === 'en' && page.byId('picture').getAttribute('src') === 'demo/en/n0.png' && page.location.search === '',
  'a page opened in German can be set to English');

// ---- the keys and the taps ------------------------------------------------------------------------------------------
page = await load('?lang=de');
const key = (name) => page.all().find((e) => e.getAttribute('data-i18n-aria-label') === 'site.key.' + name);
assert(key('select').disabled === false && key('back').disabled === true, 'only the keys that lead somewhere are enabled');
key('select').click();
assert(page.byId('picture').getAttribute('src') === 'demo/de/n1.png' && page.byId('status').textContent === 'de:de:site.screen.compose 2/3', 'a key follows its edge');
await page.select('fr');
assert(page.byId('picture').getAttribute('src') === 'demo/fr/n1.png', 'the language changes in the middle of the walk and the picture stays');
page.byId('start').click();
const tap = page.byId('taps').children[0];
assert(tap && tap.getAttribute('aria-label') === 'fr:tap fr:site.screen.sos', 'a touch target is named in the language in use');
tap.click();
assert(page.byId('picture').getAttribute('src') === 'demo/fr/n2.png', 'a tap follows its edge');

// ---- without the pictures ------------------------------------------------------------------------------------------
page = await load('', { withGraph: false });
assert(page.byId('status').textContent === 'en:site.device.missing', 'a page without the demo says so');
await page.select('de');
assert(page.byId('status').textContent === 'de:site.device.missing', 'in the language in use');

console.log(`\n${pass} checks: ${pass} passed, ${fail} failed.`);
process.exit(fail ? 1 : 0);

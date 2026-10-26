/**
 * Tests for the demo page (sim/demo/page.html): it has no words of its own, and its behaviour — the
 * keys that follow the graph, the language selector that starts the device in another language, the
 * language a picture is in, the screen that ends by itself, the scenes — is run against a small fake
 * of the parts of the DOM it uses.
 *
 *   node test/test_demo_page/test.mjs
 */
import { readFileSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';
import vm from 'vm';

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '..', '..');
const html = readFileSync(resolve(ROOT, 'sim', 'demo', 'page.html'), 'utf8');

let pass = 0, fail = 0;
const assert = (cond, msg) => { if (cond) { pass++; console.log('  PASS:', msg); } else { fail++; console.error('  FAIL:', msg); } };

// ---- no words of its own ---------------------------------------------------------------------------------
const visible = html.replace(/<script[\s\S]*?<\/script>/g, '').replace(/<style[\s\S]*?<\/style>/g, '').replace(/<[^>]*>/g, '').trim();
assert(visible === '', 'the markup holds no text: every word comes from the catalogs');
const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];
// A sentence has spaces in it; the script's strings are keys, file names and DOM names.
const strings = [...script.matchAll(/"((?:[^"\\]|\\.)*)"/g)].map((m) => m[1]).filter((t) => /\s/.test(t) && t !== 'use strict');
assert(strings.length === 0, 'the script has no sentence in it' + (strings.length ? ': ' + strings.join(' | ') : ''));

// ---- a fake DOM for what the page uses ------------------------------------------------------------------------
class Element {
  constructor(tag) { this.tag = tag; this.children = []; this.listeners = {}; this.hidden = false; this.disabled = false; this._text = ''; this.value = ''; this.attrs = {}; }
  set textContent(v) { this._text = String(v); this.children = []; }
  get textContent() { return this._text; }
  appendChild(c) { this.children.push(c); return c; }
  addEventListener(type, fn) { this.listeners[type] = fn; }
  click() { if (!this.disabled && this.listeners.click) this.listeners.click(); }
  set src(v) { this.attrs.src = v; } get src() { return this.attrs.src; }
  set className(v) { this.attrs.className = v; } get className() { return this.attrs.className; }
  get clientWidth() { return 480; } get clientHeight() { return 640; }   // the picture at twice its size
  set alt(v) { this.attrs.alt = v; } get alt() { return this.attrs.alt; }
}

function load(navigatorLanguage = 'en', search = '', languages = null) {
  const data = {
    // One graph over every language: each language has its own start, and a state is in the language of its picture
    // (n4 is the German phone page, from which the Language screen's choice leads to the English one, n1).
    graph: {
      starts: { en: 0, de: 3 }, complete: true,
      nodes: [{ id: 0, screen: 'home', lang: 'en', depth: 0 }, { id: 1, screen: 'phone', lang: 'en', depth: 1 },
              { id: 2, screen: 'call', lang: 'en', depth: 2 }, { id: 3, screen: 'home', lang: 'de', depth: 0 },
              { id: 4, screen: 'phone', lang: 'de', depth: 1 }, { id: 5, screen: 'selftest', lang: 'en', depth: 2 },
              { id: 6, screen: 'selftest', lang: 'en', depth: 3 }],
      edges: [{ from: 0, key: 'select', to: 1 }, { from: 1, key: 'back', to: 0 }, { from: 1, key: 'select', to: 2 },
              { from: 0, tap: { x: 160, y: 48, w: 74, h: 69 }, to: 1 }, { from: 2, key: 'back', to: 1 },
              { from: 3, key: 'select', to: 4 }, { from: 4, key: 'back', to: 3 }, { from: 4, key: 'up', to: 1 },
              { from: 1, key: 'down', to: 5 }, { from: 5, key: 'up', to: 6 }, { from: 6, wait: 7, to: 5 }],
    },
    scenes: ['lock'],
    languages: languages || [{ code: 'en', native: 'English' }, { code: 'de', native: 'Deutsch' }],
    words: {
      en: { 'demo.title': 'T-en', 'demo.state': 'Picture {n} of {total}', 'demo.key-select': 'Enter', 'demo.key-back': 'Back', 'demo.key-up': 'Up', 'demo.key-down': 'Down', 'demo.scene-lock': 'Locked' },
      de: { 'demo.title': 'T-de', 'demo.state': 'Bild {n} von {total}', 'demo.key-select': 'Eingabe', 'demo.key-back': 'Zurück', 'demo.key-up': 'Hoch', 'demo.key-down': 'Runter', 'demo.scene-lock': 'Gesperrt' },
    },
  };
  const elements = {};
  const timers = [];
  const document = {
    documentElement: { lang: '' }, title: '',
    getElementById: (id) => (elements[id] ||= Object.assign(new Element('x'), id === 'data' ? { _text: JSON.stringify(data) } : {})),
    createElement: (tag) => new Element(tag),
  };
  // A clock the test turns by hand: the page asks for a time to pass, and the test lets it pass or not.
  const setTimeout = (fn, ms) => { timers.push({ fn, ms, live: true }); return timers.length; };
  const clearTimeout = (id) => { if (timers[id - 1]) timers[id - 1].live = false; };
  vm.runInNewContext(script, { document, navigator: { language: navigatorLanguage }, location: { search }, URLSearchParams, JSON, console, setTimeout, clearTimeout });
  const live = () => timers.filter((t) => t.live);
  return { document, el: (id) => document.getElementById(id), data, live };
}

const keyButton = (page, word) => page.el('keys').children.find((b) => b.textContent === word);

// ---- the language ----------------------------------------------------------------------------------------------
let page = load('en');
assert(page.el('title').textContent === 'T-en' && page.document.documentElement.lang === 'en', 'it starts in the language of the browser when it has it');
assert(page.el('picture').src === 'en/n0.png', 'the first picture is the start state of that language');
assert(page.el('status').textContent === 'Picture 1 of 7', 'the status counts the states');
page = load('fr');
assert(page.el('title').textContent === 'T-en', 'a language it does not have falls back to English');
page = load('fr', '', [{ code: 'de', native: 'Deutsch' }, { code: 'en', native: 'English' }]);
assert(page.el('title').textContent === 'T-en', 'English is the fallback whatever the order of the catalogs');
page = load('fr', '', [{ code: 'de', native: 'Deutsch' }]);
assert(page.el('title').textContent === 'T-de' && page.el('picture').src === 'de/n3.png', 'without English the first catalog is the fallback, and starts where its language does');
page = load('en', '?lang=de');
assert(page.el('title').textContent === 'T-de' && page.el('language').value === 'de', 'the address chooses the language before the browser');
assert(page.el('picture').src === 'de/n3.png', 'and the device starts in it: the picture is the German start state');
page = load('en', '?lang=en');
assert(page.el('title').textContent === 'T-en' && page.el('picture').src === 'en/n0.png', 'English named in the address is English even where the browser says otherwise');
page = load('de', '?lang=en');
assert(page.el('title').textContent === 'T-en', 'the address wins over a German browser (the link from the English page names its language)');
page = load('en', '?lang=xx');
assert(page.el('title').textContent === 'T-en', 'an unknown language in the address is ignored');
page = load('de');
assert(page.el('language').value === 'de' && page.el('title').textContent === 'T-de', 'the selector shows the language in use');
page.el('language').value = 'en';
page.el('language').listeners.change();
assert(page.el('picture').src === 'en/n0.png' && page.el('title').textContent === 'T-en', 'changing the language starts the device in it: its words and its start picture');

// ---- the keys ------------------------------------------------------------------------------------------------------
page = load('en');
const enabled = () => page.el('keys').children.filter((b) => !b.disabled).map((b) => b.textContent);
assert(JSON.stringify(enabled()) === JSON.stringify(['Enter']), 'only the keys that lead somewhere are enabled');
keyButton(page, 'Enter').click();
assert(page.el('picture').src === 'en/n1.png', 'a key follows its edge');
keyButton(page, 'Enter').click();
assert(page.el('picture').src === 'en/n2.png', 'to the call page');
assert(enabled().join() === 'Back', 'where Hang up (the Back key) is the way on');
keyButton(page, 'Back').click();
assert(page.el('picture').src === 'en/n1.png', 'and it leaves the call page');
page.el('start').click();
assert(page.el('picture').src === 'en/n0.png', 'start over goes to the start of the language');

// ---- the language of a picture is the language of its state -----------------------------------------------------------
page = load('de');
keyButton(page, 'Eingabe').click();
assert(page.el('picture').src === 'de/n4.png', 'in German the walk goes through German pictures');
keyButton(page, 'Hoch').click();
assert(page.el('picture').src === 'en/n1.png' && page.el('title').textContent === 'T-de',
  'choosing English on the device\'s Language screen shows English pictures, and the page keeps its words');
page.el('start').click();
assert(page.el('picture').src === 'de/n3.png', 'start over goes back to the start of the page\'s language');

// ---- a screen that ends by itself ------------------------------------------------------------------------------------------
page = load('en');
assert(page.live().length === 0, 'a state that does not end by itself starts no clock');
keyButton(page, 'Enter').click();
keyButton(page, 'Down').click();
keyButton(page, 'Up').click();
assert(page.el('picture').src === 'en/n6.png' && page.live().length === 1 && page.live()[0].ms === 7000, 'the key test asks for seven seconds');
page.live()[0].fn();
assert(page.el('picture').src === 'en/n5.png' && page.live().length === 0, 'and when they have passed it ends, as on the device');
keyButton(page, 'Up').click();
page.el('start').click();
assert(page.live().length === 0 && page.el('picture').src === 'en/n0.png', 'leaving the screen stops the clock');

// ---- the scenes ----------------------------------------------------------------------------------------------------------
page = load('de');
page.el('scenes').children[0].children[0].click();
assert(page.el('picture').src === 'de/scene-lock.png' && page.el('status').textContent === 'Gesperrt', 'a scene shows its picture and its name');
assert(page.el('walk').hidden === false && enabled().length === 0, 'during a scene there is a way back and no keys');
page.el('walk').click();
assert(page.el('picture').src === 'de/n3.png' && page.el('walk').hidden === true, 'back to the walk');

// ---- the taps ------------------------------------------------------------------------------------------------------
page = load('en');
const picture = page.el('picture');
assert(picture.className === 'touch', 'a state with a tap target says so on the picture');
picture.listeners.click({ offsetX: 10, offsetY: 10 });
assert(picture.src === 'en/n0.png', 'a click beside every target does nothing');
picture.listeners.click({ offsetX: 2 * 190, offsetY: 2 * 80 });   // inside the target, at twice the size
assert(picture.src === 'en/n1.png', 'a click on a target follows its edge');
assert(picture.className === '', 'a state without tap targets does not invite a tap');
page.el('start').click();

console.log(`\n${pass} checks: ${pass} passed, ${fail} failed.`);
process.exit(fail ? 1 : 0);

/**
 * Tests for the demo page (sim/demo/page.html): it has no words of its own, and its behaviour — the
 * keys that follow the graph, the language selector that swaps every picture and word, the scenes —
 * is run against a small fake of the parts of the DOM it uses.
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
  set alt(v) { this.attrs.alt = v; } get alt() { return this.attrs.alt; }
}

function load(navigatorLanguage = 'en') {
  const data = {
    graph: {
      start: 0, trimmed: 7,
      nodes: [{ id: 0, screen: 'home', depth: 0 }, { id: 1, screen: 'phone', depth: 1 }, { id: 2, screen: 'call', depth: 2 }],
      edges: [{ from: 0, key: 'select', to: 1 }, { from: 1, key: 'back', to: 0 }, { from: 1, key: 'select', to: 2 }],
    },
    scenes: ['lock'],
    languages: [{ code: 'en', native: 'English' }, { code: 'de', native: 'Deutsch' }],
    words: {
      en: { 'demo.title': 'T-en', 'demo.state': 'Picture {n} of {total}', 'demo.key-select': 'Enter', 'demo.scene-lock': 'Locked', 'demo.trimmed': '{n} more', 'demo.call-note': 'call' },
      de: { 'demo.title': 'T-de', 'demo.state': 'Bild {n} von {total}', 'demo.key-select': 'Eingabe', 'demo.scene-lock': 'Gesperrt', 'demo.trimmed': '{n} weitere', 'demo.call-note': 'ruf' },
    },
  };
  const elements = {};
  const document = {
    documentElement: { lang: '' }, title: '',
    getElementById: (id) => (elements[id] ||= Object.assign(new Element('x'), id === 'data' ? { _text: JSON.stringify(data) } : {})),
    createElement: (tag) => new Element(tag),
  };
  vm.runInNewContext(script, { document, navigator: { language: navigatorLanguage }, JSON, console });
  return { document, el: (id) => document.getElementById(id), data };
}

const keyButton = (page, word) => page.el('keys').children.find((b) => b.textContent === word);

// ---- the language ----------------------------------------------------------------------------------------------
let page = load('en');
assert(page.el('title').textContent === 'T-en' && page.document.documentElement.lang === 'en', 'it starts in the language of the browser when it has it');
assert(page.el('picture').src === 'en/n0.png', 'the first picture is the start state in that language');
assert(page.el('status').textContent === 'Picture 1 of 3', 'the status counts the states');
assert(page.el('trimmed').textContent === '7 more', 'it says how many variants are left out');
page = load('fr');
assert(page.el('title').textContent === 'T-en', 'a language it does not have falls back to the first');
page = load('de');
assert(page.el('language').value === 'de' && page.el('title').textContent === 'T-de', 'the selector shows the language in use');
page.el('language').value = 'en';
page.el('language').listeners.change();
assert(page.el('picture').src === 'en/n0.png' && page.el('title').textContent === 'T-en', 'changing the language swaps the picture and the words');

// ---- the keys ------------------------------------------------------------------------------------------------------
page = load('en');
const enabled = () => page.el('keys').children.filter((b) => !b.disabled).map((b) => b.textContent);
assert(JSON.stringify(enabled()) === JSON.stringify(['Enter']), 'only the keys that lead somewhere are enabled');
keyButton(page, 'Enter').click();
assert(page.el('picture').src === 'en/n1.png', 'a key follows its edge');
page.el('language').value = 'de';
page.el('language').listeners.change();
assert(page.el('picture').src === 'de/n1.png', 'the language is changed in the middle of the walk and the state stays');
keyButton(page, 'Eingabe').click();
assert(page.el('picture').src === 'de/n2.png' && page.el('callNote').hidden === false, 'the call state says why its keys are quiet');
assert(enabled().length === 0, 'a state with no edges has no enabled keys');
page.el('start').click();
assert(page.el('picture').src === 'de/n0.png' && page.el('callNote').hidden === true, 'start over goes to the start');

// ---- the scenes ----------------------------------------------------------------------------------------------------------
page.el('scenes').children[0].children[0].click();
assert(page.el('picture').src === 'de/scene-lock.png' && page.el('status').textContent === 'Gesperrt', 'a scene shows its picture and its name');
assert(page.el('walk').hidden === false && enabled().length === 0, 'during a scene there is a way back and no keys');
page.el('walk').click();
assert(page.el('picture').src === 'de/n0.png' && page.el('walk').hidden === true, 'back to the walk');

console.log(`\n${pass} checks: ${pass} passed, ${fail} failed.`);
process.exit(fail ? 1 : 0);

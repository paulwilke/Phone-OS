#!/usr/bin/env python3
"""Generates web/index.html from config/form_generated.json and lang/web/en.toml.

    scripts/gen_web.py           # regenerate web/index.html
    scripts/gen_web.py --check   # exit 1 if web/index.html is out of date

Exits with 1 when the output is out of date (--check) or on any error.

The generated page is a single-file SPA: all CSS and JS inline, no external
resources, no libraries.  Every visible string is looked up in the language
catalog at run time (key → value from /api/lang/<code>); no literal English
text appears in the HTML.
"""
import argparse
import json
import os
import sys
import tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# All catalog keys used by the web page (web.* from lang/web/en.toml).
# This list is the authoritative reference for check_i18n.py --usage.
WEB_KEYS = [
    "web.title",
    "web.tab.status",
    "web.tab.basics",
    "web.tab.school-time",
    "web.tab.advanced",
    "web.settings.advanced-note",
    "web.day.mon",
    "web.day.tue",
    "web.day.wed",
    "web.day.thu",
    "web.day.fri",
    "web.day.sat",
    "web.day.sun",
    "web.tab.contacts",
    "web.tab.secrets",
    "web.tab.files",
    "web.tab.log",
    "web.tab.backup",
    "web.status.wifi-mode",
    "web.status.address",
    "web.status.firmware",
    "web.status.time",
    "web.status.mode-off",
    "web.status.mode-ap",
    "web.status.mode-home",
    "web.status.restart",
    "web.status.wifi-off-btn",
    "web.status.wifi-extend-btn",
    "web.status.wifi-left",
    "web.status.minutes",
    "web.status.mode-connecting",
    "web.status.mode-failed",
    "web.form.save",
    "web.form.add",
    "web.form.remove",
    "web.form.saved",
    "web.form.error",
    "web.form.unsaved",
    "web.form.apply-restart",
    "web.form.apply-immediate",
    "web.form.true",
    "web.form.false",
    "web.form.confirm-remove",
    "web.files.path",
    "web.files.size",
    "web.files.delete",
    "web.files.download",
    "web.files.upload",
    "web.files.edit",
    "web.files.save",
    "web.files.cancel",
    "web.files.confirm-delete",
    "web.files.empty",
    "web.files.show-hidden",
    "web.files.remove-mac",
    "web.files.confirm-remove-mac",
    "web.files.removed-mac",
    "web.files.mac-folders-left",
    "web.files.unused-32",
    "web.log.refresh",
    "web.log.empty",
    "web.log.clear",
    "web.log.confirm-clear",
    "web.backup.export",
    "web.backup.import",
    "web.backup.choose",
    "web.backup.restore",
    "web.backup.restored",
    "web.auth.title",
    "web.auth.password",
    "web.auth.enter",
    "web.auth.wrong",
    "web.auth.locked",
    "web.lang.label",
]


def load_schema():
    path = os.path.join(ROOT, "config", "form_generated.json")
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def load_en_toml():
    path = os.path.join(ROOT, "lang", "web", "en.toml")
    with open(path, "rb") as f:
        data = tomllib.load(f)
    strings = data.get("strings", {})
    # flatten nested TOML table into dot-separated keys
    out = {}
    def flatten(d, prefix=""):
        for k, v in d.items():
            full = f"{prefix}{k}"
            if isinstance(v, dict):
                flatten(v, full + ".")
            else:
                out[full] = v
    flatten(strings)
    return out


def js_str(s):
    return "'" + s.replace("\\", "\\\\").replace("'", "\\'") + "'"


def settings_groups(schema):
    """The tabs of settings.toml (config/schema.toml [[group]]). A tab's id is also the id of its panel and its
    form, so it must not be the name of another tab; its words must be keys the page knows."""
    groups = schema["files"]["settings"].get("groups", [])
    fixed = {"status", "contacts", "secrets", "files", "log", "backup"}
    for g in groups:
        if g["id"] in fixed:
            raise ValueError(f"settings group `{g['id']}` has the name of another tab")
        for key in (g["label"], g["note"]):
            if key and key not in WEB_KEYS:
                raise ValueError(f"settings group `{g['id']}`: `{key}` is not in WEB_KEYS")
    return groups


def generate(schema, en_strings):
    form_json = json.dumps(schema, separators=(",", ":"))
    groups = settings_groups(schema)

    # Build inline CSS
    css = """\
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,sans-serif;font-size:15px;background:#f5f5f5;color:#222}
#auth-overlay{display:flex;flex-direction:column;align-items:center;justify-content:center;
  min-height:100vh;gap:1rem;padding:2rem}
#auth-overlay h1{font-size:1.5rem}
#auth-form{display:flex;flex-direction:column;gap:.75rem;width:100%;max-width:320px}
#auth-form input{padding:.5rem;border:1px solid #ccc;border-radius:4px;font-size:1rem}
#auth-form button{padding:.5rem 1rem;background:#0057b7;color:#fff;border:none;
  border-radius:4px;cursor:pointer;font-size:1rem}
#auth-error{color:#c00;font-size:.9rem}
#app{display:none;flex-direction:column;min-height:100vh}
header{background:#0057b7;color:#fff;padding:.75rem 1rem;display:flex;
  align-items:center;gap:1rem}
header h1{font-size:1.1rem;flex:1}
header label{display:flex;align-items:center;gap:.4rem;font-size:.9rem}
header select{padding:.2rem .4rem;border-radius:4px;border:none;font-size:.9rem}
#tabs{display:flex;flex-wrap:wrap;gap:2px;background:#0046a0;padding:0 .5rem}
#tabs button{padding:.5rem .75rem;background:none;border:none;color:#aad0ff;
  cursor:pointer;font-size:.85rem;border-bottom:3px solid transparent}
#tabs button.active{color:#fff;border-bottom-color:#fff}
main{padding:1rem;max-width:860px;width:100%}
.panel{display:none}.panel.active{display:block}
fieldset{border:1px solid #ddd;border-radius:6px;padding:.75rem;margin-bottom:1rem}
legend{font-weight:600;padding:0 .5rem;color:#444}
.field{margin-bottom:.75rem}
.field label{display:block;font-size:.85rem;color:#555;margin-bottom:.2rem}
.field input,.field select,.field textarea{width:100%;padding:.4rem;border:1px solid #ccc;
  border-radius:4px;font-size:.9rem}
.field .hint{font-size:.78rem;color:#777;margin-top:.2rem}
.list-entry{border:1px solid #ddd;border-radius:6px;padding:.75rem;margin-bottom:.5rem;
  position:relative}
.list-entry .remove-btn{position:absolute;top:.4rem;right:.4rem;background:#c00;
  color:#fff;border:none;border-radius:4px;cursor:pointer;font-size:.8rem;padding:.2rem .5rem}
.form-actions{display:flex;align-items:center;gap:.75rem;margin-top:.5rem}
.form-actions button{padding:.4rem .9rem;border:none;border-radius:4px;cursor:pointer;
  font-size:.9rem}
.btn-primary{background:#0057b7;color:#fff}
.btn-secondary{background:#666;color:#fff}
.status-msg{font-size:.85rem;color:#080}
.status-err{font-size:.85rem;color:#c00}
table{width:100%;border-collapse:collapse;margin-bottom:.75rem}
th,td{padding:.4rem .6rem;text-align:left;border-bottom:1px solid #eee;font-size:.85rem}
th{background:#f0f0f0;font-weight:600}
.action-btn{padding:.2rem .5rem;border:none;border-radius:4px;cursor:pointer;
  font-size:.8rem;margin-right:.25rem}
.btn-del{background:#c00;color:#fff}
.btn-edit{background:#0057b7;color:#fff}
.btn-dl{background:#555;color:#fff}
pre{background:#222;color:#eee;padding:1rem;border-radius:6px;overflow:auto;
  font-size:.8rem;max-height:400px;white-space:pre-wrap}
#file-editor textarea{font-family:monospace;font-size:.85rem;min-height:200px;
  resize:vertical}
.dl-list{display:grid;gap:.75rem}
.dl-row{display:flex;gap:.5rem;font-size:.9rem}
.dl-label{color:#555;min-width:140px}
.dl-value{font-weight:500}
.note{font-size:.85rem;color:#555;margin-bottom:.75rem}
.field input[type=checkbox]{width:auto;margin:0 .3rem}
.days{display:flex;flex-wrap:wrap;gap:.3rem .8rem;margin-top:.25rem}
.day{display:inline-flex;align-items:center;gap:.25rem;font-size:.9rem;color:#222}
.entry-row{display:flex;flex-wrap:wrap;gap:.5rem 1rem;align-items:flex-end}
.entry-row .field{margin-bottom:0}
.entry-row .field input[type=time]{width:auto}
.list-entry.compact .field{margin-bottom:.5rem}
fieldset > .hint{font-size:.78rem;color:#777;margin-bottom:.5rem}
.list-entry.compact .remove-btn{position:static;margin-left:auto}
"""

    # Build the JS
    js = f"""\
'use strict';
var SCHEMA = {form_json};
// Every /api/ route, the catalogs included, needs the password, so before the login there is no
// catalog: the login overlay and its messages use these English words, and the real catalog is
// fetched only after a successful login.
var FALLBACK = {{
  'web.title': 'Phone OS',
  'web.auth.title': 'Password',
  'web.auth.password': 'Web password',
  'web.auth.enter': 'Enter',
  'web.auth.wrong': 'Wrong password.',
  'web.auth.locked': 'Too many attempts, wait one minute.',
  'web.form.error': 'Error'
}};
var T = FALLBACK;
var authHeader = '';
var currentLang = '';

function t(key) {{ return T[key] !== undefined ? T[key] : key; }}

function applyTranslations() {{
  document.querySelectorAll('[data-t]').forEach(function(el) {{
    el.textContent = t(el.getAttribute('data-t'));
  }});
  document.title = t('web.title');
}}

function api(method, path, body) {{
  var opts = {{ method: method, headers: {{ Authorization: authHeader }} }};
  if (body !== undefined) {{
    opts.headers['Content-Type'] = 'application/json';
    opts.headers['X-Phone-OS'] = '1';
    opts.body = JSON.stringify(body);
  }}
  return fetch(path, opts);
}}

function showPanel(name) {{
  document.querySelectorAll('.panel').forEach(function(p) {{ p.classList.remove('active'); }});
  document.querySelectorAll('#tabs button').forEach(function(b) {{
    b.classList.toggle('active', b.getAttribute('data-tab') === name);
  }});
  var p = document.getElementById('panel-' + name);
  if (p) {{ p.classList.add('active'); loadPanel(name); }}
}}

// The tabs that show a part of settings.toml: the [[group]] entries of config/schema.toml, a tab per group.
var SETTINGS_TABS = {{}};
(SCHEMA.files.settings.groups || []).forEach(function(g) {{ SETTINGS_TABS[g.id] = true; }});

// force: build the form again even when it holds changes that are not saved (a new language).
function loadPanel(name, force) {{
  if (name === 'status') loadStatus();
  else if (SETTINGS_TABS[name]) loadConfigForm('settings', name, force);
  else if (name === 'contacts') loadConfigForm('contacts', null, force);
  else if (name === 'secrets') loadConfigForm('secrets', null, force);
  else if (name === 'files') loadFiles();
  else if (name === 'log') loadLog();
  else if (name === 'backup') {{}};
}}

// ---------- Status ----------
function loadStatus() {{
  api('GET', '/api/state').then(function(r) {{ return r.json(); }}).then(function(d) {{
    var dl = document.getElementById('status-dl');
    dl.innerHTML = '';
    function row(labelKey, value) {{
      var dt = document.createElement('dt'); dt.className = 'dl-label';
      dt.textContent = t(labelKey);
      var dd = document.createElement('dd'); dd.className = 'dl-value';
      dd.textContent = value;
      dl.appendChild(dt); dl.appendChild(dd);
    }}
    var w = d.wifi || {{}};
    var modeMap = {{ off: t('web.status.mode-off'), access_point: t('web.status.mode-ap'),
                     station: t('web.status.mode-home'), connecting: t('web.status.mode-connecting'),
                     failed: t('web.status.mode-failed') }};
    row('web.status.wifi-mode', modeMap[w.state] || w.state || '');
    row('web.status.address', w.address || '');
    if (w.state && w.state !== 'off')
      row('web.status.wifi-left', t('web.status.minutes').replace('{{n}}', Math.ceil((w.seconds_left || 0) / 60)));
    row('web.status.firmware', d.firmware || '');
    if (d.epoch) {{
      var dt2 = new Date(d.epoch * 1000);
      row('web.status.time', dt2.toLocaleString());
    }}
  }}).catch(function() {{}});
}}

document.getElementById('restart-btn').addEventListener('click', function() {{
  api('POST', '/api/restart', {{}}).then(function() {{ loadStatus(); }});
}});
document.getElementById('wifi-off-btn').addEventListener('click', function() {{
  api('POST', '/api/wifi-off', {{}}).then(function() {{ loadStatus(); }});
}});
document.getElementById('wifi-extend-btn').addEventListener('click', function() {{
  api('POST', '/api/wifi-extend', {{}}).then(function() {{ loadStatus(); }});
}});

// ---------- Config forms ----------
function getNestedValue(obj, path) {{
  if (!path) return obj;
  var parts = path.split('.');
  for (var i = 0; i < parts.length; i++) {{
    if (obj == null || typeof obj !== 'object') return undefined;
    obj = obj[parts[i]];
  }}
  return obj;
}}

function setNestedValue(obj, path, value) {{
  if (!path) return;
  var parts = path.split('.');
  for (var i = 0; i < parts.length - 1; i++) {{
    if (obj[parts[i]] == null || typeof obj[parts[i]] !== 'object') obj[parts[i]] = {{}};
    obj = obj[parts[i]];
  }}
  obj[parts[parts.length - 1]] = value;
}}

// The days of a school-time window as the file writes them ("mon-fri", "fri-mon" across the weekend, "mon,wed",
// "daily"; docs/quiet-hours.md) and the seven boxes of the page. parseDays gives the days, or null for a text the
// page does not understand: that one stays a text field, so that saving never loses it.
var DAYS = ['mon', 'tue', 'wed', 'thu', 'fri', 'sat', 'sun'];
function parseDays(text) {{
  var s = String(text === undefined || text === null ? '' : text).trim().toLowerCase();
  if (s === 'daily') return DAYS.slice();
  var out = [];
  if (!s) return out;
  var parts = s.split(',');
  for (var i = 0; i < parts.length; i++) {{
    var range = parts[i].trim().split('-');
    if (range.length > 2) return null;
    var a = DAYS.indexOf(range[0].trim()), b = DAYS.indexOf(range[range.length - 1].trim());
    if (a < 0 || b < 0) return null;
    for (var d = a; ; d = (d + 1) % 7) {{
      if (out.indexOf(DAYS[d]) < 0) out.push(DAYS[d]);
      if (d === b) break;
    }}
  }}
  return out;
}}
function writeDays(days) {{
  var set = DAYS.filter(function(d) {{ return days.indexOf(d) >= 0; }});
  if (set.length === 7) return 'daily';
  if (set.join(',') === 'mon,tue,wed,thu,fri') return 'mon-fri';
  return set.join(',');
}}

// noHint: the field stands in a compact row (a school-time window); its section says once what it is for.
// The name of a field or a box: its catalog word, or, when the labels did not arrive, its name in the file
// ("timezone"), which a parent can read; a raw catalog key never is.
function label(key, name) {{ return T[key] !== undefined ? T[key] : name; }}

function buildField(key, value, namePrefix, noHint) {{
  var wrap = document.createElement('div'); wrap.className = 'field';
  var days = key.type === 'weekday_set' ? parseDays(value) : null;
  // Seven boxes are not one control: a <label> around them would tick Monday at every click on a day's word.
  var lbl = document.createElement(days ? 'div' : 'label');
  lbl.textContent = label(key.label, key.name);
  wrap.appendChild(lbl);
  var input;
  if (days) {{
    // The owner's wish of 2026-10-08: the days are ticked, not typed.
    input = document.createElement('span'); input.className = 'days';
    input.setAttribute('name', namePrefix + key.name); input.name = namePrefix + key.name;
    input.setAttribute('data-days', '1');
    DAYS.forEach(function(d) {{
      var item = document.createElement('label'); item.className = 'day';
      var box = document.createElement('input'); box.type = 'checkbox'; box.className = 'day-box';
      box.setAttribute('data-day', d);
      box.checked = days.indexOf(d) >= 0;
      var word = document.createElement('span'); word.textContent = t('web.day.' + d);
      item.appendChild(box); item.appendChild(word);
      input.appendChild(item);
    }});
  }} else if (key.type === 'bool' && key.widget === 'checkbox') {{
    // A switch the parent ticks (school-time window "active"). A key that is missing from the file has
    // the schema's default, not "off": a window written before the key existed is on.
    input = document.createElement('input');
    input.name = namePrefix + key.name;
    input.type = 'checkbox';
    input.checked = value === undefined || value === null ? key['default'] === 'true' : value === true || value === 'true';
  }} else if (key.type === 'bool') {{
    input = document.createElement('select');
    input.name = namePrefix + key.name;
    var yes = document.createElement('option'); yes.value = 'true';
    yes.textContent = t('web.form.true');
    var no = document.createElement('option'); no.value = 'false';
    no.textContent = t('web.form.false');
    input.appendChild(yes); input.appendChild(no);
    input.value = (value === true || value === 'true') ? 'true' : 'false';
  }} else if (key.type === 'enum') {{
    input = document.createElement('select');
    input.name = namePrefix + key.name;
    key.enums.split('|').forEach(function(v) {{
      // The parent reads the catalog word; the value that is saved is the raw one.
      var opt = document.createElement('option'); opt.value = v;
      opt.textContent = T[key.option_labels[v]] !== undefined ? T[key.option_labels[v]] : v;
      if (v === value) opt.selected = true;
      input.appendChild(opt);
    }});
  }} else if (key.type === 'catalog_code') {{
    input = document.createElement('select');
    input.name = namePrefix + key.name;
    input.setAttribute('data-catalog-select', '1');
    var cur = document.createElement('option'); cur.value = value || '';
    cur.textContent = value || '';
    input.appendChild(cur);
    api('GET', '/api/languages').then(function(r) {{ return r.json(); }}).then(function(langs) {{
      input.innerHTML = '';
      langs.forEach(function(l) {{
        var opt = document.createElement('option');
        opt.value = l.code; opt.textContent = l.native;
        if (l.code === (value || '')) opt.selected = true;
        input.appendChild(opt);
      }});
    }});
  }} else {{
    input = document.createElement('input');
    input.name = namePrefix + key.name;
    input.type = key.secret ? 'password' : 'text';
    // A list of words is typed as words separated by commas and sent back as an array, never as a string.
    input.value = Array.isArray(value) ? value.join(', ')
      : value !== undefined && value !== null ? String(value) : (key['default'] || '');
    // A time of day gets the browser's time field; a text it would not show ("8:00") stays a text field.
    if (key.type === 'time_of_day' && /^([01][0-9]|2[0-3]):[0-5][0-9]$|^$/.test(input.value)) input.type = 'time';
    if (key.max_val) input.maxLength = key.max_val;
  }}
  lbl.appendChild(input);
  // A hint whose text did not arrive is left out; a raw key is never shown to the parent.
  if (!noHint && key.help && T[key.help] !== undefined) {{
    var hint = document.createElement('div'); hint.className = 'hint';
    hint.textContent = t(key.help);
    wrap.appendChild(hint);
  }}
  return wrap;
}}

function readField(input, keyDef) {{
  if (keyDef.type === 'weekday_set' && input.getAttribute('data-days')) {{
    var on = [];
    input.querySelectorAll('.day-box').forEach(function(b) {{ if (b.checked) on.push(b.getAttribute('data-day')); }});
    return writeDays(on);
  }}
  if (keyDef.type === 'bool' && keyDef.widget === 'checkbox') return !!input.checked;
  if (keyDef.type === 'bool') return input.value === 'true';
  if (keyDef.type === 'integer') return parseInt(input.value, 10) || 0;
  // The schema type is a list of strings: an empty field is [] ("" is the wrong type and the device
  // reports it at every start).
  if (keyDef.type === 'string_list')
    return input.value.split(',').map(function(w) {{ return w.trim(); }}).filter(function(w) {{ return w; }});
  return input.value;
}}

// changed: called when an entry is added or removed, which no input event reports.
function buildSection(section, sectionData, changed) {{
  var fs = document.createElement('fieldset');
  // Two sections can have a key of the same name ([interface] language and [sms] language): the page reads
  // a field back from its own section's box, never the first of that name in the form.
  fs.setAttribute('data-section', section.id);
  var leg = document.createElement('legend');
  leg.textContent = label(section.label, section.id);
  if (section.label) fs.appendChild(leg);
  // A compact section says once what its entries are for, in place of a hint under every field.
  if (compactSection(section) && section.help && T[section.help] !== undefined) {{
    var about = document.createElement('div'); about.className = 'hint';
    about.textContent = t(section.help);
    fs.appendChild(about);
  }}
  if (section.list) {{
    var entries = Array.isArray(sectionData) ? sectionData : [];
    // The device uses the schema's entry when the card has none; the page shows the same one, so that
    // saving writes it to the card (the school-time window, switched off).
    if (entries.length === 0 && section.default_entry) entries = [section.default_entry];
    var container = document.createElement('div');
    container.setAttribute('data-list-section', section.id);
    entries.forEach(function(entry, idx) {{
      container.appendChild(buildListEntry(section, entry, idx, changed));
    }});
    fs.appendChild(container);
    var addBtn = document.createElement('button');
    addBtn.type = 'button'; addBtn.className = 'action-btn btn-edit';
    addBtn.textContent = t('web.form.add');
    addBtn.addEventListener('click', function() {{
      var count = container.querySelectorAll('.list-entry').length;
      container.appendChild(buildListEntry(section, {{}}, count, changed));
      changed();
    }});
    fs.appendChild(addBtn);
  }} else {{
    section.keys.forEach(function(key) {{
      var val = sectionData ? sectionData[key.name] : undefined;
      fs.appendChild(buildField(key, val, ''));
    }});
  }}
  return fs;
}}

// A section with a set of weekdays (the school-time windows) is compact: the days in one row, everything else
// (from, to, active, remove) in the row below, and no hint under each field (owner, 2026-10-08: a window took
// a whole screen).
function compactSection(section) {{
  return section.keys.some(function(k) {{ return k.type === 'weekday_set'; }});
}}

function buildListEntry(section, entry, idx, changed) {{
  var wrap = document.createElement('div'); wrap.className = 'list-entry';
  var compact = compactSection(section);
  var row = wrap;
  if (compact) {{
    wrap.className = 'list-entry compact';
    row = document.createElement('div'); row.className = 'entry-row';
  }}
  section.keys.forEach(function(key) {{
    var field = buildField(key, entry[key.name], '', compact);
    (compact && key.type !== 'weekday_set' ? row : wrap).appendChild(field);
  }});
  if (compact) wrap.appendChild(row);
  var rmBtn = document.createElement('button');
  rmBtn.type = 'button'; rmBtn.className = 'remove-btn';
  rmBtn.textContent = t('web.form.remove');
  rmBtn.addEventListener('click', function() {{
    if (confirm(t('web.form.confirm-remove'))) {{ wrap.remove(); changed(); }}
  }});
  row.appendChild(rmBtn);
  return wrap;
}}

function collectFormData(form, fileSections) {{
  var result = {{}};
  fileSections.forEach(function(sec) {{
    if (sec.list) {{
      var container = form.querySelector('[data-list-section="' + sec.id + '"]');
      if (!container) return;
      var entries = [];
      container.querySelectorAll('.list-entry').forEach(function(entry) {{
        var obj = {{}};
        sec.keys.forEach(function(k) {{
          var inp = entry.querySelector('[name="' + k.name + '"]');
          if (inp) obj[k.name] = readField(inp, k);
        }});
        entries.push(obj);
      }});
      setNestedValue(result, sec.id, entries);
    }} else {{
      var val = {{}};
      var box = form.querySelector('[data-section="' + sec.id + '"]');
      if (!box) return;
      sec.keys.forEach(function(k) {{
        var inp = box.querySelector('[name="' + k.name + '"]');
        if (inp) val[k.name] = readField(inp, k);
      }});
      if (sec.id) setNestedValue(result, sec.id, val);
      else Object.assign(result, val);
    }}
  }});
  return result;
}}

// group: the tab of settings.toml to show (null: the whole file). A form with changes that are not saved is
// left as it is when the parent comes back to its tab, so that looking at another tab loses nothing.
function loadConfigForm(fileId, group, force) {{
  var formEl = document.getElementById('form-' + (group || fileId));
  if (!formEl) return;
  if (!force && formEl.getAttribute('data-unsaved') === '1') return;
  formEl.innerHTML = '';
  formEl.setAttribute('data-unsaved', '');
  var fileDef = SCHEMA.files[fileId];
  if (!fileDef) return;
  var sections = fileDef.sections.filter(function(sec) {{ return !group || sec.group === group; }});
  api('GET', '/api/' + fileId).then(function(r) {{ return r.json(); }}).then(function(data) {{
    var statusEl = document.createElement('span');
    function changed() {{
      formEl.setAttribute('data-unsaved', '1');
      statusEl.className = 'status-err'; statusEl.textContent = t('web.form.unsaved');
    }}
    formEl.oninput = changed; formEl.onchange = changed;
    sections.forEach(function(sec) {{
      var secData = sec.id ? getNestedValue(data, sec.id) : data;
      formEl.appendChild(buildSection(sec, secData, changed));
    }});
    var actions = document.createElement('div'); actions.className = 'form-actions';
    var saveBtn = document.createElement('button');
    saveBtn.type = 'button'; saveBtn.className = 'btn-primary';
    saveBtn.textContent = t('web.form.save');
    saveBtn.addEventListener('click', function() {{
      var payload = collectFormData(formEl, sections);
      // The device stores a whole file. A tab holds only some of its sections, so they go into what the file
      // holds at this moment: saving one tab keeps what was saved on the others. Without the file there is
      // nothing to save into, and nothing is sent.
      var whole = !group ? Promise.resolve(payload) : api('GET', '/api/' + fileId).then(function(r) {{
        if (!r.ok) throw new Error('HTTP ' + r.status);
        return r.json();
      }}).then(function(current) {{
        sections.forEach(function(sec) {{ setNestedValue(current, sec.id, getNestedValue(payload, sec.id)); }});
        return current;
      }});
      whole.then(function(body) {{ return api('PUT', '/api/' + fileId, body); }})
        .then(function(r) {{ return r.json(); }})
        .then(function(d) {{
          if (d.ok) {{
            formEl.setAttribute('data-unsaved', '');
            statusEl.className = 'status-msg';
            statusEl.textContent = t('web.form.saved') + (d.restart_required ? ' ' + t('web.form.apply-restart') : ' ' + t('web.form.apply-immediate'));
          }} else {{
            statusEl.className = 'status-err';
            statusEl.textContent = t('web.form.error') + (d.error ? ': ' + d.error : '');
          }}
        }}).catch(function(e) {{
          statusEl.className = 'status-err'; statusEl.textContent = t('web.form.error');
        }});
    }});
    actions.appendChild(saveBtn); actions.appendChild(statusEl);
    formEl.appendChild(actions);
  }}).catch(function() {{}});
}}

// ---------- Files ----------
// The device answers {{"path":"/config","entries":[{{"name","size","dir"}}...]}}: one directory at a time. The
// page keeps the directory it shows; a file's own path is that directory and its name.
var filesDir = '/';
function joinPath(dir, name) {{ return (dir === '/' ? '' : dir) + '/' + name; }}
function parentDir(dir) {{ var i = dir.lastIndexOf('/'); return i <= 0 ? '/' : dir.slice(0, i); }}

// What the parent sees when the device refuses (403 on secrets.toml, 404, 400): the device's own words.
function filesError(msg) {{
  var el = document.getElementById('files-error');
  el.textContent = msg ? t('web.form.error') + ': ' + msg : '';
  el.hidden = !msg;
}}
function filesOk(r) {{
  if (r.ok) return r;
  return r.json().catch(function() {{ return {{}}; }}).then(function(d) {{
    throw new Error(d.error || ('HTTP ' + r.status));
  }});
}}
function filesFailed(e) {{ filesError(e && e.message ? e.message : t('web.form.error')); }}

function fileRow(dir, e) {{
  var tr = document.createElement('tr');
  var td1 = document.createElement('td');
  var td2 = document.createElement('td');
  var td3 = document.createElement('td');
  if (e.up) {{
    var upBtn = document.createElement('button'); upBtn.className = 'action-btn btn-up';
    upBtn.textContent = '..';
    upBtn.addEventListener('click', function() {{ loadFiles(parentDir(dir)); }});
    td1.appendChild(upBtn);
  }} else if (e.dir) {{
    var openBtn = document.createElement('button'); openBtn.className = 'action-btn btn-open';
    openBtn.textContent = e.name + '/';
    openBtn.addEventListener('click', function() {{ loadFiles(joinPath(dir, e.name)); }});
    td1.appendChild(openBtn);
  }} else {{
    td1.textContent = e.name;
    if (dir === '/' && UNUSED_32.indexOf(e.name) >= 0) {{
      var note = document.createElement('span'); note.className = 'hint';
      note.textContent = ' ' + t('web.files.unused-32');
      td1.appendChild(note);
    }}
    td2.textContent = e.size;
    var path = joinPath(dir, e.name);
    var dlBtn = document.createElement('button'); dlBtn.className = 'action-btn btn-dl';
    dlBtn.textContent = t('web.files.download');
    dlBtn.addEventListener('click', function() {{
      filesError('');
      api('GET', '/api/file?path=' + encodeURIComponent(path)).then(filesOk)
        .then(function(r) {{ return r.text(); }}).then(function(text) {{
          var a = document.createElement('a');
          a.href = URL.createObjectURL(new Blob([text]));
          a.download = e.name; a.click();
        }}).catch(filesFailed);
    }});
    var editBtn = document.createElement('button'); editBtn.className = 'action-btn btn-edit';
    editBtn.textContent = t('web.files.edit');
    editBtn.addEventListener('click', function() {{
      filesError('');
      api('GET', '/api/file?path=' + encodeURIComponent(path)).then(filesOk)
        .then(function(r) {{ return r.text(); }}).then(function(text) {{
          showEditor(path, text);
        }}).catch(filesFailed);
    }});
    var delBtn = document.createElement('button'); delBtn.className = 'action-btn btn-del';
    delBtn.textContent = t('web.files.delete');
    delBtn.addEventListener('click', function() {{
      if (!confirm(t('web.files.confirm-delete'))) return;
      filesError('');
      api('DELETE', '/api/file?path=' + encodeURIComponent(path), {{}}).then(filesOk)
        .then(function() {{ loadFiles(); }}).catch(filesFailed);
    }});
    td3.appendChild(dlBtn); td3.appendChild(editBtn); td3.appendChild(delBtn);
  }}
  tr.appendChild(td1); tr.appendChild(td2); tr.appendChild(td3);
  return tr;
}}

// What a Mac leaves on a card: AppleDouble files ("._name") and four folders of Spotlight, the file system
// events, the trash and scratch files. Hidden unless "show hidden files" is ticked (owner, 2026-10-08).
// .metadata_never_index is a Mac file too, but it keeps Spotlight away from the card, so it stays and shows.
var MAC_FOLDERS = ['.Spotlight-V100', '.fseventsd', '.Trashes', '.TemporaryItems'];
function macLeftover(e) {{ return (!e.dir && e.name.indexOf('._') === 0) || (e.dir && MAC_FOLDERS.indexOf(e.name) >= 0); }}
// Files of 3.2 that 4.0 neither reads nor writes (docs/upgrading.md); they may be deleted.
var UNUSED_32 = ['energy.log', 'firmware.txt'];

function loadFiles(dir) {{
  if (typeof dir === 'string') filesDir = dir;
  var showHidden = document.getElementById('files-show-hidden').checked;
  var shown = filesDir;
  var tbody = document.getElementById('files-tbody');
  var emptyEl = document.getElementById('files-empty');
  tbody.innerHTML = '';
  emptyEl.hidden = true;
  filesError('');
  document.getElementById('files-path').textContent = shown;
  api('GET', '/api/files?path=' + encodeURIComponent(shown)).then(filesOk)
    .then(function(r) {{ return r.json(); }}).then(function(d) {{
      var entries = (d.entries || []).filter(function(e) {{ return showHidden || !macLeftover(e); }});
      emptyEl.hidden = entries.length > 0;
      if (shown !== '/') tbody.appendChild(fileRow(shown, {{ up: true }}));
      entries.forEach(function(e) {{ tbody.appendChild(fileRow(shown, e)); }});
    }}).catch(filesFailed);
  var uploadInput = document.getElementById('files-upload-input');
  uploadInput.onchange = function() {{
    if (!uploadInput.files.length) return;
    var file = uploadInput.files[0];
    var reader = new FileReader();
    reader.onload = function(ev) {{
      var content = ev.target.result;
      var path = joinPath(filesDir, file.name);
      var opts = {{ method: 'PUT', headers: {{ Authorization: authHeader,
          'Content-Type': 'text/plain', 'X-Phone-OS': '1' }}, body: content }};
      fetch('/api/file?path=' + encodeURIComponent(path), opts).then(filesOk)
        .then(function() {{ loadFiles(); uploadInput.value = ''; }}).catch(filesFailed);
    }};
    reader.readAsText(file);
  }};
}}

// "Remove Mac leftovers": the AppleDouble files ("._name") of the root and of the folders below it, two levels
// deep, through the device's own delete route, which checks every path and logs every file it deletes. Exactly
// those: never .metadata_never_index, nothing else under /config/. The four Mac folders stay: the device has no
// route that deletes a folder, and the page names them so that a computer can remove them.
function removeMacLeftovers() {{
  if (!confirm(t('web.files.confirm-remove-mac'))) return;
  filesError('');
  var files = [], folders = [];
  function walk(dir, depth) {{
    return api('GET', '/api/files?path=' + encodeURIComponent(dir)).then(filesOk)
      .then(function(r) {{ return r.json(); }}).then(function(d) {{
        var below = [];
        (d.entries || []).forEach(function(e) {{
          if (!e.dir && e.name.indexOf('._') === 0) files.push(joinPath(dir, e.name));
          else if (e.dir && MAC_FOLDERS.indexOf(e.name) >= 0) folders.push(joinPath(dir, e.name));
          else if (e.dir && depth < 2) below.push(joinPath(dir, e.name));
        }});
        return below.reduce(function(p, sub) {{ return p.then(function() {{ return walk(sub, depth + 1); }}); }}, Promise.resolve());
      }});
  }}
  walk('/', 0).then(function() {{
    return files.reduce(function(p, path) {{
      return p.then(function() {{ return api('DELETE', '/api/file?path=' + encodeURIComponent(path), {{}}).then(filesOk); }});
    }}, Promise.resolve());
  }}).then(function() {{
    var said = t('web.files.removed-mac').replace('{{n}}', files.length);
    if (folders.length) said += ' ' + t('web.files.mac-folders-left').replace('{{folders}}', folders.join(', '));
    var st = document.getElementById('files-status');
    st.textContent = said; st.hidden = false;
    loadFiles();
  }}).catch(filesFailed);
}}

function showEditor(path, text) {{
  var editorDiv = document.getElementById('file-editor');
  var textArea = document.getElementById('file-editor-text');
  textArea.value = text;
  editorDiv.hidden = false;
  document.getElementById('file-editor-save').onclick = function() {{
    var opts = {{ method: 'PUT', headers: {{ Authorization: authHeader,
        'Content-Type': 'text/plain', 'X-Phone-OS': '1' }},
        body: textArea.value }};
    fetch('/api/file?path=' + encodeURIComponent(path), opts).then(filesOk)
      .then(function() {{ editorDiv.hidden = true; loadFiles(); }}).catch(filesFailed);
  }};
  document.getElementById('file-editor-cancel').onclick = function() {{
    editorDiv.hidden = true;
  }};
}}

// ---------- Log ----------
var LOG_PATH = '/system.log';
function loadLog() {{
  var pre = document.getElementById('log-content');
  var emptyEl = document.getElementById('log-empty');
  api('GET', '/api/file?path=' + LOG_PATH).then(function(r) {{
    // No log file yet is an empty log, not an error. The device sends the end of a long log.
    return r.ok ? r.text() : '';
  }}).then(function(text) {{
    var lines = text.split('\\n').filter(function(l) {{ return l.length > 0; }});
    if (lines.length === 0) {{ pre.textContent = ''; emptyEl.hidden = false; return; }}
    emptyEl.hidden = true;
    pre.textContent = lines.join('\\n');
  }}).catch(function() {{}});
}}

document.getElementById('files-show-hidden').addEventListener('change', function() {{ loadFiles(); }});
document.getElementById('files-remove-mac').addEventListener('click', removeMacLeftovers);
document.getElementById('log-refresh').addEventListener('click', loadLog);
document.getElementById('log-clear').addEventListener('click', function() {{
  if (!confirm(t('web.log.confirm-clear'))) return;
  api('DELETE', '/api/file?path=' + LOG_PATH, {{}}).then(function() {{ loadLog(); }});
}});

// ---------- Backup ----------
// The backup is a ZIP archive of settings.toml and contacts.toml as the device has them (WebApi::backup), saved
// as it comes; a restore sends the file back as it is, and the device checks everything before it writes.
// 4.0 sent its restore to /api/backup, a route the device does not have, so restoring never worked there.
document.getElementById('backup-export').addEventListener('click', function() {{
  var status = document.getElementById('backup-status');
  status.textContent = '';
  fetch('/api/backup', {{ headers: {{ Authorization: authHeader }} }}).then(function(r) {{
    if (!r.ok) throw new Error('HTTP ' + r.status);
    return r.blob();
  }}).then(function(blob) {{
    var a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = 'phone-os-backup.zip'; a.click();
  }}).catch(function(e) {{ status.textContent = t('web.form.error') + ': ' + e.message; }});
}});
document.getElementById('backup-restore').addEventListener('click', function() {{
  var input = document.getElementById('backup-import-input');
  if (!input.files.length) return;
  var status = document.getElementById('backup-status');
  var reader = new FileReader();
  reader.onload = function(e) {{
    var opts = {{ method: 'POST', headers: {{ Authorization: authHeader, 'X-Phone-OS': '1',
        'Content-Type': 'application/zip' }}, body: e.target.result }};
    fetch('/api/restore', opts).then(function(r) {{ return r.json(); }}).then(function(d) {{
      status.textContent = d.ok ? t('web.backup.restored') : (t('web.form.error') + ': ' + (d.error || ''));
    }}).catch(function() {{ status.textContent = t('web.form.error'); }});
  }};
  reader.readAsArrayBuffer(input.files[0]);
}});

// ---------- Language ----------
// An answer that is not ok (401, 404, 429) is an error object, never a catalog; a language the
// device does not have falls back to English.
function loadLang(code) {{
  return api('GET', '/api/lang/' + code).then(function(r) {{
    if (r.status === 404 && code !== 'en') return loadLang('en');
    if (!r.ok) throw new Error('lang ' + r.status);
    return r.json().then(function(d) {{
      // A language comes in three answers, because all of it does not fit one response buffer on the
      // device: the page's words, the labels of the form, the help texts under its fields. Without the
      // labels a field is named by its key in the file (label()), without the help texts it has no hint.
      return api('GET', '/api/lang/' + code + '/labels').then(function(l) {{
        return l.ok ? l.json() : {{}};
      }}).catch(function() {{ return {{}}; }}).then(function(labels) {{
        for (var k in labels) d[k] = labels[k];
        return api('GET', '/api/lang/' + code + '/help').then(function(h) {{
          return h.ok ? h.json() : {{}};
        }}).catch(function() {{ return {{}}; }});
      }}).then(function(help) {{
        for (var k in help) d[k] = help[k];
        T = d; currentLang = code; applyTranslations();
      }});
    }});
  }});
}}

function populateLangSelect(langs) {{
  var sel = document.getElementById('lang-select');
  sel.innerHTML = '';
  langs.forEach(function(l) {{
    var opt = document.createElement('option');
    opt.value = l.code; opt.textContent = l.native;
    if (l.code === currentLang) opt.selected = true;
    sel.appendChild(opt);
  }});
}}

document.getElementById('lang-select').addEventListener('change', function() {{
  var code = this.value;
  loadLang(code).then(function() {{
    localStorage.setItem('pp_lang', code);
    populateLangSelect(window._langs || []);
    var active = document.querySelector('.panel.active');
    if (active) loadPanel(active.id.replace('panel-', ''), true);
  }});
}});

// ---------- Auth ----------
document.getElementById('auth-form').addEventListener('submit', function(e) {{
  e.preventDefault();
  var pw = document.getElementById('auth-pw').value;
  var errEl = document.getElementById('auth-error');
  errEl.hidden = true;
  authHeader = 'Basic ' + btoa(':' + pw);
  api('GET', '/api/state').then(function(r) {{
    if (r.ok) {{
      localStorage.setItem('pp_auth', authHeader);
      document.getElementById('auth-overlay').style.display = 'none';
      document.getElementById('app').style.display = 'flex';
      r.json().catch(function() {{ return {{}}; }}).then(init);
    }} else if (r.status === 429) {{
      errEl.textContent = t('web.auth.locked'); errEl.hidden = false;
    }} else {{
      errEl.textContent = t('web.auth.wrong'); errEl.hidden = false;
    }}
  }}).catch(function() {{
    errEl.textContent = t('web.form.error'); errEl.hidden = false;
  }});
}});

// ---------- Init ----------
// Runs once the password has been accepted, from the login form and from the stored credential,
// with the answer of /api/state. The language is, in this order: the one the parent chose on the page
// (pp_lang), the device's own interface language (the parents read the page in the language the child's
// device speaks), the browser's, English.
function init(state) {{
  var code = localStorage.getItem('pp_lang')
    || (state && state.language)
    || (navigator.language || '').split('-')[0] || 'en';
  loadLang(code).catch(function() {{}}).then(function() {{
    return api('GET', '/api/languages').then(function(r) {{ return r.json(); }});
  }}).then(function(langs) {{
    window._langs = langs;
    populateLangSelect(langs);
    showPanel('status');
  }});
}}

(function() {{
  applyTranslations();
  var storedAuth = localStorage.getItem('pp_auth');
  if (!storedAuth) return;
  authHeader = storedAuth;
  api('GET', '/api/state').then(function(r) {{
    if (r.ok) {{
      document.getElementById('auth-overlay').style.display = 'none';
      document.getElementById('app').style.display = 'flex';
      r.json().catch(function() {{ return {{}}; }}).then(init);
    }} else {{
      localStorage.removeItem('pp_auth');
    }}
  }}).catch(function() {{}});
}})();
"""

    # Build full HTML
    lines = [
        "<!doctype html>",
        '<html lang="en">',
        "<head>",
        '<meta charset="utf-8">',
        '<meta name="viewport" content="width=device-width,initial-scale=1">',
        "<title>Phone OS</title>",
        f"<style>{css}</style>",
        "</head>",
        "<body>",
        "",
        '<div id="auth-overlay">',
        '  <h1 data-t="web.auth.title"></h1>',
        '  <form id="auth-form">',
        '    <label data-t="web.auth.password"></label>',
        '    <input type="password" id="auth-pw" autocomplete="current-password">',
        '    <button type="submit" data-t="web.auth.enter"></button>',
        "  </form>",
        '  <p id="auth-error" hidden></p>',
        "</div>",
        "",
        '<div id="app">',
        "  <header>",
        '    <h1 data-t="web.title"></h1>',
        '    <label>',
        '      <span data-t="web.lang.label"></span>',
        '      <select id="lang-select"></select>',
        "    </label>",
        "  </header>",
        "",
        '  <nav id="tabs">',
        '    <button data-tab="status"   data-t="web.tab.status"></button>',
    ]
    lines += [f'    <button data-tab="{g["id"]}" data-t="{g["label"]}"></button>' for g in groups]
    lines += [
        '    <button data-tab="contacts" data-t="web.tab.contacts"></button>',
        '    <button data-tab="secrets"  data-t="web.tab.secrets"></button>',
        '    <button data-tab="files"    data-t="web.tab.files"></button>',
        '    <button data-tab="log"      data-t="web.tab.log"></button>',
        '    <button data-tab="backup"   data-t="web.tab.backup"></button>',
        "  </nav>",
        "",
        "  <main>",
        "",
        '    <div id="panel-status" class="panel">',
        '      <dl id="status-dl" class="dl-list"></dl>',
        '      <div style="display:flex;gap:.5rem;margin-top:1rem">',
        '        <button id="restart-btn"  class="btn-primary action-btn"',
        '                data-t="web.status.restart"></button>',
        '        <button id="wifi-extend-btn" class="btn-primary action-btn"',
        '                data-t="web.status.wifi-extend-btn"></button>',
        '        <button id="wifi-off-btn" class="btn-secondary action-btn"',
        '                data-t="web.status.wifi-off-btn"></button>',
        "      </div>",
        "    </div>",
        "",
    ]
    for g in groups:
        lines.append(f'    <div id="panel-{g["id"]}" class="panel">')
        if g["note"]:
            lines.append(f'      <p class="note" data-t="{g["note"]}"></p>')
        lines += [f'      <form id="form-{g["id"]}"></form>', "    </div>", ""]
    lines += [
        '    <div id="panel-contacts" class="panel">',
        '      <form id="form-contacts"></form>',
        "    </div>",
        "",
        '    <div id="panel-secrets" class="panel">',
        '      <form id="form-secrets"></form>',
        "    </div>",
        "",
        '    <div id="panel-files" class="panel">',
        '      <p id="files-path"></p>',
        '      <p id="files-error" class="status-err" hidden></p>',
        '      <p id="files-status" class="status-msg" hidden></p>',
        '      <div class="form-actions" style="margin-bottom:.5rem">',
        '        <label><input type="checkbox" id="files-show-hidden"> <span data-t="web.files.show-hidden"></span></label>',
        '        <button id="files-remove-mac" class="btn-secondary action-btn" data-t="web.files.remove-mac"></button>',
        "      </div>",
        "      <table>",
        "        <thead><tr>",
        '          <th data-t="web.files.path"></th>',
        '          <th data-t="web.files.size"></th>',
        "          <th></th>",
        "        </tr></thead>",
        '        <tbody id="files-tbody"></tbody>',
        "      </table>",
        '      <p id="files-empty" data-t="web.files.empty" hidden></p>',
        "      <div>",
        '        <label>',
        '          <span data-t="web.files.upload"></span>',
        '          <input type="file" id="files-upload-input">',
        "        </label>",
        "      </div>",
        '      <div id="file-editor" hidden style="margin-top:1rem">',
        '        <textarea id="file-editor-text" class="field" style="width:100%"></textarea>',
        '        <div class="form-actions">',
        '          <button id="file-editor-save"   class="btn-primary action-btn"',
        '                  data-t="web.files.save"></button>',
        '          <button id="file-editor-cancel" class="btn-secondary action-btn"',
        '                  data-t="web.files.cancel"></button>',
        "        </div>",
        "      </div>",
        "    </div>",
        "",
        '    <div id="panel-log" class="panel">',
        '      <div class="form-actions" style="margin-bottom:.5rem">',
        '        <button id="log-refresh" class="btn-primary action-btn"',
        '                data-t="web.log.refresh"></button>',
        '        <button id="log-clear"   class="btn-secondary action-btn"',
        '                data-t="web.log.clear"></button>',
        "      </div>",
        '      <pre id="log-content"></pre>',
        '      <p id="log-empty" data-t="web.log.empty" hidden></p>',
        "    </div>",
        "",
        '    <div id="panel-backup" class="panel">',
        "      <div class='form-actions'>",
        '        <button id="backup-export" class="btn-primary action-btn"',
        '                data-t="web.backup.export"></button>',
        "      </div>",
        '      <div style="margin-top:1rem">',
        '        <label>',
        '          <span data-t="web.backup.import"></span>',
        '          <input type="file" id="backup-import-input" accept=".zip,.json"',
        '                 style="display:block;margin-top:.3rem">',
        "        </label>",
        '        <button id="backup-restore" class="btn-secondary action-btn"',
        '                style="margin-top:.5rem" data-t="web.backup.restore"></button>',
        '        <p id="backup-status" style="margin-top:.5rem"></p>',
        "      </div>",
        "    </div>",
        "",
        "  </main>",
        "</div>",
        "",
        "<script>",
        "(function() {",
    ]

    # Embed tab switching (needs to be before main JS so buttons exist)
    lines += [
        "document.querySelectorAll('#tabs button').forEach(function(btn) {",
        "  btn.addEventListener('click', function() { showPanel(btn.getAttribute('data-tab')); });",
        "});",
        "",
    ]

    lines += [js, "})();", "</script>", "</body>", "</html>", ""]
    return "\n".join(lines)


def run(check_mode):
    schema = load_schema()
    en_strings = load_en_toml()
    content = generate(schema, en_strings)

    out_path = os.path.join(ROOT, "web", "index.html")
    if check_mode:
        if not os.path.exists(out_path):
            print(f"gen_web --check: web/index.html missing", file=sys.stderr)
            print("Run scripts/gen_web.py to regenerate.", file=sys.stderr)
            return 1
        with open(out_path, encoding="utf-8") as f:
            existing = f.read()
        if existing != content:
            print("gen_web --check: web/index.html out of date", file=sys.stderr)
            print("Run scripts/gen_web.py to regenerate.", file=sys.stderr)
            return 1
        print("gen_web --check: web/index.html up to date")
        return 0
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(content)
    print(f"wrote web/index.html ({len(content)} bytes)")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--check", action="store_true",
                    help="fail if web/index.html is out of date")
    args = ap.parse_args()
    sys.exit(run(args.check))


if __name__ == "__main__":
    main()

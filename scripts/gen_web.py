#!/usr/bin/env python3
"""Generates web/index.html from config/form_generated.json and lang/en.toml.

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

# All catalog keys used by the web page (web.* from lang/en.toml).
# This list is the authoritative reference for check_i18n.py --usage.
WEB_KEYS = [
    "web.title",
    "web.tab.status",
    "web.tab.settings",
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
    path = os.path.join(ROOT, "lang", "en.toml")
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


def generate(schema, en_strings):
    form_json = json.dumps(schema, separators=(",", ":"))

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
"""

    # Build the JS
    js = f"""\
'use strict';
var SCHEMA = {form_json};
var T = {{}};
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
    opts.headers['X-Phoneplus'] = '1';
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

function loadPanel(name) {{
  if (name === 'status') loadStatus();
  else if (name === 'settings') loadConfigForm('settings');
  else if (name === 'contacts') loadConfigForm('contacts');
  else if (name === 'secrets') loadConfigForm('secrets');
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

function buildField(key, value, namePrefix) {{
  var wrap = document.createElement('div'); wrap.className = 'field';
  var lbl = document.createElement('label');
  lbl.textContent = t(key.label);
  wrap.appendChild(lbl);
  var input;
  if (key.type === 'bool') {{
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
      var opt = document.createElement('option'); opt.value = v; opt.textContent = v;
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
    input.value = value !== undefined && value !== null ? String(value) : (key['default'] || '');
    if (key.max_val) input.maxLength = key.max_val;
  }}
  lbl.appendChild(input);
  if (key.help) {{
    var hint = document.createElement('div'); hint.className = 'hint';
    hint.textContent = t(key.help);
    wrap.appendChild(hint);
  }}
  return wrap;
}}

function readField(input, keyDef) {{
  if (keyDef.type === 'bool') return input.value === 'true';
  if (keyDef.type === 'integer') return parseInt(input.value, 10) || 0;
  return input.value;
}}

function buildSection(section, sectionData, fileId) {{
  var fs = document.createElement('fieldset');
  var leg = document.createElement('legend');
  leg.textContent = t(section.label);
  fs.appendChild(leg);
  if (section.list) {{
    var entries = Array.isArray(sectionData) ? sectionData : [];
    var container = document.createElement('div');
    container.setAttribute('data-list-section', section.id);
    entries.forEach(function(entry, idx) {{
      container.appendChild(buildListEntry(section, entry, idx));
    }});
    fs.appendChild(container);
    var addBtn = document.createElement('button');
    addBtn.type = 'button'; addBtn.className = 'action-btn btn-edit';
    addBtn.textContent = t('web.form.add');
    addBtn.addEventListener('click', function() {{
      var count = container.querySelectorAll('.list-entry').length;
      container.appendChild(buildListEntry(section, {{}}, count));
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

function buildListEntry(section, entry, idx) {{
  var wrap = document.createElement('div'); wrap.className = 'list-entry';
  section.keys.forEach(function(key) {{
    wrap.appendChild(buildField(key, entry[key.name], ''));
  }});
  var rmBtn = document.createElement('button');
  rmBtn.type = 'button'; rmBtn.className = 'remove-btn';
  rmBtn.textContent = t('web.form.remove');
  rmBtn.addEventListener('click', function() {{
    if (confirm(t('web.form.confirm-remove'))) wrap.remove();
  }});
  wrap.appendChild(rmBtn);
  return wrap;
}}

function collectFormData(form, fileSections) {{
  var result = {{}};
  var nameToKey = {{}};
  fileSections.forEach(function(sec) {{
    sec.keys.forEach(function(k) {{ nameToKey[k.name] = k; }});
  }});
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
      sec.keys.forEach(function(k) {{
        var inp = form.querySelector('[name="' + k.name + '"]');
        if (inp) val[k.name] = readField(inp, k);
      }});
      if (sec.id) setNestedValue(result, sec.id, val);
      else Object.assign(result, val);
    }}
  }});
  return result;
}}

function loadConfigForm(fileId) {{
  var formEl = document.getElementById('form-' + fileId);
  if (!formEl) return;
  formEl.innerHTML = '';
  var fileDef = SCHEMA.files[fileId];
  if (!fileDef) return;
  api('GET', '/api/' + fileId).then(function(r) {{ return r.json(); }}).then(function(data) {{
    fileDef.sections.forEach(function(sec) {{
      var secData = sec.id ? getNestedValue(data, sec.id) : data;
      formEl.appendChild(buildSection(sec, secData, fileId));
    }});
    var actions = document.createElement('div'); actions.className = 'form-actions';
    var saveBtn = document.createElement('button');
    saveBtn.type = 'button'; saveBtn.className = 'btn-primary';
    saveBtn.textContent = t('web.form.save');
    var statusEl = document.createElement('span');
    saveBtn.addEventListener('click', function() {{
      var payload = collectFormData(formEl, fileDef.sections);
      api('PUT', '/api/' + fileId, payload).then(function(r) {{ return r.json(); }})
        .then(function(d) {{
          if (d.ok) {{
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
function loadFiles() {{
  var tbody = document.getElementById('files-tbody');
  var emptyEl = document.getElementById('files-empty');
  tbody.innerHTML = '';
  api('GET', '/api/files').then(function(r) {{ return r.json(); }}).then(function(files) {{
    emptyEl.hidden = files.length > 0;
    files.forEach(function(f) {{
      var tr = document.createElement('tr');
      var td1 = document.createElement('td'); td1.textContent = f.path; tr.appendChild(td1);
      var td2 = document.createElement('td'); td2.textContent = f.size; tr.appendChild(td2);
      var td3 = document.createElement('td');
      var dlBtn = document.createElement('button'); dlBtn.className = 'action-btn btn-dl';
      dlBtn.textContent = t('web.files.download');
      dlBtn.addEventListener('click', function() {{
        api('GET', '/api/file?path=' + encodeURIComponent(f.path))
          .then(function(r) {{ return r.text(); }}).then(function(text) {{
            var a = document.createElement('a');
            a.href = URL.createObjectURL(new Blob([text]));
            a.download = f.path.split('/').pop(); a.click();
          }});
      }});
      var editBtn = document.createElement('button'); editBtn.className = 'action-btn btn-edit';
      editBtn.textContent = t('web.files.edit');
      editBtn.addEventListener('click', function() {{
        api('GET', '/api/file?path=' + encodeURIComponent(f.path))
          .then(function(r) {{ return r.text(); }}).then(function(text) {{
            showEditor(f.path, text);
          }});
      }});
      var delBtn = document.createElement('button'); delBtn.className = 'action-btn btn-del';
      delBtn.textContent = t('web.files.delete');
      delBtn.addEventListener('click', function() {{
        if (!confirm(t('web.files.confirm-delete'))) return;
        api('DELETE', '/api/file?path=' + encodeURIComponent(f.path), {{}})
          .then(function() {{ loadFiles(); }});
      }});
      td3.appendChild(dlBtn); td3.appendChild(editBtn); td3.appendChild(delBtn);
      tr.appendChild(td3);
      tbody.appendChild(tr);
    }});
  }}).catch(function() {{}});
  var uploadInput = document.getElementById('files-upload-input');
  uploadInput.onchange = function() {{
    if (!uploadInput.files.length) return;
    var file = uploadInput.files[0];
    var reader = new FileReader();
    reader.onload = function(e) {{
      var content = e.target.result;
      var path = '/config/' + file.name;
      var opts = {{ method: 'PUT', headers: {{ Authorization: authHeader,
          'Content-Type': 'text/plain', 'X-Phoneplus': '1' }}, body: content }};
      fetch('/api/file?path=' + encodeURIComponent(path), opts)
        .then(function() {{ loadFiles(); uploadInput.value = ''; }});
    }};
    reader.readAsText(file);
  }};
}}

function showEditor(path, text) {{
  var editorDiv = document.getElementById('file-editor');
  var textArea = document.getElementById('file-editor-text');
  textArea.value = text;
  editorDiv.hidden = false;
  document.getElementById('file-editor-save').onclick = function() {{
    var opts = {{ method: 'PUT', headers: {{ Authorization: authHeader,
        'Content-Type': 'text/plain', 'X-Phoneplus': '1' }},
        body: textArea.value }};
    fetch('/api/file?path=' + encodeURIComponent(path), opts)
      .then(function() {{ editorDiv.hidden = true; loadFiles(); }});
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

document.getElementById('log-refresh').addEventListener('click', loadLog);
document.getElementById('log-clear').addEventListener('click', function() {{
  if (!confirm(t('web.log.confirm-clear'))) return;
  api('DELETE', '/api/file?path=' + LOG_PATH, {{}}).then(function() {{ loadLog(); }});
}});

// ---------- Backup ----------
document.getElementById('backup-export').addEventListener('click', function() {{
  api('GET', '/api/backup').then(function(r) {{ return r.json(); }}).then(function(d) {{
    var a = document.createElement('a');
    a.href = URL.createObjectURL(new Blob([JSON.stringify(d, null, 2)],
      {{ type: 'application/json' }}));
    a.download = 'phoneplus-backup.json'; a.click();
  }});
}});
document.getElementById('backup-restore').addEventListener('click', function() {{
  var input = document.getElementById('backup-import-input');
  if (!input.files.length) return;
  var reader = new FileReader();
  reader.onload = function(e) {{
    try {{
      var data = JSON.parse(e.target.result);
      api('POST', '/api/backup', data).then(function(r) {{ return r.json(); }}).then(function(d) {{
        var el = document.getElementById('backup-status');
        el.textContent = d.ok ? t('web.backup.restored') : (t('web.form.error') + ': ' + (d.error || ''));
      }});
    }} catch(ex) {{
      document.getElementById('backup-status').textContent = t('web.form.error');
    }}
  }};
  reader.readAsText(input.files[0]);
}});

// ---------- Language ----------
function loadLang(code) {{
  return api('GET', '/api/lang/' + code).then(function(r) {{ return r.json(); }})
    .then(function(d) {{ T = d; currentLang = code; applyTranslations(); }});
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
    if (active) loadPanel(active.id.replace('panel-', ''));
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
      init();
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
function init() {{
  api('GET', '/api/languages').then(function(r) {{ return r.json(); }}).then(function(langs) {{
    window._langs = langs;
    populateLangSelect(langs);
    showPanel('status');
  }});
}}

(function() {{
  var storedLang = localStorage.getItem('pp_lang')
    || (navigator.language || '').split('-')[0] || 'en';
  var storedAuth = localStorage.getItem('pp_auth');
  var firstLang = storedLang;
  loadLang(firstLang).then(function() {{
    if (storedAuth) {{
      authHeader = storedAuth;
      api('GET', '/api/state').then(function(r) {{
        if (r.ok) {{
          document.getElementById('auth-overlay').style.display = 'none';
          document.getElementById('app').style.display = 'flex';
          init();
        }} else {{
          localStorage.removeItem('pp_auth');
        }}
      }}).catch(function() {{}});
    }}
  }});
}})();
"""

    # Build full HTML
    lines = [
        "<!doctype html>",
        '<html lang="en">',
        "<head>",
        '<meta charset="utf-8">',
        '<meta name="viewport" content="width=device-width,initial-scale=1">',
        "<title>PhonePlus</title>",
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
        '    <button data-tab="settings" data-t="web.tab.settings"></button>',
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
        '    <div id="panel-settings" class="panel">',
        '      <form id="form-settings"></form>',
        "    </div>",
        "",
        '    <div id="panel-contacts" class="panel">',
        '      <form id="form-contacts"></form>',
        "    </div>",
        "",
        '    <div id="panel-secrets" class="panel">',
        '      <form id="form-secrets"></form>',
        "    </div>",
        "",
        '    <div id="panel-files" class="panel">',
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
        '          <input type="file" id="backup-import-input" accept=".json"',
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

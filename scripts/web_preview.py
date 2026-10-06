#!/usr/bin/env python3
"""Development preview server for the web page.

Serves web/index.html from disk and provides stub API responses based on
the TOML files in config/, so the page can be tested without a real device.

    python3 scripts/web_preview.py [--port 8080]

All config changes are written back to the config/ directory.
The API is read-only for state; mutating calls (PUT, POST, DELETE) modify
config/ files locally.

Password check: reads web_password from config/secrets.toml.  If the file
does not exist or has no web_password, any password is accepted (for easy
first-run testing).
"""
import argparse
import base64
import json
import os
import re
import sys
import tomllib
import tomli_w if False else None  # optional
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import urlparse, parse_qs

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HTML_PATH = os.path.join(ROOT, "web", "index.html")
LANG_DIR  = os.path.join(ROOT, "lang")
WEB_LANG_DIR = os.path.join(LANG_DIR, "web")   # the page's words; the device's are in LANG_DIR
CFG_DIR   = os.path.join(ROOT, "config")

# ── TOML helpers ────────────────────────────────────────────────────────────

def load_toml(path):
    try:
        with open(path, "rb") as f:
            return tomllib.load(f)
    except Exception:
        return {}


def flatten_toml(d, prefix=""):
    out = {}
    for k, v in d.items():
        full = f"{prefix}{k}"
        if isinstance(v, dict):
            out.update(flatten_toml(v, full + "."))
        else:
            out[full] = v if not isinstance(v, list) else v
    return out


# ── Language catalog ────────────────────────────────────────────────────────

def load_catalog(code):
    """The page's catalog; a language without one is empty (English fills in, as on the device)."""
    path = os.path.join(WEB_LANG_DIR, f"{code}.toml")
    data = load_toml(path)
    strings = data.get("strings", {})
    return flatten_toml(strings)


def available_languages():
    langs = []
    for fname in sorted(os.listdir(LANG_DIR)):
        if not fname.endswith(".toml"):
            continue
        data = load_toml(os.path.join(LANG_DIR, fname))
        meta = data.get("meta", {})
        if meta.get("code"):
            langs.append({"code": meta["code"], "name": meta.get("name", ""),
                          "native": meta.get("native", "")})
    return langs


def lang_catalog_json(code):
    en = load_catalog("en")
    target = load_catalog(code) if code != "en" else en
    out = {}
    for k, v in en.items():
        if not (k.startswith("web.") or (k.startswith("config.") and k.endswith(".label"))):
            continue
        val = target.get(k, v)
        out[k] = val if not isinstance(val, list) else val[0]
    return json.dumps(out)


# ── Config conversion (TOML dict → JSON-friendly dict) ──────────────────────

def toml_to_json(d):
    if isinstance(d, dict):
        return {k: toml_to_json(v) for k, v in d.items()}
    if isinstance(d, list):
        return [toml_to_json(i) for i in d]
    if isinstance(d, bool):
        return d
    return d


def json_to_toml_text(data, indent=0):
    """Emit a very small TOML serialiser (enough for config files)."""
    lines = []
    tables_later = []
    arrays_later = []
    for k, v in data.items():
        if isinstance(v, dict):
            tables_later.append((k, v))
        elif isinstance(v, list) and v and isinstance(v[0], dict):
            arrays_later.append((k, v))
        else:
            lines.append(f"{k} = {toml_scalar(v)}")
    if lines:
        lines.append("")
    for k, v in tables_later:
        lines.append(f"[{k}]")
        for sk, sv in v.items():
            if isinstance(sv, dict):
                lines.append(f"[{k}.{sk}]")
                for ssk, ssv in sv.items():
                    lines.append(f"{ssk} = {toml_scalar(ssv)}")
            elif isinstance(sv, list) and sv and isinstance(sv[0], dict):
                for entry in sv:
                    lines.append(f"[[{k}.{sk}]]")
                    for ek, ev in entry.items():
                        lines.append(f"{ek} = {toml_scalar(ev)}")
            else:
                lines.append(f"{sk} = {toml_scalar(sv)}")
        lines.append("")
    for k, v in arrays_later:
        for entry in v:
            lines.append(f"[[{k}]]")
            for ek, ev in entry.items():
                lines.append(f"{ek} = {toml_scalar(ev)}")
            lines.append("")
    return "\n".join(lines)


def toml_scalar(v):
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, int):
        return str(v)
    if isinstance(v, float):
        return str(v)
    if isinstance(v, list):
        return "[" + ", ".join(toml_scalar(i) for i in v) + "]"
    return '"' + str(v).replace("\\", "\\\\").replace('"', '\\"') + '"'


# ── HTTP handler ─────────────────────────────────────────────────────────────

class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        print(f"  {self.address_string()} {fmt % args}")

    def _auth_ok(self):
        secrets = load_toml(os.path.join(CFG_DIR, "secrets.toml"))
        stored = secrets.get("web_password", "")
        if not stored:
            return True  # no password set → open for preview
        hdr = self.headers.get("Authorization", "")
        if not hdr.startswith("Basic "):
            return False
        try:
            decoded = base64.b64decode(hdr[6:]).decode("utf-8", errors="replace")
            _, _, pw = decoded.partition(":")
            return pw == stored
        except Exception:
            return False

    def _send(self, code, ctype, body):
        if isinstance(body, str):
            body = body.encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-cache")
        self.end_headers()
        self.wfile.write(body)

    def _json(self, code, obj):
        self._send(code, "application/json; charset=utf-8", json.dumps(obj))

    def _read_body(self):
        n = int(self.headers.get("Content-Length", 0))
        return self.rfile.read(n) if n else b""

    def _cfg_path(self, file_id):
        names = {"settings": "settings.toml", "contacts": "contacts.toml",
                 "secrets": "secrets.toml"}
        name = names.get(file_id)
        return os.path.join(CFG_DIR, name) if name else None

    def do_GET(self):
        parsed = urlparse(self.path)
        path = parsed.path
        qs = parse_qs(parsed.query)

        if path == "/" or path == "/index.html":
            if not os.path.exists(HTML_PATH):
                self._send(404, "text/plain", "web/index.html not found — run gen_web.py first")
                return
            with open(HTML_PATH, "rb") as f:
                self._send(200, "text/html; charset=utf-8", f.read())
            return

        if not path.startswith("/api/"):
            self._send(404, "text/plain", "not found")
            return

        if not self._auth_ok():
            self.send_response(401)
            self.send_header("WWW-Authenticate", 'Basic realm="PhonePlus"')
            self.end_headers()
            return

        api = path[5:]  # strip /api/

        if api == "languages":
            self._json(200, available_languages())
        elif api.startswith("lang/"):
            code = api[5:]
            self._send(200, "application/json; charset=utf-8", lang_catalog_json(code))
        elif api == "state":
            secrets = load_toml(os.path.join(CFG_DIR, "secrets.toml"))
            settings = load_toml(os.path.join(CFG_DIR, "settings.toml"))
            self._json(200, {
                "firmware": "preview",
                "wifi_mode": "home",
                "address": "127.0.0.1",
                "epoch_utc": 0,
                "child": settings.get("child", {}).get("name", ""),
            })
        elif api.startswith("config/"):
            file_id = api[7:]
            cfg_path = self._cfg_path(file_id)
            if not cfg_path:
                self._json(404, {"error": "unknown file"})
                return
            data = load_toml(cfg_path)
            self._json(200, toml_to_json(data))
        elif api == "files":
            files = []
            for root, _, names in os.walk(CFG_DIR):
                for n in sorted(names):
                    fp = os.path.join(root, n)
                    rel = "/" + os.path.relpath(fp, os.path.dirname(CFG_DIR)).replace(os.sep, "/")
                    files.append({"path": rel, "size": os.path.getsize(fp)})
            self._json(200, files)
        elif api == "file":
            fpath = qs.get("path", [""])[0]
            abs_path = os.path.join(os.path.dirname(CFG_DIR), fpath.lstrip("/"))
            if not os.path.exists(abs_path):
                self._json(404, {"error": "not found"})
                return
            with open(abs_path, "rb") as f:
                self._send(200, "text/plain; charset=utf-8", f.read())
        elif api == "log":
            log_path = os.path.join(CFG_DIR, "system" ".log")
            if os.path.exists(log_path):
                with open(log_path, encoding="utf-8") as f:
                    lines = [l.rstrip("\n") for l in f if l.strip()]
            else:
                lines = []
            self._json(200, lines)
        elif api == "backup":
            result = {}
            for name in ("settings.toml", "contacts.toml", "secrets.toml"):
                fp = os.path.join(CFG_DIR, name)
                if os.path.exists(fp):
                    with open(fp, encoding="utf-8") as f:
                        result[name] = f.read()
            self._json(200, result)
        else:
            self._json(404, {"error": "not found"})

    def do_PUT(self):
        self._mutate("PUT")

    def do_POST(self):
        self._mutate("POST")

    def do_DELETE(self):
        self._mutate("DELETE")

    def _mutate(self, method):
        parsed = urlparse(self.path)
        path = parsed.path
        qs = parse_qs(parsed.query)

        if not path.startswith("/api/"):
            self._json(404, {"error": "not found"})
            return
        if not self._auth_ok():
            self._json(401, {"error": "Unauthorized"})
            return

        api = path[5:]
        body = self._read_body()

        if api.startswith("config/") and method == "PUT":
            file_id = api[7:]
            cfg_path = self._cfg_path(file_id)
            if not cfg_path:
                self._json(404, {"error": "unknown file"})
                return
            try:
                data = json.loads(body)
                text = json_to_toml_text(data)
                with open(cfg_path, "w", encoding="utf-8") as f:
                    f.write(text)
                self._json(200, {"ok": True, "restart_required": True})
            except Exception as e:
                self._json(400, {"error": str(e)})

        elif api == "file" and method in ("PUT", "DELETE"):
            fpath = qs.get("path", [""])[0]
            abs_path = os.path.join(os.path.dirname(CFG_DIR), fpath.lstrip("/"))
            if method == "DELETE":
                try:
                    os.remove(abs_path)
                    self._json(200, {"ok": True})
                except Exception as e:
                    self._json(404, {"error": str(e)})
            else:
                os.makedirs(os.path.dirname(abs_path), exist_ok=True)
                with open(abs_path, "wb") as f:
                    f.write(body)
                self._json(200, {"ok": True})

        elif api == "log" and method == "DELETE":
            log_path = os.path.join(CFG_DIR, "system" ".log")
            try:
                os.remove(log_path)
            except FileNotFoundError:
                pass
            self._json(200, {"ok": True})

        elif api == "backup" and method == "POST":
            try:
                data = json.loads(body)
                for name, content in data.items():
                    if re.match(r'^[a-z]+\.toml$', name):
                        fp = os.path.join(CFG_DIR, name)
                        with open(fp, "w", encoding="utf-8") as f:
                            f.write(content)
                self._json(200, {"ok": True})
            except Exception as e:
                self._json(400, {"error": str(e)})

        elif api in ("restart", "wifi"):
            self._json(200, {"ok": True})

        else:
            self._json(404, {"error": "not found"})


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", type=int, default=8080)
    args = ap.parse_args()
    addr = ("127.0.0.1", args.port)
    print(f"PhonePlus web preview at http://127.0.0.1:{args.port}/")
    print("Press Ctrl+C to stop.")
    try:
        HTTPServer(addr, Handler).serve_forever()
    except KeyboardInterrupt:
        print("\nStopped.")


if __name__ == "__main__":
    main()

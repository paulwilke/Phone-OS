#!/usr/bin/env python3
"""Generates derived configuration files from config/schema.toml.

    scripts/gen_config.py           # regenerate all outputs
    scripts/gen_config.py --check   # fail if any generated file is out of date
    scripts/gen_config.py --schema <path>  # use a different schema (for tests)
    scripts/gen_config.py --out <dir>      # write outputs to a different directory

Exits with 1 on a schema error or (with --check) when a file is out of date.
"""
import argparse
import json
import os
import sys
import textwrap
import tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

VALID_TYPES = {
    "string", "integer", "bool", "enum", "string_list",
    "weekday_set", "time_of_day", "phone_number", "catalog_code",
}
VALID_APPLY = {"restart", "immediate"}
VALID_FILES = {"settings", "contacts", "secrets"}

# Map schema type name → C++ KeyType enumerator
CPP_TYPE = {
    "string":       "KeyType::String",
    "integer":      "KeyType::Integer",
    "bool":         "KeyType::Bool",
    "enum":         "KeyType::Enum",
    "string_list":  "KeyType::StringList",
    "weekday_set":  "KeyType::WeekdaySet",
    "time_of_day":  "KeyType::TimeOfDay",
    "phone_number": "KeyType::PhoneNumber",
    "catalog_code": "KeyType::CatalogCode",
}

CPP_APPLY = {
    "restart":   "Apply::Restart",
    "immediate": "Apply::Immediate",
}


def load_schema(path):
    with open(path, "rb") as f:
        raw = tomllib.load(f)
    files    = raw.get("file",    [])
    sections = raw.get("section", [])
    keys     = raw.get("key",     [])
    return files, sections, keys


def validate_schema(files, sections, keys):
    errors = []
    file_ids = set()
    for f in files:
        for req in ("id", "path", "example", "label"):
            if not f.get(req):
                errors.append(f"[[file]]: missing required field `{req}` in {f}")
        fid = f.get("id", "")
        if fid in file_ids:
            errors.append(f"[[file]]: duplicate id `{fid}`")
        file_ids.add(fid)

    section_ids = set()
    for s in sections:
        for req in ("file", "name", "list", "label", "field"):
            if req not in s:
                errors.append(f"[[section]] {s.get('name','?')}: missing field `{req}`")
        sid = (s.get("file",""), s.get("name",""))
        if sid in section_ids:
            errors.append(f"[[section]]: duplicate ({sid[0]}, {sid[1]})")
        section_ids.add(sid)
        if s.get("file") not in file_ids:
            errors.append(f"[[section]] {s.get('name','?')}: unknown file `{s.get('file')}`")

    key_ids = set()
    for k in keys:
        for req in ("file", "section", "name", "type", "label", "help", "field", "apply"):
            if req not in k:
                errors.append(f"[[key]] {k.get('file','?')}/{k.get('section','?')}/{k.get('name','?')}: missing `{req}`")
        kid = (k.get("file",""), k.get("section",""), k.get("name",""))
        if kid in key_ids:
            errors.append(f"[[key]]: duplicate {kid}")
        key_ids.add(kid)
        if k.get("file") not in file_ids:
            errors.append(f"[[key]] {k.get('name','?')}: unknown file `{k.get('file')}`")
        t = k.get("type","")
        if t and t not in VALID_TYPES:
            errors.append(f"[[key]] {k.get('name','?')}: unknown type `{t}`")
        a = k.get("apply","")
        if a and a not in VALID_APPLY:
            errors.append(f"[[key]] {k.get('name','?')}: unknown apply `{a}`")
        if t == "enum" and not k.get("enums",""):
            errors.append(f"[[key]] {k.get('name','?')}: enum type requires `enums`")
        if t == "integer":
            if "min_val" not in k and "max_val" not in k:
                errors.append(f"[[key]] {k.get('name','?')}: integer type should have min_val/max_val")

    # A default entry fills a list section when the file has none. Its names must be keys of that
    # section and its values of the key's type, so that a typo cannot reach the device.
    keys_by_section = {}
    for k in keys:
        keys_by_section.setdefault((k.get("file"), k.get("section")), {})[k.get("name")] = k
    for s in sections:
        if "default_entry" not in s:
            continue
        where = f"[[section]] {s.get('name','?')}: default_entry"
        entry = s["default_entry"]
        if not s.get("list"):
            errors.append(f"{where} needs a list section")
        if not isinstance(entry, dict):
            errors.append(f"{where} must be a table")
            continue
        section_keys = keys_by_section.get((s.get("file"), s.get("name")), {})
        for name, value in entry.items():
            k = section_keys.get(name)
            if k is None:
                errors.append(f"{where}: `{name}` is not a key of the section")
            elif k.get("type") == "bool":
                if not isinstance(value, bool):
                    errors.append(f"{where}: `{name}` must be true or false")
            elif k.get("type") == "integer":
                if isinstance(value, bool) or not isinstance(value, int):
                    errors.append(f"{where}: `{name}` must be an integer")
            elif not isinstance(value, str):
                errors.append(f"{where}: `{name}` must be a string")
        for name, k in section_keys.items():
            if k.get("required") and name not in entry:
                errors.append(f"{where}: required key `{name}` is missing")
    for k in keys:
        if "widget" in k and not (k["widget"] == "checkbox" and k.get("type") == "bool"):
            errors.append(f"[[key]] {k.get('name','?')}: `widget` can only be \"checkbox\" on a bool")
    return errors


def _text_value(v):
    """A schema value as the text a default is kept in."""
    if isinstance(v, bool):
        return "true" if v else "false"
    return str(v)


def _esc(s):
    """C string literal escaping."""
    return s.replace("\\", "\\\\").replace('"', '\\"')


def generate_cpp(files, sections, keys):
    """Return the content of src/config/schema_generated.h."""
    file_list = ", ".join(f'"{_esc(f["id"])}"' for f in files)
    lines = [
        "// Generated by scripts/gen_config.py — run scripts/gen_config.py to update.",
        "// Check: python3 scripts/gen_config.py --check",
        "//",
        "// Do not edit this file directly.",
        "",
        "#pragma once",
        "#include <cstddef>",
        "#include <cstdint>",
        "",
        "namespace config {",
        "",
        "enum class KeyType : uint8_t {",
        "    String,      // string; max_val = max_length (0 = unlimited, min_val = minimum length)",
        "    Integer,     // integer; min_val/max_val = inclusive range",
        "    Bool,        // true / false",
        "    Enum,        // string from enums (pipe-separated)",
        "    StringList,  // TOML array of strings",
        "    WeekdaySet,  // \"mon-fri\" etc., parsed by QuietHours",
        "    TimeOfDay,   // \"HH:MM\" (24-hour)",
        "    PhoneNumber, // E.164 phone number",
        "    CatalogCode  // language code of an installed catalog",
        "};",
        "",
        "enum class Apply : uint8_t {",
        "    Restart,   // takes effect on next start",
        "    Immediate  // takes effect without restart",
        "};",
        "",
        "struct KeyDef {",
        "    const char* file;      // \"settings\", \"contacts\", \"secrets\"",
        "    const char* section;   // section path (\"child\", \"wifi.access_point\"); \"\" = root",
        "    const char* name;      // key name",
        "    KeyType     type;",
        "    bool        required;  // must be present in table-list entries",
        "    bool        secret;    // value must never be logged",
        "    Apply       apply;",
        "    const char* dflt;      // default as text; \"\" = no default (required or empty by design)",
        "    int         min_val;   // integer: lower bound; string: minimum length; others: 0",
        "    int         max_val;   // integer: upper bound; string: max byte length (0 = unlimited)",
        "    const char* enums;     // enum: pipe-separated allowed values; others: \"\"",
        "    const char* label;     // i18n catalog key for the field label",
        "    const char* help;      // i18n catalog key for the field help text",
        "    const char* field;     // Profile field path; \"\" = returned separately or sub-struct",
        "};",
        "",
        "struct SectionDef {",
        "    const char* file;",
        "    const char* name;      // section path (e.g. \"quiet_hours\", \"wifi.access_point\")",
        "    bool        list;      // true = [[array of tables]], false = [regular section]",
        "    const char* label;",
        "    const char* help;",
        "    const char* field;     // Profile field path",
        "};",
        "",
        "// One value of the entry a list section gets when the file has none (SectionDef is the list).",
        "struct DefaultEntryValue {",
        "    const char* file;",
        "    const char* section;",
        "    const char* key;",
        "    const char* value;     // as text: \"true\", \"08:00\"",
        "};",
        "",
    ]

    # kKeys array
    lines.append("// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays)")
    lines.append("inline constexpr KeyDef kKeys[] = {")
    prev_section = None
    for k in keys:
        cur = (k["file"], k["section"])
        if cur != prev_section:
            sec_label = f"[{k['section']}]" if k["section"] else "(root)"
            lines.append(f"    // {k['file']} / {sec_label}")
            prev_section = cur
        typ     = CPP_TYPE[k["type"]]
        apply   = CPP_APPLY[k["apply"]]
        req     = "true" if k.get("required") else "false"
        secret  = "true" if k.get("secret") else "false"
        dflt    = _esc(str(k.get("default", "")))
        min_v   = int(k.get("min_val", 0))
        max_v   = int(k.get("max_val", 0))
        enums   = _esc(k.get("enums", ""))
        label   = _esc(k["label"])
        help_   = _esc(k["help"])
        field   = _esc(k.get("field", ""))
        lines.append(
            f'    {{"{_esc(k["file"])}", "{_esc(k["section"])}", "{_esc(k["name"])}", '
            f'{typ}, {req}, {secret}, {apply},\n'
            f'     "{dflt}", {min_v}, {max_v}, "{enums}",\n'
            f'     "{label}", "{help_}", "{field}"}},')
    lines.append("};")
    lines.append("")
    lines.append("// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays)")
    lines.append("inline constexpr SectionDef kSections[] = {")
    for s in sections:
        label  = _esc(s["label"])
        help_  = _esc(s.get("help", ""))
        field  = _esc(s.get("field", ""))
        list_  = "true" if s["list"] else "false"
        lines.append(
            f'    {{"{_esc(s["file"])}", "{_esc(s["name"])}", {list_},\n'
            f'     "{label}", "{help_}", "{field}"}},')
    lines.append("};")
    lines.append("")
    lines.append("// The last element is a sentinel, so that the array is never empty.")
    lines.append("// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays)")
    lines.append("inline constexpr DefaultEntryValue kDefaultEntries[] = {")
    for sd in sections:
        for name, value in sd.get("default_entry", {}).items():
            lines.append(f'    {{"{_esc(sd["file"])}", "{_esc(sd["name"])}", "{_esc(name)}", "{_esc(_text_value(value))}"}},')
    lines.append('    {"", "", "", ""},')
    lines.append("};")
    lines.append("")
    lines.append("inline constexpr size_t kKeyCount     = sizeof(kKeys)     / sizeof(kKeys[0]);")
    lines.append("inline constexpr size_t kDefaultEntryCount = sizeof(kDefaultEntries) / sizeof(kDefaultEntries[0]) - 1;")
    lines.append("inline constexpr size_t kSectionCount = sizeof(kSections) / sizeof(kSections[0]);")
    lines.append("")
    lines.append("} // namespace config")
    lines.append("")
    return "\n".join(lines)


def generate_form_json(files, sections, keys):
    """Return the content of config/form_generated.json."""
    # Build a nested structure: file → section → [keys]
    section_set = {(s["file"], s["name"]): s for s in sections}
    # Collect unique (file, section) pairs in insertion order
    seen = {}
    for k in keys:
        pair = (k["file"], k["section"])
        if pair not in seen:
            seen[pair] = []
        seen[pair].append(k)

    out = {"files": {}}
    for f in files:
        fid = f["id"]
        out["files"][fid] = {"label": f["label"], "sections": []}
        # Group pairs belonging to this file
        sec_groups = {}
        for (ff, sec), ks in seen.items():
            if ff != fid:
                continue
            sec_groups.setdefault(sec, []).extend(ks)

        for sec, ks in sec_groups.items():
            sd = section_set.get((fid, sec))
            sec_obj = {
                "id":    sec,
                # The keys at the top of a file (secrets.toml) have no section and no heading: an empty
                # label, not the catalog key "config..label" that no catalog has.
                "label": sd["label"] if sd else (f"config.{sec}.label" if sec else ""),
                "help":  sd["help"]  if sd else "",
                "list":  sd["list"]  if sd else False,
                "field": sd["field"] if sd else "",
                "keys":  [],
            }
            if sd and "default_entry" in sd:
                sec_obj["default_entry"] = sd["default_entry"]
            for k in ks:
                kobj = {
                    "name":     k["name"],
                    "type":     k["type"],
                    "required": bool(k.get("required")),
                    "secret":   bool(k.get("secret")),
                    "apply":    k["apply"],
                    "default":  k.get("default", ""),
                    "min_val":  k.get("min_val", 0),
                    "max_val":  k.get("max_val", 0),
                    "enums":    k.get("enums", ""),
                    "label":    k["label"],
                    "help":     k["help"],
                    "field":    k.get("field", ""),
                }
                if k["type"] == "enum":
                    # The page shows these words, never the raw value; the saved value stays the raw one.
                    kobj["option_labels"] = {v: enum_option_label_key(k["label"], v)
                                             for v in k["enums"].split("|")}
                if "widget" in k:
                    kobj["widget"] = k["widget"]
                sec_obj["keys"].append(kobj)
            out["files"][fid]["sections"].append(sec_obj)

    return json.dumps(out, indent=2, ensure_ascii=False) + "\n"


def enum_option_label_key(label_key, value):
    """Catalog key of the word for one value of an enum key: `config.contact.role.label` + `parent` is
    `config.contact.role.parent.label`. It ends in `.label` so that it travels with the labels
    (/api/lang/<code>), which the page needs at once; scripts/check_i18n.py derives the same key."""
    stem = label_key[:-len(".label")] if label_key.endswith(".label") else label_key
    return f"{stem}.{value}.label"


def _section_heading(section, file_id):
    """A human-readable heading for a section path."""
    if not section:
        return f"`{file_id}`"
    parts = section.replace("_", "\\_").split(".")
    return "`[" + ".".join(parts) + "]`"


def generate_docs(files, sections, keys):
    """Return the content of docs/configuration-reference.md."""
    section_set = {(s["file"], s["name"]): s for s in sections}
    # Group keys by (file, section)
    by_file_sec = {}
    for k in keys:
        by_file_sec.setdefault((k["file"], k["section"]), []).append(k)

    lines = [
        "<!-- Generated by scripts/gen_config.py — do not edit; run gen_config.py to update. -->",
        "",
        "# Configuration reference",
        "",
        "Every configuration key is listed here. The source of truth is",
        "`config/schema.toml`; this page is generated from it.",
        "",
    ]

    file_map = {f["id"]: f for f in files}
    for f in files:
        fid = f["id"]
        lines.append(f"## `{f['path']}`")
        lines.append("")
        if f.get("secret"):
            lines.append("> Contains secrets — never share this file.")
            lines.append("")

        # Walk sections that belong to this file
        visited = set()
        for k in keys:
            if k["file"] != fid:
                continue
            sec = k["section"]
            if sec in visited:
                continue
            visited.add(sec)

            sd = section_set.get((fid, sec))
            is_list = sd["list"] if sd else False
            if is_list:
                lines.append(f"### `[[{sec}]]`")
            elif sec:
                lines.append(f"### `[{sec}]`")
            else:
                lines.append("### Root keys")
            lines.append("")
            if is_list and "default_entry" in sd:
                shown = ", ".join(f"`{n} = {_example_toml(v)}`" for n, v in sd["default_entry"].items())
                lines.append(f"Without any `[[{sec}]]` entry the device uses one built-in entry: {shown}.")
                lines.append("")

            ks = by_file_sec.get((fid, sec), [])
            # Table header
            lines.append("| Key | Type | Default | Required | Apply |")
            lines.append("|-----|------|---------|----------|-------|")
            for k in ks:
                typ = k["type"]
                if typ == "enum":
                    typ = "enum: " + k.get("enums","")
                dflt = k.get("default","")
                if dflt == "":
                    dflt = "—"
                req  = "yes" if k.get("required") else "no"
                appl = k["apply"]
                lines.append(f"| `{k['name']}` | {typ} | `{dflt}` | {req} | {appl} |")
            lines.append("")

            # Per-key detail
            for k in ks:
                lines.append(f"#### `{k['name']}`")
                lines.append("")
                t = k["type"]
                if t == "string":
                    ml = k.get("max_val", 0)
                    minl = k.get("min_val", 0)
                    if minl and ml:
                        lines.append(f"String, {minl}–{ml} characters.")
                    elif ml:
                        lines.append(f"String, at most {ml} characters.")
                    elif minl:
                        lines.append(f"String, at least {minl} characters.")
                    else:
                        lines.append("String.")
                elif t == "integer":
                    lines.append(f"Integer, {k.get('min_val','')}–{k.get('max_val','')}.")
                elif t == "bool":
                    lines.append("Boolean (`true` or `false`).")
                elif t == "enum":
                    vals = " | ".join(f"`{v}`" for v in k.get("enums","").split("|"))
                    lines.append(f"One of {vals}.")
                elif t == "string_list":
                    lines.append("Array of strings.")
                elif t == "weekday_set":
                    lines.append('Weekday set: `"mon-fri"`, `"daily"`, `"mon,wed,fri"`, etc.')
                elif t == "time_of_day":
                    lines.append('Time of day in 24-hour format: `"HH:MM"`.')
                elif t == "phone_number":
                    lines.append("Phone number in E.164 format (e.g. `+491701111111`).")
                elif t == "catalog_code":
                    lines.append("Language code of an installed catalog (e.g. `en`, `de`, `fr`).")

                dflt = k.get("default","")
                if dflt != "" or k.get("required"):
                    if k.get("required"):
                        lines.append("Required.")
                    else:
                        lines.append(f"Default: `{dflt}`.")

                if k.get("secret"):
                    lines.append("Secret — never logged.")

                appl = k["apply"]
                if appl == "immediate":
                    lines.append("Takes effect immediately without restart.")
                else:
                    lines.append("Takes effect on next start.")

                lines.append("")

    return "\n".join(lines)


def _example_toml(v):
    if isinstance(v, bool):
        return "true" if v else "false"
    return _toml_str(v) if isinstance(v, str) else str(v)


def _toml_str(v):
    return '"' + v.replace("\\", "\\\\").replace('"', '\\"') + '"'


def generate_examples(files, sections, keys):
    """Return a dict: file_id → example file content."""
    section_set = {(s["file"], s["name"]): s for s in sections}
    by_file_sec = {}
    for k in keys:
        by_file_sec.setdefault((k["file"], k["section"]), []).append(k)

    file_map = {f["id"]: f for f in files}
    result = {}
    for f in files:
        fid = f["id"]
        lines = [f"# {f['path']} — example file with defaults"]
        lines.append("# Generated by scripts/gen_config.py — do not edit directly.")
        lines.append("")

        visited = set()
        for k in keys:
            if k["file"] != fid:
                continue
            sec = k["section"]
            if sec in visited:
                continue
            visited.add(sec)

            sd = section_set.get((fid, sec))
            is_list = sd["list"] if sd else False
            ks = by_file_sec.get((fid, sec), [])

            if is_list and sd and "default_entry" in sd:
                # What the device uses when the file has no entry, written out so that it can be edited.
                lines.append(f"[[{sec}]]")
                for k2 in ks:
                    if k2["name"] in sd["default_entry"]:
                        lines.append(f"{k2['name']} = {_example_toml(sd['default_entry'][k2['name']])}")
                lines.append("")
            elif is_list:
                lines.append(f"[[{sec}]]")
                for k2 in ks:
                    dflt = k2.get("default", "")
                    t    = k2["type"]
                    val  = _example_value(t, dflt, k2.get("enums",""))
                    if val is not None:
                        lines.append(f"# {k2['name']} = {val}   # example")
                lines.append("")
            else:
                if sec:
                    lines.append(f"[{sec}]")
                for k2 in ks:
                    dflt = k2.get("default", "")
                    t    = k2["type"]
                    val  = _example_value(t, dflt, k2.get("enums",""))
                    if val is not None:
                        appl = "  # restart" if k2["apply"] == "restart" else "  # immediate"
                        lines.append(f"{k2['name']} = {val}{appl}")
                lines.append("")

        result[fid] = "\n".join(lines) + "\n"
    return result


def _example_value(type_, default, enums):
    """Return a TOML-formatted example value, or None to skip."""
    if type_ == "string" or type_ == "catalog_code":
        return _toml_str(default if default else "")
    if type_ == "integer":
        return default if default else "0"
    if type_ == "bool":
        return default if default else "false"
    if type_ == "enum":
        return _toml_str(default if default else enums.split("|")[0])
    if type_ == "string_list":
        return "[]"
    if type_ == "weekday_set":
        return '"mon-fri"'
    if type_ == "time_of_day":
        return '"08:00"'
    if type_ == "phone_number":
        return _toml_str(default if default else "+491701111111")
    return None


def all_outputs(files, sections, keys, out_dir):
    """Return a dict: relative path → content (str)."""
    examples = generate_examples(files, sections, keys)
    file_map = {f["id"]: f for f in files}
    out = {
        "src/config/schema_generated.h":      generate_cpp(files, sections, keys),
        "config/form_generated.json":          generate_form_json(files, sections, keys),
        "docs/configuration-reference.md":     generate_docs(files, sections, keys),
    }
    for fid, content in examples.items():
        path = file_map[fid]["example"]
        out[path] = content
    return out


def run(schema_path, out_dir, check_mode):
    files, sections, keys = load_schema(schema_path)
    errors = validate_schema(files, sections, keys)
    if errors:
        for e in errors:
            print(f"schema error: {e}", file=sys.stderr)
        return 1

    outputs = all_outputs(files, sections, keys, out_dir)
    stale = []
    for rel_path, content in outputs.items():
        abs_path = os.path.join(out_dir, rel_path)
        if check_mode:
            if not os.path.exists(abs_path):
                stale.append(f"{rel_path}: missing")
                continue
            with open(abs_path, encoding="utf-8") as f:
                existing = f.read()
            if existing != content:
                stale.append(f"{rel_path}: out of date")
        else:
            os.makedirs(os.path.dirname(abs_path), exist_ok=True)
            with open(abs_path, "w", encoding="utf-8") as f:
                f.write(content)
            print(f"wrote {rel_path}")

    if check_mode:
        if stale:
            for s in stale:
                print(f"gen_config --check: {s}", file=sys.stderr)
            print("Run scripts/gen_config.py to regenerate.", file=sys.stderr)
            return 1
        print(f"gen_config --check: {len(outputs)} files up to date")
        return 0
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--check",  action="store_true", help="fail if any output is out of date")
    ap.add_argument("--schema", default=os.path.join(ROOT, "config", "schema.toml"),
                    help="schema file to read (default: config/schema.toml)")
    ap.add_argument("--out",    default=ROOT,
                    help="root directory to write outputs into (default: repo root)")
    args = ap.parse_args()
    sys.exit(run(args.schema, args.out, args.check))


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
# OpenKal - an open source KalOnline client and server.
# Copyright (C) 2026 OpenKal contributors
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

"""Convert KalOnline NPC lisp configs into one JSON file per GenNPC index.

Sources:
  InitNPC.txt   — (npc kind/move) + (gennpc …) spawn records
  message-e.txt — (npcname id "Name")
  External decoded macro file — (npcinfo …) + (npcbone …)

Usage:
  ./convert_npcs.py --all \\
      --init path/InitNPC.txt \\
      --message path/message-e.txt \\
      --macro /path/to/legacy/macro.dat \\
      --out config/npcs

  ./convert_npcs.py --index 1 --init … --message … --macro … --out …
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Tuple


def is_json_number(value: Any) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def dumps_pretty(obj: Any, indent: int = 2) -> str:
    """JSON with indent, but keep homogeneous number arrays on one line."""

    def fmt(o: Any, level: int) -> str:
        pad = " " * (indent * level)
        pad_inner = " " * (indent * (level + 1))
        if isinstance(o, dict):
            if not o:
                return "{}"
            parts = [
                f"{pad_inner}{json.dumps(str(k), ensure_ascii=False)}: {fmt(v, level + 1)}"
                for k, v in o.items()
            ]
            return "{\n" + ",\n".join(parts) + "\n" + pad + "}"
        if isinstance(o, list):
            if not o:
                return "[]"
            if all(is_json_number(x) or isinstance(x, str) for x in o):
                return "[" + ", ".join(json.dumps(x, ensure_ascii=False) for x in o) + "]"
            parts = [pad_inner + fmt(x, level + 1) for x in o]
            return "[\n" + ",\n".join(parts) + "\n" + pad + "]"
        return json.dumps(o, ensure_ascii=False)

    return fmt(obj, 0)


def read_text(path: Path) -> str:
    raw = path.read_bytes()
    for enc in ("utf-8", "utf-8-sig", "cp949", "latin-1"):
        try:
            return raw.decode(enc)
        except UnicodeDecodeError:
            continue
    return raw.decode("latin-1", errors="replace")


def strip_comments(text: str) -> str:
    out: List[str] = []
    for line in text.splitlines():
        in_str = False
        buf: List[str] = []
        i = 0
        while i < len(line):
            ch = line[i]
            if ch == '"':
                in_str = not in_str
                buf.append(ch)
            elif ch == ";" and not in_str:
                break
            else:
                buf.append(ch)
            i += 1
        out.append("".join(buf))
    return "\n".join(out)


def iter_top_forms(text: str, head: str) -> Iterable[str]:
    """Yield balanced s-expressions whose first atom is `head` (e.g. gennpc)."""
    text = strip_comments(text)
    pattern = re.compile(rf"\(\s*{re.escape(head)}(?=[\s\)])")
    i = 0
    n = len(text)
    while True:
        m = pattern.search(text, i)
        if not m:
            return
        start = m.start()
        depth = 0
        j = start
        while j < n:
            ch = text[j]
            if ch == '"':
                j += 1
                while j < n and text[j] != '"':
                    if text[j] == "\\":
                        j += 1
                    j += 1
            elif ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
                if depth == 0:
                    yield text[start : j + 1]
                    i = j + 1
                    break
            j += 1
        else:
            return


def tokenize(expr: str) -> List[str]:
    tokens: List[str] = []
    i = 0
    n = len(expr)
    while i < n:
        ch = expr[i]
        if ch in " \t\r\n":
            i += 1
            continue
        if ch in "()":
            tokens.append(ch)
            i += 1
            continue
        if ch == '"':
            i += 1
            buf: List[str] = []
            while i < n and expr[i] != '"':
                if expr[i] == "\\" and i + 1 < n:
                    buf.append(expr[i + 1])
                    i += 2
                    continue
                buf.append(expr[i])
                i += 1
            tokens.append('"' + "".join(buf) + '"')
            if i < n and expr[i] == '"':
                i += 1
            continue
        j = i
        while j < n and expr[j] not in " \t\r\n()":
            j += 1
        tokens.append(expr[i:j])
        i = j
    return tokens


def parse_atoms(tokens: List[str], i: int = 0) -> Tuple[Any, int]:
    if i >= len(tokens):
        raise ValueError("unexpected end")
    t = tokens[i]
    if t == "(":
        items: List[Any] = []
        i += 1
        while i < len(tokens) and tokens[i] != ")":
            item, i = parse_atoms(tokens, i)
            items.append(item)
        if i >= len(tokens) or tokens[i] != ")":
            raise ValueError("unclosed list")
        return items, i + 1
    if t == ")":
        raise ValueError("unexpected )")
    if t.startswith('"') and t.endswith('"'):
        return t[1:-1], i + 1
    try:
        if "." in t:
            return float(t), i + 1
        return int(t, 10), i + 1
    except ValueError:
        return t, i + 1


def parse_form(expr: str) -> Any:
    return parse_atoms(tokenize(expr))[0]


def fields_from_form(form: List[Any]) -> Dict[str, Any]:
    """(gennpc (index 1) (kind 0) …) → {index:1, kind:0, …}."""
    out: Dict[str, Any] = {}
    if not isinstance(form, list) or not form:
        return out
    for item in form[1:]:
        if not isinstance(item, list) or not item:
            continue
        key = item[0]
        if not isinstance(key, str):
            continue
        vals = item[1:]
        if len(vals) == 0:
            out[key] = True
        elif len(vals) == 1:
            out[key] = vals[0]
        else:
            out[key] = vals
    return out


def parse_repeated_fields(form: List[Any], key: str) -> List[Any]:
    rows: List[Any] = []
    for item in form[1:]:
        if isinstance(item, list) and item and item[0] == key:
            rows.append(item[1:] if len(item) > 2 else (item[1] if len(item) == 2 else []))
    return rows


def sanitize_slug(name: Optional[str]) -> str:
    if not name:
        return ""
    out: List[str] = []
    prev_us = False
    for ch in name.lower():
        if "a" <= ch <= "z" or "0" <= ch <= "9":
            out.append(ch)
            prev_us = False
        else:
            if not prev_us and out:
                out.append("_")
                prev_us = True
    slug = "".join(out).strip("_")
    if len(slug) < 2:
        return ""
    if not any("a" <= c <= "z" for c in slug):
        return ""
    return slug


def index_pad(index: int) -> str:
    return f"{index:04d}"


def gennpc_comment_names(text: str) -> Dict[int, str]:
    """Map GenNPC index → the `; comment` on the line above the record."""
    names: Dict[int, str] = {}
    pending: Optional[str] = None
    for line in text.splitlines():
        stripped = line.strip()
        if not stripped:
            continue
        if stripped.startswith(";"):
            body = stripped.lstrip(";").strip()
            if body.startswith("("):
                pending = None
            elif body:
                pending = body
            continue
        if re.search(r"\(\s*gennpc\b", stripped, re.IGNORECASE) is None:
            pending = None
            continue
        m = re.search(r"\(\s*index\s+(\d+)", stripped)
        if m and pending:
            names[int(m.group(1))] = pending
        pending = None
    return names


def npc_filename(index: int, name: Optional[str]) -> str:
    slug = sanitize_slug(name)
    base = index_pad(index)
    return f"{base}_{slug}.json" if slug else f"{base}.json"


def as_int_list(value: Any) -> Optional[List[Any]]:
    if isinstance(value, list):
        return value
    if isinstance(value, (int, float)) and not isinstance(value, bool):
        return [value]
    return None


def build_appearance(info: Optional[Dict[str, Any]], form: Optional[List[Any]]) -> Optional[Dict[str, Any]]:
    if not info:
        return None
    app: Dict[str, Any] = {}
    if "bone" in info:
        bone = info["bone"]
        if isinstance(bone, list) and bone:
            app["boneId"] = bone[0]
            if len(bone) > 1:
                app["boneExtra"] = bone[1:]
        else:
            app["boneId"] = bone
    if "scale" in info:
        sc = info["scale"]
        if isinstance(sc, list) and sc:
            app["scale"] = sc[0]
            if len(sc) > 1:
                app["scaleParam"] = sc[1]
        else:
            app["scale"] = sc
    if "motion" in info:
        app["motion"] = info["motion"]
    if "mspeed" in info:
        app["mspeed"] = info["mspeed"]
    if "name" in info and info["name"] is not True:
        app["nameFlag"] = info["name"]
    if "fx" in info:
        app["fx"] = info["fx"]
    if info.get("object") is True:
        app["object"] = True
    if "filename" in info:
        app["filename"] = info["filename"]
    if form:
        particles = parse_repeated_fields(form, "particle")
        if particles:
            app["particles"] = particles
    return app or None


def build_bone_entry(fields: Dict[str, Any]) -> Dict[str, Any]:
    bone: Dict[str, Any] = {}
    for k in ("bone", "target", "motion", "part", "damage"):
        if k in fields:
            bone[k] = fields[k]
    return bone


def build_npc_doc(
    fields: Dict[str, Any],
    form: List[Any],
    names: Dict[int, str],
    infos: Dict[int, Dict[str, Any]],
    info_forms: Dict[int, List[Any]],
) -> Optional[Dict[str, Any]]:
    idx = fields.get("index")
    if not isinstance(idx, int) or idx <= 0:
        return None
    display_name = names.get(idx)
    doc: Dict[str, Any] = {"index": idx}
    if display_name:
        doc["name"] = display_name
    doc["nameId"] = idx
    for k in ("country", "kind", "shape", "map"):
        if k in fields and fields[k] is not True:
            doc[k] = fields[k]
    html = fields.get("html")
    if html is not None and html is not True:
        doc["html"] = html
    xy = as_int_list(fields.get("xy"))
    if xy:
        doc["xy"] = xy
    direction = as_int_list(fields.get("dir"))
    if direction:
        doc["dir"] = direction
    quest = fields.get("quest")
    if quest is not None and quest is not True:
        doc["quest"] = quest if isinstance(quest, list) else [quest]
    if "cid" in fields and fields["cid"] is not True:
        doc["cid"] = fields["cid"]
    if "warrelation" in fields and fields["warrelation"] is not True:
        doc["warrelation"] = fields["warrelation"]
    teleports = parse_repeated_fields(form, "teleport")
    if teleports:
        doc["teleports"] = teleports
    sgxy = as_int_list(fields.get("sgxy"))
    if sgxy:
        doc["sgxy"] = sgxy
    sgdir = as_int_list(fields.get("sgdir"))
    if sgdir:
        doc["sgdir"] = sgdir
    scenario = fields.get("scenario")
    if scenario is not None and scenario is not True:
        doc["scenario"] = scenario if isinstance(scenario, list) else [scenario]
    shape = fields.get("shape")
    if isinstance(shape, int):
        app = build_appearance(infos.get(shape), info_forms.get(shape))
        if app:
            doc["appearance"] = app
    return doc


def convert(
    init_path: Path,
    message_path: Path,
    macro_path: Path,
    out_dir: Path,
    only_index: Optional[int],
    overrides_dir: Optional[Path] = None,
) -> int:
    if overrides_dir is not None and overrides_dir.resolve() == out_dir.resolve():
        raise ValueError("NPC overrides and output must be separate directories")
    init_text = read_text(init_path)
    message_text = read_text(message_path)
    macro_text = read_text(macro_path)

    names: Dict[int, str] = {}
    for form in iter_top_forms(message_text, "npcname"):
        parsed = parse_form(form)
        if isinstance(parsed, list) and len(parsed) >= 3 and parsed[0] == "npcname":
            try:
                nid = int(parsed[1])
            except (TypeError, ValueError):
                continue
            name = parsed[2]
            if isinstance(name, str):
                names[nid] = name

    infos: Dict[int, Dict[str, Any]] = {}
    info_forms: Dict[int, List[Any]] = {}
    for form in iter_top_forms(macro_text, "npcinfo"):
        parsed = parse_form(form)
        fields = fields_from_form(parsed)
        key = fields.get("key")
        if isinstance(key, int):
            infos[key] = fields
            info_forms[key] = parsed

    bones: Dict[int, Dict[str, Any]] = {}
    for form in iter_top_forms(macro_text, "npcbone"):
        parsed = parse_form(form)
        fields = fields_from_form(parsed)
        key = fields.get("key")
        if isinstance(key, int):
            bones[key] = build_bone_entry(fields)

    kinds: Dict[int, Dict[str, Any]] = {}
    for form in iter_top_forms(init_text, "npc"):
        parsed = parse_form(form)
        fields = fields_from_form(parsed)
        kind = fields.get("kind")
        if not isinstance(kind, int):
            continue
        entry: Dict[str, Any] = {"kind": kind}
        if "move" in fields and fields["move"] is not True:
            entry["move"] = fields["move"]
        kinds[kind] = entry

    comment_names = gennpc_comment_names(init_text)
    npcs: Dict[int, Dict[str, Any]] = {}
    for form in iter_top_forms(init_text, "gennpc"):
        parsed = parse_form(form)
        fields = fields_from_form(parsed)
        doc = build_npc_doc(fields, parsed, names, infos, info_forms)
        if doc is None:
            continue
        if "name" not in doc:
            fallback = comment_names.get(doc["index"])
            if fallback:
                doc["name"] = fallback
        npcs[doc["index"]] = doc

    # OpenKal service roles, placement and presentation are maintained in JSON,
    # rather than inferred from legacy NPC names, shapes or dialog numbers.
    override_filenames: Dict[int, str] = {}
    if overrides_dir is not None:
        if not overrides_dir.is_dir():
            raise ValueError(f"NPC overrides directory does not exist: {overrides_dir}")
        for path in sorted(overrides_dir.glob("*.json")):
            if path.name in ("manifest.json", "bones.json", "kinds.json"):
                continue
            override = json.loads(path.read_text(encoding="utf-8"))
            if not isinstance(override, dict):
                raise ValueError(f"NPC override must be an object: {path}")
            index = override.get("index")
            if not isinstance(index, int) or isinstance(index, bool) or index <= 0:
                raise ValueError(f"NPC override needs a positive integer index: {path}")
            if index in override_filenames:
                raise ValueError(f"Duplicate NPC override index {index}: {path}")
            override_filenames[index] = path.name
            npcs[index] = {**npcs.get(index, {}), **override}

    if only_index is not None:
        if only_index not in npcs:
            print(f"error: index {only_index} not found in InitNPC", file=sys.stderr)
            return 1
        selected = {only_index: npcs[only_index]}
    else:
        selected = npcs

    out_dir.mkdir(parents=True, exist_ok=True)
    retained_files: List[str] = []
    if only_index is None:
        for stale in out_dir.glob("*.json"):
            stale.unlink()
    else:
        for previous in sorted(out_dir.glob("*.json")):
            if previous.name in ("manifest.json", "bones.json", "kinds.json"):
                continue
            document = json.loads(previous.read_text(encoding="utf-8"))
            if not isinstance(document, dict) or "index" not in document:
                continue
            if document["index"] == only_index:
                previous.unlink()
            else:
                retained_files.append(previous.name)

    needed_bones = set()
    for doc in selected.values():
        app = doc.get("appearance") or {}
        bid = app.get("boneId")
        if isinstance(bid, int):
            needed_bones.add(bid)
    # A single-record update must not discard rigs used by other NPCs.
    needed_bones |= set(bones.keys())

    bones_out = {str(k): bones[k] for k in sorted(needed_bones) if k in bones}
    bones_path = out_dir / "bones.json"
    bones_path.write_text(dumps_pretty(bones_out) + "\n", encoding="utf-8")

    kinds_out = {str(k): kinds[k] for k in sorted(kinds)}
    kinds_path = out_dir / "kinds.json"
    kinds_path.write_text(dumps_pretty(kinds_out) + "\n", encoding="utf-8")

    written = 0
    files = retained_files
    for idx in sorted(selected):
        doc = selected[idx]
        fname = override_filenames.get(idx) or npc_filename(
            idx, doc.get("name") if isinstance(doc.get("name"), str) else None
        )
        path = out_dir / fname
        path.write_text(dumps_pretty(doc) + "\n", encoding="utf-8")
        files.append(fname)
        written += 1

    manifest = {
        "count": len(files),
        "bones": str(bones_path.name),
        "kinds": str(kinds_path.name),
        "files": sorted(files),
    }
    (out_dir / "manifest.json").write_text(dumps_pretty(manifest) + "\n", encoding="utf-8")

    print(f"wrote {written} npc file(s) + bones.json + kinds.json → {out_dir}")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    mode = ap.add_mutually_exclusive_group(required=True)
    mode.add_argument("--all", action="store_true", help="Convert every GenNPC index found")
    mode.add_argument("--index", type=int, help="Convert a single GenNPC index")
    ap.add_argument("--init", type=Path, required=True, help="InitNPC.txt")
    ap.add_argument("--message", type=Path, required=True, help="message-e.txt (npcname)")
    ap.add_argument(
        "--macro",
        type=Path,
        required=True,
        help="External decoded macro file (not bundled with OpenKal)",
    )
    ap.add_argument("--out", type=Path, required=True, help="Output directory for JSON files")
    ap.add_argument(
        "--overrides", type=Path,
        help="Separate directory of maintained OpenKal NPC JSON; fields override legacy data by index",
    )
    args = ap.parse_args()

    for p in (args.init, args.message, args.macro):
        if not p.is_file():
            print(f"error: missing file {p}", file=sys.stderr)
            return 1

    return convert(
        init_path=args.init,
        message_path=args.message,
        macro_path=args.macro,
        out_dir=args.out,
        only_index=None if args.all else args.index,
        overrides_dir=args.overrides,
    )


if __name__ == "__main__":
    sys.exit(main())

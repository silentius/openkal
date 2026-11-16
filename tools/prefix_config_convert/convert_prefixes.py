#!/usr/bin/env python3
# OpenKal - an open source KalOnline client and server.
# Copyright (C) 2026 OpenKal contributors
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

"""Convert KalOnline Prefix.txt + message-e.txt prefixnames into one JSON per prefix.

Layout under --out:
  {index}_{slug}.json

Sources:
  Prefix.txt     — prefix definitions (index)
  message-e.txt  — (prefixname id "Name")

Usage:
  ./convert_prefixes.py --all \\
      --prefix path/Prefix.txt \\
      --message path/message-e.txt \\
      --out config/prefixes
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
            if all(is_json_number(x) for x in o):
                return "[" + ", ".join(json.dumps(x) for x in o) + "]"
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


def sanitize_slug(name: Optional[str]) -> str:
    if not name:
        return ""
    out: List[str] = []
    for ch in name.lower():
        if ("a" <= ch <= "z") or ("0" <= ch <= "9"):
            out.append(ch)
        elif ch in (" ", "-", "_", "'"):
            out.append("_")
    slug = re.sub(r"_+", "_", "".join(out)).strip("_")
    if not slug or not any("a" <= c <= "z" for c in slug):
        return ""
    return slug[:64]


def index_pad(index: int) -> str:
    return f"{index:04d}"


def prefix_filename(index: int, name: Optional[str]) -> str:
    slug = sanitize_slug(name)
    base = index_pad(index)
    return f"{base}_{slug}.json" if slug else f"{base}.json"


def normalize_key(key: str) -> str:
    mapping = {
        "Index": "index",
        "Hth": "hth",
        "Str": "str",
        "Dex": "dex",
        "Int": "int",
        "Wis": "wis",
        "Attack": "attack",
    }
    return mapping.get(key, key[0].lower() + key[1:] if key[:1].isupper() else key)


def list_to_value(vals: List[Any]) -> Any:
    if not vals:
        return True
    if len(vals) == 1:
        return vals[0]
    return vals


def clear_prefix_json_tree(out_dir: Path) -> None:
    if not out_dir.is_dir():
        return
    for p in sorted(out_dir.glob("*.json"), reverse=True):
        p.unlink(missing_ok=True)


def build_prefix_doc(form: List[Any], names: Dict[int, str]) -> Optional[Dict[str, Any]]:
    if not isinstance(form, list) or not form or form[0] != "prefix":
        return None

    raw: Dict[str, Any] = {}
    for item in form[1:]:
        if not isinstance(item, list) or not item:
            continue
        key = item[0]
        if not isinstance(key, str):
            continue
        nk = normalize_key(key)
        vals = item[1:]
        if nk in ("attack", "magic") and len(vals) >= 2 and all(
            isinstance(x, (int, float)) for x in vals[:2]
        ):
            raw[nk] = {"min": int(vals[0]), "max": int(vals[1])}
            continue
        raw[nk] = list_to_value(vals)

    idx = raw.get("index")
    if not isinstance(idx, int) or idx <= 0:
        return None

    display = names.get(idx)
    if isinstance(display, str):
        display = display.strip()
        if not display:
            display = None

    doc: Dict[str, Any] = {"index": idx}
    if display:
        doc["name"] = display

    field_order = (
        "level",
        "sell",
        "str",
        "hth",
        "int",
        "wis",
        "dex",
        "attack",
        "magic",
        "hp",
        "mp",
        "defense",
        "hit",
        "resistfire",
        "resistice",
        "resistlitning",
        "resistcurse",
        "resistpalsy",
    )
    for key in field_order:
        if key in raw:
            doc[key] = raw[key]

    extra = {k: v for k, v in raw.items() if k not in ("index",) and k not in field_order}
    if extra:
        doc["extra"] = extra
    return doc


def convert(
    prefix_path: Path, message_path: Path, out_dir: Path, only_index: Optional[int]
) -> int:
    prefix_text = read_text(prefix_path)
    message_text = read_text(message_path)

    names: Dict[int, str] = {}
    for form in iter_top_forms(message_text, "prefixname"):
        parsed = parse_form(form)
        if isinstance(parsed, list) and len(parsed) >= 3 and parsed[0] == "prefixname":
            try:
                nid = int(parsed[1])
            except (TypeError, ValueError):
                continue
            name = parsed[2]
            if isinstance(name, str):
                names[nid] = name

    prefixes: Dict[int, Dict[str, Any]] = {}
    errors = 0
    for form in iter_top_forms(prefix_text, "prefix"):
        try:
            parsed = parse_form(form)
        except ValueError as e:
            errors += 1
            print(f"warn: parse failed: {e}", file=sys.stderr)
            continue
        if not isinstance(parsed, list):
            continue
        doc = build_prefix_doc(parsed, names)
        if not doc:
            continue
        prefixes[doc["index"]] = doc

    if only_index is not None:
        if only_index not in prefixes:
            print(f"error: index {only_index} not found in Prefix.txt", file=sys.stderr)
            return 1
        selected = {only_index: prefixes[only_index]}
    else:
        selected = prefixes

    out_dir.mkdir(parents=True, exist_ok=True)
    if only_index is None:
        clear_prefix_json_tree(out_dir)

    written = 0
    files: List[str] = []
    for idx in sorted(selected):
        doc = selected[idx]
        rel = prefix_filename(idx, doc.get("name") if isinstance(doc.get("name"), str) else None)
        path = out_dir / rel
        path.write_text(dumps_pretty(doc) + "\n", encoding="utf-8")
        files.append(rel)
        written += 1

    with_name = sum(1 for d in selected.values() if "name" in d)
    manifest = {"count": written, "files": files}
    (out_dir / "manifest.json").write_text(dumps_pretty(manifest) + "\n", encoding="utf-8")
    print(
        f"wrote {written} prefix file(s) → {out_dir}"
        f" ({with_name} with name)"
        + (f" ({errors} parse warnings)" if errors else "")
    )
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    mode = ap.add_mutually_exclusive_group(required=True)
    mode.add_argument("--all", action="store_true", help="Convert every prefix index found")
    mode.add_argument("--index", type=int, help="Convert a single prefix index")
    ap.add_argument("--prefix", type=Path, required=True, help="Prefix.txt")
    ap.add_argument("--message", type=Path, required=True, help="message-e.txt (prefixname)")
    ap.add_argument("--out", type=Path, required=True, help="Output directory for JSON files")
    args = ap.parse_args()

    for p in (args.prefix, args.message):
        if not p.is_file():
            print(f"error: missing file {p}", file=sys.stderr)
            return 1

    return convert(
        prefix_path=args.prefix,
        message_path=args.message,
        out_dir=args.out,
        only_index=None if args.all else args.index,
    )


if __name__ == "__main__":
    sys.exit(main())

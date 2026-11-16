#!/usr/bin/env python3
# OpenKal - an open source KalOnline client and server.
# Copyright (C) 2026 OpenKal contributors
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

"""Convert KalOnline HyperText talk scripts (s######.dat) into dialog JSON.

Extracts frame lines and actionable links (close / quest) for NpcTalkWindow.

Usage:
  ./convert_dialogs.py --all --scripts path/script_pk --out config/dialogs
  ./convert_dialogs.py --id 900002 --scripts path/script_pk --out config/dialogs
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple


def is_json_number(value: Any) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def dumps_pretty(obj: Any, indent: int = 2) -> str:
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
            return raw.decode(enc).rstrip("\x00\r\n")
        except UnicodeDecodeError:
            continue
    return raw.decode("latin-1", errors="replace").rstrip("\x00\r\n")


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
        if ch in ("\"", "'"):
            quote = ch
            i += 1
            buf: List[str] = []
            while i < n and expr[i] != quote:
                if expr[i] == "\\" and i + 1 < n:
                    buf.append(expr[i + 1])
                    i += 2
                    continue
                buf.append(expr[i])
                i += 1
            tokens.append('"' + "".join(buf) + '"')
            if i < n and expr[i] == quote:
                i += 1
            continue
        j = i
        while j < n and expr[j] not in " \t\r\n()\"'":
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


def find_frame(forms: List[Any]) -> Optional[List[Any]]:
    for form in forms:
        if isinstance(form, list) and form and form[0] == "frame":
            return form
    return None


def parse_script_top(text: str) -> List[Any]:
    text = strip_comments(text)
    tokens = tokenize(text)
    forms: List[Any] = []
    i = 0
    while i < len(tokens):
        form, i = parse_atoms(tokens, i)
        forms.append(form)
    return forms


def extract_caption(forms: List[Any]) -> str:
    for form in forms:
        if not isinstance(form, list) or not form or form[0] != "background":
            continue
        for part in form[1:]:
            if not isinstance(part, list) or not part or part[0] != "link":
                continue
            if len(part) >= 2 and part[1] == "system":
                for sub in part[2:]:
                    if isinstance(sub, list) and sub and sub[0] == "param":
                        for p in sub[1:]:
                            if isinstance(p, list) and len(p) >= 2 and p[0] == "caption":
                                return str(p[1])
    return "Talk"


def convert_link(form: List[Any]) -> Optional[Dict[str, Any]]:
    # (link close (text "..."))
    # (link (text "...") (param send quest 1 2))
    # (link quest (text "...") (param 1 2)) — uncommon
    if not form or form[0] != "link":
        return None
    text = ""
    kind = "close"
    quest: Optional[List[int]] = None

    rest = form[1:]
    if rest and rest[0] == "close":
        kind = "close"
        rest = rest[1:]
    elif rest and isinstance(rest[0], str) and rest[0] not in ("text", "param"):
        # named link; keep scanning for quest params
        rest = rest[1:] if rest[0] != "quest" else rest

    for part in rest:
        if not isinstance(part, list) or not part:
            continue
        if part[0] == "text" and len(part) >= 2:
            text = str(part[1])
        elif part[0] == "param":
            args = [a for a in part[1:] if not isinstance(a, list)]
            # (param send quest INDEX FLAG) or (param INDEX FLAG)
            if "load" in args:
                load_index = args.index("load")
                if load_index + 1 < len(args):
                    target = re.fullmatch(r"s?(\d+)(?:\.dat)?", str(args[load_index + 1]))
                    if target:
                        kind = str(int(target.group(1)))
            elif "quest" in args:
                qi = args.index("quest")
                nums = [int(x) for x in args[qi + 1 :] if isinstance(x, int)]
                if len(nums) >= 2:
                    kind = "quest"
                    quest = [nums[0], nums[1]]
            else:
                nums = [int(x) for x in args if isinstance(x, int)]
                if len(nums) >= 2:
                    kind = "quest"
                    quest = [nums[0], nums[1]]

    if not text and kind == "close":
        text = "[Talk End]"
    link: Dict[str, Any] = {"type": kind, "text": text}
    if quest is not None:
        link["quest"] = quest
    return link


def convert_frame(frame: List[Any], html_id: int, caption: str) -> Dict[str, Any]:
    lines: List[str] = []
    links: List[Dict[str, Any]] = []
    for part in frame[1:]:
        if not isinstance(part, list) or not part:
            continue
        if part[0] == "line":
            if len(part) >= 2:
                lines.append(str(part[1]))
            else:
                lines.append("")
        elif part[0] == "link":
            link = convert_link(part)
            if link:
                links.append(link)
    return {"id": html_id, "caption": caption, "lines": lines, "links": links}


def script_id_from_name(name: str) -> Optional[int]:
    m = re.fullmatch(r"s(\d+)\.dat", name, flags=re.IGNORECASE)
    if not m:
        return None
    return int(m.group(1))


def convert_file(path: Path) -> Optional[Dict[str, Any]]:
    html_id = script_id_from_name(path.name)
    if html_id is None:
        return None
    text = read_text(path)
    # Skip obviously binary / encrypted scripts
    if "\x00" in text[:2048]:
        return None
    try:
        forms = parse_script_top(text)
    except Exception:
        return None
    frame = find_frame(forms)
    if frame is None:
        return None
    caption = extract_caption(forms)
    return convert_frame(frame, html_id, caption)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--all", action="store_true")
    g.add_argument("--id", type=int)
    ap.add_argument("--scripts", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    args = ap.parse_args()

    if not args.scripts.is_dir():
        print(f"missing scripts dir: {args.scripts}", file=sys.stderr)
        return 1
    args.out.mkdir(parents=True, exist_ok=True)

    files = sorted(args.scripts.glob("s*.dat"))
    written = 0
    skipped = 0
    for path in files:
        html_id = script_id_from_name(path.name)
        if html_id is None:
            continue
        if args.id is not None and html_id != args.id:
            continue
        dialog = convert_file(path)
        if not dialog:
            skipped += 1
            continue
        out = args.out / f"{html_id:06d}.json"
        out.write_text(dumps_pretty(dialog) + "\n", encoding="utf-8")
        written += 1
    print(f"wrote {written} dialogs to {args.out} (skipped {skipped})")
    return 0 if written else 1


if __name__ == "__main__":
    raise SystemExit(main())

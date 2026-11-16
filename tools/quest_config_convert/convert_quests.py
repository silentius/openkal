#!/usr/bin/env python3
# OpenKal - an open source KalOnline client and server.
# Copyright (C) 2026 OpenKal contributors
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

"""Convert KalOnline Quest.txt lisp into one JSON file per quest step.

Source:
  Quest.txt — (quest (index N M) [(linked 1)] (case (if …) (then …)) …)

Usage:
  ./convert_quests.py --all --quest path/Quest.txt --out config/quests
  ./convert_quests.py --index 1 --quest path/Quest.txt --out config/quests
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


def convert_condition(form: Any) -> Optional[Dict[str, Any]]:
    if not isinstance(form, list) or not form:
        return None
    op = form[0]
    if not isinstance(op, str):
        return None
    args = form[1:]
    if op == "class" and len(args) >= 1:
        return {"op": "class", "class": int(args[0])}
    if op == "level" and len(args) >= 1:
        return {"op": "level", "level": int(args[0])}
    if op == "specialty" and len(args) >= 2:
        return {"op": "specialty", "class": int(args[0]), "specialty": int(args[1])}
    if op == "contribute" and len(args) >= 1:
        return {"op": "contribute", "contribute": int(args[0])}
    if op == "party" and len(args) >= 1:
        return {"op": "party", "party": int(args[0])}
    if op == "quest" and len(args) >= 2:
        return {"op": "quest", "quest": int(args[0]), "flag": int(args[1])}
    if op == "clear" and len(args) >= 1:
        return {"op": "clear", "quest": int(args[0])}
    if op == "notclear" and len(args) >= 1:
        return {"op": "notclear", "quest": int(args[0])}
    if op == "gstate" and len(args) >= 2:
        return {"op": "gstate", "gstate": int(args[0]), "value": int(args[1])}
    if op == "item":
        items: List[List[int]] = []
        for pair in args:
            if isinstance(pair, list) and len(pair) >= 2:
                items.append([int(pair[0]), int(pair[1])])
            elif not isinstance(pair, list) and len(args) >= 2:
                # (item INDEX NUM) flat form used in some cases
                items.append([int(args[0]), int(args[1])])
                break
        return {"op": "item", "items": items}
    return {"op": op, "raw": args}


def convert_action(form: Any) -> Optional[Dict[str, Any]]:
    if not isinstance(form, list) or not form:
        return None
    op = form[0]
    if not isinstance(op, str):
        return None
    args = form[1:]
    if op == "html" and len(args) >= 1:
        return {"op": "html", "id": int(args[0])}
    if op == "link" and len(args) >= 2:
        return {"op": "link", "quest": int(args[0]), "flag": int(args[1])}
    if op == "specialty" and len(args) >= 1:
        return {"op": "specialty", "specialty": int(args[0])}
    if op == "exp" and len(args) >= 1:
        return {"op": "exp", "exp": int(args[0])}
    if op == "contribute" and len(args) >= 1:
        return {"op": "contribute", "contribute": int(args[0])}
    if op == "supoint" and len(args) >= 1:
        return {"op": "supoint", "supoint": int(args[0])}
    if op == "save" and len(args) >= 2:
        return {"op": "save", "quest": int(args[0]), "flag": int(args[1])}
    if op == "clear" and len(args) >= 2:
        return {"op": "clear", "quest": int(args[0]), "flag": int(args[1])}
    if op == "gstate" and len(args) >= 2:
        return {"op": "gstate", "gstate": int(args[0]), "value": int(args[1])}
    if op == "guide" and len(args) >= 1:
        return {"op": "guide", "guide": int(args[0])}
    if op == "redistribute":
        return {"op": "redistribute"}
    if op == "itemrand" and len(args) >= 1:
        return {"op": "itemrand", "group": int(args[0])}
    if op == "teleport" and len(args) >= 3:
        return {
            "op": "teleport",
            "map": int(args[0]),
            "x": int(args[1]),
            "y": int(args[2]),
        }
    if op == "item":
        ins: List[List[int]] = []
        outs: List[List[int]] = []
        for sub in args:
            if not isinstance(sub, list) or not sub:
                continue
            kind = sub[0]
            rest = sub[1:]
            if kind == "in" and len(rest) >= 3:
                ins.append([int(rest[0]), int(rest[1]), int(rest[2])])
            elif kind == "out" and len(rest) >= 2:
                outs.append([int(rest[0]), int(rest[1])])
        action: Dict[str, Any] = {"op": "item"}
        if outs:
            action["out"] = outs
        if ins:
            action["in"] = ins
        return action
    return {"op": op, "raw": args}


def convert_case(form: Any) -> Optional[Dict[str, Any]]:
    if not isinstance(form, list) or not form or form[0] != "case":
        return None
    conds: List[Dict[str, Any]] = []
    actions: List[Dict[str, Any]] = []
    for part in form[1:]:
        if not isinstance(part, list) or not part:
            continue
        head = part[0]
        if head == "if":
            for c in part[1:]:
                converted = convert_condition(c)
                if converted:
                    conds.append(converted)
        elif head == "then":
            for a in part[1:]:
                converted = convert_action(a)
                if converted:
                    actions.append(converted)
    return {"if": conds, "then": actions}


def convert_quest(form: Any) -> Optional[Dict[str, Any]]:
    if not isinstance(form, list) or not form or form[0] != "quest":
        return None
    index = None
    flag = None
    linked = False
    cases: List[Dict[str, Any]] = []
    for part in form[1:]:
        if not isinstance(part, list) or not part:
            continue
        head = part[0]
        if head == "index" and len(part) >= 3:
            index = int(part[1])
            flag = int(part[2])
        elif head == "linked":
            linked = True if len(part) == 1 else bool(int(part[1]))
        elif head == "case":
            c = convert_case(part)
            if c:
                cases.append(c)
    if index is None or flag is None:
        return None
    return {"index": index, "flag": flag, "linked": linked, "cases": cases}


def write_quest(out_dir: Path, quest: Dict[str, Any]) -> Path:
    path = out_dir / f"{quest['index']:04d}_{quest['flag']:02d}.json"
    path.write_text(dumps_pretty(quest) + "\n", encoding="utf-8")
    return path


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--all", action="store_true")
    g.add_argument("--index", type=int)
    ap.add_argument("--quest", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    args = ap.parse_args()

    if not args.quest.is_file():
        print(f"missing quest file: {args.quest}", file=sys.stderr)
        return 1
    args.out.mkdir(parents=True, exist_ok=True)

    text = read_text(args.quest)
    written = 0
    errors = 0
    for expr in iter_top_forms(text, "quest"):
        try:
            form = parse_form(expr)
            quest = convert_quest(form)
            if not quest:
                errors += 1
                continue
            if args.index is not None and quest["index"] != args.index:
                continue
            write_quest(args.out, quest)
            written += 1
        except Exception as exc:  # noqa: BLE001 — batch convert reports and continues
            errors += 1
            print(f"error: {exc}", file=sys.stderr)
    print(f"wrote {written} quest steps to {args.out} ({errors} errors)")
    return 0 if written else 1


if __name__ == "__main__":
    raise SystemExit(main())

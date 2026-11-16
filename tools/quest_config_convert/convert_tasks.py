#!/usr/bin/env python3
# OpenKal - an open source KalOnline client and server.
# Copyright (C) 2026 OpenKal contributors
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

"""Convert KalOnline task.dat (quest journal texts) into JSON.

Usage:
  ./convert_tasks.py --all --task path/task.dat --out config/tasks
  ./convert_tasks.py --key 1 --task path/task.dat --out config/tasks
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


def convert_flag(form: Any) -> Optional[Dict[str, Any]]:
    if not isinstance(form, list) or not form or form[0] != "flag":
        return None
    out: Dict[str, Any] = {}
    for part in form[1:]:
        if not isinstance(part, list) or not part:
            continue
        key = part[0]
        vals = part[1:]
        if key == "key" and vals:
            out["key"] = int(vals[0])
        elif key == "desc" and vals:
            out["desc"] = str(vals[0])
        elif key == "param" and len(vals) >= 2:
            out["xy"] = [int(vals[0]), int(vals[1])]
        elif key == "item" and len(vals) >= 3:
            out["item"] = [int(vals[0]), int(vals[1]), int(vals[2])]
        elif key == "npcdesc" and vals:
            out["npcDesc"] = str(vals[0])
        elif key == "level" and vals:
            out["level"] = int(vals[0])
        elif key == "npcindex" and vals:
            out["npcIndex"] = int(vals[0])
        elif key == "gotarget" and vals:
            out["goTarget"] = vals
    return out if "key" in out else None


def convert_task(form: Any) -> Optional[Dict[str, Any]]:
    if not isinstance(form, list) or not form or form[0] != "task":
        return None
    out: Dict[str, Any] = {"flags": []}
    for part in form[1:]:
        if not isinstance(part, list) or not part:
            continue
        key = part[0]
        vals = part[1:]
        if key == "key" and vals:
            out["key"] = int(vals[0])
        elif key == "name" and vals:
            out["name"] = str(vals[0])
        elif key == "desc" and vals:
            out["desc"] = str(vals[0])
        elif key == "npcindex" and vals:
            out["npcIndex"] = int(vals[0])
        elif key == "flag":
            flag = convert_flag(part)
            if flag:
                out["flags"].append(flag)
    return out if "key" in out else None


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--all", action="store_true")
    g.add_argument("--key", type=int)
    ap.add_argument("--task", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    args = ap.parse_args()

    if not args.task.is_file():
        print(f"missing task file: {args.task}", file=sys.stderr)
        return 1
    args.out.mkdir(parents=True, exist_ok=True)

    text = read_text(args.task)
    written = 0
    for expr in iter_top_forms(text, "task"):
        try:
            task = convert_task(parse_form(expr))
        except Exception as exc:  # noqa: BLE001
            print(f"error: {exc}", file=sys.stderr)
            continue
        if not task:
            continue
        if args.key is not None and task["key"] != args.key:
            continue
        path = args.out / f"{task['key']:04d}.json"
        path.write_text(dumps_pretty(task) + "\n", encoding="utf-8")
        written += 1
    print(f"wrote {written} tasks to {args.out}")
    return 0 if written else 1


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
# OpenKal - an open source KalOnline client and server.
# Copyright (C) 2026 OpenKal contributors
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

"""Convert KalOnline ItemGroup.txt into percentage-based JSON drop tables.

Writes:
  config/groups/{index:04d}.json       — inner loot tables (group)
  config/itemgroups/{index:04d}.json   — outer tables (itemgroup)

Legacy rolls used cumulative thresholds (outer 1–1000, inner 1–100). This
converter emits explicit chance percentages (0.01–100) as deltas:

  outer:  chance% = delta / 10          (fixed scale 1000; remainder = no drop)
  inner:  chance% = delta on scale 100  (MainSvr RANDOM(1,100); thresholds >100
          are clamped — unreachable entries are dropped; remainder = no drop)

Usage:
  ./convert_itemgroups.py --all \\
      --src /path/to/ItemGroup.txt \\
      --groups-out config/groups \\
      --itemgroups-out config/itemgroups

  ./convert_itemgroups.py --index 13 --src … --groups-out … --itemgroups-out …
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Sequence, Tuple


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


def iter_top_level_forms(text: str) -> Iterable[Tuple[int, str]]:
    """Yield (start_offset, balanced_expr) for every depth-0 s-expression."""
    text = strip_comments(text)
    i = 0
    n = len(text)
    while i < n:
        ch = text[i]
        if ch == '"':
            i += 1
            while i < n and text[i] != '"':
                if text[i] == "\\":
                    i += 1
                i += 1
            i += 1
            continue
        if ch != "(":
            i += 1
            continue
        start = i
        depth = 0
        j = i
        while j < n:
            c = text[j]
            if c == '"':
                j += 1
                while j < n and text[j] != '"':
                    if text[j] == "\\":
                        j += 1
                    j += 1
            elif c == "(":
                depth += 1
            elif c == ")":
                depth -= 1
                if depth == 0:
                    yield start, text[start : j + 1]
                    i = j + 1
                    break
            j += 1
        else:
            return


def form_head(expr: str) -> str:
    """Return the first atom of an s-expression, lowercased."""
    m = re.match(r"\(\s*([^\s()]+)", expr)
    return m.group(1).lower() if m else ""


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


def comment_name_before(raw_text: str, byte_offset_in_stripped: int, stripped: str) -> str:
    """Best-effort: find the last ;-comment line immediately above this form in the raw file.

    `byte_offset_in_stripped` is an offset into strip_comments(raw). We map by
    scanning raw line comments near the form's content fingerprint.
    """
    # Use a short unique snippet from the stripped form start to locate in raw.
    snippet = stripped[byte_offset_in_stripped : byte_offset_in_stripped + 40]
    snippet = re.sub(r"\s+", " ", snippet).strip()
    if not snippet:
        return ""
    # Search raw (with comments) for a line containing the form head near a comment.
    lines = raw_text.splitlines()
    # Build a looser search: find line that starts the form.
    head_m = re.match(r"\(\s*(group|itemgroup)\b", snippet, re.I)
    if not head_m:
        return ""
    # Walk raw lines; track last comment; when we hit a matching form line, return comment.
    last_comment = ""
    form_open = re.compile(rf"^\s*\(\s*{head_m.group(1)}\b", re.I)
    # Prefer matching on index if present in snippet.
    idx_m = re.search(r"\(\s*index\s+(\d+)\s*\)", snippet, re.I)
    want_idx = idx_m.group(1) if idx_m else None
    for line in lines:
        stripped_line = line.strip()
        if stripped_line.startswith(";"):
            # Keep non-separator comments.
            body = stripped_line.lstrip(";").strip()
            if body and not set(body) <= {"=", "-", "_"}:
                last_comment = body
            continue
        if form_open.match(line):
            if want_idx is None or f"(index {want_idx})" in line.replace("  ", " ") or f"(index  {want_idx})" in line:
                # Confirm index more carefully
                if want_idx is not None:
                    im = re.search(r"\(\s*index\s+(\d+)\s*\)", line, re.I)
                    if not im or im.group(1) != want_idx:
                        continue
                name = last_comment
                last_comment = ""
                return name
            last_comment = ""
        elif stripped_line:
            # Non-comment, non-matching content clears sticky comment only for blank-separated blocks.
            if not stripped_line.startswith("("):
                last_comment = ""
    return ""


# Documented source typos: (kind, index, old_cum) → new_cum, applied before % conversion.
# itemgroup 151: comment says Weapon [10%]; cumulative 530 after 600 is unreachable.
KNOWN_CUMULATIVE_FIXES = {
    ("itemgroup", 151, 530): 700,
}


def apply_known_fixes(
    kind: str, index: int, rows: List[Tuple[int, Dict[str, Any]]], warnings: List[str]
) -> List[Tuple[int, Dict[str, Any]]]:
    fixed: List[Tuple[int, Dict[str, Any]]] = []
    for cum, payload in rows:
        key = (kind, index, cum)
        if key in KNOWN_CUMULATIVE_FIXES:
            new_cum = KNOWN_CUMULATIVE_FIXES[key]
            warnings.append(
                f"{kind} {index}: applying known fix cumulative {cum} → {new_cum}"
            )
            cum = new_cum
        fixed.append((cum, payload))
    return fixed


def round_chance(value: float) -> float:
    """Round to 0.01% precision; clamp tiny positives up to 0.01."""
    if value <= 0:
        return 0.0
    rounded = round(value + 1e-12, 2)
    if 0 < rounded < 0.01:
        return 0.01
    return rounded


def cumulatives_to_chances(
    rows: Sequence[Tuple[int, Dict[str, Any]]],
    *,
    fixed_scale: Optional[int],
    label: str,
    warnings: List[str],
) -> List[Dict[str, Any]]:
    """Convert (cumulative_threshold, payload) rows into chance% entries."""
    if not rows:
        return []

    if fixed_scale is not None:
        scale = fixed_scale
    else:
        scale = max(max(c for c, _ in rows), 100)
    if scale <= 0:
        warnings.append(f"{label}: invalid scale {scale}")
        return []

    out: List[Dict[str, Any]] = []
    prev = 0
    for cum, payload in rows:
        # MainSvr first-match: roll in 1..scale; thresholds above scale never hit.
        capped = min(cum, scale)
        if capped <= prev:
            if cum < prev:
                warnings.append(
                    f"{label}: non-monotonic cumulative {cum} after {prev} "
                    f"(entry skipped — unreachable under first-match)"
                )
            elif cum == prev:
                warnings.append(
                    f"{label}: duplicate cumulative {cum} "
                    f"(entry skipped — unreachable under first-match)"
                )
            elif cum > scale:
                warnings.append(
                    f"{label}: cumulative {cum} exceeds scale {scale} "
                    f"(entry skipped — unreachable under RANDOM(1,{scale}))"
                )
            continue

        delta = capped - prev
        prev = capped
        chance = round_chance(delta * 100.0 / scale)
        if chance <= 0:
            continue

        entry = dict(payload)
        entry["chance"] = chance
        out.append(entry)

        if prev >= scale:
            # Any later rows are unreachable once the band already covers 100%.
            break

    # Collapse identical payloads (same keys except chance) by summing chances.
    collapsed: List[Dict[str, Any]] = []
    for entry in out:
        chance = float(entry["chance"])
        key_items = tuple(sorted((k, v) for k, v in entry.items() if k != "chance"))
        if collapsed:
            prev_e = collapsed[-1]
            prev_key = tuple(sorted((k, v) for k, v in prev_e.items() if k != "chance"))
            if prev_key == key_items:
                prev_e["chance"] = round_chance(float(prev_e["chance"]) + chance)
                continue
        collapsed.append(entry)

    total = sum(float(e["chance"]) for e in collapsed)
    if total > 100.0:
        overflow = total - 100.0
        if overflow <= 0.1 and collapsed:
            collapsed[-1]["chance"] = round_chance(float(collapsed[-1]["chance"]) - overflow)
            if collapsed[-1]["chance"] <= 0:
                warnings.append(f"{label}: trimmed last entry to non-positive after drift fix")
                collapsed.pop()
        else:
            warnings.append(f"{label}: chance sum {total:.2f}% exceeds 100%")

    return collapsed


def parse_item_tuple(
    vals: List[Any], label: str, warnings: List[str]
) -> Optional[Tuple[int, Dict[str, Any]]]:
    if len(vals) < 2:
        warnings.append(f"{label}: item tuple too short: {vals}")
        return None
    cum = int(vals[0])
    item_id = int(vals[1])
    prefix = int(vals[2]) if len(vals) >= 3 else 0
    count = int(vals[3]) if len(vals) >= 4 else 1
    payload: Dict[str, Any] = {"type": "item", "item": item_id, "prefix": prefix}
    if count != 1:
        payload["count"] = count
    return cum, payload


def parse_money_tuple(vals: List[Any], label: str, warnings: List[str]) -> Optional[Tuple[int, Dict[str, Any]]]:
    if len(vals) < 2:
        warnings.append(f"{label}: money tuple too short: {vals}")
        return None
    cum = int(vals[0])
    amount = int(vals[1])
    return cum, {"type": "money", "amount": amount}


def parse_group_form(form: List[Any], label: str, warnings: List[str]) -> Optional[Dict[str, Any]]:
    if not isinstance(form, list) or not form:
        return None
    index: Optional[int] = None
    raw_rows: List[Tuple[int, Dict[str, Any]]] = []

    for clause in form[1:]:
        if not isinstance(clause, list) or not clause:
            continue
        key = str(clause[0]).lower()
        if key == "index":
            if len(clause) >= 2:
                index = int(clause[1])
            continue
        if key == "item":
            # (item (cum id prefix [num]) …)  OR flat leftovers
            for part in clause[1:]:
                if isinstance(part, list):
                    parsed = parse_item_tuple(part, label, warnings)
                    if parsed:
                        raw_rows.append(parsed)  # type: ignore[arg-type]
                else:
                    warnings.append(f"{label}: unexpected item atom {part}")
            continue
        if key == "money":
            for part in clause[1:]:
                if isinstance(part, list):
                    parsed = parse_money_tuple(part, label, warnings)
                    if parsed:
                        raw_rows.append(parsed)
                else:
                    warnings.append(f"{label}: unexpected money atom {part}")
            continue
        warnings.append(f"{label}: unknown clause {key}")

    if index is None:
        warnings.append(f"{label}: missing index")
        return None

    entries = cumulatives_to_chances(
        raw_rows, fixed_scale=100, label=f"group {index}", warnings=warnings
    )
    return {"index": index, "entries": entries}


def parse_itemgroup_form(form: List[Any], label: str, warnings: List[str]) -> Optional[Dict[str, Any]]:
    if not isinstance(form, list) or not form:
        return None
    index: Optional[int] = None
    raw_rows: List[Tuple[int, Dict[str, Any]]] = []

    for clause in form[1:]:
        if not isinstance(clause, list) or not clause:
            continue
        key = str(clause[0]).lower()
        if key == "index":
            if len(clause) >= 2:
                index = int(clause[1])
            continue
        if key == "group":
            for part in clause[1:]:
                if not isinstance(part, list) or len(part) < 2:
                    warnings.append(f"{label}: bad group ref {part}")
                    continue
                cum = int(part[0])
                gid = int(part[1])
                raw_rows.append((cum, {"group": gid}))
            continue
        warnings.append(f"{label}: unknown clause {key}")

    if index is None:
        warnings.append(f"{label}: missing index")
        return None

    raw_rows = apply_known_fixes("itemgroup", index, raw_rows, warnings)
    entries = cumulatives_to_chances(
        raw_rows, fixed_scale=1000, label=f"itemgroup {index}", warnings=warnings
    )
    return {"index": index, "entries": entries}


def load_all(
    src: Path,
) -> Tuple[Dict[int, Dict[str, Any]], Dict[int, Dict[str, Any]], List[str]]:
    raw = read_text(src)
    stripped = strip_comments(raw)
    warnings: List[str] = []
    groups: Dict[int, Dict[str, Any]] = {}
    itemgroups: Dict[int, Dict[str, Any]] = {}

    for start, expr in iter_top_level_forms(stripped):
        head = form_head(expr)
        if head == "group":
            form = parse_form(expr)
            obj = parse_group_form(form, "group", warnings)
            if not obj:
                continue
            idx = int(obj["index"])
            if idx in groups:
                warnings.append(f"duplicate group index {idx} (later wins)")
            name = comment_name_before(raw, start, stripped)
            if name:
                obj["name"] = name
            groups[idx] = obj
        elif head == "itemgroup":
            form = parse_form(expr)
            obj = parse_itemgroup_form(form, "itemgroup", warnings)
            if not obj:
                continue
            idx = int(obj["index"])
            if idx in itemgroups:
                warnings.append(f"duplicate itemgroup index {idx} (later wins)")
            name = comment_name_before(raw, start, stripped)
            if name:
                obj["name"] = name
            itemgroups[idx] = obj
        else:
            warnings.append(f"skipping unknown top-level form head '{head}'")

    # Validate outer → inner references.
    for ig in itemgroups.values():
        for entry in ig.get("entries", []):
            gid = entry.get("group")
            if gid not in groups:
                warnings.append(f"itemgroup {ig['index']}: missing group {gid}")

    return groups, itemgroups, warnings


def write_json(path: Path, obj: Dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    # Stable key order: index, name, entries
    ordered: Dict[str, Any] = {"index": obj["index"]}
    if "name" in obj:
        ordered["name"] = obj["name"]
    ordered["entries"] = obj.get("entries", [])
    path.write_text(dumps_pretty(ordered) + "\n", encoding="utf-8")


def main(argv: Optional[Sequence[str]] = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--src", type=Path, required=True, help="Path to ItemGroup.txt")
    ap.add_argument("--groups-out", type=Path, required=True, help="Output dir for group JSON")
    ap.add_argument("--itemgroups-out", type=Path, required=True, help="Output dir for itemgroup JSON")
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--all", action="store_true", help="Convert every group and itemgroup")
    g.add_argument("--index", type=int, help="Convert a single group and/or itemgroup by index")
    ap.add_argument("--dry-run", action="store_true", help="Parse and validate only; do not write")
    args = ap.parse_args(argv)

    if not args.src.is_file():
        print(f"error: source not found: {args.src}", file=sys.stderr)
        return 1

    groups, itemgroups, warnings = load_all(args.src)

    if args.index is not None:
        idx = args.index
        sel_groups = {idx: groups[idx]} if idx in groups else {}
        sel_igs = {idx: itemgroups[idx]} if idx in itemgroups else {}
        if not sel_groups and not sel_igs:
            print(f"error: no group or itemgroup with index {idx}", file=sys.stderr)
            return 1
        groups, itemgroups = sel_groups, sel_igs

    for w in warnings:
        print(f"warning: {w}", file=sys.stderr)

    print(
        f"parsed {len(groups)} groups, {len(itemgroups)} itemgroups "
        f"({len(warnings)} warnings)",
        file=sys.stderr,
    )

    if args.dry_run:
        return 0

    for idx, obj in sorted(groups.items()):
        write_json(args.groups_out / f"{idx:04d}.json", obj)
    for idx, obj in sorted(itemgroups.items()):
        write_json(args.itemgroups_out / f"{idx:04d}.json", obj)

    print(
        f"wrote {len(groups)} files → {args.groups_out}, "
        f"{len(itemgroups)} files → {args.itemgroups_out}",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

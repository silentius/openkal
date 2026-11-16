#!/usr/bin/env python3
# OpenKal - an open source KalOnline client and server.
# Copyright (C) 2026 OpenKal contributors
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

"""Convert KalOnline InitItem.txt + message-e.txt item text into one JSON per item.

Layout under --out:
  {role}/{kind}/{index}_{slug}.json
  role = knight|mage|archer|thief when limit.class is set, else general
  kind = weapon | armor (defense) | etc|refresh|… (general subtype) | quest|…

Sources:
  InitItem.txt   — item definitions (Index)
  message-e.txt  — source for the generated name and optional description text

The generated item JSON stores display text directly. It does not retain the
legacy nameId or desc references, so the client does not need message-e.txt to
render item names or descriptions.

Usage:
  ./convert_items.py --all \\
      --init path/InitItem.txt \\
      --message path/message-e.txt \\
      --out config/items

  ./convert_items.py --index 1 --init … --message … --out config/items
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
    # Drop non-ascii-only / empty / too short garbage names.
    if not slug or not any("a" <= c <= "z" for c in slug):
        return ""
    return slug[:64]


def index_pad(index: int) -> str:
    return f"{index:04d}"


def item_filename(index: int, name: Optional[str]) -> str:
    slug = sanitize_slug(name)
    base = index_pad(index)
    return f"{base}_{slug}.json" if slug else f"{base}.json"


def role_folder(doc: Dict[str, Any]) -> str:
    """knight/mage/archer/thief when class-limited; otherwise general."""
    lim = doc.get("limit")
    if isinstance(lim, dict):
        lc = lim.get("class")
        if isinstance(lc, str):
            lc = lc.lower()
            if lc == "magician":
                lc = "mage"
            if lc in ("knight", "mage", "archer", "thief"):
                return lc
    return "general"


def kind_folder(doc: Dict[str, Any]) -> str:
    """weapon / armor / etc / refresh / quest / …"""
    clazz = doc.get("class") or []
    if isinstance(clazz, str):
        clazz = clazz.split()
    big = str(clazz[0]).lower() if clazz else "misc"
    small = str(clazz[1]).lower() if len(clazz) > 1 else "misc"
    if big == "weapon":
        return "weapon"
    if big == "defense":
        return "armor"
    if big == "general":
        return sanitize_slug(small) or "misc"
    return sanitize_slug(big) or "misc"


def item_relpath(doc: Dict[str, Any]) -> str:
    fname = item_filename(int(doc["index"]), doc.get("name") if isinstance(doc.get("name"), str) else None)
    return f"{role_folder(doc)}/{kind_folder(doc)}/{fname}"


def clear_item_json_tree(out_dir: Path) -> None:
    """Remove previously generated item JSON (keep out_dir itself)."""
    if not out_dir.is_dir():
        return
    for p in sorted(out_dir.rglob("*.json"), reverse=True):
        p.unlink(missing_ok=True)
    # Drop empty class/kind directories left behind.
    for p in sorted((x for x in out_dir.rglob("*") if x.is_dir()), reverse=True):
        try:
            p.rmdir()
        except OSError:
            pass


def normalize_key(key: str) -> str:
    # Prefer camelCase-ish stable keys for known InitItem fields.
    mapping = {
        "Index": "index",
        "Image": "image",
        "Action": "action",
        "Attack": "attack",
        "Hth": "hth",
        "Str": "str",
        "Dex": "dex",
        "Int": "int",
        "Wis": "wis",
    }
    normalized = mapping.get(key, key[0].lower() + key[1:] if key[:1].isupper() else key)
    return "stackable" if normalized == "plural" else normalized


def list_to_value(vals: List[Any]) -> Any:
    if not vals:
        return True
    if len(vals) == 1:
        return vals[0]
    return vals


def specialty_to_object(entries: List[Any]) -> Dict[str, Any]:
    """(specialty (aspeed 700) (Attack 3 10) (hit 15)) → object."""
    out: Dict[str, Any] = {}
    for entry in entries:
        if not isinstance(entry, list) or not entry:
            continue
        key = entry[0]
        if not isinstance(key, str):
            continue
        k = normalize_key(key)
        rest = entry[1:]
        if k == "attack" and len(rest) >= 2 and all(isinstance(x, (int, float)) for x in rest[:2]):
            atk: Dict[str, Any] = {"min": int(rest[0]), "max": int(rest[1])}
            if len(rest) > 2:
                atk["extra"] = rest[2:] if len(rest) > 3 else rest[2]
            out[k] = atk
        elif k == "refresh" and len(rest) >= 2:
            # (refresh hp 100) / (refresh mp 50)
            out[k] = {"stat": rest[0], "amount": rest[1]}
        elif k == "teleport" and len(rest) >= 2:
            out[k] = {"map": rest[0], "loc": rest[1]}
        else:
            out[k] = list_to_value(rest)
    return out


def build_item_doc(
    form: List[Any], names: Dict[int, str], descriptions: Dict[int, str]
) -> Optional[Dict[str, Any]]:
    if not isinstance(form, list) or not form or form[0] != "item":
        return None

    # Collect fields; specialty is nested lists.
    raw: Dict[str, Any] = {}
    for item in form[1:]:
        if not isinstance(item, list) or not item:
            continue
        key = item[0]
        if not isinstance(key, str):
            continue
        vals = item[1:]
        if key in ("specialty", "Specialty"):
            raw["specialty"] = specialty_to_object(vals)
            continue
        nk = normalize_key(key)
        # Legacy region and item codes do not affect classification or equipment slots.
        if nk in ("country", "code"):
            continue
        # Duplicate keys: keep last (rare).
        raw[nk] = list_to_value(vals)

    idx = raw.get("index")
    if not isinstance(idx, int) or idx <= 0:
        return None

    name_ref = raw.get("name")
    display = names.get(name_ref) if isinstance(name_ref, int) else None

    doc: Dict[str, Any] = {"index": idx, "rarity": 0}
    if isinstance(raw.get("rarity"), int):
        doc["rarity"] = raw["rarity"]
    if display:
        doc["name"] = display

    # Stable top-level fields consumers care about.
    if "image" in raw:
        doc["image"] = raw["image"]
    if "action" in raw:
        doc["action"] = raw["action"]
    if "class" in raw:
        clazz = raw["class"]
        if isinstance(clazz, list):
            doc["class"] = clazz
        elif isinstance(clazz, str):
            doc["class"] = clazz.split()
        else:
            doc["class"] = clazz
    if "level" in raw:
        doc["level"] = raw["level"]
    if "wear" in raw:
        doc["wear"] = raw["wear"]
    if "limit" in raw:
        lim = raw["limit"]
        if isinstance(lim, list) and len(lim) >= 2 and isinstance(lim[0], str):
            doc["limit"] = {"class": lim[0], "level": lim[1]}
        else:
            doc["limit"] = lim
    if "range" in raw:
        doc["range"] = raw["range"]
    if "buy" in raw:
        doc["buy"] = raw["buy"]
    if "sell" in raw:
        doc["sell"] = raw["sell"]
    if "endurance" in raw:
        doc["endurance"] = raw["endurance"]
    if "use" in raw:
        doc["use"] = raw["use"]
    if "stackable" in raw:
        doc["stackable"] = raw["stackable"]
    if "cooltime" in raw:
        doc["cooltime"] = raw["cooltime"]
    if "desc" in raw:
        if isinstance(raw["desc"], int) and raw["desc"] in descriptions:
            doc["description"] = descriptions[raw["desc"]]
    if "effect" in raw:
        doc["effect"] = raw["effect"]
    if "pay" in raw:
        doc["pay"] = raw["pay"]
    if "buff" in raw:
        doc["buff"] = raw["buff"]
    if "specialty" in raw:
        doc["specialty"] = raw["specialty"]

    # Preserve any remaining unknown fields under "extra" to avoid data loss.
    known = {
        "index",
        "name",
        "rarity",
        "image",
        "action",
        "class",
        "level",
        "wear",
        "limit",
        "range",
        "buy",
        "sell",
        "endurance",
        "use",
        "stackable",
        "cooltime",
        "desc",
        "effect",
        "pay",
        "buff",
        "specialty",
    }
    extra = {k: v for k, v in raw.items() if k not in known}
    if extra:
        doc["extra"] = extra

    return doc


def convert(init_path: Path, message_path: Path, out_dir: Path, only_index: Optional[int]) -> int:
    init_text = read_text(init_path)
    message_text = read_text(message_path)

    names: Dict[int, str] = {}
    descriptions: Dict[int, str] = {}
    for form in iter_top_forms(message_text, "itemname"):
        parsed = parse_form(form)
        if isinstance(parsed, list) and len(parsed) >= 3 and parsed[0] == "itemname":
            try:
                nid = int(parsed[1])
            except (TypeError, ValueError):
                continue
            name = parsed[2]
            if isinstance(name, str):
                names[nid] = name
    for form in iter_top_forms(message_text, "itemdesc"):
        parsed = parse_form(form)
        if isinstance(parsed, list) and len(parsed) >= 3 and parsed[0] == "itemdesc":
            try:
                did = int(parsed[1])
            except (TypeError, ValueError):
                continue
            text = parsed[2]
            if isinstance(text, str):
                descriptions[did] = text

    items: Dict[int, Dict[str, Any]] = {}
    errors = 0
    for form in iter_top_forms(init_text, "item"):
        try:
            parsed = parse_form(form)
        except ValueError as e:
            errors += 1
            print(f"warn: parse failed: {e}", file=sys.stderr)
            continue
        if not isinstance(parsed, list):
            continue
        doc = build_item_doc(parsed, names, descriptions)
        if not doc:
            continue
        items[doc["index"]] = doc

    if only_index is not None:
        if only_index not in items:
            print(f"error: index {only_index} not found in InitItem", file=sys.stderr)
            return 1
        selected = {only_index: items[only_index]}
    else:
        selected = items

    out_dir.mkdir(parents=True, exist_ok=True)
    files: List[str] = []
    if only_index is None:
        clear_item_json_tree(out_dir)
    else:
        # A renamed item must replace its old filename, not duplicate its index.
        for previous in sorted(out_dir.rglob("*.json")):
            if previous.name == "manifest.json":
                continue
            document = json.loads(previous.read_text(encoding="utf-8"))
            if not isinstance(document, dict) or "index" not in document:
                continue
            if document["index"] == only_index:
                previous.unlink()
            else:
                files.append(previous.relative_to(out_dir).as_posix())
    written = 0
    for idx in sorted(selected):
        doc = selected[idx]
        rel = item_relpath(doc)
        path = out_dir / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(dumps_pretty(doc) + "\n", encoding="utf-8")
        files.append(rel)
        written += 1

    with_desc = sum(1 for d in selected.values() if "description" in d)
    manifest = {"count": len(files), "files": sorted(files)}
    (out_dir / "manifest.json").write_text(dumps_pretty(manifest) + "\n", encoding="utf-8")
    print(
        f"wrote {written} item file(s) → {out_dir}"
        f" ({with_desc} with description)"
        + (f" ({errors} parse warnings)" if errors else "")
    )
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    mode = ap.add_mutually_exclusive_group(required=True)
    mode.add_argument("--all", action="store_true", help="Convert every item index found")
    mode.add_argument("--index", type=int, help="Convert a single item index")
    ap.add_argument("--init", type=Path, required=True, help="InitItem.txt")
    ap.add_argument("--message", type=Path, required=True, help="message-e.txt (itemname + itemdesc)")
    ap.add_argument("--out", type=Path, required=True, help="Output directory for JSON files")
    args = ap.parse_args()

    for p in (args.init, args.message):
        if not p.is_file():
            print(f"error: missing file {p}", file=sys.stderr)
            return 1

    return convert(
        init_path=args.init,
        message_path=args.message,
        out_dir=args.out,
        only_index=None if args.all else args.index,
    )


if __name__ == "__main__":
    sys.exit(main())

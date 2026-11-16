#!/usr/bin/env python3
# OpenKal - an open source KalOnline client and server.
# Copyright (C) 2026 OpenKal contributors
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

"""Convert KalOnline monster lisp configs into one JSON file per monster.

Sources:
  InitMonster.txt   — combat stats (index)
  GenMonster.txt    — spawn rects (index → many)
  message-e.txt     — (monstername id "Name")
  External decoded macro file — (monsterinfo …) + (monsterbone …)

Usage:
  ./convert_monsters.py --all \\
      --init path/InitMonster.txt \\
      --gen path/GenMonster.txt \\
      --message path/message-e.txt \\
      --macro /path/to/legacy/macro.dat \\
      --out config/monsters

  ./convert_monsters.py --index 1 --init … --gen … --message … --macro … --out …
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from collections import defaultdict
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Tuple


def is_json_number(value: Any) -> bool:
    # bool is a subclass of int — treat it as non-number for formatting.
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
    # Drop ;-comments but keep content inside quotes (rare in these files).
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
    """Yield balanced s-expressions whose first atom is `head` (e.g. monster)."""
    text = strip_comments(text)
    # Match "(head" or "( head" (Kal configs often put a space after '(').
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
    # number?
    try:
        if "." in t:
            return float(t), i + 1
        return int(t, 10), i + 1
    except ValueError:
        return t, i + 1


def parse_form(expr: str) -> Any:
    return parse_atoms(tokenize(expr))[0]


def info_index(key: Any) -> Optional[int]:
    """Monster index from (key N) or (key N M)."""
    if isinstance(key, int):
        return key
    if isinstance(key, list) and key and isinstance(key[0], int):
        return key[0]
    return None


def fields_from_form(form: List[Any]) -> Dict[str, Any]:
    """(monster (index 1) (str 21) …) → {index:1, str:21, …} with list values for multi-arg."""
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
            # Nested lists (quest clauses, itemgroup repeats): keep as list.
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
    # Keep only ASCII letters/digits; collapse to underscores.
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
    # Reject empty or pure junk short tokens.
    if len(slug) < 2:
        return ""
    # If mostly non-letter after sanitize failure already handled; require a letter.
    if not any("a" <= c <= "z" for c in slug):
        return ""
    return slug


def index_pad(index: int) -> str:
    # Max observed ~1112 → 4 digits.
    return f"{index:04d}"


def monster_filename(index: int, name: Optional[str]) -> str:
    slug = sanitize_slug(name)
    base = index_pad(index)
    return f"{base}_{slug}.json" if slug else f"{base}.json"


def build_stats(fields: Dict[str, Any], form: List[Any]) -> Dict[str, Any]:
    stats: Dict[str, Any] = {}
    simple_keys = [
        "country",
        "race",
        "level",
        "ai",
        "range",
        "exp",
        "str",
        "hth",
        "int",
        "wis",
        "dex",
        "hp",
        "mp",
        "aspeed",
        "hit",
        "dodge",
        "absorb",
    ]
    for k in simple_keys:
        if k in fields:
            stats[k] = fields[k]

    if "sight" in fields:
        stats["sight"] = fields["sight"]
    if "mspeed" in fields:
        stats["mspeed"] = fields["mspeed"]
    if "attack" in fields:
        at = fields["attack"]
        if isinstance(at, list) and len(at) >= 3:
            stats["attack"] = {"type": at[0], "min": at[1], "max": at[2]}
        else:
            stats["attack"] = at
    if "magic" in fields:
        stats["magic"] = fields["magic"]
    if "defense" in fields:
        stats["defense"] = fields["defense"]
    if "resist" in fields:
        stats["resist"] = fields["resist"]

    item_groups = parse_repeated_fields(form, "itemgroup")
    if item_groups:
        stats["itemGroups"] = item_groups

    # quest can be one multi-clause field: (quest (a…) (b…)) stored as nested lists
    if "quest" in fields:
        q = fields["quest"]
        if isinstance(q, list):
            stats["quests"] = q
    if "skill" in fields:
        stats["skill"] = fields["skill"]

    return stats


def normalize_attach(rows: List[Any]) -> List[List[Any]]:
    """Turn (particle/fx …) rows into [name, effectBone, ox, oy, oz]."""
    out: List[List[Any]] = []
    for row in rows:
        if isinstance(row, str):
            out.append([row, -1, 0.0, 0.0, 0.0])
            continue
        if not isinstance(row, list) or not row:
            continue
        name = row[0]
        if not isinstance(name, str) or not name:
            continue
        bone = row[1] if len(row) > 1 else -1
        ox = row[2] if len(row) > 2 else 0.0
        oy = row[3] if len(row) > 3 else 0.0
        oz = row[4] if len(row) > 4 else 0.0
        out.append([name, bone, ox, oy, oz])
    return out


def build_appearance(
    info: Optional[Dict[str, Any]], form: Optional[List[Any]] = None
) -> Optional[Dict[str, Any]]:
    if not info:
        return None
    app: Dict[str, Any] = {}
    if "bone" in info:
        app["boneId"] = info["bone"] if not isinstance(info["bone"], list) else info["bone"][0]
    if "level" in info:
        app["level"] = info["level"]
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
    if "attheight" in info:
        app["attackHeight"] = info["attheight"]
    key = info.get("key")
    if isinstance(key, list) and len(key) >= 2 and isinstance(key[1], int) and key[1] > 0:
        app["clothesIndex"] = key[1]
    if form:
        particles = normalize_attach(parse_repeated_fields(form, "particle"))
        if particles:
            app["particles"] = particles
        fxs = normalize_attach(parse_repeated_fields(form, "fx"))
        if fxs:
            app["fx"] = fxs
    return app or None


def build_bone_entry(fields: Dict[str, Any]) -> Dict[str, Any]:
    bone: Dict[str, Any] = {}
    for k in ("bone", "target", "motion", "part", "damage"):
        if k in fields:
            bone[k] = fields[k]
    return bone


def convert(
    init_path: Path,
    gen_path: Path,
    message_path: Path,
    macro_path: Path,
    out_dir: Path,
    only_index: Optional[int],
) -> int:
    init_text = read_text(init_path)
    gen_text = read_text(gen_path)
    message_text = read_text(message_path)
    macro_text = read_text(macro_path)

    names: Dict[int, str] = {}
    for form in iter_top_forms(message_text, "monstername"):
        parsed = parse_form(form)
        # (monstername 1 "Demon Vulgar")
        if isinstance(parsed, list) and len(parsed) >= 3 and parsed[0] == "monstername":
            try:
                nid = int(parsed[1])
            except (TypeError, ValueError):
                continue
            name = parsed[2]
            if isinstance(name, str):
                names[nid] = name

    infos: Dict[int, Dict[str, Any]] = {}
    info_forms: Dict[int, List[Any]] = {}
    for form in iter_top_forms(macro_text, "monsterinfo"):
        parsed = parse_form(form)
        fields = fields_from_form(parsed)
        index = info_index(fields.get("key"))
        if index is None:
            continue
        infos[index] = fields
        info_forms[index] = parsed

    bones: Dict[int, Dict[str, Any]] = {}
    for form in iter_top_forms(macro_text, "monsterbone"):
        parsed = parse_form(form)
        fields = fields_from_form(parsed)
        bone_key = info_index(fields.get("key"))
        if bone_key is not None:
            bones[bone_key] = build_bone_entry(fields)

    spawns: Dict[int, List[Dict[str, Any]]] = defaultdict(list)
    for form in iter_top_forms(gen_text, "genmonster"):
        parsed = parse_form(form)
        fields = fields_from_form(parsed)
        idx = fields.get("index")
        if not isinstance(idx, int):
            continue
        entry: Dict[str, Any] = {}
        for k in ("map", "area", "max", "cycle"):
            if k in fields:
                entry[k] = fields[k]
        if "rect" in fields:
            entry["rect"] = fields["rect"]
        spawns[idx].append(entry)

    monsters: Dict[int, Dict[str, Any]] = {}
    for form in iter_top_forms(init_text, "monster"):
        parsed = parse_form(form)
        fields = fields_from_form(parsed)
        idx = fields.get("index")
        if not isinstance(idx, int) or idx <= 0:
            continue
        name_id = fields.get("name")
        display_name = None
        if isinstance(name_id, int):
            display_name = names.get(name_id)
        # Prefer comment-less display name; fall back to nameId only.
        doc: Dict[str, Any] = {"index": idx}
        if display_name:
            doc["name"] = display_name
        if isinstance(name_id, int):
            doc["nameId"] = name_id
        doc["stats"] = build_stats(fields, parsed)
        app = build_appearance(infos.get(idx), info_forms.get(idx))
        if app:
            doc["appearance"] = app
        doc["spawns"] = list(spawns.get(idx, []))
        monsters[idx] = doc

    # Include gen indexes missing from InitMonster as minimal stubs so spawns aren't lost.
    for idx, areas in spawns.items():
        if idx in monsters:
            continue
        name_id = idx  # best-effort
        display_name = names.get(name_id)
        doc = {"index": idx}
        if display_name:
            doc["name"] = display_name
        doc["nameId"] = name_id
        doc["stats"] = {}
        app = build_appearance(infos.get(idx), info_forms.get(idx))
        if app:
            doc["appearance"] = app
        doc["spawns"] = list(areas)
        monsters[idx] = doc

    if only_index is not None:
        if only_index not in monsters:
            print(f"error: index {only_index} not found in InitMonster/GenMonster", file=sys.stderr)
            return 1
        selected = {only_index: monsters[only_index]}
    else:
        selected = monsters

    out_dir.mkdir(parents=True, exist_ok=True)

    # Shared bones: either all referenced, or all known when --all.
    needed_bones = set()
    for doc in selected.values():
        app = doc.get("appearance") or {}
        bid = app.get("boneId")
        if isinstance(bid, int):
            needed_bones.add(bid)
    if only_index is None:
        needed_bones |= set(bones.keys())

    bones_out = {str(k): bones[k] for k in sorted(needed_bones) if k in bones}
    bones_path = out_dir / "bones.json"
    bones_path.write_text(dumps_pretty(bones_out) + "\n", encoding="utf-8")

    written = 0
    for idx in sorted(selected):
        doc = selected[idx]
        fname = monster_filename(idx, doc.get("name") if isinstance(doc.get("name"), str) else None)
        path = out_dir / fname
        path.write_text(dumps_pretty(doc) + "\n", encoding="utf-8")
        written += 1

    # Manifest for tooling.
    manifest = {
        "count": written,
        "bones": str(bones_path.name),
        "files": [
            monster_filename(idx, selected[idx].get("name") if isinstance(selected[idx].get("name"), str) else None)
            for idx in sorted(selected)
        ],
    }
    (out_dir / "manifest.json").write_text(dumps_pretty(manifest) + "\n", encoding="utf-8")

    print(f"wrote {written} monster file(s) + bones.json → {out_dir}")
    return 0


def patch_appearance(macro_path: Path, out_dir: Path) -> int:
    """Fill missing appearance + monsterbone rows from macro into existing JSON."""
    if not out_dir.is_dir():
        print(f"error: missing directory {out_dir}", file=sys.stderr)
        return 1
    macro_text = read_text(macro_path)
    infos: Dict[int, Dict[str, Any]] = {}
    info_forms: Dict[int, List[Any]] = {}
    for form in iter_top_forms(macro_text, "monsterinfo"):
        parsed = parse_form(form)
        fields = fields_from_form(parsed)
        index = info_index(fields.get("key"))
        if index is None:
            continue
        infos[index] = fields
        info_forms[index] = parsed

    bones: Dict[int, Dict[str, Any]] = {}
    for form in iter_top_forms(macro_text, "monsterbone"):
        parsed = parse_form(form)
        fields = fields_from_form(parsed)
        bone_key = info_index(fields.get("key"))
        if bone_key is not None:
            bones[bone_key] = build_bone_entry(fields)

    updated = 0
    for path in sorted(out_dir.glob("*.json")):
        if path.name in ("bones.json", "manifest.json"):
            continue
        try:
            doc = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            continue
        idx = doc.get("index")
        if not isinstance(idx, int):
            continue
        app = build_appearance(infos.get(idx), info_forms.get(idx))
        existing = doc.get("appearance")
        changed = False
        if isinstance(existing, dict) and existing.get("boneId"):
            if app and "clothesIndex" in app and "clothesIndex" not in existing:
                existing["clothesIndex"] = app["clothesIndex"]
                changed = True
        elif app:
            root_clothes = doc.get("clothesIndex")
            if (
                "clothesIndex" not in app
                and isinstance(root_clothes, int)
                and root_clothes > 0
            ):
                app["clothesIndex"] = root_clothes
            doc["appearance"] = app
            if "clothesIndex" in doc:
                del doc["clothesIndex"]
            changed = True
        if changed:
            path.write_text(dumps_pretty(doc) + "\n", encoding="utf-8")
            updated += 1

    bones_path = out_dir / "bones.json"
    bone_added = 0
    if bones_path.is_file() and bones:
        try:
            bone_doc = json.loads(bones_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            bone_doc = {}
        if not isinstance(bone_doc, dict):
            bone_doc = {}
        for bone_no, entry in bones.items():
            key = str(bone_no)
            if key not in bone_doc:
                bone_doc[key] = entry
                bone_added += 1
        if bone_added:
            ordered = {str(k): bone_doc[str(k)] for k in sorted(int(x) for x in bone_doc)}
            bones_path.write_text(dumps_pretty(ordered) + "\n", encoding="utf-8")

    print(
        f"patched appearance on {updated} monster file(s), "
        f"added {bone_added} bone(s) → {out_dir}"
    )
    return 0


def patch_particles(macro_path: Path, out_dir: Path) -> int:
    """Inject appearance.particles / appearance.fx from monsterinfo into existing JSON."""
    if not out_dir.is_dir():
        print(f"error: missing directory {out_dir}", file=sys.stderr)
        return 1
    macro_text = read_text(macro_path)
    by_index: Dict[int, Dict[str, List[Any]]] = {}
    for form in iter_top_forms(macro_text, "monsterinfo"):
        parsed = parse_form(form)
        fields = fields_from_form(parsed)
        index = info_index(fields.get("key"))
        if index is None:
            continue
        particles = normalize_attach(parse_repeated_fields(parsed, "particle"))
        fxs = normalize_attach(parse_repeated_fields(parsed, "fx"))
        if particles or fxs:
            by_index[index] = {"particles": particles, "fx": fxs}

    updated = 0
    for path in sorted(out_dir.glob("*.json")):
        if path.name in ("bones.json", "manifest.json"):
            continue
        try:
            doc = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            continue
        idx = doc.get("index")
        if not isinstance(idx, int) or idx not in by_index:
            continue
        app = doc.get("appearance")
        if not isinstance(app, dict):
            app = {}
            doc["appearance"] = app
        changed = False
        particles = by_index[idx]["particles"]
        fxs = by_index[idx]["fx"]
        if particles and app.get("particles") != particles:
            app["particles"] = particles
            changed = True
        elif not particles and "particles" in app:
            del app["particles"]
            changed = True
        if fxs and app.get("fx") != fxs:
            app["fx"] = fxs
            changed = True
        elif not fxs and "fx" in app:
            del app["fx"]
            changed = True
        if changed:
            path.write_text(dumps_pretty(doc) + "\n", encoding="utf-8")
            updated += 1

    print(f"patched particles/fx on {updated} monster file(s) → {out_dir}")
    return 0


def load_named_indoor_cells(mapinfo_path: Path) -> Dict[Tuple[int, int], int]:
    """InitMap (xy CX CY) → index for records that have a mapname (dungeon cells)."""
    text = read_text(mapinfo_path)
    index_re = re.compile(r"\(\s*index\s+(-?\d+)", re.IGNORECASE)
    xy_re = re.compile(r"\(\s*xy\s+(-?\d+)\s+(-?\d+)", re.IGNORECASE)
    name_re = re.compile(r"\(\s*mapname\s+\"([^\"]+)\"", re.IGNORECASE)
    cells: Dict[Tuple[int, int], int] = {}
    for line in text.splitlines():
        if "initmap" not in line.lower() or line.lstrip().startswith(";"):
            continue
        if not name_re.search(line):
            continue
        im = index_re.search(line)
        xm = xy_re.search(line)
        if not im or not xm:
            continue
        cells[(int(xm.group(1)), int(xm.group(2)))] = int(im.group(1))
    return cells


def patch_spawn_maps(mapinfo_path: Path, out_dir: Path) -> int:
    """Write InitMap indoor indices into spawn.map (D6 31→25/32, DG03 31→33, …)."""
    if not out_dir.is_dir():
        print(f"error: missing directory {out_dir}", file=sys.stderr)
        return 1
    cells = load_named_indoor_cells(mapinfo_path)
    if not cells:
        print(f"error: no named indoor cells in {mapinfo_path}", file=sys.stderr)
        return 1
    updated = 0
    remapped = 0
    for path in sorted(out_dir.glob("*.json")):
        if path.name in ("bones.json", "manifest.json"):
            continue
        try:
            doc = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            continue
        spawns = doc.get("spawns")
        if not isinstance(spawns, list):
            continue
        changed = False
        for spawn in spawns:
            if not isinstance(spawn, dict):
                continue
            rect = spawn.get("rect")
            if not isinstance(rect, list) or len(rect) < 4:
                continue
            try:
                tx0 = int(rect[0])
                ty0 = int(rect[1])
                tx1 = int(rect[2])
                ty1 = int(rect[3])
            except (TypeError, ValueError):
                continue
            cx = ((tx0 + tx1) // 2) >> 8
            cy = ((ty0 + ty1) // 2) >> 8
            named = cells.get((cx, cy))
            if named is None or named <= 0:
                continue
            old = spawn.get("map")
            if old != named:
                spawn["map"] = named
                changed = True
                remapped += 1
        if changed:
            path.write_text(dumps_pretty(doc) + "\n", encoding="utf-8")
            updated += 1
    print(
        f"patched spawn maps on {updated} monster file(s) ({remapped} rects) → {out_dir}"
    )
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    mode = ap.add_mutually_exclusive_group(required=True)
    mode.add_argument("--all", action="store_true", help="Convert every monster index found")
    mode.add_argument("--index", type=int, help="Convert a single monster index")
    mode.add_argument(
        "--patch-appearance",
        action="store_true",
        help="Fill missing appearance/bones from macro into existing JSON (no InitMonster reconvert)",
    )
    mode.add_argument(
        "--patch-particles",
        action="store_true",
        help="Inject particle/fx lists from macro into existing JSON (no InitMonster reconvert)",
    )
    mode.add_argument(
        "--patch-spawn-maps",
        action="store_true",
        help="Set spawn.map from InitMap indoor cells (no InitMonster reconvert)",
    )
    ap.add_argument("--init", type=Path, help="InitMonster.txt")
    ap.add_argument("--gen", type=Path, help="GenMonster.txt")
    ap.add_argument("--message", type=Path, help="message-e.txt (monstername)")
    ap.add_argument(
        "--macro",
        type=Path,
        help="External decoded macro file (not bundled with OpenKal)",
    )
    ap.add_argument("--mapinfo", type=Path, help="mapinfo_new.txt (for --patch-spawn-maps)")
    ap.add_argument("--out", type=Path, required=True, help="Output directory for JSON files")
    args = ap.parse_args()

    if args.patch_spawn_maps:
        if args.mapinfo is None or not args.mapinfo.is_file():
            print("error: --patch-spawn-maps requires --mapinfo", file=sys.stderr)
            return 1
        return patch_spawn_maps(args.mapinfo, args.out)

    if args.macro is None or not args.macro.is_file():
        print(f"error: missing file {args.macro}", file=sys.stderr)
        return 1

    if args.patch_appearance:
        return patch_appearance(args.macro, args.out)

    if args.patch_particles:
        return patch_particles(args.macro, args.out)

    for p in (args.init, args.gen, args.message):
        if p is None or not p.is_file():
            print(f"error: missing file {p}", file=sys.stderr)
            return 1

    return convert(
        init_path=args.init,
        gen_path=args.gen,
        message_path=args.message,
        macro_path=args.macro,
        out_dir=args.out,
        only_index=None if args.all else args.index,
    )


if __name__ == "__main__":
    sys.exit(main())

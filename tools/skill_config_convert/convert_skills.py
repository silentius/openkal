#!/usr/bin/env python3
# OpenKal - an open source KalOnline client and server.
# Copyright (C) 2026 OpenKal contributors
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

"""Convert KalOnline skill sources into one JSON per skill (and skill books).

Layout under --out (default config/skills):
  {role}/{index}_{slug}.json
  role = knight|mage|archer|thief|common|transform|monster|gale|…

Sources:
  InitSkill.txt     — server balance (class, index, mp, limit, delay, …)
  jobsystem-e.txt   — names, images, desc, formulas, type, level/grade
  SkillBook.txt     — optional → config/skillbooks/{role}/{itemIndex}.json
  message-e.txt     — optional: resolve numeric (name <sysId>) via (sys id "…")

Usage:
  ./convert_skills.py --all \\
      --init /path/to/InitSkill.txt \\
      --jobsystem config/jobsystem-e.txt \\
      --out config/skills \\
      --skillbook /path/to/SkillBook.txt \\
      --skillbooks-out config/skillbooks \\
      --message path/message-e.txt
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Tuple

CLASS_TO_ROLE = {
    0: "knight",
    1: "mage",
    2: "archer",
    3: "thief",
    -1: "transform",
    -2: "monster",
    -3: "common",
}

ROLE_TO_CLASS = {
    "knight": 0,
    "mage": 1,
    "magician": 1,
    "archer": 2,
    "thief": 3,
    "transform": -1,
    "monster": -2,
    "common": -3,
}


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
            if all(is_json_number(x) for x in o):
                return "[" + ", ".join(json.dumps(x) for x in o) + "]"
            parts = [pad_inner + fmt(x, level + 1) for x in o]
            return "[\n" + ",\n".join(parts) + "\n" + pad + "]"
        return json.dumps(o, ensure_ascii=False)

    return fmt(obj, 0)


def read_text(path: Path) -> str:
    raw = path.read_bytes().replace(b"\x00", b"")
    for enc in ("utf-8", "utf-8-sig", "cp949", "latin-1"):
        try:
            return raw.decode(enc)
        except UnicodeDecodeError:
            continue
    return raw.decode("latin-1", errors="replace")


def strip_line_comments(text: str) -> str:
    """Strip ; comments outside strings (keeps trailing InitSkill ;name via separate pass)."""
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
    text = strip_line_comments(text)
    pattern = re.compile(rf"\(\s*{re.escape(head)}(?=[\s\)])", re.IGNORECASE)
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


def skill_filename(index: int, name: Optional[str]) -> str:
    slug = sanitize_slug(name)
    base = index_pad(index)
    return f"{base}_{slug}.json" if slug else f"{base}.json"


def normalize_role(role: str) -> str:
    r = role.lower().strip()
    if r == "magician":
        return "mage"
    return r


def role_for_class(clazz: int) -> str:
    return CLASS_TO_ROLE.get(clazz, f"class_{clazz}")


def class_for_role(role: str) -> Optional[int]:
    return ROLE_TO_CLASS.get(normalize_role(role))


def list_to_value(vals: List[Any]) -> Any:
    if not vals:
        return True
    if len(vals) == 1:
        return vals[0]
    return vals


def clear_json_tree(out_dir: Path) -> None:
    if not out_dir.is_dir():
        return
    for p in sorted(out_dir.rglob("*.json"), reverse=True):
        p.unlink(missing_ok=True)
    for p in sorted((x for x in out_dir.rglob("*") if x.is_dir()), reverse=True):
        try:
            p.rmdir()
        except OSError:
            pass


def parse_init_skill_line(line: str) -> Optional[Dict[str, Any]]:
    """Parse one InitSkill line including trailing ;comment name."""
    if "(skill" not in line.lower():
        return None
    comment = ""
    semi = line.rfind(";")
    body = line
    if semi != -1:
        # Only treat as trailing comment if ; is outside the last paren… InitSkill
        # puts ;name after the closing ).
        after = line[semi + 1 :].strip()
        if after and "(" not in after:
            comment = after
            body = line[:semi]
    try:
        form = parse_form(body[body.find("(") :])
    except ValueError:
        return None
    if not isinstance(form, list) or not form or str(form[0]).lower() != "skill":
        return None

    raw: Dict[str, Any] = {}
    for item in form[1:]:
        if not isinstance(item, list) or not item:
            continue
        key = str(item[0]).lower()
        raw[key] = list_to_value(item[1:])

    clazz = raw.get("class")
    index = raw.get("index")
    if not isinstance(clazz, int) or not isinstance(index, int):
        return None

    doc: Dict[str, Any] = {
        "class": clazz,
        "role": role_for_class(clazz),
        "index": index,
    }
    if comment:
        doc["name"] = comment
        doc["comment"] = comment

    if "redistribute" in raw:
        doc["redistribute"] = raw["redistribute"]
    if "limit" in raw:
        lim = raw["limit"]
        if isinstance(lim, list):
            doc["limit"] = lim
            if lim and isinstance(lim[0], int):
                doc["levelLimit"] = lim[0]
        elif isinstance(lim, int):
            doc["limit"] = [lim]
            doc["levelLimit"] = lim
    if "maxlevel" in raw:
        doc["maxLevel"] = raw["maxlevel"]
    if "mp" in raw:
        doc["mp"] = raw["mp"]
    if "lasttime" in raw:
        doc["lastTime"] = raw["lasttime"]
    if "delay" in raw:
        d = raw["delay"]
        doc["delay"] = d if isinstance(d, list) else [d]
    if "value1" in raw:
        doc["value1"] = raw["value1"]
    if "value2" in raw:
        doc["value2"] = raw["value2"]
    if "rage" in raw:
        doc["rage"] = raw["rage"]
    return doc


def load_init_skills(path: Path) -> Dict[Tuple[int, int], Dict[str, Any]]:
    text = read_text(path)
    out: Dict[Tuple[int, int], Dict[str, Any]] = {}
    for line in text.splitlines():
        doc = parse_init_skill_line(line)
        if not doc:
            continue
        out[(int(doc["class"]), int(doc["index"]))] = doc
    return out


def load_sys_names(message_path: Path) -> Dict[int, str]:
    names: Dict[int, str] = {}
    text = read_text(message_path)
    for form in iter_top_forms(text, "sys"):
        try:
            parsed = parse_form(form)
        except ValueError:
            continue
        if isinstance(parsed, list) and len(parsed) >= 3 and str(parsed[0]).lower() == "sys":
            try:
                sid = int(parsed[1])
            except (TypeError, ValueError):
                continue
            if isinstance(parsed[2], str):
                names[sid] = parsed[2]
    return names


def resolve_name(val: Any, sys_names: Dict[int, str]) -> Optional[str]:
    if isinstance(val, str):
        return val
    if isinstance(val, int):
        return sys_names.get(val)
    if isinstance(val, list):
        # Prefer first English-looking string.
        for item in val:
            if isinstance(item, str) and any("a" <= c.lower() <= "z" for c in item):
                return item
        for item in val:
            if isinstance(item, str):
                return item
            if isinstance(item, int) and item in sys_names:
                return sys_names[item]
    return None


def jobsystem_field_bag(form: List[Any]) -> Dict[str, Any]:
    """Collect jobsystem (skill …) fields; multi-valued keys become lists of entries."""
    bag: Dict[str, List[Any]] = {}
    for item in form[1:]:
        if not isinstance(item, list) or not item:
            continue
        key = str(item[0]).lower()
        bag.setdefault(key, []).append(item[1:])
    return bag


def first_vals(bag: Dict[str, List[Any]], key: str) -> List[Any]:
    entries = bag.get(key) or []
    return entries[0] if entries else []


def build_job_doc(form: List[Any], sys_names: Dict[int, str]) -> Optional[Dict[str, Any]]:
    if not isinstance(form, list) or not form or str(form[0]).lower() != "skill":
        return None
    bag = jobsystem_field_bag(form)

    key_vals = first_vals(bag, "key")
    key = key_vals[0] if key_vals and isinstance(key_vals[0], str) else None

    action_vals = first_vals(bag, "action")
    index_vals = first_vals(bag, "index")
    action = action_vals[0] if action_vals and isinstance(action_vals[0], int) else None
    index = index_vals[0] if index_vals and isinstance(index_vals[0], int) else action
    if index is None:
        return None

    limit_vals = first_vals(bag, "limit")
    role = None
    if limit_vals and isinstance(limit_vals[0], str):
        role = normalize_role(limit_vals[0])
    elif key and "-" in key:
        role = normalize_role(key.split("-", 1)[0])
    if not role:
        role = "common"

    clazz = class_for_role(role)
    if clazz is None:
        # Animal / special transform groups — keep role folder, class unset-ish.
        clazz = -100

    name = resolve_name(list_to_value(first_vals(bag, "name")), sys_names)

    doc: Dict[str, Any] = {
        "class": clazz if clazz != -100 else role,
        "role": role,
        "index": int(index),
    }
    if key:
        doc["key"] = key
    if action is not None:
        doc["action"] = action
    if name:
        doc["name"] = name

    image = list_to_value(first_vals(bag, "image"))
    if image not in (None, True, []):
        doc["image"] = image

    for src, dst in (
        ("desc", "desc"),
        ("ability", "ability"),
        ("type", "type"),
        ("motion", "motion"),
        ("effect", "effect"),
        ("sound", "sound"),
        ("explosion", "explosion"),
        ("damagetype", "damageType"),
        ("property", "property"),
        ("maxparamvalue", "maxParamValue"),
    ):
        vals = first_vals(bag, src)
        if vals:
            doc[dst] = list_to_value(vals)

    # level / grade / maxlevel often (level 5 0)
    for src, dst in (("level", "level"), ("grade", "grade"), ("maxlevel", "maxLevel")):
        vals = first_vals(bag, src)
        if vals:
            doc[dst] = list_to_value(vals)

    # Multi-entry fields.
    if "parameter" in bag:
        params = []
        for entry in bag["parameter"]:
            if not entry:
                continue
            pid = entry[0]
            expr = list_to_value(entry[1:]) if len(entry) > 1 else True
            params.append({"id": pid, "expr": expr})
        if params:
            doc["parameters"] = params

    if "compare" in bag:
        compares = []
        for entry in bag["compare"]:
            if not entry:
                continue
            cid = entry[0]
            expr = list_to_value(entry[1:]) if len(entry) > 1 else True
            compares.append({"id": cid, "expr": expr})
        if compares:
            doc["compares"] = compares

    # Preserve unknown single-value fields under extra.
    known = {
        "key",
        "name",
        "limit",
        "action",
        "index",
        "image",
        "desc",
        "ability",
        "type",
        "motion",
        "effect",
        "sound",
        "explosion",
        "damagetype",
        "property",
        "maxparamvalue",
        "level",
        "grade",
        "maxlevel",
        "parameter",
        "compare",
    }
    extra: Dict[str, Any] = {}
    for k, entries in bag.items():
        if k in known:
            continue
        if len(entries) == 1:
            extra[k] = list_to_value(entries[0])
        else:
            extra[k] = [list_to_value(e) for e in entries]
    if extra:
        doc["extra"] = extra

    # Client level requirement hint from jobsystem (level N …).
    if "level" in doc:
        lv = doc["level"]
        if isinstance(lv, list) and lv and isinstance(lv[0], int):
            doc.setdefault("levelLimit", lv[0])
        elif isinstance(lv, int):
            doc.setdefault("levelLimit", lv)

    if "maxLevel" in doc:
        ml = doc["maxLevel"]
        if isinstance(ml, list) and ml and isinstance(ml[0], int):
            doc["maxLevel"] = ml[0]

    return doc


def load_jobsystem_skills(path: Path, sys_names: Dict[int, str]) -> List[Dict[str, Any]]:
    text = read_text(path)
    docs: List[Dict[str, Any]] = []
    for form in iter_top_forms(text, "skill"):
        try:
            parsed = parse_form(form)
        except ValueError as e:
            print(f"warn: jobsystem parse failed: {e}", file=sys.stderr)
            continue
        doc = build_job_doc(parsed, sys_names)
        if doc:
            docs.append(doc)
    return docs


def merge_job_into_init(init: Dict[str, Any], job: Dict[str, Any]) -> None:
    """Enrich InitSkill doc with jobsystem client fields (do not clobber balance)."""
    for k in (
        "key",
        "name",
        "image",
        "desc",
        "ability",
        "type",
        "motion",
        "effect",
        "sound",
        "explosion",
        "damageType",
        "property",
        "parameters",
        "compares",
        "level",
        "grade",
        "action",
        "maxParamValue",
        "extra",
    ):
        if k in job and k not in init:
            init[k] = job[k]
        elif k in job and k == "name" and init.get("name") in (None, "", init.get("comment")):
            # Prefer jobsystem display name over InitSkill ;comment when available.
            init[k] = job[k]
    if "name" in job:
        init["name"] = job["name"]
    if "maxLevel" in job and "maxLevel" not in init:
        init["maxLevel"] = job["maxLevel"]
    # Keep InitSkill levelLimit as authoritative when present.


def match_job_to_init(
    init_docs: Dict[Tuple[int, int], Dict[str, Any]], job: Dict[str, Any]
) -> Optional[Tuple[int, int]]:
    role = normalize_role(str(job.get("role", "")))
    index = int(job["index"])
    clazz = class_for_role(role)
    if clazz is not None and (clazz, index) in init_docs:
        return (clazz, index)
    # Common jobsystem skills apply to every class that has that index.
    if role == "common":
        return None  # handled by broadcast enrich
    return None


def enrich_common(init_docs: Dict[Tuple[int, int], Dict[str, Any]], job: Dict[str, Any]) -> None:
    index = int(job["index"])
    for (clazz, idx), doc in init_docs.items():
        if idx == index and clazz in (0, 1, 2, 3):
            merge_job_into_init(doc, job)


def skill_relpath(doc: Dict[str, Any]) -> str:
    role = normalize_role(str(doc.get("role") or role_for_class(int(doc.get("class", -3)))))
    name = doc.get("name") if isinstance(doc.get("name"), str) else None
    return f"{role}/{skill_filename(int(doc['index']), name)}"


def convert_skills(
    init_path: Path,
    job_path: Path,
    out_dir: Path,
    message_path: Optional[Path],
    only_index: Optional[int],
) -> int:
    sys_names = load_sys_names(message_path) if message_path and message_path.is_file() else {}
    init_docs = load_init_skills(init_path)
    job_docs = load_jobsystem_skills(job_path, sys_names)

    used_job: set[int] = set()
    for i, job in enumerate(job_docs):
        role = normalize_role(str(job.get("role", "")))
        if role == "common":
            enrich_common(init_docs, job)
            used_job.add(i)
            continue
        key = match_job_to_init(init_docs, job)
        if key is not None:
            merge_job_into_init(init_docs[key], job)
            used_job.add(i)

    # Jobsystem-only skills (thief, animals, extras not in InitSkill).
    extras: List[Dict[str, Any]] = []
    for i, job in enumerate(job_docs):
        if i in used_job:
            continue
        role = normalize_role(str(job.get("role", "")))
        clazz = class_for_role(role)
        index = int(job["index"])
        if clazz is not None and (clazz, index) in init_docs:
            # Already merged via match.
            continue
        # Avoid duplicating common into a folder when InitSkill already has per-class copies.
        if role == "common":
            continue
        extras.append(job)

    all_docs: List[Dict[str, Any]] = list(init_docs.values()) + extras
    if only_index is not None:
        all_docs = [d for d in all_docs if int(d["index"]) == only_index]
        if not all_docs:
            print(f"error: index {only_index} not found", file=sys.stderr)
            return 1

    out_dir.mkdir(parents=True, exist_ok=True)
    if only_index is None:
        clear_json_tree(out_dir)

    files: List[str] = []
    # Disambiguate same role+index collisions (rare) by appending class.
    seen: Dict[str, int] = {}
    for doc in sorted(
        all_docs,
        key=lambda d: (str(d.get("role")), int(d["index"]) if isinstance(d.get("class"), int) else 0, str(d.get("class"))),
    ):
        rel = skill_relpath(doc)
        if rel in seen:
            stem = Path(rel).stem
            role = Path(rel).parent.as_posix()
            suffix = doc.get("class")
            rel = f"{role}/{stem}_c{suffix}.json"
        seen[rel] = seen.get(rel, 0) + 1
        path = out_dir / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(dumps_pretty(doc) + "\n", encoding="utf-8")
        files.append(rel)

    manifest = {
        "count": len(files),
        "sources": {
            "initSkill": str(init_path),
            "jobsystem": str(job_path),
            "message": str(message_path) if message_path else None,
        },
        "files": files,
    }
    (out_dir / "manifest.json").write_text(dumps_pretty(manifest) + "\n", encoding="utf-8")
    print(f"wrote {len(files)} skill file(s) → {out_dir}")
    return 0


def convert_skillbooks(skillbook_path: Path, out_dir: Path) -> int:
    text = read_text(skillbook_path)
    docs: List[Dict[str, Any]] = []
    for form in iter_top_forms(text, "skillbook"):
        try:
            parsed = parse_form(form)
        except ValueError:
            continue
        if not isinstance(parsed, list) or not parsed:
            continue
        raw: Dict[str, Any] = {}
        for item in parsed[1:]:
            if isinstance(item, list) and item:
                raw[str(item[0]).lower()] = list_to_value(item[1:])
        clazz = raw.get("class")
        index = raw.get("index")
        if not isinstance(clazz, int) or not isinstance(index, int):
            continue
        doc = {
            "class": clazz,
            "role": role_for_class(clazz),
            "itemIndex": index,
            "action": raw.get("action"),
            "upgradeAmount": raw.get("upgradeamount"),
            "maxGrade": raw.get("maxgrade"),
        }
        docs.append(doc)

    out_dir.mkdir(parents=True, exist_ok=True)
    clear_json_tree(out_dir)
    files: List[str] = []
    for doc in sorted(docs, key=lambda d: (d["class"], d["itemIndex"])):
        rel = f"{doc['role']}/{index_pad(doc['itemIndex'])}.json"
        path = out_dir / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(dumps_pretty(doc) + "\n", encoding="utf-8")
        files.append(rel)
    (out_dir / "manifest.json").write_text(
        dumps_pretty({"count": len(files), "files": files}) + "\n", encoding="utf-8"
    )
    print(f"wrote {len(files)} skillbook file(s) → {out_dir}")
    return 0


def convert_old_skill_damage(path: Path, out_dir: Path) -> int:
    text = read_text(path)
    docs: List[Dict[str, Any]] = []
    for form in iter_top_forms(text, "updatedamage"):
        try:
            parsed = parse_form(form)
        except ValueError:
            continue
        if not isinstance(parsed, list) or not parsed:
            continue
        raw: Dict[str, Any] = {}
        for item in parsed[1:]:
            if isinstance(item, list) and item:
                raw[str(item[0]).lower()] = list_to_value(item[1:])
        clazz = raw.get("class")
        action = raw.get("action")
        if not isinstance(clazz, int) or not isinstance(action, int):
            continue
        docs.append(
            {
                "class": clazz,
                "role": role_for_class(clazz),
                "action": action,
                "rate": raw.get("rate"),
            }
        )
    out_dir.mkdir(parents=True, exist_ok=True)
    clear_json_tree(out_dir)
    files: List[str] = []
    for doc in sorted(docs, key=lambda d: (d["class"], d["action"])):
        rel = f"{doc['role']}/{index_pad(doc['action'])}.json"
        path_out = out_dir / rel
        path_out.parent.mkdir(parents=True, exist_ok=True)
        path_out.write_text(dumps_pretty(doc) + "\n", encoding="utf-8")
        files.append(rel)
    (out_dir / "manifest.json").write_text(
        dumps_pretty({"count": len(files), "files": files}) + "\n", encoding="utf-8"
    )
    print(f"wrote {len(files)} skill-damage file(s) → {out_dir}")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    mode = ap.add_mutually_exclusive_group(required=True)
    mode.add_argument("--all", action="store_true")
    mode.add_argument("--index", type=int)
    ap.add_argument("--init", type=Path, required=True, help="InitSkill.txt")
    ap.add_argument("--jobsystem", type=Path, required=True, help="jobsystem-e.txt")
    ap.add_argument("--out", type=Path, required=True, help="Output dir for skill JSON")
    ap.add_argument("--message", type=Path, help="message-e.txt for (sys) name ids")
    ap.add_argument("--skillbook", type=Path, help="SkillBook.txt")
    ap.add_argument("--skillbooks-out", type=Path, help="Output dir for skillbook JSON")
    ap.add_argument("--old-damage", type=Path, help="OldSkillDamage.txt")
    ap.add_argument("--old-damage-out", type=Path, help="Output dir for skill damage JSON")
    args = ap.parse_args()

    for p in (args.init, args.jobsystem):
        if not p.is_file():
            print(f"error: missing file {p}", file=sys.stderr)
            return 1

    rc = convert_skills(
        init_path=args.init,
        job_path=args.jobsystem,
        out_dir=args.out,
        message_path=args.message,
        only_index=None if args.all else args.index,
    )
    if rc != 0:
        return rc
    if args.skillbook and args.skillbooks_out:
        if not args.skillbook.is_file():
            print(f"error: missing skillbook {args.skillbook}", file=sys.stderr)
            return 1
        rc = convert_skillbooks(args.skillbook, args.skillbooks_out)
        if rc != 0:
            return rc
    if args.old_damage and args.old_damage_out:
        if not args.old_damage.is_file():
            print(f"error: missing old damage {args.old_damage}", file=sys.stderr)
            return 1
        return convert_old_skill_damage(args.old_damage, args.old_damage_out)
    return 0


if __name__ == "__main__":
    sys.exit(main())

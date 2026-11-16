#!/usr/bin/env python3
# OpenKal - an open source KalOnline client and server.
# Copyright (C) 2026 OpenKal contributors
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

"""Convert the read-only KalOnline pet configuration into OpenKal JSON.

The output is intentionally self-contained.  OpenKal must load the generated
JSON files at runtime and must never fall back to the legacy text files.

Usage:
  ./convert_pets.py \\
      --pet-source /path/to/Pet.txt \\
      --monster-recipe-source /path/to/MonsterPet.txt \\
      --monster-price-source /path/to/CreateMonsterPet.dat \\
      --item-source /path/to/InitItem.dat \\
      --macro-source /path/to/macro.dat \\
      --switch-model-source /path/to/switchmodel.dat \\
      --out config/pets
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Sequence


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
    """Read a legacy configuration file, including padded files."""
    raw = path.read_bytes().rstrip(b"\0")
    for encoding in ("utf-8-sig", "utf-8", "cp949", "latin-1"):
        try:
            return raw.decode(encoding)
        except UnicodeDecodeError:
            continue
    return raw.decode("latin-1", errors="replace")


def strip_comments(text: str) -> str:
    lines: List[str] = []
    for line in text.splitlines():
        quoted = False
        escaped = False
        result: List[str] = []
        for char in line:
            if char == '"' and not escaped:
                quoted = not quoted
            if char == ";" and not quoted:
                break
            result.append(char)
            escaped = char == "\\" and not escaped
            if char != "\\":
                escaped = False
        lines.append("".join(result))
    return "\n".join(lines)


def iter_forms(text: str, head: str) -> Iterable[str]:
    """Yield complete s-expression forms with *head*, preserving nested data."""
    pattern = re.compile(rf"\(\s*{re.escape(head)}(?=[\s\)])", re.IGNORECASE)
    text = strip_comments(text)
    search_start = 0
    while match := pattern.search(text, search_start):
        depth = 0
        in_string = False
        escaped = False
        index = match.start()
        while index < len(text):
            char = text[index]
            if char == '"' and not escaped:
                in_string = not in_string
            elif not in_string:
                if char == "(":
                    depth += 1
                elif char == ")":
                    depth -= 1
                    if depth == 0:
                        yield text[match.start() : index + 1]
                        search_start = index + 1
                        break
            escaped = char == "\\" and not escaped
            if char != "\\":
                escaped = False
            index += 1
        else:
            return


def field_tokens(form: str, field: str) -> Optional[List[str]]:
    match = re.search(rf"\(\s*{re.escape(field)}\s+([^)]*?)\s*\)", form, re.IGNORECASE)
    if not match:
        return None
    return re.findall(r'"(?:\\.|[^"\\])*"|[^\s()]+', match.group(1))


def integer_tokens(form: str, field: str) -> Optional[List[int]]:
    tokens = field_tokens(form, field)
    if tokens is None:
        return None
    try:
        return [int(token) for token in tokens]
    except ValueError:
        return None


def number_tokens(form: str, field: str) -> Optional[List[float]]:
    tokens = field_tokens(form, field)
    if tokens is None:
        return None
    try:
        return [float(token) for token in tokens]
    except ValueError:
        return None


def quoted_or_token(token: str) -> str:
    if len(token) >= 2 and token[0] == '"' and token[-1] == '"':
        return bytes(token[1:-1], "utf-8").decode("unicode_escape")
    return token


def parse_standard_pets(text: str) -> List[Dict[str, Any]]:
    pets: List[Dict[str, Any]] = []
    for form in iter_forms(text, "pet"):
        item_index = integer_tokens(form, "index")
        duration = integer_tokens(form, "time")
        heal = integer_tokens(form, "heal")
        auto_pickup = integer_tokens(form, "autopick")
        if not item_index or not duration or not heal or not auto_pickup:
            raise ValueError(f"Invalid pet record: {form}")
        if len(item_index) != len(duration) != len(heal) != len(auto_pickup):
            raise ValueError(f"Invalid pet record fields: {form}")
        pets.append(
            {
                "itemIndex": item_index[0],
                "kind": "standard",
                "durationSeconds": duration[0],
                "healPerTwoSeconds": heal[0],
                "autoPickup": bool(auto_pickup[0]),
                "visualKey": None,
            }
        )
    if not pets:
        raise ValueError("No standard pet records found")
    if len({pet["itemIndex"] for pet in pets}) != len(pets):
        raise ValueError("Duplicate standard pet item index")
    return pets


def parse_monster_recipes(text: str) -> List[Dict[str, Any]]:
    """Parse the incomplete line-oriented MonsterPet.txt source safely."""
    recipes: List[Dict[str, Any]] = []
    for line in strip_comments(text).splitlines():
        if not re.search(r"\(\s*monsterpet\b", line, re.IGNORECASE):
            continue
        pet_match = re.search(r"\(\s*pet\s+(\d+)\s*\)", line, re.IGNORECASE)
        material_matches = re.findall(
            r"\(\s*index\s+(\d+)\s*\)\s*\(\s*count\s+(\d+)\s*\)",
            line,
            re.IGNORECASE,
        )
        if not pet_match or len(material_matches) != 2:
            raise ValueError(f"Invalid monster pet recipe: {line}")
        recipes.append(
            {
                "itemIndex": int(pet_match.group(1)),
                "materials": [
                    {"itemIndex": int(item_index), "count": int(count)}
                    for item_index, count in material_matches
                ],
            }
        )
    if not recipes:
        raise ValueError("No monster pet recipes found")
    if len({recipe["itemIndex"] for recipe in recipes}) != len(recipes):
        raise ValueError("Duplicate monster pet recipe item index")
    return sorted(recipes, key=lambda recipe: recipe["itemIndex"])


def apply_monster_recipe_prices(recipes: Sequence[Dict[str, Any]], text: str) -> List[Dict[str, Any]]:
    """Validate recipe contents and import their gold price from XML source."""
    prices: Dict[int, int] = {}
    materials_by_item: Dict[int, List[Dict[str, int]]] = {}
    for match in re.finditer(
        r'<\s*monster_pet\s+index\s*=\s*"(\d+)"\s*>(.*?)</\s*monster_pet\s*>',
        text,
        re.IGNORECASE | re.DOTALL,
    ):
        item_index = int(match.group(1))
        price_match = re.search(r'\bmoney\s*=\s*"(\d+)"', match.group(2), re.IGNORECASE)
        if not price_match:
            raise ValueError(f"Monster pet {item_index} is missing a gold price")
        prices[item_index] = int(price_match.group(1))
        materials_by_item[item_index] = [
            {"itemIndex": int(material_index), "count": int(count)}
            for material_index, count in re.findall(
                r'<\s*material\s+index\s*=\s*"(\d+)"\s+count\s*=\s*"(\d+)"\s*/\s*>',
                match.group(2),
                re.IGNORECASE,
            )
        ]
    if set(prices) != {recipe["itemIndex"] for recipe in recipes}:
        raise ValueError("MonsterPet.txt and CreateMonsterPet.dat define different recipe indices")
    priced: List[Dict[str, Any]] = []
    for recipe in recipes:
        if materials_by_item[recipe["itemIndex"]] != recipe["materials"]:
            raise ValueError(f"Monster pet {recipe['itemIndex']} has mismatched materials between sources")
        record = dict(recipe)
        record["gold"] = prices[record["itemIndex"]]
        priced.append(record)
    return priced


def parse_item_metadata(text: str) -> Dict[int, Dict[str, Any]]:
    metadata: Dict[int, Dict[str, Any]] = {}
    for form in iter_forms(text, "item"):
        item_index = integer_tokens(form, "index")
        item_class = field_tokens(form, "class")
        if not item_index or not item_class:
            continue
        macro_key = integer_tokens(form, "macrokey")
        metadata[item_index[0]] = {
            "class": " ".join(item_class).lower(),
            "visualKey": macro_key[0] if macro_key else None,
        }
    return metadata


def parse_pet_visuals(text: str) -> List[Dict[str, Any]]:
    visuals: List[Dict[str, Any]] = []
    for form in iter_forms(text, "petinfo"):
        key = integer_tokens(form, "key")
        bone = integer_tokens(form, "bone")
        scale = number_tokens(form, "scale")
        movement_speed = integer_tokens(form, "mspeed")
        part = integer_tokens(form, "part")
        position = integer_tokens(form, "petpos")
        pet_type = integer_tokens(form, "pettype")
        motion = field_tokens(form, "motion")
        if not key or not bone or not scale or not movement_speed or not part or not position or not pet_type or not motion:
            raise ValueError(f"Invalid pet visual: {form}")
        visual: Dict[str, Any] = {
            "key": key[0],
            "variantKey": key[1] if len(key) > 1 else None,
            "bone": bone[0],
            "scale": scale[0],
            "scaleMode": int(scale[1]) if len(scale) > 1 else None,
            "motion": quoted_or_token(motion[0]),
            "movementSpeed": {"move": movement_speed[0], "run": movement_speed[1]},
            "part": {"index": part[0], "variant": part[1]},
            "position": position[0],
            "petType": pet_type[0],
        }
        custom = field_tokens(form, "custom")
        monster_type = integer_tokens(form, "type")
        recall_animation = number_tokens(form, "recallani")
        fx = field_tokens(form, "fx")
        particles = re.findall(
            r'\(\s*particle\s+("(?:\\.|[^"\\])*"|[^\s()]+)\s+(-?\d+)\s+'
            r'(-?\d+(?:\.\d+)?)\s+(-?\d+(?:\.\d+)?)\s+(-?\d+(?:\.\d+)?)\s*\)',
            form,
            re.IGNORECASE,
        )
        if custom:
            visual["customModel"] = quoted_or_token(custom[0])
        if monster_type:
            visual["monsterType"] = monster_type[0]
        if recall_animation:
            visual["recallAnimationSeconds"] = recall_animation[0]
        if fx:
            visual["fx"] = [quoted_or_token(token) for token in fx]
        if particles:
            visual["particles"] = [
                {
                    "name": quoted_or_token(name),
                    "bone": int(particle_bone),
                    "offset": [float(x), float(y), float(z)],
                }
                for name, particle_bone, x, y, z in particles
            ]
        visuals.append(visual)
    if not visuals:
        raise ValueError("No pet visual records found")
    if len({visual["key"] for visual in visuals}) != len(visuals):
        raise ValueError("Duplicate pet visual key")
    return sorted(visuals, key=lambda visual: visual["key"])


def parse_pet_bones(text: str) -> List[Dict[str, Any]]:
    bones: List[Dict[str, Any]] = []
    for form in iter_forms(text, "petbone"):
        key = integer_tokens(form, "key")
        bone = integer_tokens(form, "bone")
        target = number_tokens(form, "target")
        motion = integer_tokens(form, "motion")
        if not key or bone is None or target is None or motion is None:
            raise ValueError(f"Invalid pet bone: {form}")
        bones.append({"key": key[0], "bones": bone, "target": target, "motion": motion})
    return sorted(bones, key=lambda bone: bone["key"])


def parse_switch_models(text: str) -> List[Dict[str, Any]]:
    models_by_name: Dict[str, Dict[str, Any]] = {}
    for form in iter_forms(text, "switchmodel"):
        name = field_tokens(form, "name")
        meshes = field_tokens(form, "gb")
        textures = field_tokens(form, "texture")
        if not name or not meshes or textures is None:
            raise ValueError(f"Invalid switch model: {form}")
        model = {
            "name": quoted_or_token(name[0]),
            "meshes": [quoted_or_token(mesh) for mesh in meshes],
            "textures": [quoted_or_token(texture) for texture in textures],
        }
        existing = models_by_name.get(model["name"])
        if existing is not None and existing != model:
            raise ValueError(f"Conflicting switch model {model['name']}")
        models_by_name[model["name"]] = model
    return sorted(models_by_name.values(), key=lambda model: model["name"])


def build_catalog(
    standard_pets: Sequence[Dict[str, Any]],
    recipes: Sequence[Dict[str, Any]],
    item_metadata: Dict[int, Dict[str, Any]],
) -> List[Dict[str, Any]]:
    catalog: List[Dict[str, Any]] = []
    for pet in standard_pets:
        record = dict(pet)
        metadata = item_metadata.get(record["itemIndex"])
        if metadata:
            record["visualKey"] = metadata["visualKey"]
            if metadata["class"] == "limitedpet":
                record["kind"] = "limited"
        catalog.append(record)
    for recipe in recipes:
        metadata = item_metadata.get(recipe["itemIndex"])
        catalog.append(
            {
                "itemIndex": recipe["itemIndex"],
                "kind": "monster",
                "durationSeconds": None,
                "healPerTwoSeconds": 0,
                "autoPickup": False,
                "visualKey": metadata["visualKey"] if metadata else None,
            }
        )
    catalog.sort(key=lambda pet: pet["itemIndex"])
    if len({pet["itemIndex"] for pet in catalog}) != len(catalog):
        raise ValueError("Duplicate catalog item index")
    return catalog


def apply_presentation_overrides(
    catalog: Sequence[Dict[str, Any]], overrides_source: Path
) -> List[Dict[str, Any]]:
    """Merge reviewed item-to-model bindings absent from legacy Pet.txt."""
    try:
        document = json.loads(overrides_source.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError(f"Cannot read pet presentation overrides: {error}") from error
    if document.get("schema") != "openkal.pet-presentation-overrides/v1":
        raise ValueError("Unsupported pet presentation override schema")

    merged = {record["itemIndex"]: dict(record) for record in catalog}
    allowed_fields = {"visualKey", "monsterType", "modelScale"}
    for override in document.get("overrides", []):
        item_index = override.get("itemIndex")
        if not isinstance(item_index, int) or item_index not in merged:
            raise ValueError(f"Presentation override has unknown item index: {item_index}")
        if any(key not in allowed_fields | {"itemIndex"} for key in override):
            raise ValueError(f"Presentation override has unsupported fields for {item_index}")
        merged[item_index].update({key: value for key, value in override.items() if key != "itemIndex"})

    for record in document.get("supplementalPets", []):
        item_index = record.get("itemIndex")
        if not isinstance(item_index, int) or item_index in merged:
            raise ValueError(f"Invalid supplemental pet item index: {item_index}")
        required = {
            "itemIndex",
            "kind",
            "durationSeconds",
            "healPerTwoSeconds",
            "autoPickup",
            "visualKey",
        }
        if set(record) != required:
            raise ValueError(f"Invalid supplemental pet record: {item_index}")
        merged[item_index] = dict(record)
    return [merged[item_index] for item_index in sorted(merged)]


def convert(
    pet_source: Path,
    monster_recipe_source: Path,
    monster_price_source: Path,
    item_source: Path,
    macro_source: Path,
    switch_model_source: Path,
    out_dir: Path,
    presentation_overrides_source: Optional[Path] = None,
) -> Dict[str, int]:
    standard_pets = parse_standard_pets(read_text(pet_source))
    recipes = apply_monster_recipe_prices(
        parse_monster_recipes(read_text(monster_recipe_source)), read_text(monster_price_source)
    )
    item_metadata = parse_item_metadata(read_text(item_source))
    visuals = parse_pet_visuals(read_text(macro_source))
    bones = parse_pet_bones(read_text(macro_source))
    custom_model_names = {visual["customModel"] for visual in visuals if "customModel" in visual}
    switch_models = [
        model for model in parse_switch_models(read_text(switch_model_source)) if model["name"] in custom_model_names
    ]
    if {model["name"] for model in switch_models} != custom_model_names:
        raise ValueError("A pet visual references a missing switch model")
    catalog = build_catalog(standard_pets, recipes, item_metadata)
    if presentation_overrides_source is not None:
        catalog = apply_presentation_overrides(catalog, presentation_overrides_source)

    visual_keys = {visual["key"] for visual in visuals}
    for record in catalog:
        visual_key = record["visualKey"]
        if visual_key is not None and visual_key not in visual_keys:
            raise ValueError(f"Pet {record['itemIndex']} references missing visual key {visual_key}")

    out_dir.mkdir(parents=True, exist_ok=True)
    files = {
        "catalog.json": {"schema": "openkal.pets/v1", "pets": catalog},
        "visuals.json": {
            "schema": "openkal.pet-visuals/v1",
            "visuals": visuals,
            "bones": bones,
            "switchModels": switch_models,
        },
        "monster_recipes.json": {"schema": "openkal.monster-pet-recipes/v1", "recipes": recipes},
        "manifest.json": {
            "schema": "openkal.pet-manifest/v1",
            "files": ["catalog.json", "visuals.json", "monster_recipes.json"],
        },
    }
    for name, document in files.items():
        (out_dir / name).write_text(dumps_pretty(document), encoding="utf-8")
    return {
        "catalog": len(catalog),
        "standard": len(standard_pets),
        "recipes": len(recipes),
        "visuals": len(visuals),
        "bones": len(bones),
        "switchModels": len(switch_models),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--pet-source", type=Path, required=True, help="Legacy Pet.txt")
    parser.add_argument("--monster-recipe-source", type=Path, required=True, help="Legacy MonsterPet.txt")
    parser.add_argument("--monster-price-source", type=Path, required=True, help="Later CreateMonsterPet.dat")
    parser.add_argument("--item-source", type=Path, required=True, help="Later InitItem.dat")
    parser.add_argument("--macro-source", type=Path, required=True, help="Later macro.dat")
    parser.add_argument("--switch-model-source", type=Path, required=True, help="Later switchmodel.dat")
    parser.add_argument(
        "--presentation-overrides",
        type=Path,
        help="Reviewed JSON item-to-visual bindings not present in Pet.txt",
    )
    parser.add_argument("--out", type=Path, required=True, help="Output directory")
    args = parser.parse_args()

    sources = [
        args.pet_source,
        args.monster_recipe_source,
        args.monster_price_source,
        args.item_source,
        args.macro_source,
        args.switch_model_source,
    ]
    if args.presentation_overrides is not None:
        sources.append(args.presentation_overrides)
    for source in sources:
        if not source.is_file():
            print(f"error: missing source file {source}", file=sys.stderr)
            return 1
    try:
        counts = convert(
            pet_source=args.pet_source,
            monster_recipe_source=args.monster_recipe_source,
            monster_price_source=args.monster_price_source,
            item_source=args.item_source,
            macro_source=args.macro_source,
            switch_model_source=args.switch_model_source,
            out_dir=args.out,
            presentation_overrides_source=args.presentation_overrides,
        )
    except ValueError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(
        "wrote {catalog} catalog pets ({standard} standard, {recipes} monster recipes), "
        "{visuals} visuals, {bones} bone records, and {switchModels} switch models to {out}".format(
            out=args.out, **counts
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())

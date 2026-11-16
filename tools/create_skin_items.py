#!/usr/bin/env python3
"""Create project-defined cosmetic item definitions from an optional local mesh inventory.

Only filenames and guarded name/compatibility mappings are inspected. No external
item tables, stats or art are imported.
The output must be a new directory so a curated catalog cannot be overwritten.
"""

import argparse
import json
from pathlib import Path
import re

from skin_catalog import (apply_descriptive_name, apply_weapon_compatibility,
                          costume_replaces_face, knight_weapon_type,
                          load_descriptive_name_catalog, load_name_catalog,
                          load_weapon_compatibility, load_weapon_templates, matches_name_model)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("data_root", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--names", type=Path,
                        default=Path(__file__).resolve().parent.parent / "config/skins/names.json")
    parser.add_argument("--descriptive-names", type=Path,
                        default=Path(__file__).resolve().parent.parent /
                        "config/skins/descriptive-names.json")
    parser.add_argument("--weapon-compatibility", type=Path,
                        default=Path(__file__).resolve().parent.parent /
                        "config/skins/weapon-compatibility.json")
    parser.add_argument("--catalog", type=Path,
                        default=Path(__file__).resolve().parent.parent / "config/skins/catalog.json")
    args = parser.parse_args()
    _, names = load_name_catalog(args.names)
    _, descriptive_names = load_descriptive_name_catalog(args.descriptive_names)
    _, compatibility = load_weapon_compatibility(args.weapon_compatibility)
    _, weapon_templates = load_weapon_templates(
        Path(__file__).resolve().parent.parent / 'config/skins/weapon-templates.json')
    if args.output.exists() and any(args.output.iterdir()):
        parser.error("output directory must be empty")
    jobs = {"w": (0, "Knight"), "r": (1, "Magician"),
            "m": (2, "Archer"), "t": (3, "Thief")}
    species = {"s": jobs["w"], "g": jobs["r"], "b": jobs["m"], "d": jobs["t"]}
    rows = set()
    for file in (args.data_root / "Model/Clothes").iterdir():
        match = re.fullmatch(r"C([wrmt])_(\d+)_C(\d+)\.gb", file.name, re.I)
        if match:
            job, group, variant = match.groups()
            rows.add((*jobs[job.lower()], "body", int(group), int(variant)))
    for file in (args.data_root / "Item/Weapon").iterdir():
        match = re.fullmatch(r"P([sgbd])_S(\d+)_(\d+)\.gb", file.name, re.I)
        if match:
            job, group, variant = match.groups()
            # Cosmetic asset families, including the older S21/S22/S23 models.
            if int(group) in (21, 22, 23) or int(group) >= 40:
                rows.add((*species[job.lower()], "weapon", int(group), int(variant)))
    # Per-part samples make every supported cosmetic slot testable.
    for letter, (job, name) in jobs.items():
        for part, suffix in (("helmet", "h"), ("upper", "a"), ("pants", "p"),
                             ("boots", "s"), ("gauntlet", "g"), ("face", "f")):
            matches = []
            for file in (args.data_root / "Model/Clothes").iterdir():
                match = re.fullmatch(fr"C{letter}_(\d+)_{suffix}(\d+)\.gb", file.name, re.I)
                if match:
                    matches.append(tuple(map(int, match.groups())))
            if matches:
                group, variant = min(matches, key=lambda pair: (pair[0] == 0, pair))
                rows.add((job, name, part, group, variant))
    for species_letter, (job, name) in species.items():
        matches = []
        for file in (args.data_root / "Item/Weapon").iterdir():
            match = re.fullmatch(fr"P{species_letter}_B(\d+)_(\d+)\.gb", file.name, re.I)
            if match:
                matches.append(tuple(map(int, match.groups())))
        if matches:
            group, variant = min(matches)
            rows.add((job, name, "shield", group, variant))
    # Later additions have explicit IDs so they cannot renumber the initial catalog.
    policy = json.loads(args.catalog.read_text())
    allocations = [(index, row, True) for index, row in enumerate(sorted(rows), 25000)]
    allocated_ids = {index for index, _, _ in allocations}
    supplemental = {}
    class_names = {name.lower(): (job, name) for job, name in jobs.values()}
    for entry in policy.get("supplemental_items", []):
        index, part, action = entry["index"], entry["part"], entry["action"]
        if (type(index) is not int or index in allocated_ids or
                not 25000 + len(rows) <= index < min(policy["next_id"], 30000) or
                entry["class"] not in class_names or
                part not in ("helmet", "upper", "gauntlet", "pants", "boots", "face",
                             "weapon", "shield") or
                not isinstance(action, list) or len(action) != 2 or
                any(type(value) is not int for value in action) or
                action[0] < 0 or action[1] < 1 or
                any(first <= index <= last for first, last in policy["withdrawn_ranges"])):
            parser.error(f"invalid supplemental item: {entry}")
        weapon_type = entry.get("weapon_type")
        allowed_types = {"knight": ("sword", "sword2h"), "magician": ("wand",),
                         "archer": ("bow",), "thief": ("dagger",)}
        if ((weapon_type is not None and
             (part != "weapon" or weapon_type not in allowed_types[entry["class"]])) or
                (part == "weapon" and entry["class"] == "knight" and not weapon_type)):
            parser.error(f"invalid supplemental weapon type: {entry}")
        job, name = class_names[entry["class"]]
        group, variant = action
        if part in ("weapon", "shield"):
            letter = next(key for key, value in species.items() if value[0] == job)
            expected = f"p{letter}_{'s' if part == 'weapon' else 'b'}{group}_{variant:02d}.gb"
            directory = args.data_root / "Item/Weapon"
        else:
            letter = next(key for key, value in jobs.items() if value[0] == job)
            suffix = {"helmet": "h", "upper": "a", "gauntlet": "g", "pants": "p",
                      "boots": "s", "face": "f"}[part]
            expected = f"c{letter}_{group}_{suffix}{variant:02d}.gb"
            directory = args.data_root / "Model/Clothes"
        if not any(file.name.lower() == expected
                   for file in directory.iterdir()):
            parser.error(f"missing supplemental model: {expected}")
        allocated_ids.add(index)
        supplemental[index] = entry
        allocations.append((index, (job, name, part, group, variant), False))
    args.output.mkdir(parents=True, exist_ok=True)
    active_count = 0
    for index, (job, name, part, group, variant), initial in allocations:
        if index > 29999:
            parser.error("skin definition range exhausted")
        # Exclude normal armor and unverified late models after allocation to
        # preserve previously assigned IDs, including the withdrawn families.
        if part == "body" and group in (31, 32, 33, 34, 79, 89):
            continue
        # Observed group 6, 12 and 13 costume meshes contain only a head. Classify after
        # allocation so correcting the slot does not renumber the catalog.
        head_costume = part == "body" and group in (6, 12, 13)
        if head_costume:
            part = "helmet"
        # Keep sample IDs stable while using visibly different boots and normal
        # character faces rather than faces embedded in armor model families.
        sample_model = {"boots": (14, 1), "face": (0, 2)}.get(part) if initial else None
        if sample_model:
            group, variant = sample_model
            suffix = "s" if part == "boots" else "f"
            letter = next(key for key, value in jobs.items() if value[0] == job)
            expected = f"c{letter}_{group}_{suffix}{variant:02d}.gb"
            if not any(file.name.lower() == expected
                       for file in (args.data_root / "Model/Clothes").iterdir()):
                parser.error(f"missing selected sample model: {expected}")
        # Keep the Knight shield sample distinct from the starter iron shield.
        if initial and job == 0 and part == "shield":
            group, variant = 6, 1
            expected = f"ps_b{group}_{variant:02d}.gb"
            if not any(file.name.lower() == expected
                       for file in (args.data_root / "Item/Weapon").iterdir()):
                parser.error(f"missing selected sample model: {expected}")
        label = "Body" if part == "body" else part.title()
        definition = {
            "index": index,
            "name": f"{name} {label} Skin {group:02d}-{variant:02d}",
            "image": "skinWeapon" if part in ("weapon", "shield") else "skinBody",
            "class": ["skin", part],
            "action": [group, variant],
            "wear": 1,
            "stackable": 0,
            "limit": {"class": name.lower(), "level": 1},
            "buy": 0,
            "sell": 0,
            "description": "Cosmetic only. " + (
                "Chest skin slot; replaces individual body skins." if part == "body"
                else f"Use the {label.lower()} skin slot. No combat bonuses."),
        }
        if head_costume:
            definition["skin"] = {"costume_mesh": True}
        elif part == "body" and costume_replaces_face(name.lower(), group):
            definition["skin"] = {"replace_face": True}
        elif part == "weapon" and name.lower() == "knight" and knight_weapon_type(group, variant):
            definition["skin"] = {"weapon_type": knight_weapon_type(group, variant)}
        if supplemental.get(index, {}).get("weapon_type"):
            definition["skin"] = {"weapon_type": supplemental[index]["weapon_type"]}
        template = weapon_templates.get(index)
        if template and not matches_name_model(definition, template):
            parser.error(f"weapon template model does not match allocated ID {index}")
        entry = names.get(index)
        if entry:
            if not matches_name_model(definition, entry):
                parser.error(f"name catalog model does not match allocated ID {index}")
            if "name" in entry:
                definition["name"] = entry["name"]
        if index in descriptive_names:
            try:
                apply_descriptive_name(definition, descriptive_names[index], entry)
            except ValueError as error:
                parser.error(f"{index}: {error}")
        if index in compatibility:
            try:
                apply_weapon_compatibility(definition, compatibility[index])
            except ValueError as error:
                parser.error(f"{index}: {error}")
        text = json.dumps(definition, indent=2) + "\n"
        text = re.sub(r"\[\s*(\d+)\s*,\s*(\d+)\s*\]", r"[\1, \2]", text)
        filename = f"{index}_{name}_{label}_Skin_{group:02d}_{variant:02d}.json"
        (args.output / filename).write_text(text)
        active_count += 1
    print(f"Created {active_count} skin definitions "
          f"(allocation 25000-{max(allocated_ids, default=24999)}, including reserved gaps).")


if __name__ == "__main__":
    main()

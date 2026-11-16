#!/usr/bin/env python3
# OpenKal - an open source KalOnline client and server.
# Copyright (C) 2026 OpenKal contributors
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

"""Regression tests for the one-shot pet configuration conversion."""
import json
import tempfile
import unittest
from pathlib import Path

from convert_pets import (
    apply_monster_recipe_prices,
    apply_presentation_overrides,
    convert,
    dumps_pretty,
    parse_monster_recipes,
    read_text,
)


class PetConversionTests(unittest.TestCase):
    def test_numeric_vectors_are_compact_without_changing_string_content(self):
        document = {"bones": [1, 2, 3], "label": "[1,  2]"}
        text = dumps_pretty(document)
        self.assertIn('"bones": [1, 2, 3]', text)
        self.assertEqual(json.loads(text), document)

    def test_monster_recipe_parser_accepts_the_incomplete_source_lines(self):
        recipes = parse_monster_recipes(
            "(MonsterPet (Pet 7700)(Index 7708)(Count 500)(Index 7716)(Count 5)\n"
        )
        self.assertEqual(
            recipes,
            [
                {
                    "itemIndex": 7700,
                    "materials": [{"itemIndex": 7708, "count": 500}, {"itemIndex": 7716, "count": 5}],
                }
            ],
        )
        self.assertEqual(
            apply_monster_recipe_prices(
                recipes,
                '<CREATE_MONSTERPET><MONSTER_PET Index="7700"><MATERIAL Index="7708" Count="500"/>'
                '<MATERIAL Index="7716" Count="5"/><MONSTERPET_PRICE Money="100000"/>'
                '</MONSTER_PET></CREATE_MONSTERPET>',
            )[0]["gold"],
            100000,
        )

    def test_conversion_writes_self_contained_json_catalogs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            sources = {
                "Pet.txt": "(pet (index 1747)(time 604800)(heal 100)(autopick 0))\n",
                "MonsterPet.txt": "(MonsterPet (Pet 7700)(Index 7708)(Count 500)(Index 7716)(Count 5)\n",
                "CreateMonsterPet.dat": (
                    '<CREATE_MONSTERPET><MONSTER_PET Index="7700">'
                    '<MATERIAL Index="7708" Count="500"/><MATERIAL Index="7716" Count="5"/>'
                    '<MONSTERPET_PRICE Money="100000"/></MONSTER_PET></CREATE_MONSTERPET>\n'
                ),
                "InitItem.dat": (
                    "(item (Index 1747) (class pet egg) (macrokey 1))\n"
                    "(item (Index 7700) (class pet monsteregg) (macrokey 81))\n"
                ),
                "macro.dat": (
                    '(petinfo (key 1) (bone 1) (scale 1.0 5) (motion "AAAAAAAAAA") '
                    '(mspeed 800 533) (part 2 0) (petpos 1) (pettype 1) (custom pet_dragon))\n'
                    '(petinfo (key 81) (bone 1) (scale 0.7 5) (motion "AAAAAAAAAA") '
                    '(mspeed 800 533) (part 7 1) (petpos 9) (type 1) (pettype 2))\n'
                    '(petbone (key 1) (bone 0 0 0 0 0 0 0 0) '
                    '(target -3.0 3.5 2.0 13) (motion 1 0 0 0 0 0))\n'
                ),
                "switchmodel.dat": "(switchmodel (name pet_dragon Pet Clothes) (gb P001_b1.gb) (texture P001_b1.dds))\n",
            }
            for name, contents in sources.items():
                (root / name).write_text(contents, encoding="utf-8")
            out = root / "pets"
            counts = convert(
                root / "Pet.txt",
                root / "MonsterPet.txt",
                root / "CreateMonsterPet.dat",
                root / "InitItem.dat",
                root / "macro.dat",
                root / "switchmodel.dat",
                out,
            )
            self.assertEqual(counts["catalog"], 2)
            self.assertEqual(counts["visuals"], 2)
            catalog = json.loads((out / "catalog.json").read_text(encoding="utf-8"))
            self.assertEqual(catalog["pets"][0]["visualKey"], 1)
            self.assertEqual(catalog["pets"][1]["durationSeconds"], None)
            visuals = json.loads((out / "visuals.json").read_text(encoding="utf-8"))
            self.assertEqual(visuals["switchModels"][0]["name"], "pet_dragon")

    def test_read_text_removes_padded_nuls(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "padded.txt"
            source.write_bytes(b"(pet (index 1)(time 1)(heal 1)(autopick 0))\x00\x00")
            self.assertEqual(read_text(source), "(pet (index 1)(time 1)(heal 1)(autopick 0))")

    def test_presentation_overrides_preserve_reviewed_bindings(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "presentation_overrides.json"
            source.write_text(
                json.dumps(
                    {
                        "schema": "openkal.pet-presentation-overrides/v1",
                        "overrides": [
                            {
                                "itemIndex": 2004,
                                "visualKey": None,
                                "monsterType": 611,
                                "modelScale": 0.4,
                            }
                        ],
                        "supplementalPets": [
                            {
                                "itemIndex": 1754,
                                "kind": "standard",
                                "durationSeconds": 604800,
                                "healPerTwoSeconds": 100,
                                "autoPickup": False,
                                "visualKey": 8,
                            }
                        ],
                    }
                ),
                encoding="utf-8",
            )
            catalog = [
                {
                    "itemIndex": 2004,
                    "kind": "limited",
                    "durationSeconds": 604800,
                    "healPerTwoSeconds": 100,
                    "autoPickup": True,
                    "visualKey": None,
                }
            ]
            merged = apply_presentation_overrides(catalog, source)
            by_item_index = {record["itemIndex"]: record for record in merged}
            self.assertEqual(by_item_index[2004]["monsterType"], 611)
            self.assertEqual(by_item_index[1754]["visualKey"], 8)


if __name__ == "__main__":
    unittest.main()

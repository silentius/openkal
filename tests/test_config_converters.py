#!/usr/bin/env python3
# OpenKal - an open source KalOnline client and server.
# Copyright (C) 2026 OpenKal contributors
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

"""Convert synthetic catalogs, then read them through the real C++ loaders."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[1]
READER = None


class ConverterCompatibilityTests(unittest.TestCase):
    def run_converter(self, script, *args, success=True):
        result = subprocess.run(
            [sys.executable, str(REPO / 'tools' / script), *map(str, args)],
            text=True, capture_output=True,
        )
        if success:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0)
        return result

    def test_generated_catalogs_load_in_current_client_and_server(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            inputs, config, overrides = root / 'inputs', root / 'config', root / 'overrides'
            inputs.mkdir()
            overrides.mkdir()
            sources = {
                'InitItem.txt': (
                    '(item (Index 51001) (Name 1) (class quest etc) (plural 1) '
                    '(country 0 1 2) (code 4 8 0 0) (pay 1))\n'
                    '(item (Index 51002) (Name 2) (class weapon sword) (wear 1) '
                    '(limit knight 5) (plural 0) (specialty (Attack 3 10)))\n'
                    '(item (Index 51003) (Name 3) (class refresh hp) (stackable 1) '
                    '(use 1) (cooltime 1300 33) (specialty (refresh hp 100)))\n'
                ),
                'message-e.txt': (
                    '(itemname 1 "Synthetic Token")\n(itemname 2 "Synthetic Sword")\n'
                    '(itemname 3 "Synthetic Medicine")\n(npcname 51001 "Synthetic Guide")\n'
                    '(monstername 1 "Synthetic Monster")\n(prefixname 1 "Synthetic Prefix")\n'
                ),
                'InitNPC.txt': (
                    '(npc (kind 3) (move 0))\n'
                    '(gennpc (index 51001) (kind 3) (shape 7) (map 2) (html 192) '
                    '(xy 1000 2000 300) (dir 2000 2000) (quest 91 1) '
                    '(teleport 2 4000 5000) (scenario 0 51001))\n'
                ),
                'macro.dat': (
                    '(npcinfo (key 7) (bone 9) (scale 1.25 6) (motion "AAAAAAAAAA"))\n'
                    '(npcbone (key 9) (bone 1 2 3) (target 1 2 3 4))\n'
                    '(npcbone (key 10) (bone 4 5 6) (target 1 2 3 4))\n'
                    '(petinfo (key 1) (bone 1) (scale 1.0 5) (motion "AAAAAAAAAA") '
                    '(mspeed 800 533) (part 2 0) (petpos 1) (pettype 1))\n'
                    '(petbone (key 1) (bone 0 0 0 0 0 0 0 0) (target 1 2 3 4) (motion 1 0 0 0 0 0))\n'
                ),
                'InitMonster.txt': '(monster (index 1) (name 1) (level 2) (hp 30))\n',
                'GenMonster.txt': '(genmonster (index 1) (map 2) (area 17) (rect 900 1900 1100 2100) (max 1) (cycle 10))\n',
                'Prefix.txt': '(prefix (index 1) (Attack 2 4) (hp 3))\n',
                'ItemGroup.txt': '(group (index 1) (item 100 51001 0 1))\n(itemgroup (index 1) (group 1000 1))\n',
                'Quest.txt': '(quest (index 91 1) (case (if (level 1 100)) (then (exp 7))))\n',
                'task.dat': '(task (key 91) (name "Synthetic Task") (desc "Synthetic Objective") (npcindex 51001))\n',
                'InitSkill.txt': '(skill (class 0) (index 1) (mp 3) (delay 100 200))\n',
                'jobsystem-e.txt': '(skill (key knight-test) (index 1) (action 1) (limit knight 1) (name "Synthetic Skill"))\n',
                'SkillBook.txt': '(skillbook (class 0) (index 51002) (action 1) (upgradeamount 1) (maxgrade 3))\n',
                'OldSkillDamage.txt': '(updatedamage (class 0) (action 1) (rate 25))\n',
                'Pet.txt': '(pet (index 51004) (time 60) (heal 10) (autopick 0))\n',
                'MonsterPet.txt': '(MonsterPet (Pet 51005) (Index 51001) (Count 2) (Index 51002) (Count 1))\n',
                'CreateMonsterPet.dat': '<CREATE_MONSTERPET><MONSTER_PET Index="51005"><MATERIAL Index="51001" Count="2"/><MATERIAL Index="51002" Count="1"/><MONSTERPET_PRICE Money="100"/></MONSTER_PET></CREATE_MONSTERPET>',
                'InitItem.dat': '(item (Index 51004) (class pet egg) (macrokey 1))\n(item (Index 51005) (class pet monsteregg) (macrokey 1))\n',
                'switchmodel.dat': '',
            }
            for name, contents in sources.items():
                (inputs / name).write_text(contents, encoding='utf-8')
            scripts = inputs / 'script_pk'
            scripts.mkdir()
            (scripts / 's900001.dat').write_text(
                '(frame (line "Synthetic Dialog") '
                '(link (text "Accept") (param send quest 91 1)))', encoding='utf-8',
            )
            (overrides / '51001_maintained_guide.json').write_text(json.dumps({
                'index': 51001, 'name': 'Maintained Guide', 'storage_keeper': True,
                'trading_post_collector': True, 'overhead_icon': 'test.bmp',
                'name_offset_y': 0, 'name_offset_pixels': 10,
            }), encoding='utf-8')
            (overrides / '51002_placed_guide.json').write_text(json.dumps({
                'index': 51002, 'name': 'Placed Guide', 'html': 900001,
                'placement': {'beside_dialog': 192, 'offset': [10, 20, 30]},
            }), encoding='utf-8')
            self.run_converter('item_config_convert/convert_items.py', '--all',
                               '--init', inputs / 'InitItem.txt', '--message', inputs / 'message-e.txt',
                               '--out', config / 'items')
            self.run_converter('npc_config_convert/convert_npcs.py', '--all',
                               '--init', inputs / 'InitNPC.txt', '--message', inputs / 'message-e.txt',
                               '--macro', inputs / 'macro.dat', '--overrides', overrides,
                               '--out', config / 'npcs')
            self.assertTrue((config / 'npcs/51001_maintained_guide.json').is_file())
            self.run_converter('monster_config_convert/convert_monsters.py', '--all',
                               '--init', inputs / 'InitMonster.txt', '--gen', inputs / 'GenMonster.txt',
                               '--message', inputs / 'message-e.txt', '--macro', inputs / 'macro.dat',
                               '--out', config / 'monsters')
            self.run_converter('itemgroup_config_convert/convert_itemgroups.py', '--all',
                               '--src', inputs / 'ItemGroup.txt', '--groups-out', config / 'groups',
                               '--itemgroups-out', config / 'itemgroups')
            self.run_converter('prefix_config_convert/convert_prefixes.py', '--all',
                               '--prefix', inputs / 'Prefix.txt', '--message', inputs / 'message-e.txt',
                               '--out', config / 'prefixes')
            self.run_converter('quest_config_convert/convert_quests.py', '--all',
                               '--quest', inputs / 'Quest.txt', '--out', config / 'quests')
            self.run_converter('quest_config_convert/convert_tasks.py', '--all',
                               '--task', inputs / 'task.dat', '--out', config / 'tasks')
            self.run_converter('quest_config_convert/convert_dialogs.py', '--all',
                               '--scripts', scripts, '--out', config / 'dialogs')
            self.run_converter('skill_config_convert/convert_skills.py', '--all',
                               '--init', inputs / 'InitSkill.txt', '--jobsystem', inputs / 'jobsystem-e.txt',
                               '--message', inputs / 'message-e.txt', '--skillbook', inputs / 'SkillBook.txt',
                               '--old-damage', inputs / 'OldSkillDamage.txt', '--out', config / 'skills',
                               '--skillbooks-out', config / 'skillbooks', '--old-damage-out', config / 'skill_damage')
            self.run_converter('pet_config_convert/convert_pets.py',
                               '--pet-source', inputs / 'Pet.txt', '--monster-recipe-source', inputs / 'MonsterPet.txt',
                               '--monster-price-source', inputs / 'CreateMonsterPet.dat',
                               '--item-source', inputs / 'InitItem.dat', '--macro-source', inputs / 'macro.dat',
                               '--switch-model-source', inputs / 'switchmodel.dat', '--out', config / 'pets')
            for path in (config / 'items').rglob('*.json'):
                item = json.loads(path.read_text())
                if 'index' in item:
                    self.assertFalse({'country', 'code', 'plural'} & item.keys())
            damage = json.loads(next((config / 'skill_damage/knight').glob('*.json')).read_text())
            self.assertEqual(damage['rate'], 25)
            self.assertGreater(json.loads((config / 'skillbooks/manifest.json').read_text())['count'], 0)
            result = subprocess.run([str(READER), str(config)], text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            # Single-item regeneration replaces a changed slug and keeps the catalog manifest.
            messages = inputs / 'message-e.txt'
            messages.write_text(messages.read_text().replace('Synthetic Token', 'Renamed Token'))
            self.run_converter('item_config_convert/convert_items.py', '--index', 51001,
                               '--init', inputs / 'InitItem.txt', '--message', messages,
                               '--out', config / 'items')
            item_documents = [json.loads(path.read_text())
                              for path in (config / 'items').rglob('*.json')
                              if path.name != 'manifest.json']
            self.assertEqual(sum(row['index'] == 51001 for row in item_documents), 1)
            self.assertEqual(json.loads((config / 'items/manifest.json').read_text())['count'], 3)
            # Updating a single NPC must retain bones used by other records.
            (overrides / '51001_maintained_guide.json').rename(overrides / '51001_renamed_guide.json')
            self.run_converter('npc_config_convert/convert_npcs.py', '--index', 51001,
                               '--init', inputs / 'InitNPC.txt', '--message', inputs / 'message-e.txt',
                               '--macro', inputs / 'macro.dat', '--overrides', overrides,
                               '--out', config / 'npcs')
            bones = json.loads((config / 'npcs/bones.json').read_text())
            self.assertIn('10', bones)
            self.assertFalse((config / 'npcs/51001_maintained_guide.json').exists())
            self.assertEqual(json.loads((config / 'npcs/manifest.json').read_text())['count'], 2)
            # Invalid overrides must fail before clearing a previously generated output.
            (overrides / '51001_duplicate.json').write_text('{"index": 51001}', encoding='utf-8')
            self.run_converter('npc_config_convert/convert_npcs.py', '--all',
                               '--init', inputs / 'InitNPC.txt', '--message', inputs / 'message-e.txt',
                               '--macro', inputs / 'macro.dat', '--overrides', overrides,
                               '--out', config / 'npcs', success=False)
            self.assertTrue((config / 'npcs/51002_placed_guide.json').is_file())


if __name__ == '__main__':
    READER = Path(sys.argv.pop(1))
    unittest.main()

"""Project-owned data-only regression tests for the optional skin audit."""

import importlib.util
import json
import os
import re
from pathlib import Path
import tempfile
import unittest
import sys
import subprocess


ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
SPEC = importlib.util.spec_from_file_location('audit_skins', ROOT / 'tools/audit_skins.py')
AUDIT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(AUDIT)
from skin_catalog import (apply_descriptive_name, apply_weapon_compatibility, knight_weapon_type,
                          load_descriptive_name_catalog, load_weapon_compatibility,
                          load_weapon_templates)
PROBE = os.environ.get('OPENKAL_SKIN_PROBE')


class SkinAuditTests(unittest.TestCase):
    def test_name_choices_exclude_rejected_variants(self):
        _, entries = AUDIT.load_name_catalog(ROOT / 'config/skins/names.json')
        rejected = r'^(?:Returned|Artificial|(?:Imperial\s+)?GM)'
        for entry in entries.values():
            for name in entry.get('candidates', [entry.get('name', '')]):
                self.assertNotRegex(name, re.compile(rejected, re.I))
                self.assertNotRegex(name, re.compile(r'\[\s*\d+\s*days?\s*\]', re.I))
        self.assertEqual(entries[25044]['name'], "Blood Dragon's Sword")
        self.assertEqual(entries[25068]['name'], 'Imperial Heavenly Executioner Sword')
        self.assertEqual(entries[25188]['name'], 'Blood Dragon Stick')
        self.assertEqual(entries[25189]['name'], 'Blood Dragon Imperial Stick')
        self.assertEqual(entries[25197]['name'], 'Violet Wing Staff Skin (Magician)')
        self.assertEqual(entries[25198]['name'], 'Violet Wing Crown Staff Skin (Magician)')
        self.assertTrue(all('name' in entry and 'candidates' not in entry
                            for entry in entries.values()))

    def test_curated_catalogs_omit_external_reference_annotations(self):
        def check(value):
            if isinstance(value, dict):
                self.assertTrue({'reference_names', 'source_lines'}.isdisjoint(value))
                for child in value.values():
                    check(child)
            elif isinstance(value, list):
                for child in value:
                    check(child)

        for name in ('names.json', 'weapon-templates.json'):
            data = json.loads((ROOT / 'config/skins' / name).read_text())
            self.assertTrue({'origin', 'sha256', 'permission', 'confirmed_on'}
                            .isdisjoint(data['source']))
            check(data)

    def test_curated_review_statuses_describe_observations_not_personal_authority(self):
        def check(value, path):
            if isinstance(value, dict):
                for key, child in value.items():
                    child_path = f'{path}.{key}'
                    if key == 'status' and isinstance(child, str):
                        self.assertNotRegex(
                            child.lower(), r'(?:^|_)(?:user|owner|maintainer|silentius)(?:_|$)',
                            child_path)
                    check(child, child_path)
            elif isinstance(value, list):
                for index, child in enumerate(value):
                    check(child, f'{path}[{index}]')

        for name in ('catalog.json', 'preview-review.json'):
            check(json.loads((ROOT / 'config/skins' / name).read_text()), name)

    def test_multiline_names_and_commented_records(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'owned-items.txt'
            path.write_text('''; Synthetic Outfit (Knight)
(item (name 1) (class defense cost)
 (action 4 1) (limit knight 1))
; (item (name "Must not be read") (class defense cost) (action 99 1) (limit knight 1))
; Synthetic Outfit(Event)(Knight)
(item (name 2) (class defense cost) (action 4 1) (limit knight 1))
(item (name "Distinct Outfit") (class defense cost) (action 4 1) (limit knight 1))
''', encoding='utf-8')
            rows = AUDIT.read_name_reference(path)
            self.assertEqual(len(rows), 3)
            self.assertEqual(rows[0]['key'], ('knight', 'costume', 4, 1))
            self.assertEqual(rows[0]['name'], rows[1]['name'])
            self.assertEqual(rows[0]['line'], 2)
            definition = {'class': ['skin', 'body'], 'action': [4, 1],
                          'limit': {'class': 'knight'}}
            result = AUDIT.name_result(definition, {rows[0]['key']: rows})
            self.assertEqual(result['status'], 'ambiguous')
            self.assertEqual(len(result['candidates']), 2)

    def test_unnamed_record_does_not_inherit_previous_comment(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'owned-items.txt'
            path.write_text('''; Named Outfit
(item (class defense cost) (action 1 1) (limit mage 1))
(item (class defense cost) (action 2 1) (limit mage 1))
''')
            rows = AUDIT.read_name_reference(path)
            self.assertEqual(len(rows), 1)
            self.assertEqual(rows[0]['key'][0], 'magician')

    def test_head_coverage_is_a_review_not_automatic_reclassification(self):
        row = {'class': 'knight', 'part': 'body',
               'inspection': {'loaded': True, 'bones': [17],
                              'bounds_min': [-1, 16, -1], 'bounds_max': [1, 20, 1]}}
        refs = {'head:knight': {'loaded': True, 'bones': [17], 'bounds_max': [1, 20, 1]},
                'boots:knight': {'loaded': True, 'bounds_min': [-1, 0, -1]}}
        warnings = AUDIT.coverage_warnings(row, refs)
        self.assertIn('body_slot_contains_head_only', warnings)
        self.assertIn('incomplete_body_coverage', warnings)
        self.assertEqual(row['part'], 'body')
        row['part'] = 'helmet'
        self.assertEqual(AUDIT.coverage_warnings(row, refs), [])

    def test_face_and_boot_coordinates_follow_runtime_rules(self):
        definition = {'class': ['skin', 'face'], 'action': [0, 2],
                      'limit': {'class': 'knight'}}
        self.assertEqual(AUDIT.model_filename(definition), 'Model/Clothes/cw_0_f02.gb')
        definition.update({'class': ['skin', 'helmet'], 'action': [6, 1],
                           'skin': {'costume_mesh': True}})
        self.assertEqual(AUDIT.model_filename(definition), 'Model/Clothes/cw_6_c01.gb')

    def test_numeric_arrays_and_policy(self):
        policy = json.loads((ROOT / 'config/skins/catalog.json').read_text())
        definition = {'index': 25031, 'class': ['skin', 'body'], 'action': [31, 1],
                      'limit': {'class': 'knight'}, 'wear': 1, 'stackable': 0}
        self.assertIn('withdrawn_id_reused', AUDIT.validate_definition(
            Path('25031_sample.json'), definition, policy))
        self.assertEqual(len(AUDIT.expected_ids(policy)), 784)
        self.assertNotIn(25031, AUDIT.expected_ids(policy))
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'report.json'
            AUDIT.write_json(path, {'bounds': [-1.0, 0, 1.5]})
            self.assertIn('[-1.0, 0, 1.5]', path.read_text())

    def test_generator_preserves_ids_and_both_sets_for_every_class(self):
        policy = json.loads((ROOT / 'config/skins/catalog.json').read_text())
        supplemental = {entry['index'] for entry in policy['supplemental_items']}
        definitions = {data['index']: data for path in (ROOT / 'config/items/skins').glob('*.json')
                       for data in [json.loads(path.read_text())]}
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            data_root, output = root / 'data', root / 'generated'
            (data_root / 'Model/Clothes').mkdir(parents=True)
            (data_root / 'Item/Weapon').mkdir(parents=True)
            # Empty synthetic files reproduce the filename inventory;
            # generation never opens a model or requires external art.
            for index, definition in definitions.items():
                if index not in supplemental:
                    (data_root / AUDIT.model_filename(definition)).touch()
            for letter in 'wrmt':
                for group in (31, 32, 33, 34, 79, 89):
                    (data_root / f'Model/Clothes/c{letter}_{group}_c01.gb').touch()
            for index in supplemental:
                (data_root / AUDIT.model_filename(definitions[index])).touch()
            result = subprocess.run([sys.executable, str(ROOT / 'tools/create_skin_items.py'),
                                     str(data_root), str(output)], text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            generated = {data['index']: data for path in output.glob('*.json')
                         for data in [json.loads(path.read_text())]}
            self.assertEqual(set(generated), AUDIT.expected_ids(policy))
            for index, definition in definitions.items():
                for key in ('name', 'class', 'action', 'skin', 'limit'):
                    self.assertEqual(generated[index].get(key), definition.get(key),
                                     f'{index}: {key}')

    def test_equipment_appearances_are_complete_cosmetic_and_model_guarded(self):
        policy = json.loads((ROOT / 'config/skins/catalog.json').read_text())
        definitions = {data['index']: data for path in (ROOT / 'config/items/skins').glob('*.json')
                       for data in [json.loads(path.read_text())]}
        coordinates = {}
        for index, definition in definitions.items():
            if definition.get('skin', {}).get('costume_mesh') or definition['class'][1] == 'body':
                continue
            key = (definition['limit']['class'], definition['class'][1], *definition['action'])
            self.assertNotIn(key, coordinates, f'duplicate appearance {key}')
            coordinates[key] = index
        parts = ('helmet', 'upper', 'gauntlet', 'pants', 'boots')
        for clazz in ('knight', 'magician', 'archer', 'thief'):
            for group in range(1, 9):
                for part in parts:
                    self.assertIn((clazz, part, group, 1), coordinates)
        equipment = [entry for entry in policy['supplemental_items'] if 'family' in entry]
        self.assertEqual(len(equipment), 321)
        allowed = {'index', 'name', 'image', 'class', 'action', 'wear', 'stackable',
                   'limit', 'buy', 'sell', 'description', 'skin'}
        for entry in equipment:
            definition = definitions[entry['index']]
            self.assertTrue(AUDIT.matches_name_model(definition, entry))
            self.assertLessEqual(set(definition), allowed)
            self.assertEqual(definition['limit'], {'class': entry['class'], 'level': 1})
            self.assertEqual(definition['buy'], 0)
            self.assertEqual(definition['sell'], 0)
            self.assertEqual(definition['wear'], 1)
            self.assertEqual(definition['stackable'], 0)
            if 'weapon_type' in entry:
                self.assertEqual(AUDIT.weapon_skin_type(definition), entry['weapon_type'])
            if entry['family'] == 'grade_weapon':
                self.assertIn(entry['appearance_grade'], (1, 5, 9, 13, 17, 21, 25, 29, 33,
                                                         37, 40, 45, 48, 50, 53, 56, 59))
            elif entry['family'].startswith('grade_'):
                self.assertLessEqual(entry['appearance_grade'], 55)
        g59 = [entry for entry in equipment if entry.get('appearance_grade') == 59]
        self.assertEqual(len(g59), 21)
        # Reviewed image families are not ordered by grade in the local model inventory.
        for clazz, groups, variants in [('knight', (11, 12), 4), ('magician', (9,), 6),
                                        ('archer', (9,), 4), ('thief', (8,), 3)]:
            self.assertEqual({tuple(e['action']) for e in g59 if e['class'] == clazz},
                             {(group, variant) for group in groups
                              for variant in range(1, variants + 1)})
        g56 = [entry for entry in equipment if entry.get('appearance_grade') == 56]
        self.assertEqual({entry['index'] for entry in g56}, set(range(25771, 25787)))
        for clazz, groups in [('knight', (13, 14)), ('magician', (10,)), ('archer', (10,))]:
            self.assertEqual({tuple(e['action']) for e in g56 if e['class'] == clazz},
                             {(group, variant) for group in groups for variant in range(1, 5)})
        for entry in g56 + g59:
            self.assertTrue(definitions[entry['index']]['name'].startswith(
                f'G{entry["appearance_grade"]} '))
        darkness = policy['equipment_expansion']['darkness']
        self.assertEqual(set(darkness['armor_classes']), {'knight', 'magician', 'archer'})
        self.assertEqual(set(darkness['weapon_classes']), {'knight', 'archer', 'thief'})
        for clazz in darkness['armor_classes']:
            for part in parts:
                self.assertIn((clazz, part, 9, 1), coordinates)
        self.assertNotIn(('thief', 'upper', 9, 1), coordinates)

    def test_head_appearance_flags_are_booleans(self):
        policy = json.loads((ROOT / 'config/skins/catalog.json').read_text())
        definition = {'index': 25000, 'class': ['skin', 'body'], 'action': [1, 1],
                      'limit': {'class': 'knight'}, 'wear': 1,
                      'skin': {'replace_face': 'false'}}
        self.assertIn('invalid_skin_appearance_flags', AUDIT.validate_definition(
            Path('25000_owned.json'), definition, policy))
        definition['skin']['replace_face'] = False
        self.assertNotIn('invalid_skin_appearance_flags', AUDIT.validate_definition(
            Path('25000_owned.json'), definition, policy))

    def test_texture_presence_is_distinct_from_runtime_format_support(self):
        inspection = {'textures': [{'name': 'art.bmp', 'loaded': False},
                                   {'name': 'missing.dds', 'loaded': False}]}
        diagnoses = AUDIT.diagnose_textures(inspection, {'art': ['Item/Weapon/Tex/art.bmp']})
        self.assertEqual(diagnoses, ['texture_not_found', 'unsupported_texture_extension'])
        self.assertEqual(inspection['textures'][0]['candidate_files'],
                         ['Item/Weapon/Tex/art.bmp'])

    def test_weapon_types_are_explicit_for_knight_and_fail_closed(self):
        policy = json.loads((ROOT / 'config/skins/catalog.json').read_text())
        definition = {'index': 25063, 'class': ['skin', 'weapon'], 'action': [22, 2],
                      'limit': {'class': 'knight'}, 'wear': 1,
                      'skin': {'weapon_type': 'sword2h'}}
        self.assertEqual(AUDIT.weapon_skin_type(definition), 'sword2h')
        self.assertEqual(AUDIT.validate_definition(Path('25063_owned.json'), definition, policy), [])
        definition['skin']['weapon_type'] = 'invalid'
        self.assertEqual(AUDIT.weapon_skin_type(definition), '')
        self.assertIn('invalid_skin_weapon_type', AUDIT.validate_definition(
            Path('25063_owned.json'), definition, policy))
        definition['skin'] = {}
        self.assertEqual(AUDIT.weapon_skin_type(definition), '')
        for job, expected in (('magician', 'wand'), ('archer', 'bow'), ('thief', 'dagger')):
            definition['limit']['class'] = job
            self.assertEqual(AUDIT.weapon_skin_type(definition), expected)

    def test_curated_names_cannot_follow_an_id_to_a_different_model(self):
        _, entries = AUDIT.load_name_catalog(ROOT / 'config/skins/names.json')
        entry = entries[25003]
        definition = {'index': 25003, 'class': ['skin', 'body'],
                      'action': entry['action'], 'limit': {'class': 'knight'}}
        self.assertTrue(AUDIT.matches_name_model(definition, entry))
        definition['action'] = [99, 1]
        self.assertFalse(AUDIT.matches_name_model(definition, entry))

    def test_descriptive_names_keep_model_guards_and_community_precedence(self):
        entry = {'index': 25000, 'class': 'knight', 'part': 'body',
                 'action': [1, 1], 'name': 'Owned visual label'}
        definition = {'index': 25000, 'class': ['skin', 'body'], 'action': [1, 1],
                      'limit': {'class': 'knight'}, 'description': 'Preserved manual note.'}
        apply_descriptive_name(definition, entry)
        apply_descriptive_name(definition, entry)
        self.assertEqual(definition['name'], entry['name'])
        self.assertEqual(definition['description'].count('OpenKal descriptive name'), 1)
        self.assertTrue(definition['description'].startswith('Preserved manual note.'))
        self.assertIn('knight/body 01-01', definition['description'])
        with self.assertRaisesRegex(ValueError, 'replace a community name'):
            apply_descriptive_name(definition, entry, dict(entry, name='Community name'))
        for changed in (dict(entry, index=25001), dict(entry, action=[2, 1]),
                        dict(entry, part='weapon'), dict(entry, **{'class': 'archer'})):
            with self.assertRaisesRegex(ValueError, 'model does not match'):
                apply_descriptive_name(definition, changed)

    def test_descriptive_catalog_rejects_invalid_or_duplicate_entries(self):
        entry = {'index': 25000, 'class': 'knight', 'part': 'body',
                 'action': [1, 1], 'name': 'Owned visual label'}
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'names.json'
            for entries in ([entry, entry], [dict(entry, action=[True, 1])],
                            [dict(entry, name='bad\nname')], [dict(entry, part='invalid')], [None]):
                AUDIT.write_json(path, {'schema': 'openkal.skin-descriptive-names/v1',
                                        'source': {'origin': 'owned test'}, 'entries': entries})
                with self.assertRaises(ValueError):
                    load_descriptive_name_catalog(path)

    def test_descriptive_catalog_covers_reviewed_items_without_replacing_community_names(self):
        _, community = AUDIT.load_name_catalog(ROOT / 'config/skins/names.json')
        source, descriptive = load_descriptive_name_catalog(
            ROOT / 'config/skins/descriptive-names.json')
        self.assertEqual(source['license'], 'GPL-3.0-or-later')
        remaining = set()
        for path in (ROOT / 'config/items/skins').glob('*.json'):
            definition = json.loads(path.read_text())
            index = definition['index']
            if index in community and 'name' in community[index]:
                self.assertNotIn(index, descriptive)
                self.assertEqual(definition['name'], community[index]['name'])
            elif index in descriptive:
                named = dict(definition)
                apply_descriptive_name(named, descriptive[index], community.get(index))
                self.assertEqual(named, definition)
            else:
                remaining.add(index)
        self.assertEqual(remaining, {25160, 25313, 25396})

    def test_knight_type_assignments_have_guarded_evidence(self):
        _, entries = AUDIT.load_name_catalog(ROOT / 'config/skins/names.json')
        data, templates = load_weapon_templates(ROOT / 'config/skins/weapon-templates.json')
        _, compatibility = load_weapon_compatibility(ROOT / 'config/skins/weapon-compatibility.json')
        classified = 0
        name_classified = 0
        template_classified = 0
        own_classified = 0
        material_labels = {25135: ([100, 5], 'sword2h'), 25143: ([109, 1], 'sword')}
        policy = json.loads((ROOT / 'config/skins/catalog.json').read_text())
        equipment = {entry['index']: entry for entry in policy['supplemental_items']
                     if entry.get('family') in ('grade_weapon', 'darkness_weapon')}
        equipment_classified = 0
        for path in (ROOT / 'config/items/skins').glob('*.json'):
            definition = json.loads(path.read_text())
            if (definition.get('class') != ['skin', 'weapon'] or
                    definition.get('limit', {}).get('class') != 'knight'):
                continue
            expected = knight_weapon_type(*definition['action'])
            if definition['index'] in equipment:
                entry = equipment[definition['index']]
                self.assertTrue(AUDIT.matches_name_model(definition, entry))
                self.assertEqual(AUDIT.weapon_skin_type(definition), entry['weapon_type'])
                equipment_classified += 1
                continue
            own_choice = compatibility.get(definition['index'])
            if own_choice:
                self.assertFalse(expected)
                self.assertEqual(AUDIT.weapon_skin_type(definition), own_choice['weapon_type'])
                applied = dict(definition)
                apply_weapon_compatibility(applied, own_choice)
                self.assertEqual(applied, definition)
                own_classified += 1
                continue
            self.assertEqual(AUDIT.weapon_skin_type(definition), expected)
            if not expected:
                continue
            classified += 1
            if definition['index'] in templates:
                template_classified += 1
                template = templates[definition['index']]
                self.assertTrue(AUDIT.matches_name_model(definition, template))
                self.assertEqual(expected, knight_weapon_type(*template['reference_action']))
                continue
            if definition['index'] in material_labels:
                coordinates, category = material_labels[definition['index']]
                self.assertEqual(definition['action'], coordinates)
                self.assertEqual(expected, category)
                continue
            name_classified += 1
            entry = entries[definition['index']]
            self.assertTrue(AUDIT.matches_name_model(definition, entry))
            self.assertIn(expected, ('sword', 'sword2h'))
        self.assertEqual(classified, 41)
        self.assertEqual(equipment_classified, 53)
        self.assertEqual(own_classified, 60)
        self.assertEqual(len(compatibility), own_classified)
        shape_types = {}
        for entry in compatibility.values():
            shape = (entry['expected_shape_fingerprint'], entry['expected_shape_meshes'])
            shape_types.setdefault(shape, set()).add(entry['weapon_type'])
        self.assertTrue(all(len(categories) == 1 for categories in shape_types.values()))
        self.assertEqual(name_classified, 12)
        self.assertEqual(template_classified, 27)
        for reference in data['additional_references']:
            self.assertEqual(knight_weapon_type(*reference['action']), reference['weapon_type'])
        # This coordinate includes a conflicting non-weapon name in the catalog.
        self.assertEqual(knight_weapon_type(23, 2), '')
        self.assertEqual(knight_weapon_type(40, 2), '')

    def test_original_compatibility_mapping_is_guarded_and_preserves_other_fields(self):
        entry = {'index': 25050, 'class': 'knight', 'part': 'weapon',
                 'action': [21, 7], 'weapon_type': 'sword2h'}
        definition = {'index': 25050, 'class': ['skin', 'weapon'], 'action': [21, 7],
                      'limit': {'class': 'knight', 'level': 1}, 'name': 'Owned name',
                      'description': 'Manual note.', 'buy': 0, 'sell': 0,
                      'skin': {'keep_hair': False}}
        before = json.loads(json.dumps(definition))
        apply_weapon_compatibility(definition, entry)
        self.assertEqual(before['skin'], {'keep_hair': False})
        self.assertEqual(definition['skin'], {'keep_hair': False, 'weapon_type': 'sword2h'})
        self.assertTrue(definition['description'].startswith('Manual note. '))
        self.assertIn('reference type unverified', definition['description'])
        again = json.loads(json.dumps(definition))
        apply_weapon_compatibility(definition, entry)
        self.assertEqual(definition, again)
        for key in before.keys() - {'skin', 'description'}:
            self.assertEqual(before[key], definition[key])
        for key, value in [('index', 25051), ('action', [21, 8]),
                           ('class', ['skin', 'shield']), ('limit', {'class': 'thief'}),
                           ('skin', {'weapon_type': 'sword'})]:
            with self.subTest(key=key), self.assertRaises(ValueError):
                apply_weapon_compatibility(dict(before, **{key: value}), entry)
        known = dict(before, index=25044, action=[21, 1])
        with self.assertRaisesRegex(ValueError, 'evidenced reference'):
            apply_weapon_compatibility(known, dict(entry, index=25044, action=[21, 1]))

    def test_compatibility_catalog_rejects_malformed_duplicate_and_reference_entries(self):
        row = {'index': 25050, 'class': 'knight', 'part': 'weapon', 'action': [21, 7],
               'weapon_type': 'sword2h', 'expected_shape_fingerprint': '0123456789abcdef',
               'expected_shape_meshes': 1, 'reason': 'Owned grip observation.'}
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'owned-compatibility.json'
            def write(entries):
                AUDIT.write_json(path, {'schema': 'openkal.skin-weapon-compatibility/v1',
                                        'source': {'origin': 'owned test'}, 'entries': entries})
            write([row])
            self.assertEqual(load_weapon_compatibility(path)[1], {25050: row})
            for key, value in [('index', True), ('action', [21, True]), ('action', [21, 0]),
                               ('class', 'archer'), ('part', 'body'), ('weapon_type', 'wand'),
                               ('expected_shape_fingerprint', None),
                               ('expected_shape_fingerprint', '0123456789abcdeF'),
                               ('expected_shape_meshes', True), ('reason', ''),
                               ('reason', 'Line\nbreak'), ('action', [21, 1])]:
                write([dict(row, **{key: value})])
                with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                    load_weapon_compatibility(path)
            for duplicate in [dict(row, action=[55, 99]), dict(row, index=25051)]:
                write([row, duplicate])
                with self.assertRaisesRegex(ValueError, 'duplicate'):
                    load_weapon_compatibility(path)

    def test_compatibility_audit_rejects_changed_geometry_type_and_missing_annotation(self):
        entry = {'index': 25050, 'class': 'knight', 'part': 'weapon', 'action': [21, 7],
                 'weapon_type': 'sword2h', 'expected_shape_fingerprint': '0123456789abcdef',
                 'expected_shape_meshes': 1, 'reason': 'Owned grip observation.'}
        definition = {'index': 25050, 'class': ['skin', 'weapon'], 'action': [21, 7],
                      'limit': {'class': 'knight'}}
        shape = {'loaded': True, 'attachment_shape_fingerprint': '0123456789abcdef',
                 'attachment_shape_meshes': 1}
        apply_weapon_compatibility(definition, entry)
        result, errors = AUDIT.verify_weapon_compatibility(definition, entry, shape)
        self.assertEqual(errors, [])
        self.assertFalse(result['original_type_verified'])
        for change in [{'loaded': False}, {'attachment_shape_meshes': 2},
                       {'attachment_shape_fingerprint': 'fedcba9876543210'}]:
            _, errors = AUDIT.verify_weapon_compatibility(definition, entry, dict(shape, **change))
            self.assertIn('weapon_compatibility_geometry_mismatch', errors)
        _, errors = AUDIT.verify_weapon_compatibility(
            dict(definition, skin={'weapon_type': 'sword'}), entry, shape)
        self.assertIn('weapon_compatibility_type_mismatch', errors)
        _, errors = AUDIT.verify_weapon_compatibility(dict(definition, description='Manual'),
                                                    entry, shape)
        self.assertIn('weapon_compatibility_annotation_missing', errors)
        with self.assertRaisesRegex(ValueError, 'model does not match'):
            AUDIT.verify_weapon_compatibility(dict(definition, action=[21, 8]), entry, shape)

    def test_weapon_template_catalog_rejects_duplicate_and_incompatible_rows(self):
        row = {'index': 25048, 'class': 'knight', 'part': 'weapon',
               'action': [21, 5], 'reference_action': [21, 3],
               'expected_shape_fingerprint': '0123456789abcdef', 'expected_shape_meshes': 1}
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'owned-templates.json'
            AUDIT.write_json(path, {'schema': 'openkal.skin-weapon-templates/v1',
                                    'additional_references': [], 'entries': [row]})
            self.assertEqual(load_weapon_templates(path)[1], {25048: row})
            for entries in ([row, row], [row, dict(row, index=25049)],
                            [dict(row, part='body')], [dict(row, **{'class': 'thief'})],
                            [dict(row, action=[21])], [dict(row, reference_action=[True, 3])],
                            [dict(row, index=True)], [dict(row, expected_shape_meshes=0)],
                            [dict(row, expected_shape_fingerprint='invalid')]):
                AUDIT.write_json(path, {'schema': 'openkal.skin-weapon-templates/v1',
                                        'additional_references': [], 'entries': entries})
                with self.assertRaises(ValueError):
                    load_weapon_templates(path)
            AUDIT.write_json(path, {'schema': 'openkal.skin-weapon-templates/v1', 'entries': [],
                                    'additional_references': [{'class': 'knight', 'part': 'weapon',
                                                               'action': [17, 4],
                                                               'weapon_type': 'invalid'}]})
            with self.assertRaises(ValueError):
                load_weapon_templates(path)

    def test_weapon_template_verification_rejects_changed_or_missing_active_assets(self):
        entry = {'index': 25048, 'class': 'knight', 'part': 'weapon',
                 'action': [21, 5], 'reference_action': [21, 3],
                 'expected_shape_fingerprint': 'owned-shape', 'expected_shape_meshes': 1}
        definition = {'index': 25048, 'class': ['skin', 'weapon'], 'action': [21, 5],
                      'limit': {'class': 'knight'}, 'skin': {'weapon_type': 'sword'}}
        shape = {'loaded': True, 'errors': [], 'attachment_shape_fingerprint': 'owned-shape',
                 'attachment_shape_meshes': 1}
        result, errors = AUDIT.verify_weapon_template(definition, entry, shape, shape, 'sword')
        self.assertTrue(result['shape_matches'])
        self.assertEqual(errors, [])
        _, errors = AUDIT.verify_weapon_template(
            definition, entry, shape, dict(shape, attachment_shape_fingerprint='changed'), 'sword')
        self.assertIn('weapon_reference_geometry_mismatch', errors)
        changed = dict(shape, attachment_shape_fingerprint='changed')
        _, errors = AUDIT.verify_weapon_template(definition, entry, changed, changed, 'sword')
        self.assertIn('weapon_reference_geometry_mismatch', errors)
        _, errors = AUDIT.verify_weapon_template(
            definition, entry, shape, dict(shape, loaded=False), 'sword')
        self.assertIn('missing_weapon_reference', errors)
        _, errors = AUDIT.verify_weapon_template(
            definition, entry, shape, dict(shape, attachment_shape_meshes=0), 'sword')
        self.assertIn('invalid_weapon_reference', errors)
        _, errors = AUDIT.verify_weapon_template(definition, entry, shape, shape, 'sword2h')
        self.assertIn('weapon_reference_type_mismatch', errors)
        with self.assertRaises(ValueError):
            AUDIT.verify_weapon_template(dict(definition, action=[51, 3]), entry, shape, shape,
                                         'sword')

    @unittest.skipUnless(PROBE, 'C++ probe is not configured for this run')
    def test_cli_writes_a_report_when_an_asset_is_missing(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / 'data/Model/Clothes').mkdir(parents=True)
            (root / 'data/Item/Weapon').mkdir(parents=True)
            (root / 'items').mkdir()
            AUDIT.write_json(root / 'items/25000_owned.json', {
                'index': 25000, 'name': 'Owned visual label', 'class': ['skin', 'body'],
                'description': 'OpenKal descriptive name; model knight/body 01-01.',
                'action': [1, 1], 'wear': 1, 'stackable': 0, 'limit': {'class': 'knight'}})
            AUDIT.write_json(root / 'names.json', {
                'schema': 'openkal.skin-names/v1', 'source': {'origin': 'owned test'},
                'entries': [{'index': 25001, 'class': 'knight', 'part': 'weapon',
                             'action': [21, 7], 'name': 'Owned weapon'}],
                'ambiguous': [{'index': 25000, 'class': 'knight', 'part': 'body',
                               'action': [1, 1], 'candidates': []}]})
            AUDIT.write_json(root / 'descriptive.json', {
                'schema': 'openkal.skin-descriptive-names/v1', 'source': {'origin': 'owned test'},
                'entries': [{'index': 25000, 'class': 'knight', 'part': 'body',
                             'action': [1, 1], 'name': 'Owned visual label'}]})
            own_entry = {'index': 25001, 'class': 'knight', 'part': 'weapon', 'action': [21, 7],
                         'weapon_type': 'sword2h', 'expected_shape_fingerprint': '0123456789abcdef',
                         'expected_shape_meshes': 1, 'reason': 'Owned fixture grip choice.'}
            weapon = {'index': 25001, 'name': 'Owned weapon', 'class': ['skin', 'weapon'],
                      'action': [21, 7], 'wear': 1, 'stackable': 0, 'limit': {'class': 'knight'}}
            apply_weapon_compatibility(weapon, own_entry)
            AUDIT.write_json(root / 'items/25001_owned_weapon.json', weapon)
            AUDIT.write_json(root / 'compatibility.json', {
                'schema': 'openkal.skin-weapon-compatibility/v1',
                'source': {'origin': 'owned fixture'}, 'entries': [own_entry]})
            AUDIT.write_json(root / 'policy.json', {
                'reserved_range': [25000, 29999], 'next_id': 25002,
                'withdrawn_ranges': [], 'observations': {
                    '25000': {'status': 'manual_check_recorded',
                              'note': 'Synthetic idle-only observation; attacks unverified.'}}})
            result = subprocess.run([
                sys.executable, str(ROOT / 'tools/audit_skins.py'), '--items', str(root / 'items'),
                '--names', str(root / 'names.json'), '--data-root', str(root / 'data'),
                '--descriptive-names', str(root / 'descriptive.json'),
                '--weapon-compatibility', str(root / 'compatibility.json'),
                '--policy', str(root / 'policy.json'),
                '--probe', PROBE, '--output', str(root / 'output')], capture_output=True, text=True)
            self.assertEqual(result.returncode, 1, result.stderr)
            report = json.loads((root / 'output/report.json').read_text())
            self.assertIn('missing_model', report['items'][0]['errors'])
            self.assertEqual(report['items'][0]['technical_status'], 'error')
            self.assertEqual(report['items'][0]['name_match']['status'], 'openkal_descriptive')
            self.assertEqual(report['items'][0]['reference_name_match']['status'], 'unresolved')
            self.assertEqual(report['items'][0]['visual_review']['status'], 'manual_check_recorded')
            for name in ('catalog.csv', 'review.md'):
                exported = (root / 'output' / name).read_text()
                self.assertIn('manual_check_recorded', exported)
                self.assertNotIn('user_confirmed', exported)
            self.assertIn('attacks unverified', (root / 'output/review.md').read_text())
            own_row = report['items'][1]
            self.assertEqual(own_row['name_match']['resolved'], 'Owned weapon')
            self.assertNotIn('source_lines', own_row['name_match'])
            self.assertEqual(own_row['weapon_type_origin'], 'openkal_grip_and_pose_review')
            self.assertFalse(own_row['weapon_compatibility']['original_type_verified'])
            self.assertEqual(own_row['weapon_type'], 'sword2h')
            self.assertIn('weapon_compatibility_geometry_mismatch', own_row['errors'])
            self.assertEqual(report['weapon_compatibility_source']['origin'], 'owned fixture')
            self.assertTrue((root / 'output/catalog.csv').is_file())


if __name__ == '__main__':
    unittest.main()

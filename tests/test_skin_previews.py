"""Owned, asset-free checks for batch costume preview selection and captions."""

import importlib.util
import json
from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
SPEC = importlib.util.spec_from_file_location('skin_previews', ROOT / 'tools/generate_skin_previews.py')
PREVIEW = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PREVIEW)


class SkinPreviewTests(unittest.TestCase):
    def setUp(self):
        self.files = {f'model/clothes/cw_0_{part}01.gb': Path(f'/owned/Cw_0_{part}01.gb')
                      for part in 'sgpafh'}
        self.files['model/clothes/cw_6_c01.gb'] = Path('/owned/Cw_6_c01.gb')
        self.member = {'class': 'knight', 'part': 'helmet', 'model': 'Model/Clothes/CW_6_C01.gb'}

    def test_head_costume_keeps_the_body_and_replaces_hair(self):
        paths, missing = PREVIEW.appearance_paths(self.member, self.files)
        self.assertEqual(missing, [])
        self.assertEqual(len(paths), 6)
        self.assertIn('/owned/Cw_0_a01.gb', paths)
        self.assertNotIn('/owned/Cw_0_h01.gb', paths)
        self.assertEqual(paths[-1], '/owned/Cw_6_c01.gb')

    def test_whole_body_does_not_load_underlying_armor(self):
        self.member['part'] = 'body'
        paths, missing = PREVIEW.appearance_paths(self.member, self.files)
        self.assertEqual(missing, [])
        self.assertEqual(paths, ['/owned/Cw_6_c01.gb', '/owned/Cw_0_f01.gb'])

    def test_complete_costume_head_replaces_face_and_hair(self):
        self.member.update({'part': 'body', 'skin_appearance': {'replace_face': True}})
        paths, missing = PREVIEW.appearance_paths(self.member, self.files)
        self.assertEqual(missing, [])
        self.assertEqual(paths, ['/owned/Cw_6_c01.gb'])
        self.member['skin_appearance'] = {'keep_hair': True}
        paths, missing = PREVIEW.appearance_paths(self.member, self.files)
        self.assertEqual(paths[-2:], ['/owned/Cw_0_f01.gb', '/owned/Cw_0_h01.gb'])

    def test_only_costume_families_are_selected(self):
        groups = [{'kind': 'weapon'}, {'kind': 'costume'}, {'kind': 'boots'}]
        self.assertEqual(PREVIEW.costume_groups({'families': groups}), [groups[1]])

    def test_gallery_escapes_reference_names(self):
        output = PREVIEW.build_html([{'title': '<script>alert(1)</script>',
                                     'sheet': 'sheets/owned.png', 'items': {'knight': 25000}}])
        self.assertNotIn('<script>alert(1)</script>', output)
        self.assertIn('&lt;script&gt;', output)
        self.assertIn('25000', output)

    def test_review_mapping_cannot_move_to_another_item(self):
        review = {'schema': 'openkal.skin-preview-review/v1', 'families': [
            {'action': [6, 1], 'items': {'knight': 25005}}]}
        sheets = [{'key': 'costume-06-01', 'items': {'knight': 25000}}]
        with self.assertRaisesRegex(ValueError, 'stale preview review IDs'):
            PREVIEW.attach_review(sheets, review)

    def test_curated_review_covers_current_costume_models(self):
        review = json.loads((ROOT / 'config/skins/preview-review.json').read_text())
        expected = {}
        active = set()
        for path in (ROOT / 'config/items/skins').glob('*.json'):
            item = json.loads(path.read_text())
            active.add(item['index'])
            if item['class'] == ['skin', 'body'] or item.get('skin', {}).get('costume_mesh'):
                expected.setdefault(tuple(item['action']), {})[item['limit']['class']] = item['index']
        actual = {}
        for family in review['families']:
            key = tuple(family['action'])
            self.assertNotIn(key, actual)
            actual[key] = family['items']
            for note in family['observations']:
                self.assertTrue(set(note['items']) <= set(family['items'].values()))
        self.assertEqual(actual, expected)
        for candidate in review['weapon_duplicate_candidates']:
            self.assertTrue(set(candidate['items']) <= active)
        for round_entry in review['test_rounds']:
            for case in round_entry['cases']:
                self.assertTrue(set(case['items']) <= active)

    def test_review_notes_are_escaped_and_gameplay_state_is_separate(self):
        sheet = {'title': 'Owned example', 'sheet': 'owned.png', 'items': {'knight': 25005},
                 'review': {'description': '<b>Head</b>', 'status': 'live_check_needed',
                            'observations': [{'note': '<script>bad()</script>',
                                              'kind': 'head_only', 'items': [25005]}]}}
        output = PREVIEW.build_html([sheet])
        self.assertNotIn('<script>bad()', output)
        self.assertIn('&lt;script&gt;', output)
        self.assertIn('&lt;b&gt;Head&lt;/b&gt;', output)
        self.assertIn('live check needed', output)
        self.assertIn('data-flagged="true"', output)
        self.assertIn('earlier in-game status', output)

    def test_class_specific_names_remain_visible_in_group_title(self):
        members = {1: {'name_match': {'status': 'community_mapping', 'resolved': 'First (Knight)'}},
                   2: {'name_match': {'status': 'community_mapping', 'resolved': 'Second (Archer)'}}}
        title = PREVIEW.group_title({'action': [11, 1], 'items': {'knight': 1, 'archer': 2}}, members)
        self.assertIn('First / Second', title)

    def test_reported_results_replace_completed_test_commands(self):
        review = {'test_rounds': [{'id': 'first', 'description': 'Recorded results', 'cases': [
            {'items': [25005], 'status': 'manual_check_recorded',
             'result': 'Restores <normal> head', 'check': 'Old retest instructions'}]}]}
        output = PREVIEW.build_html([], review)
        self.assertIn('manual check recorded: 25005', output)
        self.assertIn('Restores &lt;normal&gt; head', output)
        self.assertNotIn('/getitemid 25005', output)
        self.assertNotIn('Old retest instructions', output)

    def test_original_visual_names_are_searchable_without_displacing_community_titles(self):
        members = {1: {'name_match': {'status': 'openkal_descriptive',
                                     'resolved': 'Owned descriptive outfit (Knight)'}}}
        group = {'action': [1, 1], 'items': {'knight': 1}}
        title = PREVIEW.group_title(group, members)
        self.assertIn('Owned descriptive outfit', title)
        self.assertIn('Model 01-01', title)
        group['items']['archer'] = 2
        members[2] = {'name_match': {'status': 'community_mapping', 'resolved': 'Community (Archer)'}}
        self.assertIn('Community', PREVIEW.group_title(group, members))
        self.assertNotIn('Owned descriptive outfit', PREVIEW.group_title(group, members))


if __name__ == '__main__':
    unittest.main()

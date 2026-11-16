#!/usr/bin/env python3
# OpenKal - an open source KalOnline client and server.
# Copyright (C) 2026 OpenKal contributors
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

"""Exercise item cleanup with synthetic configuration, without external game data."""
import json
import tempfile
import unittest
from pathlib import Path

from convert_items import build_item_doc, convert, parse_form


class ItemConversionTests(unittest.TestCase):
    def test_legacy_stack_flag_and_metadata_cleanup(self):
        for field in ('plural', 'Plural', 'stackable'):
            for value in (0, 1):
                for region in ('(country 0 1 2)', '(Country 2)', '(country)'):
                    with self.subTest(field=field, value=value, region=region):
                        doc = build_item_doc(
                            parse_form(f'(item (Index 91001) ({field} {value}) {region} (code 4 8 0 0) (pay 1))'),
                            {}, {},
                        )
                        self.assertEqual(doc['stackable'], value)
                        self.assertEqual(doc['pay'], 1)
                        self.assertNotIn('plural', doc)
                        self.assertNotIn('country', doc)
                        self.assertNotIn('code', doc)
                        self.assertNotIn('extra', doc)

    def test_conversion_preserves_quest_and_equipment_fields(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            init = root / 'items.txt'
            messages = root / 'messages.txt'
            init.write_text(
                '(item (Index 91001) (Name 1) (class quest etc) (country 0 2) (code 0 0 0 0) (plural 1))\n'
                '(item (Index 91002) (Name 2) (class weapon sword) (wear 1) '
                '(limit knight 5) (country 2) (Code 1 2 3 4) (plural 0) (pay 1))\n', encoding='utf-8',
            )
            messages.write_text(
                '(itemname 1 "Test Token")\n(itemname 2 "Test Sword")\n', encoding='utf-8',
            )
            out = root / 'out'
            self.assertEqual(convert(init, messages, out, None), 0)
            docs = {doc['index']: doc for path in out.rglob('*.json')
                    for doc in [json.loads(path.read_text(encoding='utf-8'))] if 'index' in doc}
            self.assertEqual(len(docs), 2)
            self.assertEqual(docs[91001]['class'], ['quest', 'etc'])
            self.assertEqual(docs[91001]['stackable'], 1)
            self.assertEqual(docs[91002]['class'], ['weapon', 'sword'])
            self.assertEqual(docs[91002]['wear'], 1)
            self.assertEqual(docs[91002]['limit'], {'class': 'knight', 'level': 5})
            self.assertEqual(docs[91002]['stackable'], 0)
            self.assertEqual(docs[91002]['pay'], 1)
            for doc in docs.values():
                self.assertNotIn('country', doc)
                self.assertNotIn('code', doc)
                self.assertNotIn('plural', doc)
                self.assertNotIn('extra', doc)


if __name__ == '__main__':
    unittest.main()

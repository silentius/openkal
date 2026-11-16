#!/usr/bin/env python3
# OpenKal - an open source KalOnline client and server.
# Copyright (C) 2026 OpenKal contributors
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

"""Regression tests for source dialogue navigation and padded text files."""
import tempfile
import unittest
from pathlib import Path

from convert_dialogs import convert_link, read_text


class DialogConversionTests(unittest.TestCase):
    def test_load_link_keeps_destination(self):
        for target in ('s900023', 's900023.dat', '900023'):
            self.assertEqual(
                convert_link(['link', ['text', 'Continue'], ['param', 'load', target]]),
                {'type': '900023', 'text': 'Continue'},
            )

    def test_quest_link_keeps_action(self):
        self.assertEqual(
            convert_link(['link', ['text', 'Accept'], ['param', 'send', 'quest', 8, 3]]),
            {'type': 'quest', 'text': 'Accept', 'quest': [8, 3]},
        )

    def test_trailing_nul_does_not_hide_a_text_script(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 's900023.dat'
            path.write_bytes(b'(script (frame (line "Hello")))\x00\x00\r\n')
            self.assertEqual(read_text(path), '(script (frame (line "Hello")))')


if __name__ == '__main__':
    unittest.main()

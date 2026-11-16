"""Regression checks for the project archive boundary and Git-free reproduction."""

import importlib.util
import subprocess
import sys
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parent.parent
SPEC = importlib.util.spec_from_file_location("package_source", ROOT / "tools/package_source.py")
PACK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACK)


class SourcePackageTest(unittest.TestCase):
    def test_only_reviewed_bytes_are_archived_and_export_repackages(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "project"
            root.mkdir()
            (root / "README.md").write_text("current uncommitted contents\n")
            (root / "SourceFiles.txt").write_text("README.md\nSourceFiles.txt\n")
            for name in ("save/key.pem", "build-local/cache", ".env", "private-notes.txt",
                         "config/local-only.json", "RuntimeData/local-only.bin"):
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("synthetic local data")
            first = Path(directory) / "first.tar.gz"
            second = Path(directory) / "second.tar.gz"
            unrelated = first.with_name("first.tar.gz.tmp")
            unrelated.write_text("keep sibling file untouched\n")
            PACK.package(root, first, "0.1.0")
            PACK.package(root, second, "0.1.0")
            self.assertEqual(unrelated.read_text(), "keep sibling file untouched\n")
            self.assertEqual(first.read_bytes(), second.read_bytes())
            with tarfile.open(first) as archive:
                self.assertEqual(archive.getnames(), [
                    "openkal-0.1.0/README.md", "openkal-0.1.0/SourceFiles.txt"])
                self.assertEqual(archive.extractfile(archive.getmembers()[0]).read(),
                                 b"current uncommitted contents\n")
                exported = Path(directory) / "export"
                for entry in archive.getmembers():
                    target = exported / entry.name
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(archive.extractfile(entry).read())
            PACK.package(exported / "openkal-0.1.0", second, "0.1.0")
            self.assertEqual(first.read_bytes(), second.read_bytes())

    def test_check_tracked_requires_manifest_equality(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "SourceFiles.txt").write_text("SourceFiles.txt\nuntracked.txt\n")
            (root / "untracked.txt").write_text("a local file\n")
            output = root / "source.tar.gz"
            command = ["package_source.py", "--root", str(root),
                       "--output", str(output), "--check-tracked"]
            with (
                patch.object(sys, "argv", command),
                patch.object(subprocess, "check_output",
                             return_value="SourceFiles.txt\0tracked.txt\0"),
                patch.object(PACK.argparse.ArgumentParser, "exit",
                             side_effect=SystemExit(1)) as exit_parser,
            ):
                with self.assertRaises(SystemExit):
                    PACK.main()
                message = exit_parser.call_args.args[1]
                self.assertIn("tracked files absent from SourceFiles.txt: tracked.txt", message)
                self.assertIn("untracked files listed in SourceFiles.txt: untracked.txt", message)
            self.assertFalse(output.exists())

    def test_manifest_rejects_escape_and_symlink(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "project"
            root.mkdir()
            outside = Path(directory) / "outside.txt"
            outside.write_text("synthetic external file")
            (root / "linked.txt").symlink_to(outside)
            for entry in ("../outside.txt", "/outside.txt", "save/key.pem", "linked.txt"):
                with self.subTest(entry=entry):
                    (root / "SourceFiles.txt").write_text(
                        "\n".join(sorted(["SourceFiles.txt", entry])) + "\n")
                    with self.assertRaises(ValueError):
                        PACK.source_files(root)


if __name__ == "__main__":
    unittest.main()

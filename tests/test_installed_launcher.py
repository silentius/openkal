"""Exercise the shipped launch script in an isolated synthetic installation."""

import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent


class LauncherTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.share = self.root / "share"
        self.lib = self.root / "lib"
        self.bin = self.root / "bin"
        for directory in (self.share, self.lib, self.bin):
            directory.mkdir()
        self.client = self.root / "assets & bronze|world"
        for name in ("data/Model/Clothes", "data/Model/Motion", "data/Item/Weapon", "map/login_map"):
            (self.client / name).mkdir(parents=True)
        (self.client / "map/login_map/login_maxl.gb").touch()
        self.result = self.root / "launched"
        self.env = dict(os.environ, XDG_CONFIG_HOME=str(self.root / "settings"),
                        XDG_DATA_HOME=str(self.root / "state"),
                        OPENKAL_TEST_RESULT=str(self.result),
                        OPENKAL_TEST_ASSETS=str(self.client),
                        PATH=str(self.bin) + os.pathsep + os.environ["PATH"])
        self.env.pop("OPENKAL_PROFILE", None)
        self.write_executable(self.bin / "zenity", '''#!/bin/sh
case "$*" in
  *--file-selection*) [ "${OPENKAL_TEST_CANCEL:-0}" = 0 ] || exit 1
                     printf '%s\\n' "$OPENKAL_TEST_ASSETS" ;;
esac
''')
        self.write_executable(self.lib / "openkal_client", '''#!/bin/sh
printf '%s\\n' "$OPENKAL_SAVE_DIR" "$@" > "$OPENKAL_TEST_RESULT"
''')
        source = (ROOT / "debian/openkal").read_text()
        source = source.replace("/usr/share/openkal", str(self.share))
        source = source.replace("/usr/lib/openkal", str(self.lib))
        self.launcher = self.root / "openkal"
        self.write_executable(self.launcher, source)
        (self.share / "openkal.profile.example").write_bytes(
            (ROOT / "debian/openkal.profile.example").read_bytes())

    def write_executable(self, path, text):
        path.write_text(text)
        path.chmod(0o755)

    def run_launcher(self, unprivileged=False):
        def drop_privileges():
            os.setgroups([])
            os.setgid(65534)
            os.setuid(65534)

        return subprocess.run([str(self.launcher)], env=self.env, text=True,
                              capture_output=True, timeout=10,
                              preexec_fn=drop_privileges if unprivileged and os.geteuid() == 0 else None)

    def test_first_launch_persists_selection_and_existing_profile_avoids_picker(self):
        run = self.run_launcher()
        self.assertEqual(run.returncode, 0, run.stderr)
        profile = self.root / "settings/openkal/openkal.profile"
        self.assertIn(f"data_root={self.client}/data", profile.read_text())
        self.assertIn(f"map_root={self.client}/map", profile.read_text())
        self.assertTrue(self.result.is_file())
        self.assertIn(str(self.root / "state/openkal"), self.result.read_text())
        self.env["OPENKAL_TEST_CANCEL"] = "1"
        self.assertEqual(self.run_launcher().returncode, 0)

    def test_cancel_does_not_launch_client(self):
        self.env["OPENKAL_TEST_CANCEL"] = "1"
        self.assertNotEqual(self.run_launcher().returncode, 0)
        self.assertFalse(self.result.exists())

    def test_missing_login_model_requires_selection(self):
        self.assertEqual(self.run_launcher().returncode, 0)
        self.result.unlink()
        (self.client / "map/login_map/login_maxl.gb").unlink()
        self.env["OPENKAL_TEST_CANCEL"] = "1"
        self.assertNotEqual(self.run_launcher().returncode, 0)
        self.assertFalse(self.result.exists())

    def test_invalid_profile_and_save_locations_fail_clearly(self):
        blocked = self.root / "regular-file"
        blocked.write_text("not a directory")
        self.env["OPENKAL_PROFILE"] = str(blocked / "profile")
        run = self.run_launcher()
        self.assertNotEqual(run.returncode, 0)
        self.assertIn("Cannot create profile", run.stderr)
        self.env.pop("OPENKAL_PROFILE")
        self.env["XDG_DATA_HOME"] = str(blocked)
        run = self.run_launcher()
        self.assertNotEqual(run.returncode, 0)
        self.assertIn("Cannot create save directory", run.stderr)
        self.assertFalse(self.result.exists())

    def test_read_only_profile_and_save_directories_fail_clearly(self):
        self.assertEqual(self.run_launcher().returncode, 0)
        self.result.unlink()
        self.root.chmod(0o755)
        profile = self.root / "settings/openkal/openkal.profile"
        original = profile.read_text()
        profile.write_text(original.replace(str(self.client), "/missing-assets"))
        profile.parent.chmod(0o555)
        try:
            run = self.run_launcher(unprivileged=True)
            self.assertNotEqual(run.returncode, 0)
            self.assertIn("Cannot update profile", run.stderr)
            self.assertFalse(self.result.exists())
        finally:
            profile.parent.chmod(0o755)
        profile.write_text(original)
        readonly = self.root / "readonly-state"
        readonly.mkdir(mode=0o555)
        self.env["XDG_DATA_HOME"] = str(readonly)
        try:
            run = self.run_launcher(unprivileged=True)
            self.assertNotEqual(run.returncode, 0)
            self.assertIn("Cannot create save directory", run.stderr)
            self.assertFalse(self.result.exists())
        finally:
            readonly.chmod(0o755)


if __name__ == "__main__":
    unittest.main()

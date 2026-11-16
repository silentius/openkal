#!/usr/bin/env python3
"""Verify the project-only Debian artifact in an isolated extraction directory."""

import argparse
from pathlib import Path
import subprocess
import tempfile
import zipfile


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path)
    args = parser.parse_args()
    package = args.package.resolve()
    depends = subprocess.check_output(["dpkg-deb", "-f", str(package), "Depends"], text=True)
    for dependency in ("zenity", "fonts-dejavu-core"):
        if dependency not in depends:
            parser.exit(1, f"Missing runtime dependency: {dependency}\n")
    with tempfile.TemporaryDirectory(prefix="openkal-debian-check-") as directory:
        root = Path(directory)
        subprocess.run(["dpkg-deb", "-x", str(package), str(root)], check=True)
        for name in ("usr/games/openkal", "usr/lib/openkal/openkal_client",
                     "usr/share/doc/openkal/copyright",
                     "usr/share/doc/openkal/THIRD_PARTY_NOTICES.md",
                     "usr/share/doc/openkal/SECURITY.md",
                     "usr/share/doc/openkal/ReleaseNotes-0.1.0.md",
                     "usr/share/doc/openkal/licenses/miniaudio-MIT-0.txt",
                     "usr/share/openkal/openkal.profile.example"):
            if not (root / name).is_file():
                parser.exit(1, f"Missing installed project file: {name}\n")
        with zipfile.ZipFile(root / "usr/share/openkal/config.pak") as archive:
            if archive.testzip():
                parser.exit(1, "Corrupt runtime archive\n")
            forbidden = [name for name in archive.namelist()
                         if any(part in {".git", "save", "source", "templates"}
                                for part in Path(name).parts) or
                         Path(name).suffix.lower() in {".pem", ".key", ".env"}]
            if forbidden:
                parser.exit(1, "Unintended project-release inputs: " + ", ".join(forbidden) + "\n")
        libraries = subprocess.check_output(
            ["ldd", str(root / "usr/lib/openkal/openkal_client")], text=True)
        if "not found" in libraries:
            parser.exit(1, "Unresolved client libraries in the verification environment\n")
    print("Validated project-only Debian contents, runtime dependencies and library resolution")


if __name__ == "__main__":
    main()

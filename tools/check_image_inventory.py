#!/usr/bin/env python3
"""Check the current project image inventory without history or external sources."""

import argparse
import hashlib
import json
from pathlib import Path

IMAGE_SUFFIXES = {".png", ".bmp", ".jpg", ".jpeg", ".gif", ".svg", ".dds", ".tga", ".webp"}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent)
    args = parser.parse_args()
    root = args.root.resolve()
    document = json.loads((root / "docs/provenance/Images.json").read_text(encoding="utf-8"))
    expected = {name for name in (root / "SourceFiles.txt").read_text().splitlines()
                if Path(name).suffix.lower() in IMAGE_SUFFIXES}
    entries = document["assets"]
    actual = {entry["path"] for entry in entries}
    errors = []
    if len(actual) != len(entries):
        errors.append("duplicate image manifest paths")
    for name in sorted(expected - actual):
        errors.append(f"image missing from provenance manifest: {name}")
    for name in sorted(actual - expected):
        errors.append(f"image absent from project source manifest: {name}")
    for entry in entries:
        path = root / entry["path"]
        if not path.is_file():
            errors.append(f"missing image: {entry['path']}")
        elif hashlib.sha256(path.read_bytes()).hexdigest() != entry["sha256"]:
            errors.append(f"image hash changed: {entry['path']}")
    if document["summary"]["image_files"] != len(entries):
        errors.append("image count does not match manifest summary")
    if errors:
        parser.exit(1, "\n".join(errors) + "\n")
    print(f"Validated {len(entries)} project images and SHA-256 hashes")


if __name__ == "__main__":
    main()

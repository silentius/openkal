#!/usr/bin/env python3
"""Package the reviewed project file manifest without Git history or local data."""

import argparse
import gzip
from pathlib import Path, PurePosixPath
import re
import subprocess
import tarfile
import tempfile


MANIFEST = "SourceFiles.txt"


def source_files(root: Path, *, check_tracked: bool = False) -> list[str]:
    """Validate the explicit project source manifest, including exported trees."""
    names = (root / MANIFEST).read_text(encoding="utf-8").splitlines()
    if names != sorted(set(names)) or MANIFEST not in names:
        raise ValueError("SourceFiles.txt must be sorted, unique and include itself")
    for name in names:
        path = PurePosixPath(name)
        if (not name or path.is_absolute() or ".." in path.parts or
                path.as_posix() != name or "\\" in name or
                any(part in {".git", "save", "build", "output", ".pre-commit-cache", "__pycache__"} or
                    part.startswith(("build-", "cmake-build-")) for part in path.parts) or
                path.suffix in {".pem", ".key", ".env"} or path.name == ".env"):
            raise ValueError(f"invalid project source manifest entry: {name}")
        file = root / name
        if file.is_symlink() or not file.is_file():
            raise ValueError(f"source entry must be a regular project file: {name}")
        if not file.resolve().is_relative_to(root.resolve()):
            raise ValueError(f"source entry escapes the project: {name}")
    if check_tracked:
        tracked = set(subprocess.check_output(
            ["git", "-C", str(root), "ls-files", "-z"], text=True).split("\0")[:-1])
        manifest = set(names)
        missing = sorted(tracked - manifest)
        untracked = sorted(manifest - tracked)
        mismatches = []
        if missing:
            mismatches.append("tracked files absent from SourceFiles.txt: " + ", ".join(missing))
        if untracked:
            mismatches.append("untracked files listed in SourceFiles.txt: " + ", ".join(untracked))
        if mismatches:
            raise ValueError("\n".join(mismatches))
    return names


def package(root: Path, output: Path, version: str, *, check_tracked: bool = False) -> int:
    """Archive the validated manifest and return the number of files written."""
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9.+_-]*", version):
        raise ValueError("invalid archive version")
    names = source_files(root, check_tracked=check_tracked)
    if output.resolve() in {(root / name).resolve() for name in names}:
        raise ValueError("archive output must not replace a project file")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".openkal-source-", dir=output.parent) as directory:
        temporary = Path(directory) / "source.tar.gz"
        with temporary.open("wb") as raw:
            with gzip.GzipFile(filename="", fileobj=raw, mode="wb", mtime=0) as compressed:
                with tarfile.open(fileobj=compressed, mode="w|", format=tarfile.PAX_FORMAT) as archive:
                    for name in names:
                        file = root / name
                        info = tarfile.TarInfo(f"openkal-{version}/{name}")
                        info.size = file.stat().st_size
                        info.mode = 0o755 if file.stat().st_mode & 0o111 else 0o644
                        with file.open("rb") as stream:
                            archive.addfile(info, stream)
        temporary.replace(output)
    return len(names)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--version", default="0.1.0")
    parser.add_argument("--check-tracked", action="store_true",
                        help="Check current Git-tracked paths against the manifest, without history")
    args = parser.parse_args()
    root = args.root.resolve()
    try:
        if args.output:
            count = package(root, args.output, args.version, check_tracked=args.check_tracked)
            print(f"Created {args.output} from {count} project files")
        else:
            names = source_files(root, check_tracked=args.check_tracked)
            print(f"Validated {len(names)} project source files")
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Source packaging failed: {error}\n")


if __name__ == "__main__":
    main()

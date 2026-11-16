#!/usr/bin/env python3
"""Compatibility launcher; the client now configures TLS itself."""
import os
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
if os.environ.get("OPENKAL_BUILD_DIR"):
    binary = Path(os.environ["OPENKAL_BUILD_DIR"]).resolve() / "bin" / "openkal_client"
else:
    # Prefer the build being used, including per-account subdirectories such as bin/char1.
    working_directory = Path.cwd()
    candidates = []
    for directory in (working_directory, *working_directory.parents):
        candidates.extend((directory / "openkal_client", directory / "bin" / "openkal_client"))
        if directory == ROOT:
            break
    candidates.append(ROOT / "build-multiplayer" / "bin" / "openkal_client")
    binary = next((path for path in candidates if path.is_file()), candidates[-1])
if not binary.is_file():
    sys.exit("Build openkal_client first.")
arguments = sys.argv[1:]
if arguments:
    arguments[0] = str(Path(arguments[0]).resolve())
os.execve(binary, [str(binary), *arguments], os.environ.copy())

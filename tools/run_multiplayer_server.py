#!/usr/bin/env python3
"""Optional launcher; MainSvr now loads its own database and TLS configuration."""
import os
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
if os.environ.get("OPENKAL_BUILD_DIR"):
    binary = Path(os.environ["OPENKAL_BUILD_DIR"]).resolve() / "bin/openkal_mainsvr"
else:
    working_directory = Path.cwd()
    candidates = []
    for directory in (working_directory, *working_directory.parents):
        candidates.extend((directory / "openkal_mainsvr", directory / "bin/openkal_mainsvr"))
        if directory == ROOT:
            break
    candidates.append(ROOT / "build-multiplayer/bin/openkal_mainsvr")
    binary = next((path for path in candidates if path.is_file()), candidates[-1])
if not binary.is_file():
    sys.exit("Build openkal_mainsvr with OPENKAL_WITH_POSTGRES=ON first.")
os.execve(binary, [str(binary), *sys.argv[1:]], os.environ.copy())

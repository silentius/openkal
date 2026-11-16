"""Verify launcher build selection and preservation of per-account working directories."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


with tempfile.TemporaryDirectory(prefix="openkal-client-launcher-") as temporary:
    root = Path(temporary)
    (root / "tools").mkdir()
    launcher = root / "tools/run_multiplayer_client.py"
    shutil.copyfile(Path(__file__).resolve().parents[1] / "tools/run_multiplayer_client.py", launcher)
    for build in ("build-ninja", "build-multiplayer"):
        binary = root / build / "bin/openkal_client"
        binary.parent.mkdir(parents=True)
        binary.write_text(
            "#!/usr/bin/env python3\n"
            "import json, os, sys\n"
            "print(json.dumps({'binary': sys.argv[0], 'cwd': os.getcwd(), "
            "'args': sys.argv[1:], 'ca': os.environ.get('OPENKAL_TLS_CA')}))\n")
        binary.chmod(0o755)
    working = root / "build-ninja/bin/char1"
    working.mkdir()
    environment = {key: value for key, value in os.environ.items()
                   if key != "OPENKAL_BUILD_DIR" and not key.startswith("OPENKAL_TLS_")}

    def run(directory, overrides=None):
        result = subprocess.run([sys.executable, str(launcher), "account.profile"],
                                cwd=directory, env=dict(environment, **(overrides or {})),
                                check=True, capture_output=True, text=True)
        data = json.loads(result.stdout)
        assert data["cwd"] == str(directory), "launcher changed the working directory"
        assert data["args"] == [str(directory / "account.profile")]
        return data

    for directory in (working, working.parent):
        data = run(directory)
        assert Path(data["binary"]) == root / "build-ninja/bin/openkal_client"
        assert data["ca"] is None, "launcher should let the client configure TLS"
    data = run(working, {"OPENKAL_BUILD_DIR": str(root / "build-multiplayer"),
                         "OPENKAL_TLS_CA": "explicit.pem"})
    assert Path(data["binary"]) == root / "build-multiplayer/bin/openkal_client"
    assert data["ca"] == "explicit.pem"
    assert Path(run(root)["binary"]) == root / "build-multiplayer/bin/openkal_client"

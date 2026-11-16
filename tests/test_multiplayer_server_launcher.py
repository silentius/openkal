"""Verify the optional server launcher and native configuration argument handling."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

MAINSVR, WITH_POSTGRES = sys.argv[1:]

with tempfile.TemporaryDirectory(prefix="openkal-server-launcher-") as temporary:
    root = Path(temporary)
    (root / "tools").mkdir()
    launcher = root / "tools/run_multiplayer_server.py"
    shutil.copyfile(Path(__file__).resolve().parents[1] / "tools/run_multiplayer_server.py", launcher)
    for build in ("build-ninja", "build-multiplayer"):
        binary = root / build / "bin/openkal_mainsvr"
        binary.parent.mkdir(parents=True)
        binary.write_text(
            "#!/usr/bin/env python3\n"
            "import json, os, sys\n"
            "print(json.dumps({'binary': sys.argv[0], 'cwd': os.getcwd(), "
            "'args': sys.argv[1:], 'certificate': os.environ.get('OPENKAL_TLS_CERT')}))\n")
        binary.chmod(0o755)
    working = root / "build-ninja/bin/server1"
    working.mkdir()
    base = {name: value for name, value in os.environ.items()
            if not name.startswith("OPENKAL_TLS_")
            and name not in ("OPENKAL_BUILD_DIR", "OPENKAL_DATABASE_DSN")}

    def run(directory, overrides=None):
        arguments = ["test.profile", "--multiplayer-config", "local.json"]
        result = subprocess.run([sys.executable, str(launcher), *arguments], cwd=directory,
                                env=dict(base, **(overrides or {})), check=True,
                                capture_output=True, text=True)
        data = json.loads(result.stdout)
        assert data["cwd"] == str(directory)
        assert data["args"] == arguments
        return data

    assert Path(run(working)["binary"]) == root / "build-ninja/bin/openkal_mainsvr"
    assert run(working)["certificate"] is None
    data = run(working, {"OPENKAL_BUILD_DIR": str(root / "build-multiplayer"),
                         "OPENKAL_TLS_CERT": "explicit.pem"})
    assert Path(data["binary"]) == root / "build-multiplayer/bin/openkal_mainsvr"
    assert data["certificate"] == "explicit.pem"
    assert Path(run(root)["binary"]) == root / "build-multiplayer/bin/openkal_mainsvr"
    # Native CLI accepts the configuration option without a preceding profile.
    configuration = root / "server.json"
    configuration.write_text(json.dumps({
        "allow_client_registration": False,
        "database_dsn": "owned-private-marker",
        "tls_certificate_file": "missing.pem",
        "tls_private_key_file": "missing-key.pem",
    }))
    result = subprocess.run([MAINSVR, "--multiplayer-config", str(configuration)], cwd=working,
                            env=base, capture_output=True, text=True, timeout=10)
    assert result.returncode == 1
    output = result.stdout + result.stderr
    assert "owned-private-marker" not in output
    if WITH_POSTGRES == "1":
        assert "Supply both existing TLS" in output, output
    else:
        assert "OPENKAL_WITH_POSTGRES=ON" in output, output

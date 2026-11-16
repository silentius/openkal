"""Run the owned PostgreSQL/TLS load profile with an isolated certificate and port."""

import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile


def main():
    if len(sys.argv) < 2:
        print("Usage: test_multiplayer_load.py BINARY [load-profile arguments]", file=sys.stderr)
        return 2
    if not os.environ.get("OPENKAL_TEST_POSTGRES_DSN"):
        print("Set OPENKAL_TEST_POSTGRES_DSN to enable the isolated PostgreSQL load profile.")
        return 77
    with tempfile.TemporaryDirectory(prefix="openkal-load-tls-") as directory:
        root = Path(directory)
        cert, key = root / "server.pem", root / "key.pem"
        subprocess.run(
            ["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes",
             "-days", "1", "-keyout", str(key), "-out", str(cert),
             "-subj", "/CN=localhost", "-addext", "subjectAltName=IP:127.0.0.1"],
            check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        environment = dict(os.environ, OPENKAL_TLS_CERT=str(cert), OPENKAL_TLS_KEY=str(key),
                           OPENKAL_TLS_CA=str(cert), OPENKAL_TLS_SERVER_NAME="127.0.0.1")
        with socket.socket() as probe:
            probe.bind(("127.0.0.1", 0))
            port = probe.getsockname()[1]
        return subprocess.run(
            [sys.argv[1], "--postgres", "--port", str(port), *sys.argv[2:]],
            env=environment).returncode


if __name__ == "__main__":
    sys.exit(main())

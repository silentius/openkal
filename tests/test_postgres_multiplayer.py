"""Exercise PostgreSQL persistence and two real TLS clients with owned fixtures."""
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile

if not os.environ.get("OPENKAL_TEST_POSTGRES_DSN"):
    print("Set OPENKAL_TEST_POSTGRES_DSN to enable isolated PostgreSQL integration tests.")
    sys.exit(77)
with tempfile.TemporaryDirectory(prefix="openkal-multiplayer-tls-") as directory:
    root = Path(directory)
    cert, key = root / "server.pem", root / "key.pem"
    subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes",
                    "-days", "1", "-keyout", str(key), "-out", str(cert),
                    "-subj", "/CN=localhost", "-addext", "subjectAltName=IP:127.0.0.1"],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    environment = os.environ.copy()
    environment.update(OPENKAL_TLS_CERT=str(cert), OPENKAL_TLS_KEY=str(key),
                       OPENKAL_TLS_CA=str(cert), OPENKAL_TLS_SERVER_NAME="127.0.0.1")
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]
    sys.exit(subprocess.run([sys.argv[1], str(port), *sys.argv[2:]], env=environment).returncode)

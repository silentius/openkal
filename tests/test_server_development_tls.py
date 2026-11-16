"""Verify generated local credentials and a real TLS roundtrip without the openssl CLI."""

import hashlib
import os
from pathlib import Path
import ssl
import socket
import subprocess
import sys
import tempfile

STARTUP, TRANSPORT = sys.argv[1:]

with tempfile.TemporaryDirectory(prefix="openkal-development-tls-") as directory:
    root = Path(directory)
    tls = root / "tls"
    processes = [subprocess.Popen([STARTUP, "--generate", str(tls)]) for _ in range(2)]
    assert all(process.wait(timeout=20) == 0 for process in processes)
    certificate, key = tls / "server.pem", tls / "server-key.pem"
    before = hashlib.sha256(certificate.read_bytes()).digest()
    subprocess.run([STARTUP, "--generate", str(tls)], check=True, timeout=5)
    assert hashlib.sha256(certificate.read_bytes()).digest() == before
    assert key.stat().st_mode & 0o777 == 0o600
    base = {name: value for name, value in os.environ.items() if not name.startswith("OPENKAL_TLS_")}
    environment = dict(base, OPENKAL_TLS_CERT=str(certificate), OPENKAL_TLS_KEY=str(key))
    process = subprocess.Popen([TRANSPORT, "server"], env=environment,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        line = process.stdout.readline()
        assert line.startswith("READY "), f"TLS fixture failed: {line!r}"
        port = int(line.split()[1])
        subprocess.run([TRANSPORT, "client-auto", str(port), str(root / "bin"),
                        str(root / "resources"), str(certificate)], env=base,
                       check=True, timeout=12)
        # Check the second advertised identity as well as the numeric-IP C++ roundtrip.
        context = ssl.create_default_context(cafile=str(certificate))
        with socket.create_connection(("127.0.0.1", port), timeout=3) as tcp:
            with context.wrap_socket(tcp, server_hostname="localhost") as connection:
                connection.sendall(b"owned synthetic TLS fixture")
                assert connection.recv(100) == b"owned synthetic TLS fixture"
    finally:
        process.terminate()
        process.communicate(timeout=10)

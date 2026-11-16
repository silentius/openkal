"""Exercise the actual OpenKal TCP transport with temporary synthetic credentials."""

import contextlib
import os
from pathlib import Path
import socket
import ssl
import subprocess
import sys
import tempfile
import threading
import time

BINARY, OPENSSL = sys.argv[1:]


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def run_openssl(*args, **kwargs):
    result = subprocess.run([OPENSSL, *args], capture_output=True, text=True, **kwargs)
    check(result.returncode == 0,
          f"OpenSSL {args[0]} failed ({result.returncode}): {result.stderr}")
    return result


def certificate(root, name):
    cert, key = root / f"{name}.pem", root / f"{name}-key.pem"
    run_openssl(
        "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-sha256",
        "-keyout", str(key), "-out", str(cert), "-days", "1",
        "-subj", "/CN=localhost", "-addext", "subjectAltName=DNS:localhost,IP:127.0.0.1",
    )
    return str(cert), str(key)


def expired_certificate(root, cert, key):
    """Use explicit past validity dates supported across OpenSSL 3 releases."""
    config = root / "expired-ca.cnf"
    config.write_text("""[ca]
default_ca = fixture
[fixture]
database = expired-index.txt
serial = expired-serial.txt
new_certs_dir = .
default_md = sha256
policy = subject
x509_extensions = server
[subject]
commonName = supplied
[server]
basicConstraints = critical, CA:TRUE
keyUsage = critical, digitalSignature, keyEncipherment, keyCertSign
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid:always
subjectAltName = DNS:localhost, IP:127.0.0.1
""", encoding="utf-8")
    (root / "expired-index.txt").write_text("", encoding="utf-8")
    (root / "expired-serial.txt").write_text("01\n", encoding="utf-8")
    request, expired = root / "expired.csr", root / "expired.pem"
    run_openssl("req", "-new", "-key", key, "-subj", "/CN=localhost",
                "-out", str(request))
    run_openssl("ca", "-batch", "-selfsign", "-notext", "-config", str(config),
                "-cert", cert, "-keyfile", key, "-in", str(request),
                "-startdate", "20200101000000Z", "-enddate", "20200102000000Z",
                "-out", str(expired), cwd=root)
    verification = subprocess.run(
        [OPENSSL, "verify", "-CAfile", str(expired), "-purpose", "sslserver",
         "-verify_ip", "127.0.0.1", str(expired)], capture_output=True, text=True,
    )
    check(verification.returncode != 0 and "certificate has expired" in verification.stderr,
          f"expired fixture must fail verification due to expiry: {verification.stderr}")
    return expired


@contextlib.contextmanager
def server(env, mode="server", path=None):
    proc = subprocess.Popen(
        [BINARY, mode] + ([str(path)] if path else []), env=env,
        stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True,
    )
    try:
        line = proc.stdout.readline()
        check(line.startswith("READY "), f"server failed to start: {line!r}")
        yield int(line.split()[1])
    finally:
        proc.terminate()
        proc.communicate(timeout=10)


def client(port, env, expected=0, mode="client", paths=(), cwd=None):
    result = subprocess.run([BINARY, mode, str(port), *map(str, paths)], env=env, cwd=cwd,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=12)
    check(result.returncode == expected,
          f"client exit {result.returncode}, expected {expected}: {result.stderr!r}")


def context(cert):
    ctx = ssl.create_default_context(cafile=cert)
    ctx.minimum_version = ssl.TLSVersion.TLSv1_3
    return ctx


class WireClient:
    """TLS client exposing wire records to test malicious network modifications."""

    def __init__(self, port, cert):
        self.socket = socket.create_connection(("127.0.0.1", port), timeout=3)
        self.incoming, self.outgoing = ssl.MemoryBIO(), ssl.MemoryBIO()
        self.tls = context(cert).wrap_bio(self.incoming, self.outgoing,
                                         server_side=False, server_hostname="localhost")
        while True:
            try:
                self.tls.do_handshake()
                self.flush()
                break
            except ssl.SSLWantReadError:
                self.flush()
                self.incoming.write(self.socket.recv(65536))
        # Let the server consume Finished before the attack.
        time.sleep(0.03)

    def flush(self):
        data = self.outgoing.read()
        if data:
            self.socket.sendall(data)

    def collect(self):
        plaintext = b""
        failed = False
        wire = b""
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            try:
                plaintext += self.tls.read(65536)
            except ssl.SSLWantReadError:
                data = self.socket.recv(65536)
                if not data:
                    failed = True
                    break
                wire += data
                self.incoming.write(data)
            except ssl.SSLError:
                failed = True
                break
        return plaintext, failed, wire


def attack(port, cert, operation):
    wire = WireClient(port, cert)
    secret = b"OpenKal transport authenticated application payload!" * 20
    try:
        wire.tls.write(secret)
        record = wire.outgoing.read()
        check(secret not in record, "application payload visible on the wire")
        if operation == "tamper":
            damaged = bytearray(record)
            damaged[-1] ^= 1
            wire.socket.sendall(damaged)
        elif operation == "replay":
            wire.socket.sendall(record + record)
        elif operation == "plaintext":
            wire.socket.sendall(b"forged game packet" * 5)
        else:
            # Fragment a valid TLS record across multiple TCP writes.
            for offset in range(0, len(record), 7):
                wire.socket.sendall(record[offset:offset + 7])
        if operation == "fragment":
            received = b""
            encrypted = b""
            while len(received) < len(secret):
                try:
                    received += wire.tls.read(65536)
                except ssl.SSLWantReadError:
                    data = wire.socket.recv(65536)
                    check(data, "unexpected EOF")
                    encrypted += data
                    wire.incoming.write(data)
            check(received == secret, "fragmented roundtrip changed data")
            check(secret not in encrypted, "server response visible on the wire")
        else:
            received, failed, _ = wire.collect()
            check(failed, f"{operation} did not terminate the connection")
            if operation == "replay":
                check(received in (b"", secret), "replayed record reached the application twice")
            else:
                check(not received, f"{operation} reached the application")
        return record
    finally:
        wire.socket.close()


def malicious_server(env, cert, key, mode):
    """Check the C++ client's rejection path independently of the C++ server."""
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        listener.settimeout(5)
        errors = []

        def peer():
            try:
                with listener.accept()[0] as connection:
                    connection.settimeout(5)
                    if mode == "plaintext":
                        connection.recv(65536)
                        connection.sendall(b"not a TLS server" * 10)
                        return
                    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
                    ctx.load_cert_chain(cert, key)
                    ctx.num_tickets = 0
                    if mode == "tls12":
                        ctx.maximum_version = ssl.TLSVersion.TLSv1_2
                        try:
                            with ctx.wrap_socket(connection, server_side=True):
                                raise AssertionError("C++ client accepted TLS 1.2")
                        except ssl.SSLError:
                            return
                    incoming, outgoing = ssl.MemoryBIO(), ssl.MemoryBIO()
                    tls = ctx.wrap_bio(incoming, outgoing, server_side=True)
                    while True:
                        try:
                            tls.do_handshake()
                            break
                        except ssl.SSLWantReadError:
                            output = outgoing.read()
                            if output:
                                connection.sendall(output)
                            incoming.write(connection.recv(65536))
                    output = outgoing.read()
                    if output:
                        connection.sendall(output)
                    total = 0
                    while total < 100000:
                        try:
                            total += len(tls.read(65536))
                        except ssl.SSLWantReadError:
                            incoming.write(connection.recv(65536))
                    tls.write(b"forged reply")
                    damaged = bytearray(outgoing.read())
                    damaged[-1] ^= 1
                    connection.sendall(damaged)
            except Exception as error:
                errors.append(error)

        thread = threading.Thread(target=peer, daemon=True)
        thread.start()
        try:
            client(listener.getsockname()[1], env, 4 if mode == "tamper" else 2)
        finally:
            thread.join(timeout=6)
        check(not thread.is_alive() and not errors, f"malicious peer failed: {errors}")


def expect_plaintext_rejection(connection):
    # OpenSSL may send a fatal TLS alert before closing an invalid handshake.
    response = b""
    while True:
        try:
            data = connection.recv(1024)
        except ConnectionResetError:
            break
        if not data:
            break
        response += data
        check(len(response) <= 7, "unexpected application data after plaintext injection")
    if response:
        check(len(response) == 7 and response[0] == 21 and response[1] == 3
              and response[2] in (1, 2, 3) and response[3:6] == b"\x00\x02\x02",
              f"expected only a fatal TLS alert before close, got {response.hex()}")


def main(root):
    cert, key = certificate(root, "server")
    other, other_key = certificate(root, "other")
    expired = expired_certificate(root, cert, key)
    base = {k: v for k, v in os.environ.items() if not k.startswith("OPENKAL_TLS_")}
    env = dict(base, OPENKAL_TLS_CERT=cert, OPENKAL_TLS_KEY=key, OPENKAL_TLS_CA=cert)
    for invalid in (base, dict(env, OPENKAL_TLS_KEY=other_key)):
        proc = subprocess.run([BINARY, "server"], env=invalid, capture_output=True, timeout=3)
        check(proc.returncode == 2, "invalid server credentials did not fail closed")
    with server(env) as port:
        client(port, env)
        # Client defaults perform a real verified handshake from a per-account directory.
        bundle, resources = root / "bin", root / "resources"
        working = bundle / "char1"
        working.mkdir(parents=True)
        paths = (bundle, resources, cert)
        client(port, base, mode="client-auto", paths=paths, cwd=working)
        (resources / "tls").mkdir(parents=True)
        (resources / "tls/ca.pem").write_bytes(Path(cert).read_bytes())
        client(port, base, mode="client-auto", paths=(bundle, resources, ""), cwd=working)
        (bundle / "tls").mkdir()
        (bundle / "tls/ca.pem").write_bytes(Path(cert).read_bytes())
        client(port, base, mode="client-auto", paths=(bundle, root / "absent", ""), cwd=working)
        client(port, dict(base, OPENKAL_TLS_CA=other), 2,
               mode="client-auto", paths=paths, cwd=working)
        client(port, dict(base, OPENKAL_TLS_SERVER_NAME="impostor.invalid"), 2,
               mode="client-auto", paths=paths, cwd=working)
        client(port, dict(env, OPENKAL_TLS_SERVER_NAME="localhost"))
        client(port, dict(env, OPENKAL_TLS_SERVER_NAME="impostor.invalid"), 2)
        client(port, dict(env, OPENKAL_TLS_CA=other), 2)
        client(port, dict(env, OPENKAL_TLS_CA=str(root / "missing.pem")), 2)
        client(port, dict(env, OPENKAL_TLS_SERVER_NAME="127.0.0.2"), 2)
        with socket.create_connection(("127.0.0.1", port), timeout=2) as plain:
            plain.sendall(b"plaintext packet injection")
            expect_plaintext_rejection(plain)
        # A stalled handshake cannot prevent another peer from connecting.
        with socket.create_connection(("127.0.0.1", port), timeout=2) as stalled:
            client(port, env)
            stalled.settimeout(7)
            check(not stalled.recv(1024), "stalled TLS handshake did not expire")
        old = context(cert)
        old.minimum_version = ssl.TLSVersion.TLSv1_2
        old.maximum_version = ssl.TLSVersion.TLSv1_2
        try:
            with old.wrap_socket(socket.socket(), server_hostname="localhost") as connection:
                connection.settimeout(3)
                connection.connect(("127.0.0.1", port))
        except ssl.SSLError:
            pass
        else:
            raise AssertionError("TLS 1.2 downgrade accepted")
        for operation in ("fragment", "tamper", "replay", "plaintext"):
            attack(port, cert, operation)
        # Records captured from a different session must fail too.
        captured = attack(port, cert, "fragment")
        fresh = WireClient(port, cert)
        try:
            fresh.socket.sendall(captured)
            received, failed, _ = fresh.collect()
            check(failed and not received, "cross-session replay accepted")
        finally:
            fresh.socket.close()
        client(port, env)
    with server(dict(env, OPENKAL_TLS_CERT=str(expired))) as port:
        client(port, dict(env, OPENKAL_TLS_CA=str(expired)), 2)
    for mode in ("plaintext", "tls12", "tamper"):
        malicious_server(env, cert, key, mode)
    # TLS configuration is irrelevant to Unix sockets; bytes stay unchanged.
    path = root / "local.sock"
    with server(base, "unix", path):
        with socket.socket(socket.AF_UNIX) as local:
            local.settimeout(3)
            local.connect(str(path))
            local.sendall(b"plain local application data")
            check(local.recv(1024) == b"plain local application data", "Unix transport changed")
    print("TLS transport: roundtrip, trust, identity, expiry, downgrade, tamper, replay, timeout and Unix checks passed")


with tempfile.TemporaryDirectory(prefix="openkal-tls-") as directory:
    main(Path(directory))

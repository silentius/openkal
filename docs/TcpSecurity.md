# TCP transport security

OpenKal TCP connections require TLS 1.3 through the system OpenSSL 3 library.
Every connection gets its own TLS state and traffic keys. Certificate chain,
validity period, and server identity verification are mandatory on the client.
TLS early data (0-RTT) is disabled, and the server issues no session tickets.
There is no plaintext fallback or certificate-verification bypass.

The existing `IPacketCipher` interface remains an optional protocol compatibility
transform. It is process-wide, modifies bytes in place without expanding them,
and does not provide the connection state or authenticated framing needed here.
TLS wraps the full packet stream, including packet lengths, below the game
protocol instead. Leave `NullPacketCipher` in place for this reference setup.
Unix-domain sockets retain their existing plaintext behavior and ignore TLS
configuration, including when the process also has TCP sessions.

## What this protects

TLS record authentication detects network modification and forged ciphertext.
Per-connection keys and record sequencing reject copied records both within a
connection and across connections. Invalid records terminate the connection;
unauthenticated application bytes are never passed to the packet parser. A peer
can still interrupt or flood a connection; confidentiality and integrity do not
provide availability.

A modified client can create valid TLS records on its own connection. TLS does
not prevent packet construction inside the client process, credential theft,
exploits in packet handlers, or cheating by a legitimate authenticated player.
Server-side session state, authorization, packet validation, action timing, and
rate limits remain necessary. The reference transport authenticates the server;
it does not implement client certificates or replace game-account authentication.

## Account protocol violations

The multiplayer server assigns one point to the authenticated account for
each detected protocol violation. It checks frame lengths, out-of-phase
requests, opcodes outside the client protocol, and wire layouts or discrete
values in selected requests. Examples include an invalid attack target type,
a malformed movement or teleport packet, and an invalid trade, trading post,
or talent request. An invalid frame length still closes the connection
immediately. Only a threshold disconnect increments the kick counter.

Gameplay rejections such as a target moving out of range, a skill on
cooldown, or a movement correction leave the score unchanged. A well-formed
teleport acknowledgment can arrive after a teleport has already completed
without adding a point.

The server starts a fresh 300-second countdown when the character enters the
world with points. Each violation in the world restarts the countdown. Every
completed interval without another violation removes one point, down to zero.
Time in the lobby, character selection, the entry handshake, or offline
contributes no decay. Leaving the world discards any partial interval.

At ten points, the server resets the score to zero, increments the kick
counter, and disconnects the account. The third threshold disconnect also
sets the ban flag in the same database update. A banned account cannot sign
in or enter the world. The score and kick history follow the account across
characters and server restarts.

The automatic migration adds `protocol_points`, `protocol_kicks`, and
`banned` to the `accounts` table. Existing accounts start at zero points,
zero kicks, and an unset ban flag. The decay countdown lives in the world
session. Local single-player sessions are outside account enforcement.

## Configuration

The transport reads these environment variables when a TCP acceptor or client is
created. They can also be supplied explicitly as `openkal::server::TlsConfig` to
`Transport::create_tcp_acceptor()` and `Transport::connect_tcp()` (or their free
function wrappers).

- `OPENKAL_TLS_CERT`: server PEM certificate chain, leaf first.
- `OPENKAL_TLS_KEY`: server unencrypted PEM private key. Keep this on the server only.
- `OPENKAL_TLS_CA`: explicit client PEM trust roots. The game client also discovers
  bundled or local development trust as described below; low-level transport
  callers use system trust roots when no explicit trust is supplied.
- `OPENKAL_TLS_SERVER_NAME`: expected server DNS name or IP address. If absent,
  verify the identity from packaged `server/client.json` in the game client, or the
  IPv4 address supplied to low-level `connect_tcp()` callers. DNS identities use SNI.

The game client reads its fixed server address, port and expected certificate
identity from `config/server/client.json` in the configuration package. No server
address or port fields are presented to the player. Client trust is selected in
this order: explicit `OPENKAL_TLS_CA`, physical `tls/ca.pem` beside the executable,
physical `tls/ca.pem` under the resource root, and system roots. Development builds
add the repository's `save/multiplayer/tls/server.pem` before system roots only for
`127.0.0.1` with a local certificate identity. Release builds omit this development
path. An existing but invalid explicit or bundled trust file fails the connection;
it does not trigger a different trust fallback. The client does not change its
working directory, create certificates or need the server private key.

The current connector takes a numeric IPv4 address. A DNS name can be supplied
as the expected certificate identity while connecting to its resolved IPv4
address. Certificate identities must be in the Subject Alternative Name; a
legacy Common Name alone is not accepted.

TCP connection establishment and the client TLS handshake each have a
five-second deadline. Server TLS handshakes advance without blocking the game
loop, expire after five seconds, and have a limit of 64 pending connections.
Application sends have a one-second deadline; failed sends close the TLS session
to avoid continuing after a partially transmitted packet. Receive-buffer limits
remain unchanged. Socket writes suppress SIGPIPE without changing process-wide
signal handlers.

## Standalone server startup

The dedicated server loads its private `server.json`, PostgreSQL credentials and
TLS paths itself; see [Multiplayer setup](Multiplayer.md). In local development
on `127.0.0.1`, missing `save/multiplayer/tls/server.pem` and `server-key.pem` are
created through OpenSSL APIs. The pair uses an RSA 3072-bit key, SHA-256 signature,
30-day validity, and Subject Alternative Names for localhost and 127.0.0.1.
Creation is serialized with a file lock; new private keys have mode 0600.
Existing pairs are reused, and partial pairs are not overwritten. Development
certificate creation is not selected by release builds or non-loopback binds.
Production certificates remain explicitly supplied server deployment files.

## Manual transport reference setup

Install `libssl-dev` and `openssl` on Debian/Ubuntu before building. Generate a
short-lived development certificate outside the source tree:

```bash
umask 077
mkdir -p "$HOME/.local/share/openkal-tls"
openssl req -x509 -newkey rsa:3072 -nodes -sha256 -days 7 \
  -keyout "$HOME/.local/share/openkal-tls/server-key.pem" \
  -out "$HOME/.local/share/openkal-tls/server.pem" \
  -subj '/CN=localhost' \
  -addext 'subjectAltName=DNS:localhost,IP:127.0.0.1'
```

With the database configured in a private `server.json`, start MainSvr with the
certificate and private key overrides:

```bash
OPENKAL_TLS_CERT="$HOME/.local/share/openkal-tls/server.pem" \
OPENKAL_TLS_KEY="$HOME/.local/share/openkal-tls/server-key.pem" \
./build-multiplayer/bin/openkal_mainsvr /path/to/openkal.profile \
  --multiplayer-config /path/to/server.json
```

Clients using `NetClient::connect_tcp()` or `server::connect_tcp()` can trust this
certificate with `OPENKAL_TLS_CA` set to the public `server.pem` file and connect
to `127.0.0.1`. Distribute only the public certificate through a trusted channel.
For deployment, provision a certificate with the actual server identity and a
trusted CA or explicitly managed private trust root; do not reuse development
keys. Private keys and trust settings are deployment files, not packaged game
configuration, and are never placed in `config.pak` by this implementation.

The entrance screen offers Multiplayer through the authenticated TLS lobby.
Registration is available only when the dedicated server enables it. The existing
TLS connection carries login, owned character selection and world entry; clients
never receive database credentials. Single Player retains its embedded Unix
server and local SQLite store. See [Multiplayer setup](Multiplayer.md).

## Verification

```bash
cmake -S . -B build-null -DOPENKAL_ENGINE=null
cmake --build build-null --target openkal_test_tls_transport openkal_test_protocol
ctest --test-dir build-null -R '^(tls_transport|protocol)$' --output-on-failure
```

The integration test generates synthetic, temporary certificates and keys. It
exercises the real C++ transport with a C++ client and Python's standard-library
TLS peer, including multi-record transfers, fragmented records, untrusted,
expired and wrong-identity certificates, missing/mismatched server credentials,
TLS 1.2 rejection by both peers, plaintext injection, ciphertext modification
in both directions, same-session
and cross-session replay, stalled-handshake expiry, and unchanged Unix sockets.
It requires permission to create loopback TCP and Unix sockets and never contacts
an external server. No certificates or keys are committed.

## Design sources and provenance

The transport integration and native development certificate setup are
independently written for OpenKal against public OpenSSL API documentation,
reviewed 2026-10-21. No third-party implementation was copied.
OpenSSL is an Apache-2.0 system dependency; see `THIRD_PARTY_NOTICES.md`.

- [Protocol bounds](https://docs.openssl.org/3.0/man3/SSL_CTX_set_min_proto_version/)
- [Server identity verification](https://docs.openssl.org/3.0/man3/SSL_set1_host/)
- [Nonblocking I/O results](https://docs.openssl.org/3.0/man3/SSL_get_error/)
- [TLS early data and replay considerations](https://docs.openssl.org/3.0/man3/SSL_read_early_data/)

Native development certificate API references (OpenSSL Project; no implementation
code or external fixtures were imported):

- [Key generation](https://docs.openssl.org/3.0/man3/EVP_PKEY_keygen/)
- [Certificate signing](https://docs.openssl.org/3.0/man3/X509_sign/)
- [Certificate extension configuration](https://docs.openssl.org/3.0/man5/x509v3_config/)

# OpenKal 0.1.0

Initial experimental source-only release for Linux, using C++17 and
GPL-3.0-or-later. The archive provides client sources, shared server implementation,
dedicated-server sources, developer tools, maintained configuration and
project artwork and retained compatibility metadata. See
[third-party notices](../THIRD_PARTY_NOTICES.md). The project license does not
assert exclusive authorship of retained community labels or license the
separately supplied compatible game files.

## Build configurations

- Ubuntu 22.04/24.04-compatible core dependencies, CMake 3.20+, Python 3.10+
  and OpenSSL 3.x.
- OpenGL client with an optional local Debian package recipe.
- Headless null backend and project-owned tests without optional game data.
- Dedicated-server sources with optional PostgreSQL support. Multiplayer remains
  experimental; see [setup](Multiplayer.md) and [TCP transport](TcpSecurity.md).
- Qt 6.9+ editors and other optional tools are developer features and are not
  included in the core client package's support claim.

## Runtime inputs and distribution

A source build is not a complete playable game-data installation. Compatible
client models, textures, motions, world catalogs and navigation maps must be
supplied separately for the world being run. The project-only resource archive
does not contain a complete world or the login-map model. Missing optional data
is reported at runtime; unit-test success does not certify a particular data pack.

The resource packer supports external user-maintained roots. Users select and
are responsible for their local inputs and resulting packages. Project review
covers contributions to this repository and artifacts published by OpenKal.
The project release recipe uses the repository's reviewed files; it does not
impose that selection on users' local packages.

The Debian package installs the client and its resources. It does not install
the standalone MainSvr executable or provision a server/database/TLS identity.
No proprietary asset download or automatic external-data conversion is included.

## Known limitations

- Battle Riding Cache and progression mutations are not fully transactional
  across inventory and persistent mount state. Database failure/retry may leave
  inconsistent progression. Treat these paths as experimental; do not operate
  an economy requiring durable guarantees until F01 in the
  [implementation queue](../TODO.md) is resolved.
- Some Battle Riding rules still use C++ defaults; abilities, feedback,
  collection history and mounted observer presentation have incomplete scope.
- Knight costume variants 25003/25004 have known skirt clipping. Optional data
  may lack textures for appearances 25160, 25313 and 25396. Full motion/effect
  verification is not claimed for every cosmetic appearance or BOF set.
- Pet mixing, extended guild systems and the other explicitly listed future
  features are not complete. See the [work index](TASKS.md).
- Castle War includes configurable siege rules and a Test Players simulation.
  The deployment example uses placeholder positions and definition IDs. The
  original castle's layout, gate and Black Crow interactions, and complete
  live round still need verification. See [Castle War](CastleWar.md).
- Cosmetic item-ID renames do not migrate old saved inventories or shortcuts.
  Back up saves before updating. Existing database migrations do not establish
  compatibility with arbitrary old snapshots or external catalogs.
- The client connector uses numeric IPv4 addresses; TLS certificate identity
  verification remains mandatory. Release clients do not use development trust.

## Build and verification

The [release procedure](ReleaseProcedure.md) describes the source archive
and checksum. See the [README](../README.md) for local builds and optional
data. Source archives use `SourceFiles.txt` and require no Git history.
Contribution changes must update that manifest and the image inventory where
applicable.

Security reports: [SECURITY.md](../SECURITY.md). The
[source-release review](OpenSourceReleaseChecklist.md) tracks repository contents,
licensing and redistribution statements. Technical results and remaining work
are recorded separately in [TODO.md](../TODO.md); passing tests are not a rights clearance.

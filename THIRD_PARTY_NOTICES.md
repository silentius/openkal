# Third-party notices

OpenKal is licensed under `GPL-3.0-or-later`; see `LICENSE`. This document
records software used by the source tree and its build. User-supplied game data
is not part of the OpenKal distribution.

The project declaration applies to OpenKal contributions; it is not an assertion
of exclusive authorship or a new license for external material. Retained
compatibility labels and numeric mappings are classified separately in the
[repository content review](docs/OpenSourceReleaseChecklist.md). Their recorded
external origins are not additional vendored software libraries.

## Distributed in this repository

### miniaudio

- Author: David Reid
- Version: 0.11.22, dated 2025-02-24
- Origin: <https://github.com/mackron/miniaudio>
- Selected license: MIT No Attribution (`MIT-0`)
- License text: `third_party/LICENSES/miniaudio-MIT-0.txt`
- OpenKal use: `third_party/miniaudio.h`, included by
  `client/src/misc/Audio.cpp`
- Provenance reviewed: 2026-10-16

The upstream single-header file also contains its complete alternative public
domain and MIT-0 license statements.

## Build and system dependencies

These dependencies are discovered from the build environment and are not
copied into this source repository. Their versions and exact binary-distribution
obligations depend on the packages selected by the builder.

- CMake — BSD-3-Clause; <https://gitlab.kitware.com/cmake/cmake>
- Python — Python-2.0; <https://www.python.org/psf-landing/>
- Pillow — HPND; <https://github.com/python-pillow/Pillow>
- libpng — PNG Reference Library License (libpng-2.0 for current releases);
  <https://www.libpng.org/pub/png/libpng.html>. Used by the independently written
  PNG texture reader and local preview tool. Public API documentation:
  <https://www.libpng.org/pub/png/libpng-manual.txt>. Reviewed 2026-10-29; no
  upstream implementation or image fixtures are copied into the repository.
- FreeType — FTL or GPL-2.0-or-later;
  <https://gitlab.freedesktop.org/freetype/freetype>
- OpenSSL 3.x — Apache-2.0; <https://openssl-library.org/source/license/>;
  used by `libs/server/src/net/TlsSession.cpp` and linked as a system library.
  API documentation: <https://docs.openssl.org/3.0/>. Reviewed 2026-10-20.
  OpenKal contains independently written integration code; no upstream code or
  certificate/private-key fixtures are copied into the repository.
- SQLite — public domain; <https://www.sqlite.org/copyright.html>
- GLFW — Zlib; <https://github.com/glfw/glfw>
- GLEW — BSD-3-Clause, MIT, and public-domain components;
  <https://github.com/nigels-com/glew>
- Qt 6 — available under LGPL-3.0-only, GPL licenses, or commercial terms;
  <https://www.qt.io/licensing/open-source-obligations>
- OpenGL implementation and system threading/dynamic-loading libraries —
  supplied by the operating system.

Downstream binary distributors must record the versions they actually ship and
comply with the corresponding license and relinking/source-offer requirements.
This inventory is not a substitute for the notices supplied by those packages.

## Optional content not distributed by OpenKal

Externally supplied audio is optional user data and is not distributed or
covered by the OpenKal GPL license. The three bundled quest cues were synthesized
for OpenKal; their provenance and project license
are documented in `RuntimeData/sounds/README.md`.

## Optional PostgreSQL persistence

PostgreSQL libpq — PostgreSQL Global Development Group; PostgreSQL License;
<https://www.postgresql.org/about/licence/>. It is an optional system dependency
of `libs/server/src/store/PostgresStore.cpp`, enabled by
`OPENKAL_WITH_POSTGRES`. Neither libpq source nor binaries are copied into the
repository. The integration is independently written using the public libpq API
documentation: <https://www.postgresql.org/docs/current/libpq-exec.html>.
Reviewed 2026-10-20. The schema and record mappings adapt OpenKal's existing
project-owned persistence model.

`libs/server/src/AccountPassword.cpp` uses the existing OpenSSL system dependency
for random salts, scrypt derivation and constant-time comparison. Its design
uses public API documentation at
<https://docs.openssl.org/3.0/man7/EVP_KDF-SCRYPT/>. Reviewed 2026-10-20; no upstream
implementation, password corpus, private code or certificate fixtures are copied.

# Configuration package

`config.pak` deploys OpenKal configuration and runtime resources in one
ZIP-compatible, uncompressed container. It is a resource package, not encryption
or a security boundary.

## Build inputs

CMake's `openkal_config_archive` target runs `tools/package_config.py` and
produces `build/config.pak`. The builder collects `config/` and `RuntimeData/`,
converts owned PNG icon templates to runtime BMPs and includes the exported
Dragon UI textures. Template source artwork is not a runtime resource.
Entries are sorted, use fixed timestamps and ZIP Store, and are written to a
temporary archive before replacing the output. Duplicate packaged item indices
fail generation.

An optional `OPENKAL_DATA_ROOT` contributes compatible `config/` and
`RuntimeData/` trees at build time. Project files take precedence over external
files with the same archive path. Only the three owned quest audio cues are
included from the checkout. Additional external audio requires
`OPENKAL_INCLUDE_USER_AUDIO=ON`, which the release preset rejects.
See the [external data setup](../../README.md#3-prepare-external-data).

Database credentials, TLS keys, saves, logs and private dedicated-server settings
remain physical deployment files outside these resource trees.

## Runtime selection

`platform::ConfigArchive::select` selects one configuration source at startup:

1. An existing loose directory named by `configuration_root`.
2. Otherwise, `config.pak` below the resource root.

Relative configured roots resolve below the build-time resource root. An empty
root selects the archive. This is a whole-tree choice: a missing loose file does
not fall through to a packaged copy. Build a combined archive to combine project
and external configuration.

`ConfigArchive` mounts the logical `config/` and `RuntimeData/` paths.
Platform file reads, existence checks and directory enumeration support the
mounted paths. Catalogs retain their ordinary JSON validation. Loose files have
filesystem modification times; packaged resources are immutable for the process
lifetime and have no modification time.

The reader accepts ZIP Store only and rejects compressed, encrypted, multi-disk,
duplicate and traversal entries. It is deliberately narrower than a general ZIP
extractor.

## Development and deployment

Use a loose tree for direct configuration editing. Development clients support
UI hot reload; release clients load each layout on first use and retain it for
the session. The UI editor remains editable in either build.

For packaged configuration changes, rebuild `openkal_config_archive` and restart
the client/server. Release builds use `/usr/share/openkal` as their resource root;
the Debian package installs the archive there. Dedicated-server policy files
such as `server.json` and `guilds.json` are selected separately and are never
client-authoritative.

Inspect an archive with ordinary ZIP tooling:

```sh
python3 -m zipfile -l build/config.pak
```

The `config_archive` test covers archive reading, source selection and malformed
inputs. Packaging checks cover deterministic output, resource selection and
item-index conflicts. No external game data is needed for these checks.

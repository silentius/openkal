# OpenKal

OpenKal is a Linux-first C++17 implementation of a KalOnline-compatible client
and MainSvr. It includes an embedded single-player server with SQLite saves, a
dedicated multiplayer server with PostgreSQL, and game-data inspection tools.

This guide takes a new Ubuntu user from cloning the source to installing a
Release client package. **A successful build is not a complete game-data
installation.** The repository builds and runs its project-owned tests without
external data, but playing requires compatible client assets, JSON game
catalogs and server navigation maps supplied separately.

OpenKal 0.1.0 is a source-only release. See the
[release procedure](docs/ReleaseProcedure.md),
[0.1.0 release notes](docs/ReleaseNotes-0.1.0.md) and [security policy](SECURITY.md).
Dedicated multiplayer and the documented incomplete gameplay systems are experimental.

## Contents

1. [Install Ubuntu dependencies](#1-install-ubuntu-dependencies)
2. [Clone the source](#2-clone-the-source)
3. [Prepare external data](#3-prepare-external-data)
4. [Convert local text configuration to JSON](#4-convert-local-text-configuration-to-json)
5. [Choose the multiplayer server address](#5-choose-the-multiplayer-server-address)
6. [Build Release and config.pak](#6-build-release-and-configpak)
7. [Build and install the Debian package](#7-build-and-install-the-debian-package)
8. [Start and configure the installed client](#8-start-and-configure-the-installed-client)
9. [Run a dedicated multiplayer server](#9-run-a-dedicated-multiplayer-server)
10. [Development builds, tests and tools](#10-development-builds-tests-and-tools)
11. [Troubleshooting](#11-troubleshooting)
12. [C++ style checks](#12-c-style-checks)

## 1. Install Ubuntu dependencies

The commands below use Ubuntu 24.04 LTS package names. They also cover the core
build on Ubuntu 22.04 LTS. Use a graphical desktop and working OpenGL drivers to
play; a headless server or the `null` backend cannot display the game.

```bash
sudo apt update
sudo apt install -y \
  git build-essential cmake ninja-build pkg-config nano \
  python3 python3-pil \
  libssl-dev openssl libsqlite3-dev libpng-dev libfreetype-dev \
  libgl1-mesa-dev libglfw3-dev libglew-dev \
  dpkg-dev fakeroot zenity fonts-dejavu-core
```

CMake must be at least 3.20, the compiler must support C++17, and OpenSSL must be
3.x. Python 3.10 or newer covers the conversion tools as well as packaging.
Pillow creates runtime icons; FreeType renders UI text. `dpkg-dev` provides
`dpkg-shlibdeps` for Debian packaging, and `zenity` supplies the installed
launcher's asset-folder picker. Package availability can be checked in the
[Ubuntu package index](https://packages.ubuntu.com/).

```bash
cmake --version
g++ --version
openssl version
python3 -c 'from PIL import Image; print("Pillow is available")'
```

The commands in this guide explicitly disable the Area Editor. It is enabled
by default in CMake and requires **Qt 6.9 or newer**, which is separate from the
core client dependencies. Installing an older `qt6-base-dev` does not satisfy
that requirement. See [optional tools](#optional-qt-tools) if you want it.

For a PostgreSQL-backed multiplayer server, also install:

```bash
sudo apt install -y libpq-dev
```

Single Player does not require PostgreSQL or Docker.

## 2. Clone the source

Use HTTPS so cloning does not require a GitHub SSH key:

```bash
mkdir -p "$HOME/OpenKalProject"
cd "$HOME/OpenKalProject"
git clone https://github.com/silentius/openkal.git
cd openkal
export OPENKAL_SRC="$PWD"
```

Run subsequent commands from this `openkal` checkout unless a step explicitly
changes directories. Variables exported in this guide last only for the current
terminal; set them again when opening another terminal.

To update an existing clean checkout before rebuilding:

```bash
git switch main
git pull --ff-only
```

Keep a copy of local configuration edits before updating. Do not discard edits
merely to make an update command succeed.

## 3. Prepare external data

There are three separate inputs:

- **OpenKal source**: this checkout, including maintained JSON, UI and runtime
  artwork, build scripts and converters.
- **User-maintained data**: an external directory such as `openkal_data`,
  containing additional compatible JSON under `config/` and `.akarea` navigation
  maps under `RuntimeData/areas/`. CMake can combine these with the source data
  into `config.pak`.
- **Client assets**: a separate compatible installation containing `data/` and
  `map/` (models, textures, motions and client maps). These are selected at
  runtime; `OPENKAL_DATA_ROOT` does not point to this installation.

Only use source inputs, assets and tools that you are authorized to use.
OpenKal supplies no proprietary data download, extraction or decryption
workflow. Conversion does not grant permission to redistribute the inputs or
the generated JSON. Keep records of the origin and permission for your local
inputs; review redistribution rights before sharing any resulting package.
The OpenKal source license does not license a third party's data.

The resource packer supports combining OpenKal resources with user-supplied
external data. Those local inputs and the packages users create from them are
outside the OpenKal source distribution. Users are responsible for selecting,
using and sharing their own inputs. Project review covers contributions to this
repository and artifacts published by OpenKal: pull requests must be checked for
the origin and redistribution permissions of contributed code, data and assets
before they are merged. The packer does not verify those permissions for local
inputs.

Create your own data directory; a separate Git repository is not required:

```bash
export OPENKAL_DATA_ROOT="$HOME/OpenKalProject/openkal_data"
mkdir -p "$OPENKAL_DATA_ROOT/config"
mkdir -p "$OPENKAL_DATA_ROOT/RuntimeData/areas"
```

A populated directory can look like this:

```text
OpenKalProject/
├── openkal/                       # This source checkout
└── openkal_data/                  # Local data, outside the source checkout
    ├── config/
    │   ├── mapinfo.json           # Compatible map catalog
    │   ├── init_map.json          # Additional map metadata, when needed
    │   ├── portals.json          # Compatible portal definitions
    │   ├── items/
    │   ├── monsters/
    │   ├── npcs/
    │   ├── groups/
    │   ├── itemgroups/
    │   ├── prefixes/
    │   ├── skills/
    │   ├── skillbooks/
    │   ├── skill_damage/
    │   ├── quests/
    │   ├── tasks/
    │   ├── dialogs/
    │   └── pets/
    └── RuntimeData/
        └── areas/                # n_XXX_YYY.akarea navigation cells
```

Creating empty directories does not populate a playable world. Supply the
catalogs and area cells for the world you intend to run. If you already have
reviewed, compatible JSON and maps, keep using them and skip regeneration.
The converters below cover specific text catalogs; they do **not** reconstruct
an entire data installation, including map catalogs and portals, automatically.
Prepared compatible map JSON must be supplied separately. This checkout has no
text-to-JSON converter for those map/portal catalogs. The current accepted JSON
fields are defined by the [map catalog reader](libs/map/src/MapCatalog.cpp) and
[portal reader](libs/map/src/Portal.cpp); filenames alone do not establish
compatibility with a world or data version.

Keep text inputs outside both `config/` and `RuntimeData/`. The packager
includes files from those trees; they are not a safe place for text,
backups, credentials or private keys.

### Client asset directory

Set this to your own compatible installation, replacing the example path:

```bash
export OPENKAL_CLIENT_ROOT="/absolute/path/to/KalOnline"
test -d "$OPENKAL_CLIENT_ROOT/data/Model/Clothes"
test -d "$OPENKAL_CLIENT_ROOT/data/Model/Motion"
test -d "$OPENKAL_CLIENT_ROOT/data/Item/Weapon"
test -f "$OPENKAL_CLIENT_ROOT/map/login_map/login_maxl.gb"
```

These are initial checks, not a complete asset compatibility audit. Linux paths
and filenames are case-sensitive. The asset picker selects the **parent folder
containing `data` and `map`**. Do not select `openkal_data` there.

Dungeon entry also needs the matching `.map` and region/object `.gb` geometry
under `map_root` on the server. Missing geometry denies entry. The dedicated
server therefore needs its own access to the relevant map assets.

## 4. Convert local text configuration to JSON

The offline Python converters live in this source checkout under
`tools/*_config_convert/` and use the Python standard library. They accept local
readable text inputs; some legacy `.dat` files are text despite their extension.
Encrypted or packed files are not valid inputs to these commands.

**Generate into a new staging directory.** Several `--all` operations remove
existing JSON in their output category, and other converters overwrite matching
files. Existing catalogs and area maps can contain manual corrections. Never
point these examples at the source checkout's `config/` or regenerate a curated
`openkal_data/config/` in place.

```bash
cd "$OPENKAL_SRC"
export OPENKAL_INPUT_ROOT="/absolute/path/to/authorized-readable-inputs"
export OPENKAL_CONVERSION_ROOT="$(mktemp -d "$HOME/OpenKalProject/openkal-converted.XXXXXX")"
mkdir -p "$OPENKAL_CONVERSION_ROOT/config"
```

The filenames below describe expected input types. Adapt each input path to your
own directory layout and supported data version. Run only the sections for
which you have suitable inputs. Every converter provides `--help`, for example:

```bash
python3 tools/item_config_convert/convert_items.py --help
python3 tools/npc_config_convert/convert_npcs.py --help
```

### Items and drop tables

```bash
python3 tools/item_config_convert/convert_items.py --all \
  --init "$OPENKAL_INPUT_ROOT/InitItem.txt" \
  --message "$OPENKAL_INPUT_ROOT/message-e.txt" \
  --out "$OPENKAL_CONVERSION_ROOT/config/items"

python3 tools/itemgroup_config_convert/convert_itemgroups.py --all \
  --src "$OPENKAL_INPUT_ROOT/ItemGroup.txt" \
  --groups-out "$OPENKAL_CONVERSION_ROOT/config/groups" \
  --itemgroups-out "$OPENKAL_CONVERSION_ROOT/config/itemgroups"
```

Items use `stackable` in current JSON; the converter maps legacy `plural` to it
and removes obsolete item `country` and `code` metadata. Display names and
available descriptions are stored directly in JSON. Text files are
not needed by the runtime item loader.

The converter preserves source item IDs; it does not migrate legacy IDs to
OpenKal's maintained item allocations. Resolve references and collisions with
maintained items before adopting generated output. The packager rejects two
item definitions with the same `index`, even if their filenames differ.

### Monsters and NPCs

```bash
python3 tools/monster_config_convert/convert_monsters.py --all \
  --init "$OPENKAL_INPUT_ROOT/InitMonster.txt" \
  --gen "$OPENKAL_INPUT_ROOT/GenMonster.txt" \
  --message "$OPENKAL_INPUT_ROOT/message-e.txt" \
  --macro "$OPENKAL_INPUT_ROOT/macro.dat" \
  --out "$OPENKAL_CONVERSION_ROOT/config/monsters"

python3 tools/npc_config_convert/convert_npcs.py --all \
  --init "$OPENKAL_INPUT_ROOT/InitNPC.txt" \
  --message "$OPENKAL_INPUT_ROOT/message-e.txt" \
  --macro "$OPENKAL_INPUT_ROOT/macro.dat" \
  --overrides "$OPENKAL_SRC/config/npcs" \
  --out "$OPENKAL_CONVERSION_ROOT/config/npcs"
```

`macro.dat` must already be readable compatible text. The NPC `--overrides`
directory supplies maintained OpenKal JSON. Records are matched by `index`;
provided top-level fields replace generated fields, and additional NPCs are
included. Override filenames are retained. This preserves service roles such as
`storage_keeper` and `trading_post_collector`, placement, overhead icons and
name offsets that cannot be inferred from legacy text. Duplicate override IDs
are rejected. Input overrides and output must be separate directories.
Use a separately reviewed override directory for additional local corrections;
keep the project's current NPC definitions in that directory too. See
[NPC configuration](docs/Npcs.md) for placement and presentation semantics.

### Prefixes, quests, journal tasks and dialogs

```bash
python3 tools/prefix_config_convert/convert_prefixes.py --all \
  --prefix "$OPENKAL_INPUT_ROOT/Prefix.txt" \
  --message "$OPENKAL_INPUT_ROOT/message-e.txt" \
  --out "$OPENKAL_CONVERSION_ROOT/config/prefixes"

python3 tools/quest_config_convert/convert_quests.py --all \
  --quest "$OPENKAL_INPUT_ROOT/Quest.txt" \
  --out "$OPENKAL_CONVERSION_ROOT/config/quests"

python3 tools/quest_config_convert/convert_tasks.py --all \
  --task "$OPENKAL_INPUT_ROOT/task.dat" \
  --out "$OPENKAL_CONVERSION_ROOT/config/tasks"

python3 tools/quest_config_convert/convert_dialogs.py --all \
  --scripts "$OPENKAL_INPUT_ROOT/script_pk" \
  --out "$OPENKAL_CONVERSION_ROOT/config/dialogs"
```

The dialog input is a directory of readable `s######.dat` scripts. Conversion
alone does not verify quest reachability or that all conditions and actions
are supported in gameplay.

### Skills, skill books and skill damage

```bash
python3 tools/skill_config_convert/convert_skills.py --all \
  --init "$OPENKAL_INPUT_ROOT/InitSkill.txt" \
  --jobsystem "$OPENKAL_INPUT_ROOT/jobsystem-e.txt" \
  --message "$OPENKAL_INPUT_ROOT/message-e.txt" \
  --skillbook "$OPENKAL_INPUT_ROOT/SkillBook.txt" \
  --old-damage "$OPENKAL_INPUT_ROOT/OldSkillDamage.txt" \
  --out "$OPENKAL_CONVERSION_ROOT/config/skills" \
  --skillbooks-out "$OPENKAL_CONVERSION_ROOT/config/skillbooks" \
  --old-damage-out "$OPENKAL_CONVERSION_ROOT/config/skill_damage"
```

The skill-book and old-damage pairs are optional. Omit both the corresponding
source and output option if you are not converting that category.

### Pets

```bash
python3 tools/pet_config_convert/convert_pets.py \
  --pet-source "$OPENKAL_INPUT_ROOT/Pet.txt" \
  --monster-recipe-source "$OPENKAL_INPUT_ROOT/MonsterPet.txt" \
  --monster-price-source "$OPENKAL_INPUT_ROOT/CreateMonsterPet.dat" \
  --item-source "$OPENKAL_INPUT_ROOT/InitItem.dat" \
  --macro-source "$OPENKAL_INPUT_ROOT/macro.dat" \
  --switch-model-source "$OPENKAL_INPUT_ROOT/switchmodel.dat" \
  --out "$OPENKAL_CONVERSION_ROOT/config/pets"
```

If you maintain reviewed pet presentation corrections, add
`--presentation-overrides /absolute/path/to/presentation_overrides.json`.

### Review and adopt generated files

Check converter output for errors, empty catalogs, unknown fields and missing
references. Keep a backup of your data directory. For example, compare a newly
converted item category with an existing one:

```bash
diff -ru "$OPENKAL_DATA_ROOT/config/items" \
  "$OPENKAL_CONVERSION_ROOT/config/items"
```

`diff` exits with status 1 when files differ. Review and merge corrections
category by category. **Only for a new, empty data installation**, the reviewed
staging configuration can be copied as follows:

```bash
cp -a "$OPENKAL_CONVERSION_ROOT/config/." "$OPENKAL_DATA_ROOT/config/"
```

This does not supply missing map/portal catalogs or fix incompatible IDs. Do not
use it to overwrite an established curated installation.

### Convert navigation maps

Build the C++ importer independently of the Qt editor:

```bash
cmake -S . -B build-import -G Ninja \
  -DOPENKAL_ENGINE=null -DOPENKAL_BUILD_TESTS=OFF \
  -DOPENKAL_BUILD_TOOLS=OFF -DOPENKAL_BUILD_AREA_EDITOR=OFF \
  -DOPENKAL_BUILD_PROFILE_CLIENT=OFF -DOPENKAL_BUILD_AREA_TOOLS=ON
cmake --build build-import --target area_import area_info --parallel 2
```

For an authorized compatible KSM cell, import into staging first:

```bash
mkdir -p "$OPENKAL_CONVERSION_ROOT/RuntimeData/areas"
./build-import/bin/tools/area_import \
  "$OPENKAL_INPUT_ROOT/n_032_029.ksm" \
  "$OPENKAL_CONVERSION_ROOT/RuntimeData/areas/n_032_029.akarea" --smooth
./build-import/bin/tools/area_info \
  "$OPENKAL_CONVERSION_ROOT/RuntimeData/areas/n_032_029.akarea"
```

The coordinate pair is inferred from the input filename; `--cell 32 29` can
supply it explicitly. Add `--player-collision` to apply the KSM collision
subcells to player movement as well as navigation. `--smooth` is optional and
changes the imported collision masks. Review the result before copying cells into
`$OPENKAL_DATA_ROOT/RuntimeData/areas/`. Never overwrite hand-edited cells as a
routine build step. See the [area-map format](docs/area-map-format.md).

## 5. Choose the multiplayer server address

Do this **before building the client package** if it should connect to your own
server. Edit [`config/server/client.json`](config/server/client.json) in the
source checkout:

```json
{
  "host": "127.0.0.1",
  "port": 30001,
  "tls_server_name": "localhost"
}
```

- `host` is the numeric IPv4 address reachable by players. `127.0.0.1` works only
  when client and server run on the same computer. For another machine, replace
  it with the server's reachable LAN or public IPv4 address.
- `port` is the server TCP port (default 30001).
- `tls_server_name` is the DNS name or IP covered by the server certificate's
  Subject Alternative Name. It can differ from the numeric connection address.

The login dialog uses this packaged endpoint; players do not enter another
server there. Client profile `network_host`/`network_port` do not override it.
Editing an external `openkal_data/config/server/client.json` also cannot
override the checked-in file: **source-checkout files win at the same archive
path**. Rebuild `config.pak` and the `.deb` after changing the endpoint, then
reinstall the package.

MainSvr's **bind address** is separate: configure `network_host` and
`network_port` in the server's profile. A loopback bind accepts only local
clients; a specific interface address or `0.0.0.0` can serve other machines.
`0.0.0.0` is a bind address, not a destination for the client. Allow the selected
TCP port through the host firewall and, when applicable, configure router port
forwarding. The PostgreSQL port should remain private to the server.

TCP requires TLS 1.3 with trust and identity verification. Use a certificate
for the actual server identity. For a private CA, a public trust file at
`/usr/share/openkal/tls/ca.pem` is discovered by the installed client. This file
is a separate deployment input; the current package does not install it for
you. `OPENKAL_TLS_CA=/absolute/path/to/ca.pem` is an explicit alternative.
Never give clients the server's private key or database credentials.
See [TCP security](docs/TcpSecurity.md) for trust setup and local certificates.
Single Player uses a Unix socket and needs no certificates.

## 6. Build Release and config.pak

From the source checkout, with the prepared data directory selected:

```bash
cd "$OPENKAL_SRC"
cmake --preset release \
  -DOPENKAL_ENGINE=gl \
  -DOPENKAL_BUILD_AREA_EDITOR=OFF \
  -DOPENKAL_DATA_ROOT="$OPENKAL_DATA_ROOT" \
  -DOPENKAL_INCLUDE_USER_AUDIO=OFF
cmake --build build-release --parallel 2
```

Increase `--parallel 2` if you have enough memory. The preset uses Ninja,
`CMAKE_BUILD_TYPE=Release` and `OPENKAL_RELEASE_BUILD=ON`. It disables unit tests,
general developer tools and the duplicate profiling client. The Area Editor
must be disabled separately as shown above.

The build produces:

- `build-release/bin/openkal_client`
- `build-release/bin/openkal_mainsvr` (TCP multiplayer needs PostgreSQL enabled;
  see the server section)
- `build-release/config.pak`, containing project resources plus the selected
  external `config/` and `RuntimeData/` trees

The archive is an uncompressed ZIP container. Check its integrity and contents:

```bash
python3 -m zipfile -t build-release/config.pak
python3 -m zipfile -l build-release/config.pak
```

Look for your expected `config/monsters/`, `config/npcs/` and
`RuntimeData/areas/*.akarea` entries. CMake does not require a complete playable
data catalog, so a build with an empty external directory can still succeed.
To update just the archive after data changes, build `config_pak` in the
configured build directory:

```bash
cmake --build build --target config_pak
```

CMake tracks additions, removals and modifications in enabled data trees.
At identical archive paths, source files take precedence over external files.
A valid loose `configuration_root` at runtime selects one loose tree instead
of combining it with the archive. Leave that profile field empty to use the
combined `config.pak`.

Release binaries always look for resources under **`/usr/share/openkal`**,
including when configured manually with `-DCMAKE_BUILD_TYPE=Release`. Running
`build-release/bin/openkal_client` does not select the freshly built archive in
`build-release/`; install the package before testing the Release client. Use a
Debug build for source-tree testing.

The build does not copy your client `data/` and `map/` assets into the archive.
Optional external audio is excluded. Only OpenKal's synthesized quest
cues are included from the source tree. Local development can explicitly enable
external audio with `OPENKAL_INCLUDE_USER_AUDIO=ON`; the release preset rejects
that option. See [audio provenance](RuntimeData/sounds/README.md).

To build and test OpenKal without external data at all, explicitly configure
`-DOPENKAL_DATA_ROOT=`. CMake caches options: omitting an old data-root argument
does not clear a previously configured value.

## 7. Build and install the Debian package

After the Release configuration and build above:

```bash
cmake --build --preset release-package --parallel 2
ls -lh build-release/*.deb
```

The result is currently named `openkal-client_0.1.0_amd64.deb` on amd64;
version and architecture depend on the build. Inspect the package before
installing, substituting the actual filename:

```bash
dpkg-deb --info build-release/openkal-client_0.1.0_amd64.deb
dpkg-deb --contents build-release/openkal-client_0.1.0_amd64.deb
sudo apt install ./build-release/openkal-client_0.1.0_amd64.deb
```

Build and install on compatible Ubuntu releases and architectures. CPack derives
shared-library dependencies from your build machine; this is not a universal
binary package for every Linux distribution.

The package installs the client binary under `/usr/lib/openkal`, resources and
`config.pak` under `/usr/share/openkal`, and the launcher as
`/usr/games/openkal`. It includes a desktop entry, profile example and license
notices. **It does not install MainSvr, a PostgreSQL database, or your client
asset directories.** `zenity` from step 1 is also required by the launcher.

A package made with `OPENKAL_DATA_ROOT` contains the eligible external files
from that directory. Review its contents and their redistribution permissions
before giving it to anyone else. Packaging does not remove third-party rights.

## 8. Start and configure the installed client

Start **OpenKal** from the desktop menu or run:

```bash
/usr/games/openkal
```

On first launch, select your compatible installation's parent folder containing
`data/` and `map/`. Canceling exits. The launcher stores absolute asset paths in
`${XDG_CONFIG_HOME:-$HOME/.config}/openkal/openkal.profile`.

To configure paths manually, open that profile after its creation, or create it
from the installed example:

```bash
mkdir -p "$HOME/.config/openkal"
cp -n /usr/share/openkal/openkal.profile.example \
  "$HOME/.config/openkal/openkal.profile"
nano "$HOME/.config/openkal/openkal.profile"
```

With a custom `XDG_CONFIG_HOME`, use that directory instead. Relevant values are:

```ini
data_root=/absolute/path/to/KalOnline/data
map_root=/absolute/path/to/KalOnline/map
server_map_root=/usr/share/openkal/RuntimeData/areas
configuration_root=
```

The server-map path can refer to virtual area files in the installed archive;
you do not need a physical `/usr/share/openkal/RuntimeData/areas` directory.
Use an absolute external path only when deliberately testing loose area maps.
Profile values are literal paths: do not write `$HOME`, `~` or shell quotes
inside them. Relative asset roots resolve from the executable directory,
not from the profile's directory.

Choose **Single Player** to use the embedded MainSvr and local SQLite save;
a separate server process is unnecessary. Choose **Multiplayer** to connect to
the endpoint packaged in step 5, then sign in or register if the server permits
it, create/select a character, assign the starting points and enter the world.

The installed launcher saves user data beneath
`${XDG_DATA_HOME:-$HOME/.local/share}/openkal`. Back up that directory before
changing saves. Client display/audio settings are stored in the profile;
per-character UI placement is stored beneath the save directory. A different
profile can be selected with `OPENKAL_PROFILE=/absolute/path/to/openkal.profile`
before invoking `/usr/games/openkal`.

For a first manual test, check that the entrance screen renders, a character can
enter the world, movement and NPC interaction work, and progress survives a
quit/restart. Successful unit tests alone do not verify your external assets.

## 9. Run a dedicated multiplayer server

Single Player can skip this section. Standalone TCP MainSvr requires a build with
`OPENKAL_WITH_POSTGRES=ON`, a running PostgreSQL database, private server settings,
TLS credentials, and matching game configuration and map geometry.

For a local development server, build both executables without Qt:

```bash
cmake -S . -B build-multiplayer -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DOPENKAL_ENGINE=gl -DOPENKAL_WITH_POSTGRES=ON \
  -DOPENKAL_BUILD_AREA_EDITOR=OFF -DOPENKAL_BUILD_PROFILE_CLIENT=OFF \
  -DOPENKAL_DATA_ROOT="$OPENKAL_DATA_ROOT"
cmake --build build-multiplayer \
  --target openkal_mainsvr openkal_client --parallel 2
```

Prepare the database and ignored `save/postgres.env` using
[PostgreSQL setup](docs/PostgreSql.md). That document includes the container,
volume, credentials, health-check and restart commands. Then follow
[Multiplayer setup](docs/Multiplayer.md) to launch a local loopback server.
Development startup can create local TLS credentials; Release startup cannot.

Create a separate server profile to set absolute asset paths and the bind
endpoint:

```bash
mkdir -p save
cp -n config/profiles/default.profile save/server.profile
nano save/server.profile
```

For a local-only server, retain `network_host=127.0.0.1` and
`network_port=30001`. Set `map_root` to the compatible map directory and
`server_map_root` to the absolute external area directory; leave
`configuration_root=` empty for the build's combined archive. Then launch:

```bash
./build-multiplayer/bin/openkal_mainsvr "$PWD/save/server.profile"
```

MainSvr logs its configuration source and listening endpoint. Stop with Ctrl+C.
Configure client asset paths as in the development section below and start the
client from this build to use the automatic local development trust setup.

For deployment, copy [`server.example.json`](server.example.json) to a private
`server.json` outside packaged resource trees. Supply the referenced credentials,
TLS certificate chain, private key, guild rules and user-role policy. Set
`allow_client_registration` deliberately; the shipped deployment example
rejects registration. Paths inside `server.json` are relative to that file.
An explicit launch selects it with:

```bash
./build-multiplayer/bin/openkal_mainsvr /absolute/path/to/server.profile \
  --multiplayer-config /absolute/path/to/private/server.json
```

A Release server build can be enabled with
`cmake --preset release -DOPENKAL_WITH_POSTGRES=ON` **plus the same data-root,
engine and editor options from step 6**. Build `openkal_mainsvr` explicitly;
the client `.deb` still does not install that executable. Release MainSvr uses
`/usr/share/openkal/config.pak`, so deploy matching resources there separately
or through the client package. Provision its private startup settings yourself;
there is no automatic Release certificate or database setup.

For standalone local Unix-socket operation, `openkal_mainsvr PROFILE --unix`
uses SQLite and needs neither PostgreSQL multiplayer startup nor TLS. This
listens on `save/openkal.sock` beneath its executable directory. It is separate
from the client-managed embedded Single Player server.

## 10. Development builds, tests and tools

### Build and test without external data

This headless build tests project-owned fixtures without a display or game data:

```bash
cmake -S . -B build-null -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DOPENKAL_ENGINE=null -DOPENKAL_DATA_ROOT= \
  -DOPENKAL_BUILD_TESTS=ON -DOPENKAL_BUILD_AREA_EDITOR=OFF \
  -DOPENKAL_BUILD_PROFILE_CLIENT=OFF -DOPENKAL_RUN_USER_DATA_TESTS=OFF
cmake --build build-null --parallel 2
ctest --test-dir build-null --output-on-failure
```

Use a fresh build directory if an existing one uses a different generator.
Tests labeled `optional-user-data` are disabled without external data; with a
data root supplied they are enabled by default unless
`-DOPENKAL_RUN_USER_DATA_TESTS=OFF` is set. These catalog checks are separate from
project-owned tests. PostgreSQL integration tests require their own test database
configuration; see [Multiplayer setup](docs/Multiplayer.md).

To rerun only the converter regressions and the Python-to-C++ catalog
compatibility test after building:

```bash
ctest --test-dir build-null -R '^config_converter_' --output-on-failure
```

These tests use synthetic inputs, including current item stackability and NPC
service/placement/presentation fields. They do not certify arbitrary external
data versions, ID migrations, visual assets or complete quest behavior.

### Run a graphical development client

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DOPENKAL_ENGINE=gl -DOPENKAL_BUILD_AREA_EDITOR=OFF \
  -DOPENKAL_BUILD_PROFILE_CLIENT=OFF \
  -DOPENKAL_DATA_ROOT="$OPENKAL_DATA_ROOT" \
  -DOPENKAL_RUN_USER_DATA_TESTS=OFF
cmake --build build --parallel 2
mkdir -p save
cp -n config/profiles/default.profile save/client.profile
nano save/client.profile
```

Set absolute `data_root`, `map_root` and `server_map_root` in that copied profile
and keep `configuration_root=` empty. The development build reads
`build/config.pak`. Launch with the profile as the first argument:

```bash
./build/bin/openkal_client "$PWD/save/client.profile"
```

The direct binary does not run the Debian launcher's folder picker. By default
its save directory is beneath `build/bin/save`; `OPENKAL_SAVE_DIR` can select
another writable location. Development UI layouts hot-reload; Release clients
load layouts on first use and retain them for the session.

### Optional Qt tools

The core setup above avoids Qt. To build the Area Editor or other Qt desktop
tools, install a compatible Qt **6.9+** Widgets/Gui development kit separately
and point CMake at its installation prefix. Use the
[Qt installation documentation](https://doc.qt.io/qt-6/get-and-install-qt.html)
and the applicable Qt license terms. Replace the path in this example:

```bash
cmake -S . -B build-tools -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/absolute/path/to/Qt/6.9.x/gcc_64 \
  -DOPENKAL_ENGINE=gl -DOPENKAL_BUILD_AREA_EDITOR=ON \
  -DOPENKAL_BUILD_LOOT_EDITOR=ON -DOPENKAL_BUILD_ITEM_VIEWER=ON \
  -DOPENKAL_BUILD_UI_EDITOR=ON -DOPENKAL_BUILD_SETTINGS_APP=ON \
  -DOPENKAL_BUILD_PROFILE_CLIENT=OFF \
  -DOPENKAL_DATA_ROOT="$OPENKAL_DATA_ROOT"
cmake --build build-tools --parallel 2
```

These executables are in `build-tools/bin/tools/`. Other opt-in tools include
`OPENKAL_BUILD_TEST_PLAYERS` and `OPENKAL_BUILD_MANAGEMENT_CONSOLE`; see
[Test players](docs/TestPlayers.md) and [Management console](docs/ManagementConsole.md).

General developer tools (`OPENKAL_BUILD_TOOLS=ON`) include `gtx_view`, `tile_view`,
`object_view`, `character_info_ui_view`, `three_d_item_viewer`, `skill_effect_view`
and `itemgroup_drop_view` in OpenGL builds. `skin_asset_probe` is headless;
[skin catalog auditing](docs/SkinCatalogAudit.md) documents the optional preview
workflow. `area_import` and `area_info` are controlled separately by
`OPENKAL_BUILD_AREA_TOOLS` and do not require Qt. Run tools with `--help` for
arguments and profile selection.

### Game Content Editor

Build the Qt content editor to manage NPCs, monsters, items, quest steps, dialogs,
shops, task descriptions, drop groups, item groups and prefixes.

```bash
cmake -S . -B build-tools -DOPENKAL_BUILD_CONTENT_EDITOR=ON \
  -DOPENKAL_DATA_ROOT="$OPENKAL_DATA_ROOT"
cmake --build build-tools --target content_editor
build-tools/bin/tools/content_editor --profile /path/to/openkal.profile
```

Pass the profile as a positional argument, use `--profile`, set `OPENKAL_PROFILE`,
or choose **File → Open profile**. The editor uses the profile's client asset
paths and writes to its loose `configuration_root`. If that setting is empty,
the editor uses `OPENKAL_DATA_ROOT/config` when an external data directory is
configured in the environment or development build. Packaged definitions from
`config.pak` are available in read-only mode when no loose directory is selected.
`--config` or **File → Open configuration** selects a different writable
directory. `--client` or **File → Open client assets** overrides the client paths.

A development build also shows definitions from its source checkout as read-only
references when you open a different configuration. Use `--base-config` to
choose another reference directory. Where both directories contain the same
relative path, the active configuration takes precedence in the editor.

Choose a catalog and filter records by name or ID. Use **New**, **Duplicate**,
or edit the selected record in **Fields** or **JSON**, then select **Save
definition**. Fields include templates for quest rules, dialog options, and
drop-table entries. **Choose reference target** selects an item, quest step, or
other linked definition. **Open reference** jumps to it. The editor checks known
references in the loaded catalogs before deleting a definition. To edit the
fallback files, open their configuration directory directly.

The map uses the same outdoor imagery as the client's M key. Select
**Overworld** or a dungeon/instance in the map selector. For a dungeon with
multiple atlases, choose its region in the second selector. Scroll to zoom at the pointer,
right-drag to pan, and use **Locate on map** to center the selected record.
At close zoom, **Detailed map** renders outdoor terrain and placed buildings
from the original client assets in a top-down view. The M atlas provides the
overview and covers outdoor areas without matching geometry. A tile grid
appears at close zoom for precise placement. Cyan markers represent NPCs.
Gold markers show the spawn points and areas of the selected monster.

Selecting an NPC or monster centers and zooms the map on its position.
**Locate on map** frames a whole monster spawn area. The 3D preview below the
map follows the selection. Left-drag to orbit, right-drag to pan, and scroll
to zoom. NPCs use the current draft's position and facing. Monsters use the
selected spawn, or their first spawn
when no spawn is selected. A rectangular spawn is previewed at its center,
with height sampled from the terrain or the dungeon floor. Creatures without
a position can still be inspected in the preview. The view requires OpenGL 3.3
and the corresponding model and map assets from the selected profile.

- Select an NPC and choose **Place / move NPC**, then click the map. The editor
  samples the outdoor KCM height or the dungeon floor geometry and writes the
  server Z. If floors overlap, choose the intended height. A placement with
  no available ground sample stays unchanged.
- Select a monster and choose **New spawn**. Click for a point or drag for a
  rectangle. Choose an existing spawn to redraw or remove it. The spawn's
  count and respawn time are editable in **Fields**.
- **Set teleport target** adds a destination for an NPC. Select an existing
  entry in `teleports` to move it, or select a quest's teleport action to place
  that action's destination.

The editor writes individual loose JSON files. For a runtime configured to use
`config.pak`, rebuild the archive and restart the runtime after saving changes.

### 3D World Editor

Build the optional outdoor world editor with Qt 6.9+ (Widgets and OpenGLWidgets).
The viewport uses an OpenGL 3.3 core context. Use the Qt prefix from the setup
above when Qt is installed outside CMake's search path.

```bash
cmake -S . -B build-tools -G Ninja -DOPENKAL_BUILD_WORLD_EDITOR=ON
cmake --build build-tools --target world_editor
build-tools/bin/tools/world_editor /path/to/openkal.profile
```

The World Editor loads profiles like the Area Editor. Choose **File → Open
Profile** (Ctrl+O), pass a profile as shown above, or use `--profile FILE`.
With no explicit path, the editor uses `OPENKAL_PROFILE` or the packaged default
profile. The profile's `data_root` supplies the `MAPS` and `OBJECTS` directories.
Use **File → Open Client Data** or `--data DIR` to open a directory directly.
Select a chunk to center the camera on it. The editor loads chunks that enter
the 3D view as you pan, rotate or zoom. Use **View → Fit World** to see the
entire map. Wide views use simpler terrain meshes. Zoom in to see the full
terrain detail and nearby objects.

With the viewport focused, use **W** to zoom in at the pointer and **S** to zoom out. Use **A/D**
to pan left or right, right-drag to pan in any direction, or hold both left and
right buttons while dragging to orbit around the terrain point under the pointer
when the gesture starts. Press **F** or
choose **View → Walk Mode** to explore the terrain at eye level. Use WASD to walk
and the mouse to look. Hold **Shift** to run. On Wayland,
right-drag to look. Neighboring chunks load as you cross boundaries. Press
**Esc** to return to the orbit camera at your current location. **View → Invert
Mouse Y** reverses the vertical mouse input in both modes. Walk Mode follows
the terrain surface. Placed objects are passable.

**View → Show Water Surface** toggles the outdoor water preview at OpenKal's
standard level (Y = 1584). Lower terrain below that level to reveal the surface.
The preview uses the client water texture when available. Water and waterfall
models in the object catalog can be placed separately and exported as OPL objects.

Pick an editing tool and use the left mouse button on the terrain.
Height strokes update every copy of a shared
edge or corner height, including in adjacent chunks outside the preview.
Texture painting uses the layers in `MAPS/n.env` and the textures in `MAPS/Tex`.
Use **Replace Layer in Active Chunk** to substitute a texture across an entire
chunk, including when its layer palette is full.

Double-click a GB model in the searchable catalog and click the terrain to place
it. In **Select / Move Object**, click an object and drag it to move it, including
into another visible chunk. Set **Elevation (Y)** to move the selection vertically.
Use **View → Show Invisible Placements** to locate and edit collision-only and
other placements that have no visible mesh.
Use **Delete** in the viewport to remove the selected
object. Undo and redo cover complete gestures.

**File → Export Changed Files** writes the edited KCM and OPL files into a new
directory. To continue editing, open the original client root and choose
**File → Open Export**, or pass `--overlay /path/to/export`. To use the edits in
OpenKal, copy the export into `MAPS` in a working copy of the original client data.
Use the Area Editor for server-side `.akarea` attributes.

## 11. Troubleshooting

- **Qt6 6.9 cannot be found:** add `-DOPENKAL_BUILD_AREA_EDITOR=OFF` and leave
  optional Qt tools disabled, or configure a compatible Qt kit explicitly.
- **`ninja` or `dpkg-shlibdeps` cannot be found:** install `ninja-build` or
  `dpkg-dev`. Do not reuse a build directory with another CMake generator.
- **Missing Pillow, SQLite, FreeType, PNG or OpenSSL headers:** install the
  development packages and `python3-pil` from step 1, then configure again.
- **Compiler killed during build:** reduce the build's `--parallel` value.
- **Build succeeds but catalogs/maps are absent:** inspect the configure log's
  `OPENKAL_DATA_ROOT` and the archive listing. An empty directory is not a data
  installation. Supply the missing compatible files and rebuild/repackage.
- **Duplicate item index while packaging:** the two reported files must be
  reconciled by ID and reference, not just renamed. Review generated and
  maintained definitions together; do not renumber curated items arbitrarily.
- **Release reads old configuration:** install the newly built `.deb`.
  Release uses `/usr/share/openkal`, not the build directory. Also check that
  a loose profile `configuration_root` is not selecting another tree.
- **Client exits on a missing login model:** verify `map_root` and
  `map/login_map/login_maxl.gb`. Proprietary client assets are not in the source
  checkout or generated archive.
- **Missing monsters, NPCs or dungeon entry:** check matching catalogs, map IDs,
  spawn data, navigation cells and indoor geometry. Converters cannot supply
  missing source inputs or recreate hand-maintained gameplay corrections.
- **Multiplayer connects to the wrong address:** change the source
  `config/server/client.json`, rebuild and reinstall; profile network fields
  configure the server bind endpoint.
- **TLS connection fails:** check reachability, certificate expiry, trust roots
  and Subject Alternative Name against `tls_server_name`. Release has no
  development-certificate fallback. See [TCP security](docs/TcpSecurity.md).
- **MainSvr asks for PostgreSQL support:** rebuild with
  `-DOPENKAL_WITH_POSTGRES=ON` and prepare its database/startup settings. Normal
  TCP startup is authenticated multiplayer; `--unix` is the local alternative.
- **Installed launcher fails on `zenity`:** install `zenity`, or prepare a valid
  absolute-path profile before launching from a graphical session.

## 12. C++ style checks

Use braces for every `if`/`else`, `for`, range-for, `while` and `do` body,
including single statements and empty bodies. Ordinary `else if` chains are
allowed, with braces around each branch body. See [AGENTS.md](AGENTS.md).

The formatter requires clang-format 18.1.8; the pre-commit hook installs that
version in its own Python environment. Run `pre-commit run --all-files` to format
project sources. Formatting preserves authored include order, comments and
string literals. Vendored third-party sources are excluded.

Clang-format inserts ordinary control-flow braces. Clang-tidy's
`readability-braces-around-statements` check additionally diagnoses empty bodies
and compiler-visible cases that the formatter cannot rewrite. CI runs both
checks. For a local compiler-aware check, install clang-tidy and configure a
separate build with `-DCMAKE_CXX_CLANG_TIDY=clang-tidy`, then build normally.

## License and references

OpenKal is free software under the GNU General Public License, version 3 or
later (`GPL-3.0-or-later`); see [LICENSE](LICENSE) and
[third-party notices](THIRD_PARTY_NOTICES.md). External game data remains subject
to its own permissions and is not part of the source distribution.

OpenKal is an independent compatibility/emulator project and does not claim
official affiliation or endorsement. Users supply their compatible game files
separately. The project license covers OpenKal contributions;
third-party software keeps its stated license, and retained compatibility
labels/metadata keep their recorded provenance. See the
[repository content review](docs/OpenSourceReleaseChecklist.md) for the scope
and limits of the current source-release review.

- [Documentation index](docs/README.md): gameplay, operation, architecture and tools.
- [Architecture](docs/ARCHITECTURE.md): client/server boundaries.
- [Monster strength](docs/MonsterStrength.md): server balance configuration.
- [Open work](docs/TASKS.md): remaining feature work and verification.
- [Development guidance](AGENTS.md): repository conventions and workflows.

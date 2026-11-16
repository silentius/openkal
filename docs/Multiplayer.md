# Multiplayer setup

The dedicated server owns PostgreSQL access. Game clients use the existing
TLS 1.3 transport for account, character and world messages. Single-player uses
its existing SQLite save and an embedded Unix-domain server, without connecting
to PostgreSQL. Online mode never opens that local character database.

## Build

Install the development packages for PostgreSQL (`libpq-dev` on Debian/Ubuntu)
and the existing OpenSSL/client dependencies, then build a development binary:

```bash
cmake -S . -B build-multiplayer -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DOPENKAL_ENGINE=gl -DOPENKAL_WITH_POSTGRES=ON
cmake --build build-multiplayer --target openkal_client openkal_mainsvr
```

Supply the same locally owned runtime asset paths and optional `OPENKAL_DATA_ROOT`
used for ordinary development. The normal build defaults to
`OPENKAL_WITH_POSTGRES=OFF`; single-player does not require libpq development
packages in that build. A build with PostgreSQL enabled links the libpq runtime.
Existing curated game configuration and local SQLite migrations are unchanged.

## Start locally

Prepare the container and private `save/postgres.env` as described in
[PostgreSQL setup](PostgreSql.md). Start it again after a manual stop:

```bash
docker start openkal-postgres
./build-multiplayer/bin/openkal_mainsvr
```

MainSvr reads database credentials from the ignored `save/postgres.env` and
creates missing development TLS credentials under `save/multiplayer/tls` through
OpenSSL itself. Python and the `openssl` command-line executable are not needed
for server startup. The database container must already be running; MainSvr does
not create or start database services. The normal development profile binds to
`127.0.0.1:30001`.

Existing `save/multiplayer/server.json` registration policy and TLS files are
preserved. Without that policy file, local development enables registration by
default. A missing half of a TLS pair produces an error rather than replacing
existing credentials. The certificate covers localhost and 127.0.0.1 and lasts
30 days. Certificate renewal is explicit: stop the local server and remove both
development PEM files to generate a new pair on the next start.

Launch the client directly; no Python launcher or TLS environment variables are
needed for the local development server:

```bash
./build-ninja/bin/openkal_client
```

Use the binary from the build you want to test (`build-multiplayer` also works).
For separate local auto-login files, start each client from its own directory:

```bash
cd build-ninja/bin
mkdir -p char1
cd char1
../openkal_client
```

The working directory stays unchanged. An `auto_login.json` in `char1` applies
only to that instance. Development builds automatically use the public
`save/multiplayer/tls/server.pem` certificate created by MainSvr for
connections to `127.0.0.1`. The client never needs the server's private key.
MainSvr must have been started once to create its certificate.

`tools/run_multiplayer_client.py` remains an optional compatibility launcher. It
preserves the working directory and selects the nearest client binary, including
from `build-ninja/bin/char1`. `OPENKAL_BUILD_DIR` explicitly selects another build;
when no nearby binary exists it falls back to `build-multiplayer`. An optional
first argument selects a profile. `tools/run_multiplayer_server.py` is also an
optional wrapper with the same build-selection behavior; it preserves the working
directory and forwards MainSvr arguments without generating settings or secrets.

## Dedicated server deployment

Server startup settings are physical files outside `config.pak`. Copy
[`server.example.json`](../server.example.json) to `server.json` in the server's
working directory or beside `openkal_mainsvr`, then supply the referenced
`postgres.env`, TLS certificate and private key. Paths inside this JSON are
relative to its directory. For example:

```json
{
  "allow_client_registration": false,
  "database_credentials_file": "postgres.env",
  "tls_certificate_file": "tls/server.pem",
  "tls_private_key_file": "tls/server-key.pem"
}
```

`postgres.env` uses the same three unquoted `POSTGRES_DB`, `POSTGRES_USER` and
`POSTGRES_PASSWORD` values described in [PostgreSQL setup](PostgreSql.md). They
select a local PostgreSQL instance at `127.0.0.1:5432`. For other database endpoints,
use a server-only `database_dsn` string in `server.json` or `OPENKAL_DATABASE_DSN`.
The environment DSN takes precedence, followed by the JSON DSN and credentials
file. Explicit `OPENKAL_TLS_CERT` and `OPENKAL_TLS_KEY` remain supported; relative
environment paths use the process working directory. Do not distribute the
server configuration, database credentials or private key with the client.

An explicit `--multiplayer-config /path/to/server.json` selects a different
settings file. Otherwise the working-directory file takes precedence over the
file beside the binary. Local development builds on `127.0.0.1` additionally
fall back to the repository's `save/multiplayer/server.json` and existing local
credentials. Release builds have no repository fallback and never generate TLS
credentials automatically. Configure the dedicated server bind address and port
in its profile, and match the client package's endpoint to it.

Standalone TCP startup selects authenticated multiplayer and requires a build
with `OPENKAL_WITH_POSTGRES=ON`. `--unix` retains the standalone local socket
path without multiplayer setup. The embedded single-player server does not use
this startup configuration.

## One server per client package

The client endpoint comes from `config/server/client.json`, packaged as
`config/server/client.json` inside `config.pak`:

```json
{
  "host": "127.0.0.1",
  "port": 30001,
  "tls_server_name": "localhost"
}
```

Set these values for your server before building the client package. The host is
a numeric IPv4 address; `tls_server_name` is the DNS name or IP in the server's
certificate. Rebuilding regenerates `config.pak`. Players cannot edit the
address or port in the login dialog; profile `network_host` and `network_port`
continue to configure the dedicated server but do not select the client's server.
Missing or invalid endpoint configuration prevents multiplayer connections.

For distribution, a server with a certificate trusted by the operating system
requires no client certificate setup. For a private CA, distribute its public
trust file at `tls/ca.pem` beside `openkal_client` or under the installed resource
root (normally `/usr/share/openkal`). The client finds it automatically. The
local development certificate fallback is excluded from release builds. Explicit
`OPENKAL_TLS_CA` and `OPENKAL_TLS_SERVER_NAME` overrides remain available for
special setups; normal players do not need them. See [TCP security](TcpSecurity.md).

## Register, select and enter

1. Click **Multiplayer** on the entrance screen.
2. The client connects automatically to the server supplied in `config.pak`.
3. Choose **Create account** and enter a username, email, password and password
   confirmation. **Show passwords** reveals both password fields; passwords are
   hidden by default. Live feedback shows the 12–128 byte requirement and whether
   confirmation matches. Tab/Shift+Tab changes fields; Enter submits a valid form.
   Ctrl+A selects the focused field, Ctrl+C copies, Ctrl+X cuts and Ctrl+V pastes.
   A paste that exceeds the field limit or contains control characters is rejected
   as a whole, leaving the previous value intact.
4. Sign in, create a character, spend all five starting attribute points, and
   enter the world. The server owns character validation and starter equipment.
5. Repeat with a second account and a different character name in the other client.
6. Check that both characters see each other, movement and chat arrive, and that
   disconnecting one removes it from the other client.
7. Check combat and item pickup, then quit and reconnect. Position, inventory,
   skills and quest state must remain attached to the correct character.
8. Stop/restart MainSvr, then the PostgreSQL container, and check persistence again.
9. Stop both services and choose **Single Player**. Existing local characters must
   still load. Repeat after cancelling an online login or losing the connection.

The development preview build has its own save directory by default. Local saves
can be selected explicitly with `OPENKAL_SAVE_DIR`. Online HUD/settings files are
further separated by server endpoint and account beneath that save directory;
online character IDs never select a local character's HUD state.

## Optional automatic login

When running the development client directly, copy
[`auto_login.example.json`](../auto_login.example.json) to `auto_login.json` in
your private working directory. Restrict read and write access to your account
with `chmod 600 auto_login.json` before adding credentials. The file stores
the account password as plaintext. Use manual login with the installed
launcher, whose working directory is the shared `/usr/share/openkal`.

Place `auto_login.json` in the client's working directory to connect and sign in
automatically when opening **Multiplayer**. The client uses the server address
and port from `server/client.json` in its configuration package and the first
entry of the `login` array:

```json
{
  "login": [
    {
      "id": "USERID",
      "password": "YOUR_ACCOUNT_PASSWORD"
    }
  ]
}
```

Use valid JSON without trailing commas and the same credentials accepted by the
normal login form, including the 12–128 byte password requirement. The file is
read again each time multiplayer is opened. Missing or invalid files leave the
normal account login available after connecting. Connection or login failures show the usual
error and allow manual login; there is no automatic retry loop. Remove the file
to disable automatic login. Character selection remains manual.

## Registration policy

The checked-in server policy `config/server/multiplayer.json` defaults to:

```json
{
  "allow_client_registration": false
}
```

Local development defaults to `true` when the ignored
`save/multiplayer/server.json` policy is absent. Set that file to `false` and
restart MainSvr to disable registration.
The client then hides **Create account**, and the server rejects direct
registration requests too. Existing accounts can still sign in.

Usernames contain 3–32 ASCII letters, digits or underscores, stored lowercase.
Player names use a separate policy: 2–14 ASCII letters or digits, with no spaces,
underscores or other special characters. Client entry and lobby creation use
the shared validator in `protocol/CharacterName.h`.
Emails use a documented ASCII development-address policy, are stored lowercase
and must be unique. They are initially marked unverified; this milestone sends
no verification or recovery email. Passwords contain 12–128 bytes without control
characters and are stored as versioned salted scrypt hashes. Requests have
credential/request limits, at most two password derivations run concurrently,
and derivation runs outside the world thread. Registration and login logic live
on the server and can be reused for a future website. Administrative slash commands remain disabled on this multiplayer server.
The explicit player commands `/party`, `/secedeparty`, `/guild create`,
`/guildrank`, and `/expel` are available through their permission-checked services.

The server allows one active connection per account. A PostgreSQL advisory lease
prevents two authoritative MainSvr processes from opening the same world. The
server closes its world if PostgreSQL becomes unreachable; restart MainSvr after
restoring the database connection. The first PostgreSQL schema is independently
versioned; SQLite saves are never
implicitly imported, and an unsupported PostgreSQL schema version fails startup.

## Automated verification

Build `openkal_test_postgres_multiplayer`, set `OPENKAL_TEST_POSTGRES_DSN` to the
local test connection and run:

```bash
ctest --test-dir build-multiplayer \
  -R '^(postgres_multiplayer|account_password|ui_document|character_creation|tls_transport|protocol)$' \
  --output-on-failure
```

The integration test creates a unique temporary PostgreSQL schema and synthetic
TLS certificates. It runs shared SQLite/PostgreSQL persistence contracts, two TLS
clients, registration, account ownership checks, world entry, visibility,
movement, chat, concurrent loot pickup, owned-character deletion/recovery and
restart with registration disabled. It drops only
its own temporary schema. Without a test DSN it is explicitly skipped. Test
fixtures contain project-owned data only; no private game implementation is used.

## Player interaction

Select another nearby player and press **O** to open Player options. The server
offers only currently available actions. **Invite to party** invites the selected
player. **Request to join <guild>** submits an application to that player's guild;
the selected member does not need leadership or recruitment permissions. Online
guild officers are notified and can accept under **Guild > Applications**.
Application fees are shown before submitting. Selection changes, disconnects and
Escape close the dialog; O toggles it. Typing in chat or a guild editor does not
trigger the shortcut. Additional player actions can extend the same action list.
**G** toggles the Guild window; typing in chat or the guild editor suppresses
this shortcut as well.

New guilds appear immediately in Ranking, including for guildless players. Daily
ranking snapshots still determine existing guilds' cached points. Search results
also offer Details and Apply through the same guild browser.

## Guild owner policy

Guilds are multiplayer-only and persist in PostgreSQL. `guild_rules_file` in
private `server.json` selects a physical JSON ruleset relative to that server
configuration. Copy `config/server/guilds.json` to `guilds.json` beside it for
normal deployment. Development defaults to the checked-in ruleset; test hosts
can explicitly select `tests/guild/TwoMemberRules.json` for two founders and
immediate grade 4. No owner files are rewritten. Restart MainSvr after edits.

The rules configure founding level/member/party requirements, prices, temporary
capacity, rank quotas, grade thresholds, penalties, point coefficients, grade
buffs, and daily currency rewards. Equal grade thresholds are allowed and the
highest satisfied grade wins immediately. Invalid enabled policies fail startup.
The Community button opens the Dragon guild window; `$` routes to current online
guild members across maps. See [GuildPlan.md](GuildPlan.md) for configuration,
resolved behavior, player commands and validation.

## Castle War

Castle War uses the dedicated guild service and PostgreSQL. Add
`"castle_war_file": "castle_war.json"` to private `server.json` to select the
event settings. See [Castle War](CastleWar.md) for the deployment template,
schedule, runtime map data, and player commands.

## Character roles and privileged commands

`characters.role` is server-owned: 0 is Player, 1 Moderator, 2 Co-Admin, and
3 Admin. New characters and migrated existing characters default to Player.
The field follows `name` in the character model, explicit SQL projections, and
new database schemas. Existing databases receive the column at the end through
SQLite schema 29 / PostgreSQL schema 5; column position does not affect loading.
Ordinary gameplay saves preserve the database role, and lobby requests cannot
assign it.

`user_roles_file` in private `server.json` selects `user_roles.json`, relative to
that server configuration. Copy `config/server/user_roles.json` beside the
server settings when deploying. Development uses the checked-in file by default.
A missing selection grants no privileged multiplayer commands; an explicitly
selected missing or malformed policy fails startup. Restart MainSvr after
changing the policy.

The policy contains `schema: "openkal.user_roles/v1"` and a `roles` object keyed
by `"1"`, `"2"`, and `"3"`. Each entry has an explicit `commands` array of
lowercase command names without `/`. The shipped policy grants `getitemid`
only to Admin. Moderator and Co-Admin have empty arrays. Roles do not inherit
permissions, and there is no wildcard grant. Player cannot receive privileged
commands. Existing party and guild gameplay commands retain their normal rules.

Assign a role through the server database, for example:

```sql
UPDATE characters SET role = 3 WHERE name = 'YourCharacter';
```

The server reads the current database role for each privileged command, so
promotion and revocation apply immediately, including to online characters.
An Admin can use `/getitemid INDEX [PREFIX] [COUNT]` in multiplayer; the existing
argument and inventory checks still apply. Item grants persist normally and
carry the character's account ID. Other privileged commands remain denied until
explicitly added to that role's policy. Single-player commands are unchanged.

## Player actions and direct trade

The shared Guild, Party, Trade and Whisper actions appear only in multiplayer.
Party and Trade occupy the active mouse skill slot, including after restoring
saved shortcuts. Select either action, then right-click another player to invite
them. Guild opens the guild window and Whisper uses a separate interaction mode;
neither replaces the active skill. Whisper fills the chat draft with `@Name `
when a player is selected with the left mouse button. Run and Rest execute directly, like emotes
and Meditation. T&S relation is not supported or shown.

Direct trade uses project-owned payloads on the reserved `AskTrade` and
`OpenTrade` opcodes; it does not claim legacy wire compatibility. Both players
are locked from invitation until authoritative close. Client and server reject
all other outgoing client commands, including chat, movement, skills, inventory,
lobby and exit requests. Disconnect closes the trade and saves normally. Server
notifications continue to arrive. Invitations expire after 30 seconds; open
trades expire after five minutes. Death, teleport, lost proximity, disconnect or
an externally changed offered instance cancels the exchange.

The trade dialog shows a scrollable inventory on the left. On the right, the
partner's twelve-slot offer and Geons appear above your twelve-slot offer and
editable Geons. Each offer grid has two rows of six cells. Add an inventory item
by double-clicking or dragging it to an empty own-offer cell. A quantity selector
allows partial stacks. Double-click an offered stack to edit its quantity;
right-click it or drag it back to inventory to withdraw it. Item and Geon changes
publish automatically. Accept confirms the offer; Reject ends the entire trade.

Only unequipped, unbound items can be offered; quest/task items are disabled.
Hover over an offered item to inspect its instance tooltip.
Every offer change resets both confirmations, and confirmations carry
the exact trade token and offer revision. Both confirmations are required.
The server rechecks balances, ownership, item attributes, binding and available
unlocked inventory cells, then commits both character balances and all item
ownership changes and stack splits in one database transaction. A full bag,
balance overflow or failed save leaves both inventories and balances unchanged.
Full stacks retain their instance IDs; partial transfers keep the source IID on
the remainder and allocate a fresh IID for the recipient. All enhancements and
quantities are preserved. Money-only trades are supported. The balance
limit is 2,000,000,000 Geon per participant.

Successful completion includes a recipient-specific committed receipt: incoming
items appear as green `You got <item>` lines (`You got <count>x <item>` for stacks), followed by
white `You got <amount> geons` for incoming money, then `Trade completed.`.

## Native management console

See [ManagementConsole.md](ManagementConsole.md) for the Qt player roster, role
editor, local management API, in-game mail and support PM console. The API is
explicitly enabled through private server settings and never listens on TCP.

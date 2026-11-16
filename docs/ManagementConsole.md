# Management Console

`management_console` is a native Qt 6 Widgets application for the multiplayer
server. Its current functions are:

- List all persisted characters, search by name or exact character ID, and page
  through groups of 100 characters. Search text is parameterized and SQL
  wildcard characters are treated literally.
- Show account, level, role and current online status. A disconnected local API
  produces **Unknown** presence rather than marking everyone offline.
- Change active characters' roles directly in PostgreSQL. An expected-role
  condition rejects conflicting changes. Normal gameplay saves preserve roles;
  privileged command checks see changes immediately.
- Send durable in-game mail with one item attachment, or a message without an
  attachment, to online or offline characters. The server validates item IDs
  and stack quantities against its loaded catalog and updates online inboxes.
- Send and receive private messages using the reserved `!Support` identity.
  Players reply with `@!Support message`. Offline PMs are rejected; use mail
  for durable delivery.

World chat in the style of Horn of Spirit, announcements, and countdown-based
shutdown are visibly marked as planned and disabled. No server actions for
these features are implemented yet.

## Build and launch

The console requires Qt 6.9 Widgets and libpq (PostgreSQL development libraries).
Enable it alongside the PostgreSQL server:

```sh
cmake -S . -B build -DOPENKAL_BUILD_MANAGEMENT_CONSOLE=ON -DOPENKAL_WITH_POSTGRES=ON
cmake --build build --target management_console openkal_mainsvr -j
build/bin/tools/management_console
```

It also builds with `-DOPENKAL_ENGINE=null`; the console itself uses native Qt
text and does not require the game's graphics backend or external game assets.
The database must already have the current MainSvr schema, including character
roles. The console does not run migrations or create database users.

## Enable the local management API

Add these settings to the server's private physical `server.json`:

```json
{
  "management_socket": "management.sock",
  "management_token_file": "management.token"
}
```

These are additions to the existing server settings, which still include the
registration, database and TLS settings. Both management paths resolve relative
to that `server.json`. The resolved socket path must fit the platform's Unix
socket path limit. Create the private token beside it, for example:

```sh
umask 077
openssl rand -hex 32 > management.token
```

Restart MainSvr after adding these settings. The API is disabled when neither
setting is present. Selecting just one setting, an unreadable/invalid token
file, or a socket path that already exists fails startup. The API does not
replace another server's socket or delete unrelated files. After an unclean
shutdown, inspect the old process and remove its stale socket before restarting.

This is a Unix-domain socket, not a TCP listener: it cannot bind a public
network address. Its permissions are `0600`, and every request additionally
requires the private token (32–256 bytes, excluding a trailing line ending).
Run the console as the server's local OS user. No test-control option or game
account is needed. Game clients never receive the secret or database credentials.

In the console, enter the resolved **absolute** socket path and the token
contents, then choose Connect. Leave **Database override (optional)** empty:
the authenticated local API supplies the server's PostgreSQL connection string,
including its host/IP, port, database, user and any configured password or TLS
options. The worker uses these settings directly without putting them into the
GUI or its activity log. The default therefore uses the server's database user.

To use a separate database user, enter a connection string in the optional
override field. `OPENKAL_DATABASE_DSN` and `OPENKAL_MANAGEMENT_TOKEN` optionally
prefill the secret fields. The console saves only window geometry and the
socket path; database credentials and tokens remain in memory. PostgreSQL
connections and API operations run on a worker thread with bounded waits, so
network failures do not freeze the GUI. If automatic configuration cannot be
authenticated or fetched, connecting fails instead of falling back to libpq's
default database.

The database connection needs `SELECT` on `characters` and permission to update
its `role` column. It does not need to insert items or edit inventories. All
mail and PM mutations use the authoritative server API.

## Mail and PM semantics

Mail supports one plain item definition and quantity per message. A subject is
required; limits are 96 UTF-8 bytes for the subject and 1024 for the body. The
attachment pair is either `(0, 0)` for no attachment, or a known item ID and a
positive quantity. Non-stackable items require quantity one. The normal mail
claim transaction creates the inventory instance and records the account owner.
Messages without attachments can be read and deleted without claiming anything.
The server rejects new management mail if the projected inbox cannot fit the
current game's mail packet limit.

A draft has an immutable request ID until its recipient or contents change.
Retrying an uncertain send with the same draft cannot duplicate its mail.
Successful sends clear the draft. Deleted mail retains its source key, so a
replayed request does not recreate it. A timeout can mean an operation committed
without its response reaching the console; there is no automatic mutation retry.

PMs use the ordinary game whisper channel and its byte limits. The console
limits outgoing bodies to 95 UTF-8 bytes so a maximum-length character name
still fits the packet. Delivery confirms a successful send to the current live
session, not that the human has read the message. The server keeps the most
recent 1000 support PM events in memory. Independent consoles poll by cursor,
25 events at a time. History overruns and detected server restarts are reported;
PMs are not a permanent chat archive. The GUI also bounds its local transcript.

## Local API

The transport is one compact JSON object plus a newline per Unix connection,
with one JSON response and then connection close. Requests include `token`;
this field is removed before world command dispatch. Limits bound connection
count, receive size and incomplete-request lifetime. All commands execute on
the world thread, alongside ordinary gameplay mutations.

- `status`: optional numeric `after` cursor. Returns online character IDs,
  support events, instance identifier, next cursor, continuation and gap flags.
- `database`: returns `database_dsn` from the server's active PostgreSQL
  configuration. This credential-bearing response requires the same private
  token and owner-only local socket as all other management commands.
- `pm`: `character` ID and `body`. The recipient must currently be playing.
- `mail`: `character`, `request_id`, `subject`, `body`, `item_index` and
  `item_count`. The request ID is a 16–64 character alphanumeric/hyphen token.

Errors return an `error` string. Unknown operations have no effect. The API is
separate from the game's public TLS listener and its test-control protocol.

## Verification

`management_console_ui` exercises real Qt widgets with a synthetic roster,
including role selection, mail/PM action availability and disconnection.
`management_console --capture /absolute/path.png` captures that synthetic
preview without connecting or mutating a server.
`management_api` checks authentication, socket permissions and ownership,
bounded PM history, event cursors and cleanup. `management_backend` runs the
actual Qt worker against an isolated PostgreSQL schema and local API, checking
automatic database configuration, authentication failure, optional overrides,
search escaping, pagination, presence, role changes and conflict rejection.
The latter returns skip code 77 unless `OPENKAL_TEST_POSTGRES_DSN` is configured.
The existing PostgreSQL multiplayer integration additionally exercises online
and offline mail, message-only protocol delivery and deletion, idempotency,
item attachment claims and two-way support PMs through real game connections.

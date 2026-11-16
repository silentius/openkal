# Guilds

Guilds are multiplayer-only, using the authenticated dedicated server and
PostgreSQL. Membership, applications, management UI, chat, grades, points, buffs
and daily rewards are implemented. Castle War supplies timed alliances and castle
ownership for dedicated multiplayer servers. Single-player neither loads nor
manages guilds. See [Castle War](CastleWar.md) for its separate server policy.

## Server policy

`config/server/guilds.json` is the development ruleset. For deployment, copy it
beside private `server.json` and set `"guild_rules_file": "guilds.json"`.
Relative paths resolve against `server.json`. Rules load before the world opens;
invalid enabled rules fail startup. Changes take effect on restart and preserve
existing guild records. An empty `ServerConfig::guild_rules_path` leaves guilds
unavailable for embedded/test hosts.

The `openkal.guilds/v1` policy controls:

- `creation`: minimum level/member count, party/full-party requirements,
  consent timeout and creation fee.
- `membership`: joining level, application/admission fees, application lifetime,
  rejoin lock, temporary capacity, counted members and optional overall cap.
- `rank_quotas` and `ranks`: fixed/proportional office limits, default titles and
  permission masks. Defaults seed new guilds; they do not overwrite existing grants.
- `grades` and `features`: member thresholds, member-loss policy and feature gates.
- `names`, `time`, `points`, `buffs` and `daily_reward`: text limits, day boundary,
  ranking refresh, contribution formulas, grade bonuses and daily currency.

The checked-in defaults allow one level-1 founder without a party and charge
500000 Zamogeon. Applications and admission are free, expire after seven days,
and membership removal imposes a 24-hour rejoin lock. Buff coefficients are zero;
daily rewards pay 1000 Zamogeon per grade from grade 2 after one day of membership.
These are editable defaults, not protocol constants.

`tests/guild/TwoMemberRules.json` is an explicit test variant: level 1,
100-Zamogeon creation and immediate grade 4 for two permanent members. It does
not grant test money or enable administrative multiplayer commands.

## Membership and management

Each character belongs to at most one guild. Exactly one leader is required;
disconnect updates presence without removing membership or transferring leadership.
Persistent character IDs identify members, applications and cooldowns.

Party founding includes every current connected party member and is initiated
by the party leader. All participants must meet the founding requirements, be
guildless and have no rejoin lock. When consent is required, each accepts an
expiring Guild-window request. Party changes invalidate formation. Enrollment
and charges cannot partially succeed.

Applications may target several guilds. The first acceptance wins and removes
all other applications for that character. Expired, withdrawn, rejected and
already-member applications cannot be accepted. Temporary admission is explicit,
has no automatic expiry and converts through the normal rank action.

The founder pays creation, the applicant pays application fees and the accepting
officer pays admission fees. Application fees are not refunded on rejection,
expiry, withdrawal or acceptance elsewhere. The server revalidates displayed
prices before charging; a mismatch requires a refresh.

A member can leave. A leader with other members must transfer leadership first;
the last member disbands the guild and releases its canonical name. Leave,
expulsion and disband use the same configured rejoin duration. PostgreSQL rejects
character deletion while membership exists; restore does not invent membership.

## Ranks, names and display

Member ranks are separate from guild grades:

1. Leader: every permission; cannot be expelled.
2. Sub-Leader: at most one, with a vacant slot allowed. Transfer promotes the
   current sub-leader and demotes the former leader to Official Member.
3. HeadChief: proportional quota, default one per 50 permanent members.
4. Chief: proportional quota, default one per 10 permanent members.
5. Official Member: ordinary accepted member.

Temporary status is separate from ranks 1–5 and grants no management permissions.
Only the leader changes permission masks or the sub-leader office. Other rank
managers affect lower ranks only and cannot promote to their own rank or above.
A sub-leader must be demoted to Official Member before expulsion. Quotas count
permanent members including the leader unless configured otherwise. A shrinking
quota preserves existing officers but rejects excess promotions.

Permission options cover applications/invitation text, expulsion, announcements,
rank changes and rank-title edits. Renaming a title does not change its numeric
rank or permissions. The leader always retains every permission.

New guild names accept precomposed Hangul syllables or ASCII letters/digits,
without whitespace or mixed alphabets. Maximum lengths are 7 Hangul or 14 Latin
characters by default, bounded by 14 Unicode scalars. Latin uniqueness ignores
case; stored spelling is preserved. Rank titles use 1–14 scalars without
whitespace. Invitation and announcement text allow spaces. Malformed UTF-8 and
control characters are rejected.

Character names are stored independently. Eligible nameplates show
`[CharacterName]` and a second `Title GuildName` line. The configured grade and
maintenance policy gate that line; Character Info always shows actual membership
and rank. Chat uses the unchanged character name. Public display updates cover
login, visibility, membership and grade/penalty changes.

## Grades, points and rewards

A guild immediately receives the highest grade whose member threshold it meets.
Equal thresholds and skipped grades are valid; points, promotion fees and waiting
periods do not gate this advancement. Thresholds are positive and nondecreasing.
Grade 1 is the base for a nonempty guild. Thresholds and transport page sizes do
not imply a member cap.

On member loss, `penalty` retains the attained grade but disables configured
features until headcount recovers; `downgrade` immediately recalculates the grade.
Disabling guilds preserves records and removes their active effects.

Default level-up points are 1 for newly reached levels 1–19 and `new level - 10`
from level 20. Each connected hour adds 10; the default total cap is two billion.
Joining grants no historical points. Connected-hour remainders survive reconnects
but exclude offline time and are discarded on leaving.

The server day defaults to UTC. `day_offset_minutes` supplies a fixed offset,
independent of client timezone and daylight saving. Daily contributions use day
keys. Rankings sort by points, canonical guild name and guild ID; point snapshots
remain frozen between configured refreshes. New guilds appear immediately in
Ranking and Search without replacing existing cached point snapshots.

Grade buffs add configured amounts to primary attributes, maximum HP and defense
without changing stored base stats. Leave, disablement and penalties remove them.
The daily box is a direct Guild-window currency claim of
`grade * zamogeon_per_grade`, requiring grade, membership age, eligibility and
wallet capacity. A persistent character/day key prevents duplicate claims and
guild hopping. There is no offline catch-up or inventory item.

## Client and persistence

Community or **G** opens the Dragon/FreeType Guild window; typing suppresses the
shortcut. Ranking is the first tab. Search/Details/Apply, Members, Applications,
My requests, Invitation, Announcement, Transfer and Rank options expose the
service operations. Founding consent opens the recipient's window. Destructive
actions require a second deliberate click. Escape closes the current dialog.

Selecting another online player and pressing **O** opens Player options. An
eligible guildless character can apply to that player's guild even when the
selected member is not an officer. Recruitment officers receive a notification.
The Executive (NPC 210, dialog 184) also opens creation or the guild browser.

Player commands are `/guild create Name`, `/guildrank CharacterName 2` and
`/expel CharacterName`. They share window validation and do not enable debug
commands. `$` sends rate-limited guild chat to connected members across maps.
Single-player exposes no guild actions and rejects requests server-side.

`GuildService` owns domain validation; the host owns sessions and packet delivery.
`IGuildStore` is an optional `IDataStore` capability implemented by PostgreSQL.
Transactions cover money, membership, ranks, cooldowns, revisions and mutation
receipts. Unique constraints and revision checks prevent competing acceptances,
double charges and stale retries. Publish state only after a successful commit;
a read failure is an error rather than an empty roster.

The Guild codecs in `libs/protocol` define project-owned versioned payloads
within reserved Guild opcodes. Requests use authenticated actor identity and carry request IDs;
responses distinguish results, snapshots, public displays and bounded pages.
Private application and permission data are not sent to observers.
`ChangeGuildName` remains unused. The contract does not claim external binary
compatibility. UI bindings are documented in [VARIABLES.md](ui/VARIABLES.md).

## Verification and provenance

Tests cover rules, protocol, service, PostgreSQL transactions, server integration
and window/editor behavior. For a PostgreSQL-enabled verification build:

```sh
ctest --test-dir build-multiplayer --output-on-failure \
  -R '^(guild_.*|postgres_multiplayer|party_service|party_server|chat|protocol|nameplate|menu_bar_hud|ui_document|ui_layout_.*)$'
```

Set `OPENKAL_TEST_POSTGRES_DSN` for isolated database integration tests. A missing
DSN is a skip, not a successful database check. Default/null builds and synthetic
fixtures remain independent of PostgreSQL and optional game data.

The public [Guild guide](https://gameagit.com/kalonline/Guild) is a behavioral
reference. OpenKal's configurable policy, immediate member-based grading and
payloads are independently maintained. No external code or artwork is imported.

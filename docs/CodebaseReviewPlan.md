# Codebase complexity and performance review plan

Date: 2026-11-02. Source baseline: `6d1f5c5222530ad155ea525df42cac4395bdf055`.
Status: bounded R01–R12 implementations completed on `codex/codebase-optimizations`;
see [OptimizationProgress.md](OptimizationProgress.md) for the delivered scope,
measurements, validation and remaining broader proposals. The initial review
changed no implementation. The authorized regression preparation is documented
in [OptimizationCoverage.md](OptimizationCoverage.md).

## Scope and interpretation

The review screened the repository's client, server, shared libraries, engine,
tools, converters and test/build setup. The inventory contains 2,196 tracked
files, including 482 `.cpp`, 344 `.h` and 44 `.py` files; these counts include
test and third-party files. Graph-wide complexity and nested-work queries guided
targeted source inspection, with parallel client, server and tools reviews.

This is a broad, prioritized source review, not a claim that every line was
manually audited. The twelve work packages below have source evidence and
specific validation plans. The initial review verified cost mechanisms
statically; it did not measure production bottlenecks, frame-time gains or speedup
percentages. Later local benchmarks and their limits are recorded in
[OptimizationProgress.md](OptimizationProgress.md).
High branch counts alone were not treated as performance defects.
Third-party implementation, ignored build trees and optional proprietary game
data are outside the refactor scope.

Priorities mean implementation value: **P1** addresses shared-loop stalls or
scaling costs; **P2** addresses repeated work or material maintenance difficulty;
**P3** requires measurement before adding machinery. They are not claims of
security severity. Effort estimates are relative: small, medium and large.

## Recommended implementation sequence

1. **Characterize first.** Establish the test baseline, add the missing owned
   fixtures described below, and capture observable behavior before refactoring.
   The subsequent test-preparation work is recorded in
   [OptimizationCoverage.md](OptimizationCoverage.md), including remaining gaps.
2. **Reduce cheap repeated work.** R11 (test compilation), R01 (receive buffers)
   and R05 (FIFO queues), as separate changes. R01 should migrate one caller at
   a time while retaining a common framing contract.
3. **Protect server progress.** R02 (bounded output), then independently R03
   (interest) and R04 (occupancy). Do not change both world indexes together.
4. **Improve independent paths.** R06 (text), R09 (test-player catalog joins)
   and R10 (item picker). Each can be implemented independently.
5. **Isolate complex rules.** R07 in two steps: pure stat extraction, then
   removal of preview mutation. R08 proceeds one request family at a time,
   independently of the performance changes. Start with well-tested small
   families rather than a wholesale Host rewrite.
6. **Reconsider R12 after measurement.** Prefer bounded temporary storage over
   introducing cache invalidation for a 216-cell inventory.

Each work package must remain separately reviewable and reversible. These completed package notes describe their validation sequence. Current
contributions follow CONTRIBUTING.md and the repository delivery rules.

## R01 — Drain receive buffers without moving the suffix after every frame

**P1 · small/medium · network throughput · high confidence in cost mechanism**

Evidence: `client/src/misc/NetClient.cpp:1607–1625`,
`libs/server/src/Host.cpp:9679–9695` and
`tools/testPlayers/TestPlayerWorker.cpp:31–47` erase the consumed vector prefix
after each frame. Client update invokes the drain at `NetClient.cpp:255–260`;
the worker uses it for world and spectator connections. For F equal frames of L
bytes, the erasures move L×F×(F−1)/2 bytes. Small batches can be inexpensive;
coalesced snapshots/combat bursts exercise the unnecessary repeated copying.

Later changes:

1. Characterize each caller's distinct handshake, lobby, invalid-frame,
   disconnect and pending-event policies.
2. Parse using a local consumed offset and compact the remaining suffix once
   when the drain ends. Preserve receive limits and buffer ownership. Audit
   callback/early-return paths before retaining pointers into the buffer.
3. Reuse payload capacity where safe. Keep `FrameParser` and cipher semantics
   intact; its frame/payload copies (`PacketReader.cpp:225–230`) are a separate
   optimization and do not justify a zero-copy API rewrite here.

Required tests: all-at-once input versus every header/payload split; multiple
valid frames plus an incomplete suffix completed next time; malformed length
after a valid prefix; unknown opcode; maximum frame/buffer boundaries; receive
failure; reconnect/adopt; graceful close; worker spectator attach and ordering.
Assert exact bytes/events, once-only dispatch and the existing failure policy.
Use a work counter to show at most one suffix compaction per drain. Benchmark
1/32/256/2,048-frame batches and multiple workers, separating ingestion from
event draining.

Existing guards: `protocol`, `multiplayer_presentation`, `tls_transport`, and
the separately configured Qt `test_player_worker`. Extend owned
`BufferedSession`/synthetic-session fixtures. Do not combine this change with
R05 or packet-handler extraction.

## R02 — Prevent congested peers from blocking the shared simulation loop

**P1 · large · server tail latency · high confidence in blocking behavior**

Evidence: `libs/server/src/net/TlsSession.cpp:229–254` retries writes up to an
absolute one-second deadline. `libs/server/src/net/Transport.cpp:64–92` polls
for 1,000 ms on EAGAIN, without an absolute deadline across repeated partial
progress. Host calls synchronous sends during interest reconciliation
(`Host.cpp:4189–4196`, `19501–19505`) under its world lock. The normal world tick
is 20 Hz (`Host.cpp:8582`), giving a 50 ms period; a congested established peer
can therefore hold up unrelated work. This is a blocking-path finding, not a
measured incident or CPU-speed claim.

Later changes:

1. Introduce a controllable write seam and explicit bounded **host output**
   queue. Preserve `ISession::send_all`'s documented full-delivery contract for
   callers that still need it (`net/Transport.h:49–55`).
2. Retain frame ownership and unsent offsets; drain by readiness with bounded
   bytes/attempts per loop and correct TLS WANT_READ/WANT_WRITE handling.
3. Define queue acceptance, delivery, overflow, disconnect, shutdown and
   persistence/flush barriers before migration. Preserve existing TLS
   failure/deadline semantics; any changed timeout/flush policy is a separately
   reviewed behavior decision. Queue acceptance cannot silently substitute for
   completed delivery.
4. Migrate one output path first; remove socket waits from world-lock scopes.
   Migrate remaining paths only after the lifecycle tests pass.

Required tests: deterministic partial writes, EAGAIN, TLS wants, no progress,
peer close mid-frame, exact FIFO bytes without duplication, bounded memory,
overflow policy and cleanup. With two established clients, stop A from reading
while B continues movement/chat and world ticks. Test startup bursts, logout and
shutdown. Existing stalled-handshake tests do not cover this case. Assert
bounded write attempts and progress deterministically; report p50/p95/p99 tick
duration and queue high-water marks for 1/16/64 peers separately.

Existing guards: `tls_transport`, `multiplayer_presentation`; owned replacements
for relevant `server` cases; optional `postgres_multiplayer` with a dedicated
test database. High lifecycle risk warrants its own implementation series.

## R03 — Avoid full-world interest scans for every viewer every tick

**P1 · medium/large · server scaling · high confidence in repeated work**

Evidence: `Host.cpp:4171–4257` scans all monsters, ground items and NPCs,
allocating fresh sets and reserving global entity counts. All three functions
run per playing client each world tick (`19501–19505`). Cost is approximately
O(P×(M+I+N+known-visible)), including entities on unrelated maps. Existing
player-to-player interest already has a dirty-position gate (`9560–9609`).

Later changes:

1. Isolate visibility predicates and delta generation with a reference oracle.
   First partition by map and reuse scratch capacity; measure this simpler step.
2. Add a coarse spatial broad phase only if needed. Maintain monster/loot
   entries on spawn, movement, corpse transition, pickup, expiry and removal;
   NPC indexes can be static per map.
3. Apply the current exact sight/corpse predicates after candidate selection.
   Preserve create traversal order and causal packet ordering; compare remove
   membership without inventing an ordering contract for unordered-set iteration.
4. Add version/dirty gating only once all mutations are observable. A stationary
   viewer must still see spawns, deaths and despawns.

Required tests: differential visible IDs and ordered creates at sight boundary
±1 pixel, identical coordinates on different maps, cell crossings, viewer
teleport, stationary-viewer spawn/death/despawn, corpses, loot pickup/expiry,
NPC changes and reconnect. Unchanged ticks must not emit duplicate deltas.
Benchmark P=1/16/64, M=256/1,024/4,096, local density and unrelated-map count;
report candidate checks, allocations and tick distribution. Default monster cap
is 256 and client cap 64; larger populations are synthetic stress cases.

Existing guards: `monster_refill`, `inventory_server`, `harvest_plants`,
`multiplayer_presentation`; `server`, `portal` and `quest` are optional-data
tests in the baseline and require owned focused equivalents. Entity interest
does not require changing renderer culling or the camera.

## R04 — Index occupancy queries inside movement and bounded path search

**P1 · large · server scaling · high confidence in nested scans**

Evidence: `libs/server/src/world/GameWorld.cpp:1836–1866` scans all monsters
and player poses for every occupancy query. `1868–1902` repeats it at 8 px
movement samples. Path search calls it through `2303–2307`;
`FindPath.cpp:27–28,111–168` already limits search to 50 processed nodes with
eight neighbors. Moving monsters can therefore approach an O(M×(M+P))
population term with bounded path-search multipliers. Replacing A* is not the
first useful change.

Later changes:

1. Keep the current query as a differential oracle. Add a tick-local index by
   map and 8 px cell, storing counts/IDs for self-exclusion and overlaps.
2. Preserve target-cell reservation when dirty/moving (`1826–1834`) and update
   it immediately during sequential monster processing. An immutable snapshot
   for the entire tick would change collision behavior.
3. Define refresh/update boundaries for exposed mutable monster collections,
   spawn/removal/death, nonblocking races and player-pose replacement. Migrate
   movement and spawn checks incrementally.
4. Preserve segment sampling, terrain/village checks, neighbor order, search
   budget, fallback routes and attack timing. Do not alter rendering.

Required tests: oracle comparison with overlaps/self exclusion, source/target
reservations, two monsters swapping/converging, corpse/nonblocking races,
separate maps, player movement, negative and 8 px boundary coordinates, and
creation/removal during a tick. Compare deterministic complete trajectories,
hostility and chase-arrival attack timing. Benchmark dense blocked paths with
M=32/128/256/1,024, counting occupancy candidates and path callbacks.

Existing guards: `monster_refill`, `talent_tree`, `harvest_plants`,
`dungeon_server`; `monster_occupancy`, `combat`, `server` and `portal` require
owned fixtures before they can be relied on without user data. Risk is stale
reservation state, not merely lookup correctness.

## R05 — Use constant-time removal for genuine pending-event FIFOs

**P2 · small · client burst processing · high confidence**

Evidence: `NetClient.cpp:386–444,1254–1259` removes vector fronts for queued
combat/skill/transform/chat events. `ClientSession.cpp:9207,9272,9302,9372,9700`
drains queues in loops. Draining N events moves N×(N−1)/2 remaining elements;
some values own strings or JSON. This is independent of receive-buffer copying.

Later changes: classify each queue's operations; convert true FIFOs to
`std::deque`/`pop_front` or a bounded cursor representation, preserving `take_*`
APIs. Move values where safe. Retain filtering/cancellation/random-access
collections as vectors unless their distinct behavior has been addressed.
Preserve existing 32/128-entry caps and oldest-item eviction. Start with one
coherent event group.

Required tests: numbered events preserve payload/order and once-only delivery;
empty/alternating enqueue/dequeue; cap−1/cap/cap+1; reset/disconnect/reconnect;
unchanged create/remove cancellation. Benchmark drain independently at
32/256/2,048 events; assert linear removal/move work rather than elapsed-time
thresholds. Existing guards: `multiplayer_presentation`,
`multiplayer_chat_hud`, `battle_feed_hud`, `guild_window`,
`player_options_window`.

## R06 — Avoid repeatedly measuring growing text prefixes during wrapping

**P2 · medium · UI layout · high confidence in cost, impact unmeasured**

Evidence: `client/src/ui/TextWrap.cpp:98–106` measures each growing prefix;
`FreetypeFont.cpp:216–221` scans the supplied string. Multiline input draws
repeat layout at `UiView.cpp:465–474`, even when text is unchanged. The work is
O(sum of squared candidate line lengths); fixed narrow fields bound that
multiplier. `GuildWindow.cpp:319–323,616–621` also uses this layout.

Later changes: characterize byte spans/break rules, then provide an additive
FreeType path that visits each codepoint/advance once. Match existing UTF-8,
fallback glyph and scale behavior exactly. Keep the generic callback path: an
arbitrary measure callback is not necessarily additive. Only add owner-local
caching if still justified; keys must include text, width, font generation and
scale. Retain TrueType and the current UI schema.

Required tests: differential spans on explicit and randomized ASCII/UTF-8,
spaces, long words, empty/trailing lines, unsupported glyphs, exact-width and
zero/tiny-width cases, scales .75/1/1.5. For caching, test font reload, resizing,
text changes, caret and scrolling. Count glyph/byte visits for 256/1,024/4,096
characters; benchmark unchanged multiline drawing with owned font fixtures.
Existing guards: `quest_journal`, `quest_journal_hud`, `latin_text`,
`guild_window`, `guild_packaged_ui`, `skin_shop_ui`, `ui_document`.

## R07 — Calculate alternate combat profiles without mutating live state

**P2 · large · combat complexity and repeated work · high confidence**

Evidence: `Host.cpp:4577–4589` toggles `collection_pve`, rebuilds live stats,
constructs an alternate combatant, toggles back, rebuilds again and restores
HP/MP. Preview users include `8138–8139` and `14747`. The calculator at
`1761–2681` spans 921 lines and mixes input resolution, ordered arithmetic,
resource clamping, appearance and state mutation. This is event-driven work,
not an unconditional per-tick rebuild.

Later changes:

1. Characterize full output fields and arithmetic order, including fallback
   initialization and equipment/prefix/mix/buff/guild/collection/form/talents.
2. Extract immutable inputs and derived results, separating calculation from
   live-session initialization/application. First preserve behavior with no
   optimization; keep integer rounding and clamp order.
3. Calculate a preview result directly, without changing HP/MP, collection
   mode, lock deadlines, appearance or emitted packets. Preserve actual PvP
   contact/profile-switch resource-fraction and timeout rules.
4. Split calculation into named ordered phases; do not rearrange percentages.
   Do not add stat caching in the same change.

Required tests: golden vectors for four classes and all derived fields;
adversarial rounding/clamping; preview at HP/MP 0/1/partial/max; compare every
session field, persistence call and packet before/after repeated previews.
Interleave real PvP contact and timeout restoration. Compare controlled-RNG
damage variants. Instrument rebuild calls; alternate views should need one
nonmutating calculation. Existing guards: `collection_equipment`,
`collection_server`, `skill_buff`, `talent_tree_server`, `battle_riding`,
`qigong`, `dragon_fusing`, `weapon_enchantment_server`,
`polishing_stone_server`; supplement optional `combat`/`server` with owned cases.

## R08 — Extract bounded request families and explicit lifecycle operations

**P2 · medium/large in small steps · maintainability · no speedup claim**

Evidence: `Host.cpp:7307–19964` combines poll/session ownership, lobby,
management, request validation, persistence rollback, skills and scheduled world
phases. For example TalentTree mutation/rollback is at `12708–12764`.
`NetClient.cpp:1607–3176` mixes framing, state and presentation dispatch;
online-view cleanup is duplicated at `263–279,1493–1512,1553–1569`. A receive
failure can execute both client cleanup paths. Graph complexity scores
(Host 1,910; client drain 302) are discovery signals, not performance measurements.

Later changes:

1. Replay requests against controlled clocks/RNG/store outcomes. Record
   ordered bytes, state, persistence calls and rejection/disconnect results.
2. Extract one server family, initially TalentTree or consumable crafting,
   using a narrow context and explicit handled/rejected/disconnect result.
   Keep ownership/poll/phase transitions in Host. Preserve nested `continue`
   semantics; do not introduce a handler inheritance framework.
3. Extract scheduled phases separately, preserving order and timer semantics.
4. On the client, name the identical cleanup operation while first preserving
   invocation counts/serial effects. Treat idempotence changes as a separate
   behavior decision. Then extract private methods by packet family, keeping
   framing in the drain loop and retaining exact event order.

Required tests: per-family differential replay for valid/truncated/trailing
payloads, wrong phase, dead/teleporting actors, stale values, cooldown edges,
inventory full, save failures, duplicate requests and two interleaved clients.
TalentTree must restore mask/beacon deadline on save failure; crafting must
preserve items, money, produced IDs and rollback. Client cases include EOF,
repeated update after EOF, graceful-close timeout/ack and reconnect, explicitly
checking serials and retained events. Scheduled replay compares deadlines and
event counts/order. Existing guards: `protocol`, `multiplayer_presentation`,
`talent_tree_server`, `consumable_crafting_server`, `inventory_server`, `trade`,
`trading_post_server`, `party_server`, `guild_window`, `trading_post_ui` and
optional multiplayer integration. Acceptance is behavior equivalence and
smaller testable responsibilities, with no new hot-path allocations.

## R09 — Precompute immutable joins for test-player quest monster selection

**P2 · medium · tool population scaling · high confidence in nested work**

Evidence: `tools/testPlayers/TestPlayerAgent.cpp:1113–1165` repeatedly joins
quest objectives, monster definitions, full spawn catalogs (`1143–1152`) and
nested drops (`1114–1128`). It is used by `hunt` at `1646` and `tick` at `2671`.
The `nextAction` gate at `2631–2632` means this is not an unconditional 100 Hz
scan. Worst-case selection work is O(Q×D×S) plus drop traversal, across agents.

Later changes: build shared immutable per-definition spawn/drop membership
during catalog construction; preserve quest/definition iteration and first-match
order. Keep quest state, item counts, level, reachability and avoidance dynamic.
Do not extend the portal route cache across ticks: failures and avoidance
expiry are correctness-sensitive. Measure before broad policy extraction.

Required tests: duplicate/tied definitions and spawns; no spawn/no match;
ordinary/quest drops, missing/nested groups and nonpositive chances; immediate
pickup completion, quest clear, level/map change and route failure/expiry;
reload invalidation and multi-agent independence. Differential selections on
owned small catalogs; benchmark Q/D/S independently at 1/16/64 agents, recording
index build cost/memory and selection work. Existing Qt guard:
`test_player_agent`, including hunting, quest priorities, navigation and parallel
players; `test_player_worker` and optional `test_player_integration` supplement.

## R10 — Reuse item-picker results and eliminate duplicate presentation work

**P2 · medium/large · editor responsiveness · high confidence**

Evidence: search `textChanged` directly refreshes at
`tools/lootEditor/dialogs/ItemPickerDialog.cpp:131–132`. Refresh destroys and
recreates matching rows/group widgets (`463–522,590–600`), re-sorts already
ordered grade partitions (`487–501`; catalog sorted at `82,300–310`) and calls
`tile_icon` twice per row (`237,240`). Icon decoding is already cached; repeated
composition/materialization is the claim. Selection restoration scans rows per
selected ID (`602–620`).

Later changes: first compute one icon per row and remove redundant sorting with
ordering tests. Then separate filtered sorted IDs from widgets, using a model
or row reuse. Cache composition only with occupied/overlay revision and DPI in
the key; preserve reload invalidation and accessibility/tooltips. Keep filtering
immediate initially; debounce would change behavior and needs a separate decision.
Retain multi-selection ordering and removal of selected IDs no longer visible.

Required tests: synthetic catalog with every tab/category, tied names/sort keys,
grades and exclusions; exact ordered IDs for empty/name/index/class queries;
preselection, Ctrl/Shift, narrowing/widening, empty results and acceptance.
Owned icon snapshots cover missing icons, overlays, occupied marks, reload and
DPI. Benchmark opening/tab/typing at 1k/10k/50k items with GUI-thread p50/p95,
row creations and composition counts. `tools/lootEditor/CMakeLists.txt` currently
defines no test; add an offscreen Qt picker test. Schema or test-player UI tests
do not establish coverage for this dialog.

## R11 — Compile shared UI test implementation once per compatible variant

**P2 · medium · developer build performance · measured build-plan duplication**

Evidence: `tests/CMakeLists.txt:544–571` defines shared HUD source files, then
reuses that source list in many executables (`572–733`); other tests repeat
many of the same files. The fresh Debug/null compile database contains **923
compile commands for 403 unique source paths**. `UiDocument.cpp` is compiled
28 times, `FreetypeFont.cpp` 27 and `UiView.cpp` 23. Normalized commands still
contain groups of ten and six identical invocations per shared UI source after
excluding only source/output arguments. This establishes avoidable compilation,
not a measured build-time saving. Not all variants are equivalent.

Later changes: introduce a shared static/object test-support target for one
group with equivalent definitions/includes/options, and reuse it. Keep test
fixtures and mains separate. Expand only after inspecting other variants;
preserve resource roots, target definitions, generated-resource dependencies,
warning settings and engine differences. In particular keep intentional
`ui_layout_development` versus `ui_layout_release` hot-reload variants
(`tests/CMakeLists.txt:856–869`) separate. Do not fold release client resource
semantics into test-support code.

Required validation: compare normalized compilation variants and dependency
graphs; fresh and incremental builds, including touching a shared UI source and
adding/changing generated resources. Snapshot registered CTest names, properties,
labels and disabled states before/after; run the full Debug/null suite and
selected GL tests. Verify development/release hot reload and packaged UI tests.
Record compile count, build wall/CPU time and disk use on the same machine;
accept fewer equivalent compilations with identical test behavior. No new
unit test should merely mirror the CMake implementation.

## R12 — Simplify bounded filtered inventory projection before considering caches

**P3 · small first · bounded UI allocation · profile before caching**

Evidence: `client/src/ui/Inventory.cpp:1337–1368` rebuilds a filtered projection
with temporary vectors. Pointer update (`1567–1576`) and draw (`2471–2474`)
invoke it before their open checks; interactions invoke it too. The body only
runs with an active filter, and inventory has just 216 cells (`Inventory.h:39–41`).
This is not established as a material frame bottleneck.

Later changes: measure and first replace small heap temporaries with bounded
storage/iteration. Cache only if still useful, with explicit handling of
externally mutable bags, item/category/catalog changes and register expiry.
An address-only cache is unsafe. Check hidden-window refresh dependencies
before moving guards.

Required tests: compare all 216 source-slot mappings across all 64 filter masks;
same-address bag edits, authoritative removal, count/index changes, catalog
replacement, unlock/expiry, hidden→open and draw→click changes; preserve IDs,
occupancy, drag/double-click/destroy and nonwritable expired pages. Existing
guards: `inventory_scroll`, `revision_hammer_drag`,
`weapon_enchantment_inventory`; focused cases in `inventory_hud` need extraction
from its optional-data registration. Measure allocations/catalog lookups across
1,000 unchanged and frequently changing frames before accepting cache complexity.

## Common test and measurement contract

- Add characterization/equivalence tests **before** each implementation and
  verify they pass on the behavior. Freeze observable expectations
  from owned fixtures or a reference oracle, not a copy of the proposed
  implementation. Keep improvement acceptance tests separate: slow-reader
  progress and reduced-work bounds should first demonstrate the initial
  stall/excess work and pass after the intended change. Do not weaken these
  tests merely to make the implementation pass.
- Compare network bytes, ordered effects, inventory ownership, resource values,
  persistence/rollback, timer boundaries and controlled RNG consumption as
  applicable. Preserve protocol limits and malformed-input policy.
- New correctness fixtures must be redistributable, synthetic/project-owned,
  independent of external catalogs and enabled in the headless suite.
- In the fresh null configuration, **149 tests are registered and 17 are
  disabled as `optional-user-data`**. Important names include `server`,
  `monster_occupancy`, `combat`, `inventory_hud`, `portal`, `quest` and
  `item_tooltip`. A disabled or skipped test is not a passing regression guard.
  Add focused owned tests; do not simply remove the optional label or enable
  a test with missing catalogs. PostgreSQL and Qt tests require separate setups.
- Use Debug for assertion-sensitive behavior checks. Use an optimized build
  with tests explicitly enabled for local benchmarks; the release preset
  normally disables tests. Keep timing benchmarks separate from correctness
  gates. Report warm/cold setup, compiler/options, population/input sizes,
  repetitions, median/p95 (p99 for server ticks), allocations/work counters and
  index/cache memory. No microsecond or speedup-percentage CI assertions.
- For queue/index algorithms, gate deterministic work bounds and exact outputs.
  Run sanitizers on the touched server/network/index paths where supported,
  using a separate build. Do not infer race freedom from a passing null suite.
- For UI changes, add owned render/interaction checks and targeted GL/offscreen
  tests when actual graphics behavior matters. Null rendering alone does not
  verify pixels, text metrics on every platform, FX timing or sounds.
- Preserve C++17, repository formatting/documentation conventions, TrueType,
  curated JSON values and existing ID allocations. No terrain/map rendering,
  camera X-mirror, global culling/FrontFace or player draw changes are planned.

### Reproducible baseline and later validation commands

The review's fresh build is outside the repository at `/tmp/openkal-review-null`:

```bash
cmake -S . -B /tmp/openkal-review-null -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DOPENKAL_ENGINE=null -DOPENKAL_DATA_ROOT= \
  -DOPENKAL_BUILD_TESTS=ON -DOPENKAL_BUILD_TOOLS=ON \
  -DOPENKAL_BUILD_AREA_EDITOR=OFF -DOPENKAL_BUILD_PROFILE_CLIENT=OFF \
  -DOPENKAL_RUN_USER_DATA_TESTS=OFF -DOPENKAL_WITH_POSTGRES=OFF
cmake --build /tmp/openkal-review-null --parallel 2
ctest --test-dir /tmp/openkal-review-null --output-on-failure
```

Use a fresh compatible build directory for optional Qt verification; add
`-DOPENKAL_BUILD_TEST_PLAYERS=ON -DOPENKAL_BUILD_LOOT_EDITOR=ON` and the installed
Qt 6.9+ prefix. Run `test_player_agent`, `test_player_worker` and
`test_players_ui`; the planned picker test must be registered separately.
For PostgreSQL, follow `docs/PostgreSql.md` and `docs/Multiplayer.md`, use a
dedicated test database and explicitly record skips. For graphics use a GL
configuration and the existing display/Xvfb guidance. Do not use real saves or
private catalogs as a prerequisite for normal regression tests.

### Review-time validation result

Fresh Debug/null configure and build succeeded with GCC 11.4.0. The build used
two parallel jobs initially and eight after resuming; no build-speed benchmark
is inferred from that run.

The initial CTest run passed 112 tests, disabled 17 optional-data tests and
failed 20 network/server tests because the restrictive sandbox denied local
Unix/TLS socket operations. All 20 failed tests passed when rerun with local
socket access using `ctest --test-dir /tmp/openkal-review-null --rerun-failed
--output-on-failure` (49.17 seconds). **Combined baseline: all 132 enabled tests
passed; 17 remained disabled.** No PostgreSQL, Qt-tool or GL test coverage is
claimed by this null configuration.

Diagnostic logs are `/tmp/openkal-review-build.log`,
`/tmp/openkal-review-ctest.log` and `/tmp/openkal-review-ctest-rerun.log`.
This baseline predates the regression preparation documented in
[OptimizationCoverage.md](OptimizationCoverage.md). No proposed optimization
has been implemented or benchmarked. These baseline results characterize the
application source at the stated revision; future changes must rerun
their own relevant checks.

## Deferred candidates and evidence limitations

No first-wave change is justified solely by the large skill/opcode switches,
`UiView` type dispatch, startup parsers or `GbLoader` length. SQLite full
autosave is scheduled every 60 seconds (`Host.cpp:851,17883`), not each tick;
durability-sensitive persistence batching needs separate evidence. Pathfinding
is already bounded; floor geometry has caches/indexes; archive listings use
ordered prefix lookup. Linear `MapCatalog` queries and repeated offline quest
audit grouping are lower priorities without a measured workload. Animated
player skinning has intentional per-frame behavior and is protected; it is not
an authorized optimization target here.

Graph evidence used Tier 2 verification for project
the graph project indexed from this checkout, generation
`2026-11-02T11:58:09Z`, coverage recorded complete at `12:05:40Z`. The graph's
Branch HEAD metadata was older than the worktree; exact evidence paths returned
`metadata_match`, and material claims were checked against current source.
Coverage was checked for relied-on paths/scopes, with reported partial ranges
read directly. No skipped-source files were recorded; 67 files had recorded
partial parsing, including third-party miniaudio and Qt/default-argument cases.
This is best-effort metadata, not proof of completeness.

Relevant bounded traces were completed where relied on. The initial broad
client outbound dispatch trace remained truncated and is not a claim of a
complete impact graph. Several generic-name graph edges were heuristic or
wrong, and virtual transport inbound edges were incomplete; direct call sites
were used for material conclusions. Config/build files and compile databases
were inspected directly. Existing generated trees and caches were excluded.

Before later implementation, recheck the source revision and cited line
locations, select one work package, establish its owned behavior fixtures and
record its baseline measurements. Reprioritize from those measurements without
combining unrelated algorithms or changing gameplay rules.

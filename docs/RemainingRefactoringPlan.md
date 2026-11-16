# Remaining refactoring execution plan

Planning source: `6da506b7fb72740a7e305603df93ab9909248ece` (2026-11-02).
The bounded optimizations, picker model/view, player regeneration phases and
[multiplayer load harness](MultiplayerLoadTesting.md) are already delivered.
This plan finishes the remaining maintainability work identified by
[R08](CodebaseReviewPlan.md#r08--extract-bounded-request-families-and-explicit-lifecycle-operations).
Historical measurements and completed validation remain in
[OptimizationProgress.md](OptimizationProgress.md).

The finite execution order is **H1–H4, C1–C7, H5–H9: 16 packages**.
All 16 refactoring packages H1–H9 and C1–C7 are implemented, verified and locally committed.
Final 64-client Release PostgreSQL/TLS validation passes, and the owned test cluster is stopped.
These completed implementation notes describe their validation sequence;
current contribution and delivery rules apply to new changes.
The three conditional cache/index proposals below are closed from this queue.

## Execution contract

- Assign production/test ownership explicitly and preserve concurrent edits.
  Host packages primarily touch `libs/server/src/Host.cpp`; client packages touch
  `client/src/misc/NetClient.cpp` and its existing header. Add narrow free/private
  helpers, not a handler hierarchy, general scheduler or large context framework.
- Before moving production statements, pass owned characterization against the
  unchanged implementation and pin its commit, source hashes, fixtures and results.
  Reuse the same scenario driver after extraction; do not maintain a formula clone
  or copy whole live ClientSession/NetClient objects into an oracle.
- Record ordered owner/observer bytes, mutated state/deadlines, persistence calls
  and rows, rejection/disconnection behavior and next seeded RNG values. Drain all
  output with success/progress checks before comparing packets.
- Preserve arithmetic grouping, clamps, lazy clock reads, RNG consumption, lock
  lifetimes, rollback/ignored-failure behavior and nested control flow. Use a narrow
  source/AST/token comparison with disclosed normalization to supplement replay.
- Retain meaningful reachable-path tests and visible raw coverage gaps. Domain
  unit tests supplement Host routing/phase replay; they do not replace it.
- These are behavior-preserving extractions, with no speedup claim or new hot-path
  allocation. Do not bundle gameplay, UI rendering, backend or cleanup fixes.

Ranges below identify **snapshot source**, not permanent edit coordinates.
Re-resolve the named symbols and read each complete moved body before implementation.

## H1 — Collection, autopick and autosave upkeep

Snapshot: `Host.cpp:18425–18445,18462–18500,18509–18513` in `thread_main`.
Extract only `tick_collection_expiry_and_profile`, `tick_talent_autopick` and
`autosave_player_if_due`. Keep Playing/cooldown/aggro guards, village revival and
its callback, the existing regeneration coordinator, beacon ending and subsequent
fishing/medicine/pay-buff/unlock/skill-buff/Archer-drain order visible in Host.

Baseline: collection bag before collection slots, expiry −1/equal/+1, conditional
Unix read per eligible item, ignored delete failure, recompute/progression before
PvP-profile restoration. Autopick covers absent talent/dead/teleport, initial/exact/
late deadlines, radius/map/ownership/party/full bag, traversal order, successful-pick
quota, failed candidate followed by success, interleaved contention and IID changes.
Autosave covers zero/exact/late deadlines and all three persistence calls even after
an earlier failure. Preserve both scheduling ifs and the precise autopick mutex scope.

Generate the old-source adapter mechanically from three uniquely anchored, hashed
bodies; rebind only the collection clock name in that adapter. Freeze expectations
and a small real-transport scheduler witness before replacing adapters with helpers.
Keep reproducible evidence outside normal CI's Git-history requirements. Successful
autosave uses its own steady-clock timestamp; bracket it rather than substituting
`now_ms` or filtering a supposedly exact snapshot. Restore moved bodies at call sites
to check unchanged coordinator order. Core Host replay plus collection/talent/
inventory/party, `player_regen`, `auto_hp_server` and `skill_buff` are required guards.

## H2 — Account-storage requests

Snapshot: StorageInfo/PutInStorage/PutOutStorage, `Host.cpp:11419–11603`.
Reuse `send_storage_info` and `storage_item_of`; retain phase/opcode gates in Host.
Baseline valid/truncated/trailing requests, NPC activation/reset/map/range, ownership,
zero/oversized quantities, first free slot, full bag/storage, whole/partial stacks,
non-stack rejection, equipped/task/character-bound rules and complete item metadata.
Pin the inbound-only 60-Geon fee, IID allocation, insert/update/compensating-delete
order, each store failure, duplicate/interleaved requests and bag/money/transform
packet order. Do not assume transaction guarantees or dead/teleport gates absent
from the current branch; retain source-slot lifetime across compaction.
The owned `storage_server` guard exercises actual Unix/SQLite routing and reconnect.
`storage_server_postgres` uses distinct authenticated TLS accounts and a disposable
schema when `OPENKAL_TEST_POSTGRES_DSN` is configured; it otherwise skips explicitly.
Keep the frozen direct replay for boundary/failure cases; `inventory_server` alone
does not cover this family. Run core Host/protocol guards and these real account/
storage-isolation checks before the extraction commit.

## H3 — NPC shop requests

Snapshot: ItemList/BuyItem/SellItem, `Host.cpp:11604–11771`.
Extract routing and operations while retaining existing insertion/crafting helpers.
Baseline offer order/filtering, ordinary/skin shops, NPC context/map/range, class,
quantity, prices/overflow/currency, bound/task/pay/equipped rules, stack sales and
full inventory. Pin partial insertion followed by quantity mismatch, ignored
persistence failures, compaction snapshots, every store failure and exact bag/money/
chat/transform output. Replay duplicate requests and two interleaved clients.
Keep skin exchange's additional alive/teleport/activity gates specific to that path;
do not apply them to ordinary shops or invent rollback.
Run owned shop replay, core Host/protocol, `inventory_server`, `skin_shop_server`
and `consumable_crafting_server`.

## H4 — NPC dialog, quest and activity routing

Snapshot: AskNpc/Quest/Cooking/QuestJobEnd, `Host.cpp:11199–11418`.
Use several narrow route helpers; retain the consumable-crafting handler and the
quest/event/ritual domain services. Baseline invalid-NPC clearing of the same four
dialog contexts, valid assignment, ritual/teleport/quest/HTML precedence, two-step
teleport confirmation/reset, null quest engine and trailing Quest compatibility.
Pin donation consumption/contribution persistence and their current failure behavior;
ritual activation may consume before activation. Cover cooking ownership/type/range/
dead/busy/material/deadline cases and QuestJobEnd's two activity flags/timers/order.
Preserve malformed/rejected continue versus successful fall-through, conditional
time reads, NPC lifetimes and ordered two-player frames/store operations.
Run owned route replay, core Host/protocol, `consumable_crafting_server`, `server_event`,
`dungeon_server` and `doggebi_ritual`; domain guards alone do not prove routing.

## Client-family contract

Keep private `handle_*_packet(type, const payload&[, origin_patch])` methods in the
existing NetClient source/header. False means only “opcode outside this family”;
recognized malformed packets remain handled with their existing close/ignore policy.
The seven families partition the 52 snapshot top-level dispatch groups. Preserve
immediate per-frame handling and mixed-batch order. `process_frames` retains
FrameDrain, incomplete/invalid-length handling, suffix discard and exception behavior;
borrowed payloads must not escape. Do not turn nested buff-loop continues into returns.

All client packages retain these existing characterizations: malformed market payload
closes transport but drains the current batch; invalid outer length discards its suffix;
throwing close propagates with only unread bytes retained. EOF performs two view-serial
increments, repeated EOF repeats them, and explicit close adds one. Receive tails and
pending chats currently survive close/adopt. Keep cleanup invocation counts and the
handshake/update/close drain call sites unchanged; do not add new phase gates.

## C1 — Online views and communication

`handle_online_packet`: TradingPostPickup/Collection/TradingPost, OpenTrade,
UpdateParty, Guild, PartyInfo, PlayerOptions, Chatting.
Snapshot: `NetClient.cpp:1586–1684,2946–2955`.
Baseline malformed close-versus-ignore packets with valid neighbors, completed-trade
item→money→status receipts and serials, Guild Display versus view/request-ID injection,
own guild/title, 128/32 queue eviction and wrong-party/local-recipient notice rejection.
Retain FIFO/refill, request correlation, locked-trade gates and cleanup guards.
Run core client checks plus guild/window, trade/UI, trading-post/UI, player-options
protocol/window and `party_server` guards.

## C2 — Inventory, equipment and item replies

`handle_inventory_packet`: Mail, InsertItem, PutOn/OffItem, Riding/RidingCache,
TrashItem, ItemList, StorageInfo, UpdateItemInfo.
Snapshot: `NetClient.cpp:1722–1743,1947–2019,2091–2106,2781–2800`.
Baseline optional charm/expiry boundary lengths and trailing tolerance, count-zero
insert/zero-IID trash, Mail List versus claim classification and retained dirty state,
Animal EXP feedback (including negative added values if accepted), and immediate
transform progress versus pending FIFO. Existing FIFO tests pin complete binding,
charm, expiry, riding/equipment/revision and storage metadata.
Run core client, `inventory_hud` and `companion_ui`.

## C3 — Quest, conversation and learned-state replies

`handle_quest_packet`: SendHtml, SaveQuest, QuestInfo, SkillInfo, SkillUp, Shortcut.
Snapshot: `NetClient.cpp:2080–2090,2107–2174`.
Baseline explicit NPC versus zero-ID last-asked fallback, duplicate quest entries,
QuestInfo replacement versus SaveQuest dirty/changed/accepted flags, active/completed/
restart transitions and single-consumption flags. Pin SkillInfo replacement and
duplicate-index last-write behavior, Shortcut present/absent and malformed retention.
Retain existing quest-presentation, HTML-correlation and SkillUp FIFO guards.
Run core client, `npc_talk_ui`, `quest_journal_hud` and `quest_journal`.

## C4 — Session, progression and time snapshots

`handle_snapshot_packet`: Code, AnsGameExit, ClockSync, ExpEventState, MapState,
UpdateProperty. Snapshot: `NetClient.cpp:1685–1708,2040–2071,2581–2757`.
Baseline every property kind/scalar destination through public stats/getters, HP/MP
message asymmetry, first-snapshot suppression, healing floats, vital/current-only
updates, EXP sign/Level order and unread-value flags. Pin malformed ClockSync close
versus ignored ExpEventState and following-frame handling. Replay during handshake
and graceful-close drain; retain split challenge/exact writes/EOF/timeout/ack guards.
Run core client, `clock_hud`, `character_info_hud` and `battle_feed_hud`.

## C5 — World entities, movement and teleport

`handle_world_packet` receives `origin_patch`. Packets: Create/RemovePlayer, GState,
Create/RemoveMonster, Create/RemoveNpc, Create/RemoveItem, MoveMonsterOn/End,
MovePlayerOn/End, MoveBefore, Teleport.
Snapshot: `NetClient.cpp:1744–1946,2020–2039,2072–2079,2175–2331`.
Baseline monster→NPC→player GState precedence, walking/idle snapshot correction at
128 pixels, direction zero, attack-latched motion, death/knee/behead removal, unknown
movement, signed XYZ/origin conversion and remote-before-local identity precedence.
Pin ground-item source ordering and Teleport's exact monsters/NPC/HTML/last-asked/item
clear set. Preserve same-batch create/remove cancellation, observers and interpolation;
do not extend clearing to remote players/quests/buffs. Run core client movement guards.

## C6 — Buff, status and system messages

`handle_status_packet`: TalentTree, MState, UpdateMProperty, Message/MessageV.
Snapshot: `NetClient.cpp:1709–1721,2873–2945,2956–3148`.
Use owned catalogs to pin item/class-skill/Mage/bit icon-name fallback order and
haste/EXP exceptions, count/time buffs, bit iteration/upsert position/subtraction,
inventory-register expiry, food blink, speed-stop/reset, specialty/enchant/system
messages and chest default-six/supplied/truncated Open duration plus stop feedback.
Keep lazy clock-read locations; no per-batch cached time. Bracket deadlines via public
buff/expiry getters without adding a production clock API. Run core client,
`talent_tree` and `inventory_hud`.

## C7 — Combat and action presentation events

`handle_combat_packet` receives `origin_patch`: Attack, Skill, PlayerAnimation,
Effect, Action. Snapshot: `NetClient.cpp:2332–2580,2758–2780,2801–2872`.
Baseline animation-only early exit, explosive base/palsy split, fatal text/colors,
normal floats on known monsters, Mage-bolt delay, Archer splash suppression, expected
cast identity and Action-skill parse precedence. Pin rest/knee/die/behead/form/speed
state and same-batch movement/attack/MState/removal ordering. Preserve all five facing
helper call positions. Existing all-skill-byte/class, observer, arrow, facing and FIFO
guards remain. Run core client, `battle_feed_hud` and affected existing effect guards;
no gameplay/model/camera/effect-rendering changes are included.

## H5 — Doggebi ritual and lord phase

Snapshot: `Host.cpp:18516–19151`; ritual broadcast/reset helpers around 9241–9310.
Keep this before debug dummies and persistent combat. A small aggregate may group
existing long-lived counters; do not introduce a general scheduler.
Baseline monument sequence/equality, absent/dead lord, spawn-once/reset/retry,
quake/action/state/chat order, throne/ascension deadlines, pulse/damage/status,
failed placement/summon, map/known-set observers, death/expiry and RNG/IDs/rewards.
Audit the complete body first: nested continues, lock scopes, RNG and spawn/erase
lifetimes make this higher risk than the earlier packages. The pure ritual test
does not exercise this Host phase. Run owned phase replay, core Host/world,
`doggebi_ritual` and `dungeon_server`.

## H6 — Archer scheduled damage and areas

Snapshot: `Host.cpp:19153–19644`, timed damage, areas, explosions and hit delivery.
Retain effect ordering and local owner/target pointer lifetime within the stable
client batch. Baseline exact/late/final-grace deadlines, each timer's advancement
style, owner death/disconnect/map/movement, player/monster targets, caps/immunity/
reflect/defense/profile effects, overlapping due effects, one death/reward and RNG.
Do not add catch-up pulses or move rewards/broadcasts across locks.
The learned/rank `player_skill` guard is not a Host persistent-effect witness.
Run owned replay, core Host/world, `skill_buff`, `collection_server`, protocol
and the committed load smoke; use focused sanitizers for pointer lifetimes.

## H7 — Mage persistent effects and lightning marks

Snapshot: `Host.cpp:19645–20038`, after Archer and before world production.
Baseline mark expiry/cleanup on owner/target/map/death, channel/movement cancellation,
exact/late/final-grace pulses, hop/strike controllers, spawned/removed entities,
overlapping areas, immunity/knockback/death/reward and hit/move/death packet order.
Record complete persistent lists, session/world mutations, failed placement/persistence
where reachable, observer filtering and next RNG. Preserve cleanup-before-processing,
erase iteration and deferred delivery; no retained dangling owner/controller pointers.
Run owned replay, core Host/world, `skill_buff`, `collection_server`, protocol,
load smoke and focused sanitizers.

## H8 — World production and visibility batch

Snapshot: `Host.cpp:20039–20143`, poses/refill/harvest/world/ground-item updates,
dirty slow packets, corpse removal and interest. Reuse existing output vectors.
Baseline zero/one/multiple viewers, dead/hidden/immune/beacon poses, refill timing/
traversal/map/radius/cap, harvest/ground progression, slow packet order, corpse action/
remove before interest and exact ordered entity deltas. Compare index/fallback paths.
Preserve the full lock region: borrowed WorldInterestIndex addresses rebuild after
corpse removal, then remain valid through all consumers without source mutation.
Keep the >1-viewer rule and R03/R04 lifetimes; do not move scheduler/metrics boundaries.
Run owned replay, core Host/world, `monster_refill`, `harvest_plants`, `dungeon_server`,
load profiles and focused ASan/UBSan/leak guards.

## H9 — World-event delivery and consequences

Snapshot: `Host.cpp:20144–20590`, chest/harvest, item despawn, movement and monster
attack/skill/death consequences. Reuse H8 event vectors and preserve their order.
Baseline chest complete/cancel/remove with missing/dead/out-of-range/full-bag owner,
reward/store failure, observer known sets, despawn→move→attack order, skill prepare/
fire, immunity/nonlethal/lethal/reflect, rest cancellation, death/aggro-drop moves,
mixed simultaneous/repeated events, IDs, full state/store results and next RNG.
Retain staging/commit behavior, pointer lifetimes and reward/store lock boundaries;
do not deliver death twice. Run owned replay, core Host/world, `harvest_plants`,
`monster_refill`, protocol, load profiles and focused sanitizers. H9 ends this queue.

## Validation and local delivery

Core Host checks mean `optimization_host_coverage`; core world checks mean
`optimization_world_coverage`. Core client checks mean `optimization_client_coverage`,
`multiplayer_presentation`, `protocol` and `optimization_protocol_coverage`.
Use public-frame BufferedSession replay for clients and owned Host/SQLite fixtures
with audited failures for server packages. Compare old/current snapshots before commit;
disclose bracketed real-clock fields separately from exact deterministic comparisons.

Build directories are coordinated and shared fixture/profile writers run serially.
Example focused commands after configuration and directory release:

```sh
cmake --build "$OPENKAL_REFACTOR_BUILD" --target openkal_test_optimization_host_coverage -j2
ctest --test-dir "$OPENKAL_REFACTOR_BUILD" --output-on-failure -R '^optimization_host_coverage$'
cmake --build "$OPENKAL_REFACTOR_BUILD" --target openkal_test_optimization_client openkal_test_multiplayer_presentation openkal_test_protocol openkal_test_optimization_protocol -j2
ctest --test-dir "$OPENKAL_REFACTOR_BUILD" --output-on-failure -R '^(optimization_client_coverage|multiplayer_presentation|protocol|optimization_protocol_coverage)$'
ctest --test-dir "$OPENKAL_REFACTOR_BUILD" --output-on-failure
```

Build selected supplementary guards before running them. Require the full owned Debug
suite, affected GL/all-tools builds and focused sanitizers as appropriate before each
implementation commit. Persistence/world changes also use the committed SQLite smoke
and owned-schema PG/TLS checks from [load testing](MultiplayerLoadTesting.md); optional
user-data tests remain separate. Investigate failures without weakening assertions or
unexplained retries. Format C++ with pinned clang-format18 and check the diff.

## Conditional proposals closed from this queue

- **Picker cross-refresh composition cache:** keep refresh-local lazy roles. Verified
  probes record 60/114/120 cumulative compositions for opening/class/search-or-restore
  at each of 1k/10k/50k definitions; these are viewport scenarios, not universal role
  bounds. Remaining 50k ten-match filtering is about 28 ms and still scans the catalog.
  Reopen only if repeated composition, separately profiled from filtering/selection,
  is material and catalog/overlay/occupied/base-icon/DPI invalidation is coherent.
- **R03 spatial grid or dirty gate:** keep the map partition and scratch reuse. The
  saved stable-known CPU visibility batch is 133.479 us p50 at 64 viewers/256 fully
  visible monsters; the synthetic 4,096-local/three-other-map dense case is 3,723.399 us.
  Neither is a complete tick/network measurement. Reopen for a measured sparse same-map
  workload with material attributed visibility cost, including index maintenance;
  dirty gating additionally requires all mutation paths, including stationary viewers.
- **R12 inventory cache:** retain fresh bounded classification of 216 cells. Saved
  1,000-call medians imply about 11.6 us per unchanged call with zero scalar-new requests;
  malloc/array/aligned/retained memory are not measured. Reopen only for material full-UI
  cost with coherent same-address bag/catalog/category/filter/register-expiry invalidation.
  Preserve hidden refresh and take-out-only expired-page semantics.

These are qualified local observations recorded in OptimizationProgress.md and checked
against the saved raw artifacts, not newly rerun measurements or universal limits.

## Finish line and evidence limits

After these 16 packages, the finite backlog is complete. Global tick/event/clock/guild/
purge cadence remains visible as coordination. Authentication/lobby, poll/output queues,
management deferred replies, retirement/shutdown ownership, the large Skill dispatcher
and other wear/riding/enhancement/party/guild/trade branches are outside this queue.
R08 does not require moving every opcode. New caches, asynchronous gameplay, formula
changes or a handler registry require separate concrete evidence, not code-size goals.

Planning used Tier 2 graph evidence in project
the graph project indexed from this checkout (generations 19:50:07, 20:10:27 and
20:13:41 UTC on 2026-11-02), exact source reads and coverage fallback. Client method
pagination completed 142/142; process_frames inbound three/outbound 166 depth-one
results completed. Host evidence is bounded and does not establish its full call graph
or a complete H5–H9 arithmetic audit. Fresh complete-body review is required there.
Known snapshot parser gaps were read directly: Host7094, Host test1777, NetClient.h1709,
picker model.h49/dialog.h45/46/57/store.h63/268. Inferred STL/Qt/name-collision edges
were not treated as proof; no recorded gap is not a completeness guarantee.

# Optimization implementation progress

Branch: `codex/codebase-optimizations`. Preparation commit: `27b6251f`.
All commits remain local. Each package is implemented, verified and committed
before starting the next one.

Priority order: P1 R01, R02, R03, R04; then P2 R11, R05, R06, R09, R10,
R07 and R08; finally the measurement-led P3 R12. Within P2, the shared test
compilation change comes first to reduce the cost of subsequent validation.
The design and acceptance criteria remain in
[CodebaseReviewPlan.md](CodebaseReviewPlan.md).

All twelve bounded packages are implemented. Follow-up work and the current
status of the finite Host/client refactoring queue are recorded below and in
[RemainingRefactoringPlan.md](RemainingRefactoringPlan.md). These sections are
chronological delivery records; later entries supersede earlier future-work notes.
The three conditional cache/index proposals are closed pending the measured-need
and invalidation criteria recorded in that plan. No universal frame/tick speedup
is claimed.

## R01 — Scoped receive draining

Client, Host and the test-player world/spectator worker now use the shared
`protocol::FrameDrain`. It advances an offset through complete frames and
compacts only at drain exit. A reusable payload vector retains capacity between
packets. Malformed-buffer discard versus retention remains a caller decision.
RAII commits the consumed prefix even if a packet handler throws or returns.
The small constructor, destructor and pop wrapper are inline; compaction and
discard are implemented in the protocol library.

Lifetime audits found no receive-buffer mutation/reentrant drain in the actual
handlers. Host drain destruction precedes the next client receive/erase; client
and worker disconnect and malformed-payload policies remain unchanged. Added
tests cover callback exceptions and closing a worker transport during dispatch.

The protocol test deterministically checks 1/32/256/2,048-frame batches: no
intermediate buffer movement, numbered payload order, exactly two suffix bytes
moved once, repeated compaction, invalid suffix retention, discard and unwinding.
Existing tests exercise all splits, bursts, malformed lengths, reconnects,
spectator forwarding and real TLS paths.

The optional `openkal_test_optimization_protocol --benchmark` compares the
erase loop with the new cursor, without event draining. It uses Release
GCC 11.4.0, seven-byte frames, complete batches and 1/16/64 independent serialized
streams, including receive-vector setup. Nine process samples record cumulative
microseconds (median/p95); these are not concurrent worker/tick measurements or
CI timing thresholds. The exact work improvement is independent of timing:
for a 2,048-frame batch the loop moves 14,672,896 suffix bytes and the
new loop moves none; an incomplete suffix moves exactly two bytes in one
compaction. For one serialized stream, cumulative median/p95 microseconds were
528/1,038 versus 568/717 for single-frame batches, 371/378 versus 303/329 for
32, 404/424 versus 293/307 for 256, and 782/815 versus 293/303 for 2,048
(versus cursor). Small single-frame batches retain a small constant
overhead, about five nanoseconds per frame in this microbenchmark; large burst
suffix movement is removed. These local timings do not establish a server tick
speedup.

Benchmark data: `/tmp/openkal-r01-benchmark-summary.json`.
Validation logs: `/tmp/openkal-r01-build-final.log`,
`/tmp/openkal-r01-ctest-final.log` and `/tmp/openkal-r01-release-build-final.log`.

Validation passed: all 150 enabled Debug/null/Qt CTests (113.89 seconds),
17 optional-data tests disabled, Release protocol test and benchmark, formatting
and diff checks. The fresh source-fingerprinted coverage report has no missing
instrumentation; R01 executes 158/158 lines and 162/218 raw branch arcs, retaining
exception and invariant arcs without exclusions. This package is complete.

## R02 — Bounded Host output without socket waits in world processing

`ISession::send_all` remains a synchronous full-delivery API. A distinct
`try_send` returns progress, readiness, interruption or failure after one POSIX
or TLS attempt. Unsupported implementations fail without falling back to a
blocking send. TLS pins an unfinished write's pointer and length, suppresses
reads during that retry and rejects an intervening `send_all`.

Host sessions now own `HostConnection`, whose explicit `enqueue` accepts owned
buffers and does not claim wire delivery. All Host output uses this API. The
existing shared poll supplies readiness; one flush per peer runs before client
dispatch, outside the world mutex. A flush accepts at most 64 KiB and makes at
most 64 transport attempts. A retry waits until its unchanged request fits the
budget. Output produced later in the iteration is sent on a subsequent pass.

The explicit backpressure policy is a maximum of 1 MiB retained vector capacity
and 4,096 nonempty chunks per connection; partially transmitted buffers remain
fully charged until released. Each accepted chunk has a one-second deadline.
Overflow, deadline expiry or fatal I/O disconnects that peer and releases its
queue. Input is deferred while output remains, preventing a later EOF from
discarding accepted replies. Protocol closure stops new work immediately but
drains accepted replies; shutdown drains all peers against one shared one-second
deadline, with short readiness waits outside world locks, then closes them.

Disconnected actors leave the simulation immediately, while a separate connection
list drains their final output. They cannot regenerate, attack or remain online
while their socket is closing. Poll indices are taken from one stable snapshot.
A connection closes its transport before destroying a buffer borrowed by a TLS
retry.

Management PMs use optional completion receipts: acceptance is distinct from
complete transmission. Their local RPC remains pending without rerunning the
command or blocking other management peers. Success and the outgoing event are
recorded only after complete transmission; disconnect, expiry, overflow and
shutdown resolve unfinished receipts as failures. The world owns these receipts
independently of the requesting console. Shutdown services pending responses and
rejects new commands without mutating the world.

A pre-existing navigation test assumed that the first route-planning slice would
always fit its two-millisecond work budget. The assertion now accepts at most 32
explicitly reported planning yields, checks that authoritative position remains
unchanged in each slice, and still requires actual movement output before the
unchanged echo/rejection checks. No navigation implementation was changed.

Tests script POSIX/TLS partial writes, EAGAIN, EINTR, WANT_READ/WANT_WRITE,
unchanged retry arguments, zero/fatal results and no synchronous fallback.
Queue tests assert exact FIFO bytes, independent byte/attempt limits, physical
capacity versus vector size, retained partial-buffer accounting, chunk limits,
absolute deadline edges, input deferral, graceful final answers and progress of
another peer. The real management integration checks exact support-frame bytes,
success acknowledgment and one outgoing event, then verifies that a PM behind
unread output returns a delivery failure without adding an outgoing event while
another management request completes. Host characterization verifies a valid response preceding invalid
framing arrives before EOF.

The same real Unix integration test was added to an isolated archive of
`2dd01ecd`: 2,048 valid `/time` requests from an unread peer stall the initial
Host. The active peer's movement/chat/save barrier and authoritative monster
timer checks fail until the independent three-second watchdog disconnects the
slow peer. With R02, those checks pass before watchdog intervention. Shutdown
also must finish within two seconds without that watchdog releasing the peer.
The watchdog bounds the diagnostic failure, not the success condition.

Release `openkal_test_optimization_output_queue --benchmark` measures synthetic
queue-flush sweeps for 1/16/64 peers, including fake transport/capture overhead;
it is not a server tick execution-time or socket throughput measurement. Across
128 sweeps of 32 sixteen-byte frames per peer, median/p95/p99 microseconds were
0.020/0.030/0.030, 10.490/20.150/27.910 and 43.049/97.669/825.556. Mock wire-vector
growth contributes to the tails. The stalled queue peaked at 65,536 pending
bytes and used one transport attempt; healthy queues peaked at 512 pending
bytes and drained exact output. No wall-time speedup or production p99 claim is
inferred from this microbenchmark. The real two-peer test separately verifies
that authoritative world processing continues under backpressure.

Logs: `/tmp/openkal-r02-baseline-test-final.log`,
`/tmp/openkal-r02-targeted.log`, `/tmp/openkal-r02-backpressure.log`,
`/tmp/openkal-r02-ctest-final.log`, `/tmp/openkal-r02-release-tests.log` and
`/tmp/openkal-r02-queue-benchmark.csv`.

Final validation passed: all 152 enabled Debug/null/Qt CTests (117.15 seconds),
17 optional-data tests disabled, and the three focused Release queue,
management and backpressure tests. Formatting and diff checks passed. Logs:
`/tmp/openkal-r02-ctest-verified.log`, `/tmp/openkal-r02-release-verified.log`,
`/tmp/openkal-r02-backpressure-pm.log` and `/tmp/openkal-r02-coverage-final.log`.

The fresh fingerprinted coverage report has no missing instrumentation. All
122 executable lines in the new queue, all 53 selected POSIX transport lines,
all 99 selected TLS lines and all six selected transport-header lines execute.
The expanded R02 scope, including Host integration and management behavior,
executes 465/526 lines (88.40%) and 508/817 raw branch arcs (62.18%). Remaining
lines include multiplayer-only player reconciliation/admission, PM validation
and shutdown rejection, plus defensive management limits. PostgreSQL integration
was not available in this build. These raw gaps and exception arcs remain
visible without exclusions; complete queue execution is not a claim of complete
multiplayer coverage. This package is complete.

## R03 — Map-partitioned visibility candidates and retained scratch storage

A `WorldInterestIndex` borrows monsters, ground items and NPCs in source traversal
order, grouped by logical map. For multiple eligible viewers it rebuilds once
immediately before a complete world-tick visibility batch, after population updates, item expiry and corpse
removal, under the existing world lock. No source collection changes during
that batch. Existing stream/entry paths retain a direct traversal fallback so
mutable world access cannot silently leave a persistent index stale. Zero or
one eligible viewer uses that fallback and avoids the extra index pass.

The exact tile-sight and corpse predicates remain in the Host helpers. Creates
retain source order. A per-session vector replaces three freshly allocated
visibility hash sets; it retains capacity, collects visible IDs, then sorts for
removal membership checks. After deduplication, matching visible/known counts
prove that no removal is needed because all visible IDs are already known. This
fast path skips per-ID binary searches on unchanged snapshots; a differential
duplicate-ID fixture verifies that raw duplicate counts cannot hide stale IDs.
The index builder reuses the last bucket for adjacent same-map entities.
The existing known-entity sets still determine
removal traversal and update behavior. No spatial grid or mutation-version cache
is introduced in this package.

Differential tests compare the full traversal/per-call hash set with
both new paths. They check exact ordered create bytes, a fixed full packet
sequence, removal membership and all known-entity sets across sight edges,
corpse lifetimes, spawn/despawn, loot pickup/expiry, NPC reload, teleport,
reconnect and empty maps. Every container mutation precedes a fresh rebuild.
Stable repetitions assert retained per-map storage and session scratch capacity.
The optional benchmark covers stable-known monster interest only: 1/16/64
viewers, 256/1,024/4,096 local monsters, zero or three equally populated unrelated
maps and visible fractions 0/0.125/1. It includes index rebuilds and separately
reports the one-viewer fallback. Twenty-five alternating-order samples use
nearest-rank p50/p95/p99; p99 is therefore the maximum sample. These are CPU-only
visibility batches, not complete simulation ticks or socket throughput. Candidate
counts and retained capacities are reported; heap allocation counts are not
measured. The capacity columns describe one viewer scratch vector and the map-0
monster candidate vector, not total memory across all maps.

Release measurements on GCC 11.4.0, in microseconds (legacy versus selected
production path, p50/p95/p99): 64 viewers with 256 fully visible local monsters
on one map measured 652.976/661.196/663.886 versus
133.479/140.590/141.800. With 64 viewers, 4,096 fully visible local monsters
and three equally populated unrelated maps, the values were
11,953.105/12,316.642/12,477.401 versus 3,723.399/4,988.553/5,215.950.
That case reduces viewer candidate checks from 1,048,576 to 262,144, plus one
16,384-entity rebuild. For 16 viewers, 1,024 local monsters, three unrelated
maps and 12.5% visibility, values were 155.279/172.840/178.329 versus
39.120/40.310/51.210. The largest median regression among these 54 samples was
0.45 microseconds (23.02 to 23.47) for an empty visible set on a single map;
there is no universal speedup claim. The observed visibility-batch costs do not
justify adding a spatial grid in this package. Complete world-tick costs were
not measured here.

Raw benchmark assertions are retained in
`/tmp/openkal-r03-interest-benchmark-verified.raw.log`; the saved CSV omits
comment and test-status lines:
`/tmp/openkal-r03-interest-benchmark-verified.csv`.

Validation passed: all 152 enabled Debug/null/Qt CTests (115.76 seconds),
17 optional-data tests disabled, the Release differential test and benchmark,
formatting and diff checks. Fresh source-fingerprinted R03 coverage executes
127/127 lines and 196/252 raw branch arcs, with no missing instrumentation or
exclusions. Logs: `/tmp/openkal-r03-build-verified.log`,
`/tmp/openkal-r03-ctest-verified.log`, `/tmp/openkal-r03-release-verified.log`
and `/tmp/openkal-r03-coverage-verified.log`. This package is complete.

## R04 — Tick-local occupancy cells with sequential reservation updates

The movement/path-search occupancy predicate keeps its full-scan
fallback. A tick-local index groups living blocking monsters and player poses by
logical map and exact eight-pixel cell. Monster IDs preserve self-exclusion and
overlap counts; players always block independently of the querying monster ID.
Dirty or moving monsters reserve their target, as before.

A scoped index is available only while processing a tick with multiple monsters.
It builds lazily at the first occupancy query, using the states as they exist
then and resynchronizing the current monster's prior reservation. Ticks without
occupancy queries perform no index rebuild or cell allocation.
Before the next monster is processed, the previously processed reservation is
updated from its final state, including all early-continue paths. Queries while
processing a monster exclude that monster's ID, so its previous own reservation
cannot affect those queries. The final monster needs no refresh because the
scope ends before another occupancy query. The destructor only restores the
previous index/pose pointers and performs no allocating update. Direct mutable
world access and spawn queries outside that scope use the scan; a
foreign player-pose vector also falls back. Terrain checks, segment sampling,
path-search order/budget and combat timing remain unchanged.

Owned differential tests compare indexed queries, the retained full scan and an
independent arithmetic-floor oracle at signed cell boundaries and integer
extrema, across maps, duplicate IDs, self exclusion, overlapping players,
source/target reservations, dead/nonblocking bodies, numeric updates, spawn and
removal. Foreign pose vectors and nullable poses keep their behavior.
The sampled movement predicate is checked with both query paths. A borrowed-test
context also restores its pointers when a test callback throws; allocator
failure unwinding in the production tick scope was reviewed, not force-injected.

Two independently initialized worlds run 160 ticks with identical per-tick random
seeds, comparing 47 movement/combat/timer/hostility/path fields and all ordered
move, attack, chest and corpse events. Cases include converging/swapping bodies,
interrupts, control and corpse/opening continuations, player movement/immunity,
creation between ticks, death and next-tick attack after movement arrival.
A separate dirty-stop case makes the first occupancy query after its reservation
changes and compares eight subsequent ticks. The production tick does not insert
or erase its monster vector during traversal; tests do not add an artificial
mid-tick mutation callback.

The Release benchmark runs bounded path searches with 32/128/256/1,024 monsters
and one or 16 players. Each indexed timing constructs, rebuilds, queries and
destroys a fresh index, including bucket allocation. Twenty-five alternating
samples report nearest-rank p50/p95/p99 (p99 is the maximum sample). Recorded
step-callback order and waypoints must match. Candidate columns are modeled
population upper bounds across possible sampled cells, not measured scan visits;
short-circuited probes make these loose bounds. This is a CPU-only bounded-search
benchmark, not full simulation-tick latency.

For one player, 256 monsters measured 425.777/437.037/439.577 microseconds in
full-scan mode versus 43.459/49.929/52.080 with the index. At 1,024 monsters, the
search exhausts its unchanged budget with no waypoints: values were
6,863.737/7,344.543/7,369.933 versus 157.729/269.098/370.268. A separate complete
256-monster idle-tick benchmark, with no occupancy queries, measured median
5.790 versus 5.910 microseconds; its p95/p99 values were 5.910/7.250 versus
5.990/6.310. This compares the retained scan mode with the production lazy scope;
it is not an archived pre-change binary comparison.

Benchmark assertions: `/tmp/openkal-r04-benchmark.raw.log`. Filtered data:
`/tmp/openkal-r04-path-benchmark.csv` and
`/tmp/openkal-r04-idle-benchmark.csv`.

Validation passed: all 152 enabled Debug/null/Qt CTests (115.93 seconds),
17 optional-data tests disabled, the focused Debug and Release oracle tests,
the Release benchmark, formatting and diff checks. Fresh source-fingerprinted
R04 coverage executes 138/138 lines and 143/168 raw branch arcs, with no missing
instrumentation or exclusions. Logs: `/tmp/openkal-r04-build-accepted.log`,
`/tmp/openkal-r04-ctest-final.log`, `/tmp/openkal-r04-focused.log`,
`/tmp/openkal-r04-release-test.log` and `/tmp/openkal-r04-coverage-final.log`.
This package is complete.

## R11 — Share compatible UI test compilation

Four explicit object-support targets share the seven core UI sources and the
remaining eleven HUD sources in separately verified server-linked and
engine-linked groups. Each support target copies its exemplar's ordered build
properties and link dependencies. Consumers receive only the compiled objects;
their test mains, definitions, include paths and link libraries stay unchanged.
Runtime-icon generation remains a dependency of both support and consumer
targets. The sharing helper runs after characterization targets have copied
source lists, so those variants retain their own compilation context.

The guild archive, settings/battle-feed, inventory/skill/mix resource roots,
client runtime roots and development/release hot-reload variants remain
separate. No C++ implementation or curated resource is changed. The current
Debug/null/Qt compile database decreases from 1,055 to 858 commands: 233 repeated
objects become 36 shared objects, removing 197 compilations. All 822 retained
object-output commands match their previous normalized compiler arguments,
including every test main. Each shared source/flags combination exists in the
baseline; definition and include order are preserved. All 169 registered CTest
names, commands and properties match, excluding source backtrace metadata.
The independent GL configuration likewise decreases from 997 to 800 commands,
with all 764 retained commands and 168 registered test configurations unchanged.
These profile counts differ because the GL check disables optional tools.

Authoritative metadata snapshots and comparison results are in
`/tmp/openkal-r11-before`, `/tmp/openkal-r11-final-audit.json` and
`/tmp/openkal-r11-gl/audit.json`. Build measurements use an isolated archive of
`f073ed32`, identical source/build paths, Debug/null, tools off and four jobs;
only the two R11 CMake files change between the paired builds. An interrupted
intermediate build is discarded and excluded from results.

One paired clean build measured 215.85 versus 173.35 seconds wall time and
859.05 versus 691.81 seconds CPU time (19.69% and 19.47% lower). The compile
database has 991 versus 794 entries in that tools-off profile; the default build
executes 990 versus 793 C++ compilations because the unchanged fixture generator
is excluded from `all`. Link action counts remain 154. After touching only the
archived `UiView.cpp`, incremental builds measured 9.89 versus 6.46 seconds wall
and 38.33 versus 24.96 seconds CPU, with 25 versus 11 compilations and 25 links
in both cases. Build-directory size measured 2,621,072 versus 2,437,184 KiB.
Caches were not flushed; these are local observations from one pair, not CI
performance thresholds or a statistical speedup estimate. Maximum child-process
RSS was essentially unchanged and is not total concurrent build memory.

Adding/removing an owned JSON probe in the isolated archive updates the generated
manifest and packaged contents. Touching an owned icon input rebuilds its BMP
and dependent archive, whose bytes match the generated output; asset
bytes remain unchanged. Six focused resource/UI/hot-reload tests pass. Full
measurements and dependency checks are recorded in
`/tmp/openkal-r11-bench/results/summary.json`.

The selected GL build and seven CTests pass: clock/minimap HUD, NPC talk,
consumable crafting, development/release UI layout reload and packaged guild UI.
Actual software-OpenGL clock, NPC and minimap capture paths exercise all four
shared object groups; 18 generated frames are valid and nonblank. These smoke
checks do not assert pixel equality against archived captures. GL logs and
reproduction commands are under `/tmp/openkal-r11-gl/after`.

Validation passed: all 152 enabled Debug/null/Qt CTests (116.38 seconds),
17 optional-data tests disabled, the seven selected GL tests and captures,
paired build/dependency checks, compiler-command/CTest metadata comparisons,
and diff checks. A fresh coverage report remains valid for every runtime scope,
with no missing instrumentation or exclusions; R11 is a build-only package
verified by the checks above. Logs: `/tmp/openkal-r11-build-final.log`,
`/tmp/openkal-r11-ctest-final.log` and
`/tmp/openkal-r11-coverage-final.log`. This package is complete.

## R05 — Constant-time pending-event FIFO removal

The 27 private queues used only for append, front consumption, clear and
bounded oldest-entry eviction use `std::deque`. Their `take_*` APIs retain
payloads and order while `pop_front` avoids shifting every remaining event.
Owned payloads move to the caller before removal. The create/remove-player
queues remain vectors because packet handling cancels matching entries in the
opposite collection. Guild and player-option snapshot caps remain 128 and 32.
Existing close/adopt cleanup behavior is preserved, including pending chats
and partial receive bytes surviving close; those characterized behaviors are
outside this container optimization.

The expanded characterization checks both caps at cap-1/cap/cap+1, partial
drain followed by refill, alternating numbered input/output and unchanged output
on an empty take. Explicit create/remove tests retain cancellation of unread
creates and duplicate removals. Numbered 32/256/2,048-event skill bursts validate
real packet-to-queue delivery. Existing EOF, close and reconnect guards remain.

The optional `openkal_test_optimization_client --benchmark` measures only actual
`take_skill_action` draining, excluding reception and parsing. An isolated
`f073ed32` source archive has the same pre-R05 runtime implementation as
`0f1d7329`; it receives only the new test driver for the baseline. Both builds
use ordinary Debug/null, GCC 11.4.0, without coverage instrumentation. Across
25 local samples, median/p95 microseconds for 32, 256 and 2,048 events are
3.470/3.570 versus 1.210/1.400, 29.549/30.540 versus 4.640/4.690, and
465.747/904.005 versus 36.510/36.720 (vector versus deque). These are small
single-process observations, not Release or end-to-end latency claims.

A separately labeled counted container surrogate has the same element size as
`PendingSkillAction`. It asserts 528/32,896/2,098,176 assignments for legacy
vector draining versus 32/256/2,048 for deque draining, including one output
assignment per event. These are surrogate counts, not production instrumentation.
The tradeoff is eager empty-queue storage in this libstdc++ implementation:
`sizeof(NetClient)` increases from 2,848 to 4,360 bytes, and its default
constructor requests 54 allocations totaling 15,248 bytes instead of none.
Requested bytes exclude allocator metadata and do not measure retained RSS.
This bounded per-client cost removes backlog-dependent element movement.

Benchmark logs: `/tmp/openkal-r11-bench/results/r05-baseline-benchmark.log` and
`/tmp/openkal-r05-benchmark-current.log`. Queue classification evidence:
`/tmp/openkal-r05-queue-audit.json`.

The expanded normal guard also passes against the unchanged vector baseline,
confirming that its behavioral expectations precede this optimization.

Validation passed: all 152 enabled Debug/null/Qt CTests (116.19 seconds),
17 optional-data tests disabled, the five named client guards (18.35 seconds),
both baseline/current benchmark modes, the old-vector expanded guard,
formatting, independent production/test reviews and diff checks. Final Debug
builds include the client executable and its changed-header callers. Logs:
`/tmp/openkal-r05-build-final.log`, `/tmp/openkal-r05-ctest-final.log` and
`/tmp/openkal-r05-focused.log`.
Fresh source-fingerprinted R05 coverage executes 185/185 lines and 77/80 raw
branch arcs, with no missing instrumentation or exclusions. The changed raw
branch denominator reflects the different standard-container implementation;
it is not a claim that tests removed exception paths. Coverage log:
`/tmp/openkal-r05-coverage-final.log`. This package is complete.

## R06 — Incremental FreeType line measurement

The FreeType overload of `layout_text_lines` appends each source cluster to the
current measured width. `append_text_width` adds each folded glyph advance in
rendering order, preserving float rounding for ellipsis expansion and malformed
input. The generic callback layout stays unchanged, so arbitrary measurement
functions retain their first-overflow behavior. The new overload lives with
FreeType implementation code; generic-only consumers such as the quest journal
and Qt UI editor gain no FreeType link dependency.

UiView multiline inputs, guild announcements with a loaded font and skin-shop
card names use the specialized overload. Guild's unloaded-font byte-width
fallback remains on the generic path. The Qt editor retains QFontMetricsF
measurement. Font scale, fallback glyphs, whitespace and explicit blank lines
are preserved. No layout cache, font data or UI schema/content change is added.

Preferred-space wrapping can revisit an unfinished word prefix. That prefix
contains no ASCII spaces and is then consumed or hard-wrapped; it cannot cause
another preferred-space rewind before consumption. Together with an overflow
cluster retry, this gives a conservative three-times-input bound on glyph work.
The implementation does not claim to visit every glyph exactly once.

The differential guard compares exact source spans against the unchanged
generic callback for 17 explicit and 120 seeded mixed/malformed strings,
eight scales and thirteen widths, including adjacent float values around a
mixed-width ellipsis prefix. It repeats with unloaded, loaded, destroyed and
reloaded fonts. Existing UiView quad checks retain scrolling, source color spans,
line height, pixel size, empty input and fallback behavior.

Nine isolated gcov probes count the actual `append_text_width` glyph lambda,
without a product counter API. Wide ASCII inputs of 256/1,024/4,096 bytes
perform exactly 256/1,024/4,096 additions and cluster calls. Mixed UTF-8 inputs
of 513/2,049/8,193 bytes perform 360/1,434/5,736 additions in
256/1,024/4,096 cluster calls, matching all folded glyphs. Narrow ASCII inputs
with spaces perform 371/1,499/6,019 additions and cluster-byte visits, below
the asserted three-times-input bound. The counters measure glyph-advance
callbacks; ASCII maps one callback to one width-processed byte, not every
byte read by the decoder or line walker. These probes are separate processes;
the authoritative coverage run clears their profiles first. Reproduction and
counts: `/tmp/openkal-r06-work/check_work.py` and
`/tmp/openkal-r06-work/summary.json`.

The optional `openkal_test_optimization_multiline --benchmark` draws unchanged
multiline text through UiView into the existing glyph-quad capture engine.
Font/document setup and post-frame quad validation are outside timing; every
frame must contain exactly one quad per source `W`. A pre-R06 archive receives
only that unchanged-API driver. Both builds use the same installed system
TrueType face, GCC 11.4.0, null engine and 25 samples. For ordinary Debug,
median/p95 microseconds for 256, 1,024 and 4,096 characters are
614.247/636.558 versus 35.900/39.620, 9,409.047/10,922.590 versus
132.580/134.169, and 148,010.878/149,795.503 versus 521.258/575.308
(versus incremental). These deliberately wide inputs expose prefix
remeasurement; they do not predict typical narrow-field or GPU frame latency.

Release uses `-O3 -DNDEBUG` in both builds. Median/p95 microseconds are
45.760/56.740 versus 4.690/5.280, 670.947/791.727 versus 16.370/19.490,
and 10,752.725/12,655.358 versus 63.120/70.800 for the same character counts.
Timings are local observations; the deterministic work counts establish the
complexity change. Normal multiline guards also pass against the old Debug
and Release implementations, confirming the benchmark driver preserves behavior.

Debug benchmark logs: `/tmp/openkal-r06-bench/baseline-benchmark.log` and
`/tmp/openkal-r06-benchmark-current.log`. Release logs:
`/tmp/openkal-r06-bench/release-benchmark.log` and
`/tmp/openkal-r06-release-benchmark.log`.

All 152 enabled Debug/null/Qt CTests pass (118.15 seconds), with 17 optional-data
tests disabled. Release differential/span and multiline tests pass. The full
coverage report identified additional reachable caller gaps in loaded-font guild
announcements and skin-shop card drawing. New normal guards exercise both.
Guild pagination is compared against the legacy loaded-font callback and is
deliberately different from unloaded byte estimates. The test checks scrolling,
UTF-8 editing and complete saving at three zoom levels with an announcement
inside the existing 1,024-byte save limit. A synthetic skin catalog draws its
long UTF-8 card name and checks the offer, selection and ticket count remain
unchanged. Precise glyph geometry stays covered by the multiline quad oracle.

The direct and packaged guild tests and skin-shop test also pass against an
archive whose six production files match pre-R06 `f073ed32` exactly (3/3,
0.55 seconds). Baseline logs:
`/tmp/openkal-r11-bench/baseline-r06-caller-build.log` and
`/tmp/openkal-r11-bench/baseline-r06-caller-ctest.log`.
A final caller-complete suite exposed a pre-existing timing assumption in
`stalled_movement`: an unchanged TestPlayerAgent fixture required a movement
packet in the first 2 ms planning slice. The identical binary passes in an
isolated rerun (28.20 seconds); source review confirms that `Searching` is a
valid no-send return. The test now permits at most 32 planning slices while
checking the confirmed pose, then retains exact one-movement, no-echo and
delayed-echo assertions relative to the actual first-send time. Production
agent behavior is unchanged. This fixture repair is committed separately as `f11a77cd`.

The fixture-repaired full suite passes all 152 enabled tests (116.41 seconds).
Its separate commit caused the pre-commit hook to refresh the other source
files' timestamps, so the strict coverage check correctly required rebuilding
those unchanged sources. After that complete rebuild, all 152 tests pass again
with fresh profiles (116.82 seconds); 17 optional-data tests remain disabled.
The final fingerprinted report is valid with no missing instrumentation or
exclusions: R06 executes 102/104 lines and 108/136 raw branch arcs. The two
unexecuted source locations are the FreeType layout's closing brace and a
constant in guild pagination; their raw counts remain visible. Allocation and
compiler/exception arcs are retained without exclusions.

Final logs: `/tmp/openkal-r06-rebuild-after-hook.log`,
`/tmp/openkal-r06-ctest-after-hook.log` and
`/tmp/openkal-r06-coverage-after-hook.log`. Formatting and independent
implementation/test/probe reviews pass. This package is complete.

## R09 — Shared immutable monster-selection joins

`TestPlayerSelectorIndex` builds owned spawn/drop membership once before each
`TestPlayerCatalogs` instance is published as shared const data. Definition
positions remain separate even when IDs repeat. Spawn maps retain authored
order and duplicates, so runtime portal checks have the same call order and
short-circuit behavior. Only pure item memberships are sorted and deduplicated.
Temporary itemgroup expansion results are reused during construction.

The agent still visits its current quests, task flags and definitions in their
existing order. Level, item counts, quest completion and map/portal reachability
remain dynamic. Reachability is checked before drop membership, including for
definitions with no useful drops. Per-agent navigation, avoidance expiry and
failed-route caches are unchanged. Reload creates a replacement immutable index;
existing agents retain their previous snapshot until explicitly restarted.
Ordinary slots still require a positive attempt count, nested reference/item
chance comparisons retain their exact predicates, and quest-specific membership
retains the flag/id matching. Direct helper tests cover zero quest roll
counts; the unchanged JSON loader rejects those records before agent selection.

A standalone non-Qt test compares the production helper with the initial
nested predicate and ordered selector driver. It covers duplicate IDs/maps,
empty/missing data, positive/zero/negative/NaN chances, money entries, source
lifetime, dynamic filters and 288 seeded catalog/state differentials. Agent
integration checks pickup/quest-clear on the next target selection, level and
map changes, route behavior, independent snapshots and restart onto a reloaded
catalog. Current focused tests pass (15.79 seconds). The same integration driver
also passes against byte-identical pre-R09 Agent source (15.76 seconds), with
no new helper in that archive. Baseline logs:
`/tmp/openkal-r11-bench/baseline-r09-selector-configure.log`,
`/tmp/openkal-r11-bench/baseline-r09-selector-build.log` and
`/tmp/openkal-r11-bench/baseline-r09-selector-ctest.log`.

The optional `openkal_optimization_selector_test --benchmark` uses Release,
GCC 11.4.0, `-O3 -DNDEBUG`, nine alternated-order samples and one shared index
for 1/16/64 sequential selector calls. It independently varies objective,
definition and spawn counts in seven no-match catalogs, plus a tiny early match
and a shared large drop table: 27 rows in total. These are selector-driver/helper
measurements with simple map reachability, not concurrent agents or whole ticks.
The existing action gate remains in place; no unconditional 100 Hz scan is
claimed. With nine samples the reported p95 is the maximum observed sample.

For Q=4/D=64/S=64 and 64 calls, median/p95 microseconds decrease from
977.182/983.302 to 193.728/223.838. The driver counts 532,480 versus 16,384
spawn iterations and 524,288 legacy nested item iterations; indexed queries use
precomputed membership lookup instead of traversing ItemGroupDb. With D=256 and
S=64, the same Q/A case decreases from 3,789.127/3,972.087 to
567.545/623.955 microseconds. Definition traversal remains unchanged.

Construction is a deliberate shared startup/memory cost. The 64-definition,
32-item case builds in median 11.129 microseconds and requests 205 allocations
for 17,836 bytes. The 256-definition shared 512-item case builds in median
796.943 microseconds and requests 786 allocations for 562,100 bytes; flattened
membership can grow with definitions times matching item cardinality. The index
object itself is 80 bytes in this build. Requested bytes include temporary
construction storage and exclude allocator metadata; they are not retained RSS.
These costs are shared by all agents using the catalog. Tiny early-match
selection is near clock resolution (0.040 microseconds median in both paths).
The large-table early-match case has no useful scan reduction and, across 64
calls, measures 1.060 versus 1.330 microseconds; no universal speedup is claimed.
Local observations are not CI timing thresholds. Full data:
`/tmp/openkal-r09-release-benchmark.log`.

Validation passed: all 153 enabled Debug/null/Qt CTests (116.62 seconds),
17 optional-data tests disabled, focused Agent/selector guards, the Release
selector test and benchmark, baseline Agent integration, independent reviews,
formatting and diff checks. Fresh fingerprinted R09 coverage executes 83/83
lines and 101/128 raw branch arcs, retaining compiler/exception arcs without
exclusions and with no missing instrumentation. Logs:
`/tmp/openkal-r09-build-accepted.log`, `/tmp/openkal-r09-focused-fixed.log`,
`/tmp/openkal-r09-ctest-final.log`, `/tmp/openkal-r09-release-test.log` and
`/tmp/openkal-r09-coverage-final.log`. This package is complete.

## R10 — Remove repeated item-picker presentation work

Each item-picker row now computes its composed pixmap once and reuses it for
both existing decoration assignments. The stored decoration remains a QPixmap;
occupied marks, prefix overlays, tooltips and DPI behavior retain their existing
path. Grade partitions normally inherit the sorted catalog order. They now
check that order before sorting, retaining the sort when an open dialog observes
changed name, kind or grade keys after a store reload.

The change deliberately retains immediate filtering and the existing selection
signals, including the characterized duplicate/lost-selection behavior.
Matching rows are still rebuilt for every immediate refresh.
Widget/model reuse would change destruction/clear selection events; a lookup
alone would also leave the synchronous full-list selection callbacks in place.
There is no catalog revision contract for a composition cache. This package
therefore takes the bounded repeated-work reduction without introducing a
cross-refresh cache or changing selection behavior.

Regression additions check filtering before event dispatch, live name/kind/grade
changes with the flat-view ordering, and pixel output after replacing
owned icons/removing overlays/resetting occupied state. Normal and double-DPI
guards pass against both current and byte-identical pre-R10 production. The
full Debug/null/Qt suite passes all 153 enabled tests (29.46 seconds with four
CTest jobs); 17 optional-data tests remain disabled. The fresh coverage report
is valid with no missing instrumentation: R10 executes 401/407 lines and
427/676 raw branch arcs. Unreachable fallback paths, multiline drawing/closing
brace bookkeeping and exception arcs remain visible without exclusions.

The optional `openkal_optimization_item_picker_test --benchmark` measures seven
fresh dialogs at each of 1,000, 10,000 and 50,000 owned definitions. Catalog
loading is outside the timer; each opening uses a new icon cache and includes
show/event processing. Tab timings include event processing; successive search
updates are synchronous and timed without it. The 16-selection observation is
a complete search/rebuild/restoration cost, not isolated restoration work.
Qt model insertion signals count flat rows; separate gcov probes count actual
row creation, icon composition and ordering comparisons. No CI timing limit is
introduced. Both Release compile commands match after normalizing only source
and build roots (GCC 11.4.0, `-O3 -DNDEBUG`, Qt 6.9.0, offscreen platform).

Sequential, otherwise idle local runs report median/p95 milliseconds at 50,000
items: Knight tab 203.371/224.760 to 116.638/128.310; Mage tab
71.701/72.900 to 49.885/50.489; return to All 333.245/343.599 to
267.356/271.393. Opening changes only from 2,337.273/2,577.797 to
2,267.588/2,363.411; new icon-cache loading and all row materialization remain.
The complete 16-selection refresh changes from 342.855/355.260 to
277.023/282.480. Narrowing at 50,000 items has no consistent gain: its three
medians change from 90.678/28.861/27.212 to 88.838/29.102/28.386 ms.
At 1,000 items opening is 2.25% slower and the small Mage tab 1.72% slower in
this pair. These observations do not establish a universal improvement or a
production desktop/frame-time bound. The first opening includes process-local
Qt/font initialization, and seven-sample p95 is the maximum observed value.
Observed row insertion counts are unchanged.

Data: `/tmp/openkal-r10-release-benchmark-idle.log`,
`/tmp/openkal-r10-baseline-benchmark-idle.log`,
`/tmp/openkal-r10-work/timing-comparison.json` and
`/tmp/openkal-r10-work/release-compile-commands.json`.

Actual isolated gcov probes verify 1,000 opening rows in both versions with
2,000 versus 1,000 `tile_icon` calls. Opening plus class refresh creates 1,250
rows with 2,500 versus 1,250 compositions; opening plus search or restoration
creates 2,000 rows with 4,000 versus 2,000 compositions. Initial catalog sorting
uses 12,185 comparisons in both versions; the class refresh adds 1,560 versus
248. The old probe instruments only its picker translation unit after the
Release timing run, and is not used for those timings. Counts:
`/tmp/openkal-r10-work/current-counts.json` and
`/tmp/openkal-r10-work/baseline/old-counts.json`.

Validation logs: `/tmp/openkal-r10-build-accepted.log`,
`/tmp/openkal-r10-focused-accepted.log`, `/tmp/openkal-r10-ctest-final.log`,
`/tmp/openkal-r10-coverage-final.log`, and baseline normal/HiDPI results under
`/tmp/openkal-r10-baseline`. Independent source/test reviews, formatting and
diff checks pass. This bounded package is complete; row/model lifecycle changes
and composition caching remain future work rather than an unverified claim.

## R07 — Separate combat calculation from session application

Combat inputs now borrow a const session and world, with an explicit requested
collection profile and one captured guild-bonus array. The calculator returns
owned derived values; an explicit application function initializes effective
base stats, updates the world association and assigns the existing calculated
fields. It copies no inventory, mount map, buff set or complete ClientSession.
Ordered phase comments retain the straight arithmetic flow without adding a
handler hierarchy or a large calculation context.

Alternate noncontact PvP previews calculate one temporary profile and project
its Combatant directly. They do not toggle the live collection mode, apply
intermediate results, clamp live resources or rebuild the profile.
Actual contact selection, suppression deadlines, proportional resource changes
and progression packets retain their existing path. No stat cache is introduced.

A source audit anchored to commit `78ec5206` verifies all 47 application
assignments (46 top-level session fields), including five base/prefix values,
collection bonuses, HP/MP, BoF state, transform speed and only wear slots 0/3.
It verifies the 30 result defaults that replace in-place resets. Under explicit
field/base/profile/guild normalization rules, all 7,818 formula tokens and 488
Combatant projection tokens match. This is bounded mapping/token evidence,
not a C++ semantic proof; the runtime comparisons below supply separate
validation. Audit: `/tmp/openkal-r07-review/review.py`, `summary.json` and
`review.log` in that directory.

New owned fixtures record 96 combinations of four classes, initialized versus
uninitialized bases, six loadouts and two profiles. They include equipment
fallbacks, prefixes/charms/mix/Qigong/Dragon/BoF effects, skills and curses,
collections, talents, mounts, transformations and resource boundaries. Each
record includes 86 named session fields, all 42 Combatant inputs for current
and alternate profiles, and fixed-seed physical attack sequences against player
and monster targets. The 1,536 attack rolls include every AttackResult field
and the following random value to check RNG consumption. Existing independent
guild golden vectors remain enabled; these snapshot fixtures have no guild
service.

A separate improvement guard performs three repeated previews in 48 raw
uninitialized/stale-state cases, comparing resources, initialization, derived
values, all wear cells, collection mode/world association, deadlines,
inventory/collection/buff inputs and output queue state. It compares the preview
to an independently recomputed PvP reference. The old implementation fails all
48 state-preservation checks; the new implementation passes. The established
profile/contact/resource guards remain intact.

All 96 full records, including controlled damage and subsequent RNG values,
match the byte-identical pre-R07 Host implementation. Comparison evidence:
`/tmp/openkal-r07-work/differential.json`,
`/tmp/openkal-r07-current-snapshot.jsonl` and
`/tmp/openkal-r07-baseline-snapshot-records.jsonl`. Snapshot mode also emits
ordinary test diagnostics; the comparison extracts the JSON record lines.

Validation passes all 153 enabled Debug/null/Qt CTests (29.50 seconds with four
jobs), with 17 optional-data cases disabled. Fresh fingerprinted coverage is
valid with no missing instrumentation: R07 executes 836/844 lines and 457/605
raw branch arcs. Its eight remaining lines correspond to the same multiline
initializer/call bookkeeping and unreachable guards/defaults as before the
extraction; compiler and exception arcs remain visible without exclusions.
Current Release guards also pass. Logs: `/tmp/openkal-r07-build-final-driver.log`,
`/tmp/openkal-r07-focused-accepted.log`, `/tmp/openkal-r07-ctest-final.log`,
`/tmp/openkal-r07-coverage-final.log` and `/tmp/openkal-r07-release-test.log`.

The optional `--combat-benchmark` uses the same owned mixed loadout for four
classes, 25 samples of 256 repeated alternate previews per class, Release GCC
11.4.0 with `-O3 -DNDEBUG`. Old/current commands match after normalizing only
source and build roots. Sequential local runs have identical output checksum
16,947,200. Median/p95 microseconds per 256 previews decrease from
1,220.572/1,227.193 to 583.977/589.447 for Knight;
1,162.843/1,169.133 to 551.897/562.216 for Mage;
1,188.743/1,220.633 to 576.726/580.946 for Archer; and
1,158.923/1,173.183 to 566.986/583.567 for Thief. This measures the preview
calculation/projection, not network handling, whole combat or world tick time.
Recalculation remains event-driven, and no universal speedup is claimed.
Data: `/tmp/openkal-r07-current-release-benchmark.log`,
`/tmp/openkal-r07-baseline-release-benchmark.log`, and
`/tmp/openkal-r07-work/{timing-comparison,release-compile-commands}.json`.

Isolated actual gcov probes subtract setup from setup-plus-preview. The old
path adds two `recompute_combat` calls and two `session_combatant` entries
(one recursive);
the new path adds one `calculate_combat`, zero `apply_combat_result` and zero
`recompute_combat` calls. The baseline test translation unit is instrumented
only after its Release timing run. Counts and scripts:
`/tmp/openkal-r07-work/current-counts.json`,
`/tmp/openkal-r07-work/baseline/old-counts.json`,
`/tmp/openkal-r07-work/count_current.py` and `count_baseline.py`.
Independent source and fixture reviews, formatting and diff checks pass.
This package is complete.

## R08 — Isolate bounded requests and name client lifecycle cleanup

Talent requests and the Medicine Alchemist quest subroute now have private Host
handlers with explicit malformed/handled/not-this-family outcomes. Phase and
opcode dispatch, Quest parsing, session ownership, the poll loop and scheduled
phases remain in Host. Each caller preserves its continue or
fall-through behavior. The existing crafting transaction helper is unchanged.
The captureless server-chat lambda becomes a private free function with exactly
the same guards, speaker and enqueue operation so both request handlers and the
existing Host paths can use it.

Talent handling has a small two-function-pointer clock view. Default reads use
the existing system/steady clocks; reads remain lazy and occur at the initial
conditions. Successful Beacon persistence precedes the fresh activation-time
read. Final state projection retains its steady-then-Unix reads and the existing
two-argument API. No handler hierarchy, asynchronous work or new hot-path
allocation is introduced. Talent mask and cooldown rollback remain unchanged.

`NetClient::clear_online_views` replaces five identical 16-statement blocks in
Unix/TCP connection attempts, update, close and receive failure. All calls retain
their position and count, including two serial increments on EOF and repeated
cleanup for retained dead sessions. Close keeps its separate additional resets;
buffered receive suffixes and pending chats retain their characterized lifetime.

Anchored source audits reconstruct the entire Host thread body after
expanding the two handlers and restoring the former chat lambda. Extracted
conditions/statements and the chat body retain their exact token order; the
craft transaction and complete R07 calculator block are byte-identical.
Independent client replay expands all five helper calls to the full
source token stream and verifies the header changes only by the private helper
declaration/comment. Evidence: `/tmp/openkal-r08-audit.py`,
`/tmp/openkal-r08-review/host-audit.log`,
`/tmp/openkal-r08-client-token-audit.json` and
`/tmp/openkal-r08-review/independent-client-audit.json`. Lexer counts differ
between the two client audits; both independently report exact equality under
their stated tokenization, not a general semantic proof.

Owned direct Talent replay uses a real disposable SQLite store, audit and
save-failure triggers, exact ordered wire frames and clock-call traces. It
covers every Apply truncation, trailing/unknown requests, malformed then valid
requests, stale/dead/teleport/removal rejection, persistence rollback, all Beacon
guards, equality/+1 deadline edges, duplicate requests and interleaved clients.
External fixtures add pre-Connect/pre-Answer phase gates and characterize the
accepted trailing-byte Quest request through a successful crafting exchange.
Client guards assert all three serial counters and transport close counts in
seven exit modes, repeated EOF/close, no-session update, adoption, and owned Unix
success/failure followed by an invalid-IPv4 TCP replacement attempt.

The expanded public client lifecycle guard passes against exact pre-R08 source
(3.11 seconds). Both external Talent and Crafting guards also pass against the
pinned old Host (2.31 and 0.19 seconds). Only their test drivers were copied into
the isolated archive; relevant implementation hashes match `1438f799`, recorded
in `/tmp/openkal-r08-review/baseline-source.json`. Logs:
`/tmp/openkal-r08-baseline-client-{build,ctest}.log` and
`/tmp/openkal-r08-baseline-external-{build,ctest}.log`. Current focused six-test
validation passes in 7.97 seconds. This is a maintainability change with no
runtime speedup claim; further request families and scheduled-phase extraction
remain separate future work.

The first full run and an isolated recheck exposed an inherited emote-fixture
setup race. Immediately before the first broadcast, a temporary diagnostic
recorded observer clock not ready and player ID zero while the sender was ready.
The unchanged fixture waited only for the sender; completing handshake sends the
client answer but does not await server entry. The fixture now explicitly waits
for the observer's clock and nonzero identity, with clean teardown on setup
failure. The complete 20-emote loop, delivery assertions and 260 ms cooldown
spacing are byte-identical; no production behavior or arbitrary delay changes.
Diagnostic evidence: `/tmp/openkal-r08-multiplayer-diagnostic.log` and
`/tmp/openkal-r08-multiplayer-diagnostic-full.log`. Fixture source was
byte-identical in the current and pre-R08 archive before this correction.

The corrected emote fixture passes twice against current code (36.42 seconds,
40 observer-delivery assertions) and once against the pre-R08 Host/Net baseline
(18.14 seconds, 20 assertions). Full proof and bounded-diff details:
`/tmp/openkal-r08-review/observer-readiness-proof.json`; run logs:
`/tmp/openkal-r08-multiplayer-fixed-current.log` and
`/tmp/openkal-r08-multiplayer-fixed-baseline.log`.

Final validation passes all 153 enabled Debug/null/Qt CTests (29.44 seconds with
four jobs), with 17 optional-data cases disabled. A final no-transport state
projection guard additionally verifies that neither injected clock is read when
no socket exists; the rebuilt Host target passes (0.41 seconds). The fresh
fingerprinted report is valid with no missing instrumentation: the expanded R08
scope covers all five cleanup callers, both handlers, chat and clock/state
projection, executing 186/186 lines and 160/198 raw branch arcs. Exception and
unreachable/invariant arcs remain visible without exclusions. Logs:
`/tmp/openkal-r08-build-final.log`, `/tmp/openkal-r08-focused-accepted.log`,
`/tmp/openkal-r08-ctest-accepted.log`,
`/tmp/openkal-r08-host-final-{build,test}.log` and
`/tmp/openkal-r08-coverage-accepted.log`. Independent reviews, formatting and
diff checks pass. This bounded package is complete.

## R12 — Bounded temporary storage for filtered inventory projection

`Window::refresh_general_projection` now uses fixed arrays for the writable
domain (216 source slots) and one reused expired-page domain (36 slots). Only
initialized prefixes are read. The two-pass packing order, category predicates,
empty-cell handling and page isolation remain unchanged. The arrays require
1,008 bytes of explicit temporary storage on this platform, plus small counters,
instead of transient vector allocations. There is no persistent cache or
revision/invalidation machinery. Refresh still precedes the existing hidden/open
guards and occurs independently for interactions, so externally mutable contents,
same-address catalog reloads and register expiry remain immediately observable.

An anchored source check compares the actual packing body with pre-R12
`c8cca3a1`, under explicit pointer/count and indexed-loop normalization. All
187 tokens match. Enumeration of all 64 writable-page masks checks exactly 216
unique source cells with identical domain order. Bounds follow from initialized
prefixes and disjoint matching-occupied/empty passes. This is bounded source
evidence, supplemented by the runtime oracle, not a semantic proof. Evidence:
`/tmp/openkal-r12-review/{projection-proof.py,projection-proof.log,summary.json}`.

The existing independent stable-priority oracle checks all 216 display mappings
for 64 filter masks, 64 writable masks and three scales. All reference category,
short/empty bag, metadata, authoritative replacement, count/index mutation,
catalog replacement/reload, unlock/expiry, hidden draw, click, drag, destroy,
double-click and expired-storage guards remain intact. Normal Debug focused
validation passes all four inventory tests (4.41 seconds). The full Release UI
guard passes against both old and current implementations, using the same new
measurement driver and unchanged assertions.

The optional `--projection-benchmark` measures 25 samples of 1,000 actual
closed-window pointer updates or hidden draws, with unchanged and continuously
mutated contents. Mutation changes same-address index/count/instance data and
writable-page masks. Fixture creation, reset and the final independent 216-cell
oracle are outside timing/allocation intervals. Observable mapping checksums
match between old/current and timed/counted intervals. The two Release builds
use GCC 11.4.0, `-O3 -DNDEBUG` and the null engine; all 45 relevant normalized
compiler commands match after replacing only source/build roots. The baseline
is a pristine `c8cca3a1` archive with only the test driver and its wrapper flag
added. Sequential otherwise idle timings precede any baseline instrumentation.

Median/p95 microseconds per 1,000 unchanged pointer updates decrease from
12,085.519/12,455.788 to 11,649.682/11,760.561; continuously mutated updates
change from 13,898.829/13,955.528 to 13,226.723/13,476.711. Unchanged hidden
draws change from 12,033.609/12,210.579 to 11,559.153/11,742.581; mutated draws
change from 14,214.006/15,150.452 to 13,298.472/13,616.811. These local median
reductions of 3.6–6.4% describe the active-filter projection callers, not complete
rendering or desktop frame latency. No CI timing threshold is introduced.

A GNU scalar-new linker wrapper counts requests and requested bytes separately
from timing. Per 1,000 unchanged calls, requests decrease from 29,000 and
2,544,000 bytes to zero; continuously mutating calls decrease from 29,253 and
2,833,776 bytes to zero, on both paths. The counter excludes malloc, array/aligned
allocation and retained heap; it is not a whole-process memory measurement.
The optional `--projection-work-probe MODE --require-no-alloc` passes for all
four current paths. The old implementation intentionally fails only that
improvement assertion; its ordinary behavior guard passes.

Actual isolated gcov probes subtract setup from setup-plus-1,000-calls. Both
implementations make exactly the same `ItemDb::find` calls: 864 during setup,
then 144,000 additional lookups for unchanged contents or 160,128 for continuously
mutated contents, for both pointer and draw paths. Only archived ItemDb.cpp is
instrumented after the clean Release timing run. Classification still runs
freshly; the measurements do not justify adding cache invalidation complexity.

Data: `/tmp/openkal-r12-{baseline,current}-release-benchmark.log` and
`/tmp/openkal-r12-work/{timing-comparison,work-comparison,current-counts,release-compile-commands}.json`.
The baseline counts are in `/tmp/openkal-r12-work/baseline/old-counts.json`;
the expected improvement-test failure is in
`/tmp/openkal-r12-work/baseline-noalloc-negative-control.log`.

The scalar-new macro is source-specific to the driver. A before/after comparison
of all 861 Debug compile commands finds exactly that one added definition;
the other 860 commands are byte-identical and R11 shared-object contexts remain
unchanged. Evidence: `/tmp/openkal-r12-work/compile-context-proof.json`.
Independent source/test review reads the entire reported graph parser gap in
the test driver directly, and verifies the Inventory bytes outside this function
are unchanged. Formatting and diff checks pass.

Final full validation passes all 153 enabled Debug/null/Qt CTests (29.28 seconds
with four jobs), with 17 optional-data tests disabled. All profiles are cleared
before this run. The fresh fingerprinted coverage report is valid with no
missing instrumentation: R12 executes 90/90 lines and 133/142 raw branch arcs;
the nine unexecuted arcs are reported exception arcs. Raw gaps remain visible
without exclusions, in accordance with the relaxed coverage requirement.
Logs: `/tmp/openkal-r12-debug-build.log`,
`/tmp/openkal-r12-focused-test.log`, `/tmp/openkal-r12-release-build.log`,
`/tmp/openkal-r12-ctest-final.log` and `/tmp/openkal-r12-coverage-final.log`.

## Final focused sanitizer validation

A separate Debug/null build in `/tmp/openkal-optimization-sanitize` executes
12 focused guards with AddressSanitizer, UndefinedBehaviorSanitizer and leak
detection. All pass (17.31 seconds), with no sanitizer findings or suppressions.
The set covers protocol/FrameDrain, Host calculation/request/interest behavior,
world occupancy, output queues/backpressure, POSIX transport seams, real and
scripted TLS, immutable selector membership and Inventory projection/interaction.
This is focused memory/undefined-behavior validation, not a full sanitized suite
or a claim of race freedom.

Configuration uses tests enabled, tools/PostgreSQL/coverage disabled and global
`CMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer`,
`CMAKE_CXX_FLAGS_DEBUG=-O0 -g1` and
`CMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined`. The existing
`OPENKAL_SANITIZE` helper is not invoked by the current targets, so setting that
option alone would not establish instrumentation; this build explicitly supplies
the flags. Actual relevant compile/link commands and ASan/UBSan runtime linkage
are checked. The build uses two jobs and tests run serially with
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. Twelve relevant source hashes
remain unchanged after execution; the authoritative coverage build is separate.

Reproduction commands, command audit, source/environment evidence and result:
`/tmp/openkal-optimization-sanitize/{reproduce.sh,sanitizer-command-audit.json,sanitizer-source-and-environment.json,sanitizer-result.json}`.
Logs: `/tmp/openkal-sanitize-{configure,build,ctest}.log`.
The final manifest check verifies all 23 unique scoped source fingerprints and
no missing/unmapped coverage sites:
`/tmp/openkal-r12-work/final-source-coverage-check.json`.
All twelve bounded packages are complete and remain local on the shared branch.

## Follow-up sequence

The next authorized work stays on this branch, with local commits after verified
steps: first mandatory control-flow braces across project-owned C++, then
item-picker model/view rendering, ordered Host expiry/regeneration/transform
responsibilities, and an integrated multiplayer regression/load harness.

The brace rule is recorded in `AGENTS.md` for `if`/`else`, ordinary/range `for`,
`while` and `do` bodies, including empty and single-statement bodies. Ordinary
`else if` chains remain allowed. The pinned clang-format 18.1.8 hook inserts
braces without reordering includes or rewriting comments/string literals.
Clang-tidy independently treats `readability-braces-around-statements` as an
error, including empty bodies that the formatter intentionally leaves unchanged.
CI runs the formatter and compiler-aware check; the developer commands are in
README section 12.

The migration enumerates all 851 tracked project-owned C++ files; the single
vendored `third_party/miniaudio.h` is excluded. Of these, 628 files remain
byte-identical and 223 receive 4,012 opening/closing brace pairs plus formatting.
An anchored Clang raw-token subsequence check preserves every token,
literal and comment (normalizing comment whitespace only). All 2,753
preprocessor directives in changed files retain their token sequences and macro
name/parameter boundaries. All 851 files are formatter-idempotent. Evidence:
`/tmp/openkal-braces-review/{preview.json,token-proof-repository.json,idempotence.json}`.
The graph coverage metadata was checked for every path in batches; all reported
parser gaps are read from the pinned baseline rather than treated as absent code.
Coverage source hashes and selected ranges are remapped through the mechanical
diff without exclusions.

Independent source AST comparison checks all 851 files, permitting only a new
compound wrapper at an existing control-body position. Two Qt `Q_EMIT` parser
artifacts are resolved by compiler AST and by masking only that known empty
macro in memory. All 4,012 new compounds retain the control/body/else
associations; no unbraced parsed controls remain. The source grammar still has
146 unchanged parser gaps across 67 files, including default arguments, va_arg,
Qt declarations and preprocessor-split expressions. Direct-source review and
17 before/after compiler AST cases cover high-risk Host/client/GL/Qt/test code
and the relevant GPERFTOOLS/wrap-new macro variants. Nine representative strict
clang-tidy cases pass with no project-owned diagnostic. This is complementary
evidence rather than a whole-program semantic proof. Details:
`/tmp/openkal-braces-review/independent-summary.json` and its referenced reports.

Both broad GCC builds pass: Debug/null with Qt test players/loot editor, and
Debug/OpenGL with all optional Qt tools and PostgreSQL support. The full null
suite passes all 153 enabled tests (29.90 seconds). The isolated Xvfb/software-GL
suite passes 159 tests (39.87 seconds); five PostgreSQL integration tests skip
because no DSN was supplied, and 17 optional-user-data tests remain disabled in
both variants. Skips/disabled cases are not counted as passes. The fresh coverage
report is valid with unchanged executable line/branch totals after the mechanical
range remap. Formatting, pre-commit configuration and diff checks pass.
Logs: `/tmp/openkal-braces-debug-{configure-final,build-final,ctest}.log`,
`/tmp/openkal-braces-all-tools-{configure,build}.log`,
`/tmp/openkal-braces-gl-ctest.log`, `/tmp/openkal-braces-coverage.log` and
`/tmp/openkal-braces-format-check.log`.

The same 12 focused ASan/UBSan guards are rebuilt against the converted sources
and all pass serially (17.95 seconds), with leak detection, halt-on-error and no
suppressions or reported findings. Relevant instrumentation/runtime linkage and
all 12 fresh source hashes are checked. Evidence:
`/tmp/openkal-braces-sanitize-result.json` and
`/tmp/openkal-braces-sanitize-{build,ctest,full-test}.log`.
The mandatory-braces migration is complete.

## Item-picker prerequisite — Stable selection across lifecycle changes

Before replacing the widgets, selection now has one explicit contract: retain
unique visible IDs in current display order, including valid preselection before
showing the dialog. Intermediate Qt selection signals during destructive rebuild
and restoration are guarded; the final result is collected once from the logical
active tab. Acceptance is idempotent while the dialog is accepted, and reopening
the same dialog permits a fresh selection and acceptance. Filtering, catalog
ordering, tile composition and grade reconstruction are unchanged.

The intentional improvement assertions first fail on the unchanged pre-fix
implementation in both normal and HiDPI modes. They reproduce lost preselection,
duplicate/lost survivors and selection erasure on repeated activation. New guards
also cover reload/deletion ordering, narrowing/widening, multi-selection across
hidden-window filter/tab changes, and reopening after acceptance. Existing
Ctrl/Shift, filtering, ordering and role-pixel guards remain. Negative evidence:
`/tmp/openkal-picker-selection-negative-{build,test}.log`.

Final focused guards pass in both modes (0.80 seconds), and all 153 enabled
Debug/null tests pass (29.93 seconds); 17 optional-data cases remain disabled.
The fresh source-fingerprinted report is valid: expanded R10 executes 406/412
lines and 435/686 raw arcs, retaining all gaps without exclusions. Independent
review, formatting and diff checks pass. Logs:
`/tmp/openkal-picker-selection-build-final.log`,
`/tmp/openkal-picker-selection-focused-final.log`,
`/tmp/openkal-picker-selection-ctest-final.log` and
`/tmp/openkal-picker-selection-coverage-final.log`.
This correctness prerequisite is committed separately before model/view work.

Additional baseline validation closes the five earlier PostgreSQL skips: all
five integration tests pass (80.94 seconds) against an owned temporary PostgreSQL
14.24 cluster with its own data directory and private local Unix socket. This
includes managed test-player integration, multiplayer, guild and management
backends. Existing backend/TLS/security gates are unchanged; no existing database
or user-supplied game data is used. Log: `/tmp/openkal-pg-baseline-ctest.log`.

## Item-picker follow-up — Lazy model/view presentation

The picker now uses `QListView` and an ID-only `QAbstractListModel` instead of
allocating a `QListWidgetItem` for every matching definition. Labels, tooltips
and composed icons are resolved on demand and cached only until the next explicit
refresh. A constant-size delegate keeps layout sizing independent of appearance
roles. The flat view and present grade views are reused; absent grade groups are
pruned rather than retained as a history cache. Selection restoration scans the
visible order once against a saved-ID set and retains the separately committed
selection contract.

Catalog construction, filters, class assignment and comparison bodies are
unchanged. Icon composition is moved without changing its token sequence other
than pointer/reference member access. The model owns IDs and its occupied-ID set,
borrows the dialog's existing store/icon dependencies, and retains no item
pointers. A catalog replacement before first appearance access therefore resolves
current data safely; already resolved roles remain stable until explicit refresh.
This lazy snapshot timing is deliberate and is covered separately from supported
reload-plus-refresh behavior.

The shared widget-independent guard passes against both corrected legacy widgets
(`0ee71604`) and the model/view version. Dedicated model tests cover reset signals,
persistent-index invalidation, role caching and catalog replacement/deletion before
first role access, invalid/foreign indexes and read-only flags. The delegate guard
verifies that sizing requests no roles. An owned 1,000-definition scrolling guard
also selects the tail through real viewport input and checks filtering/restoration.
Exact pinned Fusion/font renders have identical geometry and all pixels in eight
viewport images at each of DPR 1 and DPR 2, including occupied tiles, overlays,
selection, grade frames and the empty result. No masks or image tolerances are
used. These controlled fixtures do not claim equivalence for every platform style.
Evidence: `/tmp/openkal-picker-model-work/{production-source-proof,render-comparison}.json`.

The separate source-specific gcov probe delivers pending paint events and counts
actual icon-composition entries, including opening. At 1,000/10,000/50,000 owned
definitions, the new totals stay 60 for opening, 114 for opening plus Knight,
and 120 for opening plus search or 16-ID restoration. At 50,000 definitions the
corresponding legacy totals are 50,000, 62,500, 51,000 and 100,000. These are
cumulative per-process counts, not inferred object or paint counts. ID filtering,
verification and selection scans remain proportional to catalog/visible size;
no bound on every Qt data-role request is claimed. Raw diagnostic role counters
are retained without using them as an acceptance metric. Files:
`/tmp/openkal-picker-model-work/composition-counts-{old,current}.json` and
`counts-{old,current}/`.

The paired Release benchmark uses the same seven-fresh-dialog driver, GCC
`-O3 -DNDEBUG`, Qt 6.9.0, pinned Fusion/DPR1 environment and an otherwise idle
measurement window. All nine common compile commands match after normalizing
source/build roots; the only added unit is the list model. At 50,000 definitions,
median opening falls from 2,292.35 to 158.35 ms, returning to All from 246.94 to
11.82 ms, and restoring 16 selected IDs from 264.89 to 10.84 ms. Knight/Mage tabs
fall from 98.49/48.75 to 12.41/14.15 ms. Filtering to 1,000 matches falls from
89.07 to 28.26 ms; filtering to only ten matches is essentially unchanged/slightly
slower (27.68 to 28.28 ms), since the full catalog still needs scanning. Small
catalogs do not improve uniformly: the 1,000-definition Mage transition rises
from 3.08 to 5.59 ms. These local observations have no timing acceptance threshold;
seven-sample p95 is the observed maximum, not a stable tail-latency estimate.
Logs: `/tmp/openkal-picker-model-baseline/benchmark.log`,
`/tmp/openkal-picker-model-current/benchmark.log`, and
`/tmp/openkal-picker-model-work/{compile-command-proof,timing-comparison}.json`.

Final validation passes all 155 enabled Debug/null tests (29.97 seconds), with
17 optional-data cases disabled; all four focused guards also pass in Release,
OpenGL-tool and ASan/UBSan builds. The sanitizer run enables leak detection and
reports no findings (2.34 seconds), without suppressions. The fresh coverage
report is valid: expanded R10 executes 449/456 lines and 497/772 raw arcs,
including the new model; no exclusions are introduced. Independent bounded
review, pinned formatting and diff checks pass. Final logs:
`/tmp/openkal-picker-model-{debug,release,gl,sanitize}-frozen-build.log`,
`/tmp/openkal-picker-model-frozen-{ctest,coverage}.log`,
`/tmp/openkal-picker-model-{release,gl,sanitize}-frozen-ctest.log`.

## Host phase prerequisite — Deterministic scheduled-player regression baseline

Before extracting expiry, regeneration and transformation, the unchanged
coordinator accepts one internal Unix-seconds function pointer, defaulting to the
existing helper with the same system-clock conversion. Its invocation remains
once per Playing session after the phase guard. This describes that callback,
not all clock reads in downstream progression helpers. Reversing only this seam
restores the committed `135c2165` function byte-for-byte.

New owned fixtures record three recipients, complete relevant session/bag/form
state, deadlines, ordered wire bytes, SQLite audit operations and surviving item
rows, plus the next seeded RNG result. They cover Unix expiry boundaries, pet
before ride deletion, dead-player expiry/dismount, null/gated/failed persistence,
simultaneous regeneration sources, initialization/disable/full-resource paths,
repeat and late ticks, and transform level-up before rage/form exit. Food state
is also probed on a value copy and through subsequent calls. RNG assertions derive
expected consumption on the same runtime instead of hardcoding libc values.

The focused assertions pass, and two independent serial snapshot runs are
byte-identical without normalizing output. All 155 enabled Debug/null tests pass
(29.66 seconds); 17 optional-data cases remain disabled. The fresh report is
valid: new R13 covers 148/148 executable lines and 214/294 raw arcs, including
66 exception arcs, with no exclusions. Independent source review and formatting
checks precede extraction. The tests and clock seam are committed separately so
the baseline remains reviewable and reproducible.

Commands: `openkal_test_optimization_host_coverage --scheduled-player-tests` and
`--scheduled-player-snapshot`. Evidence:
`/tmp/openkal-host-phase-baseline-{build,ctest,coverage}-final.log`,
`/tmp/openkal-host-phase-baseline-snapshot-{single,repeat}.log`,
`/tmp/openkal-host-phase-baseline-source-audit.json`, and
`/tmp/openkal-host-phase-review/{baseline-proof,baseline-coverage}.json`.

## Host follow-up — Ordered scheduled-player phases

The 220-line scheduled-player function is now a 20-line coordinator. Three
internal helpers separate pet/ride expiry, ordered resource regeneration, and
transform EXP/rage. The Playing guard, single Unix-seconds callback, expiry before
dead-player dismount/return, and resource-before-transform order remain explicit
in the coordinator. Thread ownership, polling, tick scheduling and other Host
source are unchanged; this is a complexity reduction, without a runtime speed claim.

The three moved blocks preserve all 515/970/213 tokens. An independent
normalized-source reconstruction also preserves the complete effect sequence,
with unchanged frozen tests and all source outside the region byte-identical to
`9e284485`. Only the inert initialization of a local bool crosses the Unix read
when moved into the expiry helper. This is a bounded source proof rather than a
whole-program semantic proof. The stronger runtime check also passes: all 32
complete per-peer state/packet/SQLite/RNG observations match the committed baseline
byte-for-byte, SHA256
`2651db2a1f46e6e868a9b86f7dfb885de04e44d0962ffe976fd63461ce03656b`.
Proof: `/tmp/openkal-host-phase-extraction-audit.{py,json}`,
`/tmp/openkal-host-phase-review/independent-extraction-review.{py,json}`, and
`/tmp/openkal-host-phase-current-snapshot.log`.

All 155 enabled Debug/null regressions pass (30.35 seconds), with 17 optional-data
cases disabled. The complete Host guard passes under ASan/UBSan with leak
detection and no suppressions/findings (1.90 seconds). Fresh coverage is valid:
R13 executes 157/157 lines and 207/280 raw arcs, including 59 exception arcs;
new helper entry/call locations remain included without exclusions. Raw totals
change with the compiler's function/exception layout and are not a speed metric.

The rebuilt OpenGL/all-tools variant also passes all 161 enabled non-PostgreSQL
tests on isolated Xvfb/software GL (30.25 seconds), and all five PostgreSQL/TLS
integration tests pass serially against the owned temporary cluster (82.52
seconds). The 17 optional-user-data cases remain disabled. Independent review,
pinned formatting and diff checks pass. Logs:
`/tmp/openkal-host-phase-{debug,gl,sanitize}-build.log`,
`/tmp/openkal-host-phase-gl-all-build.log`,
`/tmp/openkal-host-phase-{debug,gl,sanitize,postgres}-ctest.log`, and
`/tmp/openkal-host-phase-coverage.log`.

## Host follow-up — Multiplayer load and runtime measurements

An opt-in Host recorder now captures dispatch work, scheduled tick duration and
lateness, overruns, active/retiring connection counts and observed output queues.
Separate fixed 4,096-entry rings keep recording bounded without sample allocation.
Snapshots preserve chronological order and lifetime totals; configuration refuses
a running or unjoined worker. Collection is disabled by default. All initial
Host source lines remain an exact ordered subsequence of `0cde4824`; the four
additional clock reads execute only when measurements are enabled. Polling,
existing clock reads and tick scheduling remain unchanged. The default-disabled
32-case scheduled-player snapshot still matches the baseline byte-for-byte.

The new Linux harness uses real SQLite/Unix and PostgreSQL/TLS transports with
independently authored temporary catalogs and maps. Mixed movement, authoritative
talent responses, combat, guaranteed loot and stale/truncated requests execute
while the requested population remains connected. It also checks ordinary chat,
PostgreSQL pickup-race conservation/IID uniqueness, a slow reader with sustained
queued output while healthy peers and world ticks progress, portal/ACK, durable
exit and exact reconnect state. SQLite sessions share one persisted character;
PostgreSQL uses distinct accounts and characters. The harness preserves existing
queue limits, receive backpressure and command permissions. Its watchdogs are
coarse correctness guards; latency percentiles never determine test success.

All 157 enabled Debug/null tests pass (33.42 seconds), with 17 optional-user-data
cases disabled. All 163 enabled non-PostgreSQL OpenGL/all-tools tests pass on an
isolated Xvfb/software-GL display (33.83 seconds). The metrics, complete Host guard
and SQLite load cases also pass under ASan/UBSan with leak detection and no
suppressions/findings (3/3, 13.21 seconds). Independent metrics and mixed-workload
reviews, pinned formatting and diff checks pass.

All six PostgreSQL/TLS integration cases pass serially against the owned
temporary PostgreSQL 14.24 cluster (99.96 seconds), including the four-player
mixed workload. After the driver corrections, the final PostgreSQL load
CTest passes again (16.36 seconds). No optional user data, existing accounts or
system database instance is used.

Fresh coverage is valid. New R14 executes 121/123 lines and 54/80 raw arcs,
including 22 exception arcs, without exclusions. One uncovered line is recorder
cleanup attributed to its closing brace; the other observes a retiring connection
that no completed measured loop retained. The same output aggregation is covered
for active peers, and recorder unit tests cover retiring counts and simultaneous
peaks. This is not full retiring-connection integration coverage. Artificial
allocation/mutex failures are not injected. Existing R13 remains 157/157 lines
and 207/280 raw arcs. Raw results remain available in the generated report.

The first 64-player Release run exposed a test-driver flaw: healthy-client replies
kept aggregate pending output nonzero and prevented refilling a drained slow peer.
The pressure driver now separates each healthy FIFO round from a drain-only
interval before evaluating pending output and uses bounded adaptive read-only
talent requests. Production backpressure, deadlines and limits are unchanged.

A later 64-player Release run exposed a separate fixture assumption: scattered
loot can be outside the 64-pixel pickup radius even when the attack was legal.
One observed stationary attempt had squared distance 4,145, beyond the 4,096
limit. The receiver now retains each drop's wire coordinates and approaches its
exact ID with ordinary acknowledged movement while every peer continues draining.
A deterministic 96-pixel rejection case first checks unchanged wire inventory,
money, all 30 persisted item fields and the still-present world identity. Legal
approach then requires that exact removal. Final quantity, money, portal and
durable reconnect assertions remain unchanged. SQLite4, PostgreSQL4 and two
consecutive PostgreSQL/TLS64 Release runs pass this final driver before the full
matrix rerun. Production pickup rules and watchdogs are unchanged.

Reproduction and measurement semantics are documented in
`docs/MultiplayerLoadTesting.md`. Host samples describe completed main-loop work
over the whole synthetic scenario, excluding startup and final shutdown work
outside that loop. Barrier samples describe an all-peer round, not individual
packet latency. Queue peaks are sampled lower bounds; RSS combines Host, harness
and clients while excluding the external PostgreSQL process. These results do
not establish production throughput or statistically reliable tail latency.

Final idle-window Release/GCC 11.4/null PostgreSQL/TLS profiles all pass with the
same final driver, two warmup rounds and ten measured all-peer rounds. Observed
barrier p50/p95/p99 values are:

- 1 player: 1.089/1.106/1.106 ms; tick p99 0.410 ms; peak RSS 48,060 KiB.
- 16 players: 5.433/5.455/5.455 ms; tick p99 0.444 ms; peak RSS 48,136 KiB.
- 64 players: 45.327/47.766/47.766 ms; tick p99 0.650 ms; peak RSS 48,440 KiB.

Full-scenario scheduling-lateness p99 is 9.634/468.926/464.957 ms respectively;
these samples include synchronous provisioning/admission, not just gameplay.
The 64-player run retains 868 tick samples and the latest 4,096 dispatch samples
from 6,839 completed loops, observes exactly 64 simultaneous connections, and
reports zero measured tick overruns. Observed per-peer application-queue peaks
are 791/49,152/49,152 bytes. These are descriptive synthetic results with ten
barrier samples, not a capacity or latency guarantee. The final driver also has
two preceding consecutive successful 64-player Release runs.

Evidence: `/tmp/openkal-load-pickup-{debug,gl,release,sanitize}-build.log`,
`/tmp/openkal-load-pickup-{debug,sanitize,postgres}-ctest.log`,
`/tmp/openkal-load-final-gl-ctest.log`,
`/tmp/openkal-load-pickup-coverage.log`,
`/tmp/openkal-load-release-{1,16,64}.{json,log}`,
`/tmp/openkal-load-release-populations.json`, and
`/tmp/openkal-load-harness-{adaptive,pickup}/results.json`.
The failed initial drivers are preserved in
`/tmp/openkal-load-release-initial` and `/tmp/openkal-load-release-pve-failure`.

## Remaining H1 prerequisite — Pinned upkeep baseline

The authorized continuation is bounded by
[RemainingRefactoringPlan.md](RemainingRefactoringPlan.md): H1–H4, C1–C7, then
H5–H9. Conditional cache/index proposals have a recorded measurement-based
disposition. This first prerequisite changes tests and documentation only;
production Host remains byte-identical to `6da506b7`, SHA256
`ff784f825b81048bf6a4c241a32c2774890bd24119e3b439cd723a63e315c6eb`.

A build-directory adapter mechanically lifts the three exact old collection,
autopick and autosave statement ranges. The generator checks the complete pinned
source, unique anchors, declared member-reference bindings and reverse-token
identity (148/316/54 tokens). It preserves the GPL header and needs no Git history.
Only the collection wrapper's lexical clock binding is controlled; downstream
profile/persistence clocks keep their existing reads. The same fixture driver
will call real helpers after extraction, without a maintained formula clone or
whole-session oracle copy.

New fixtures provide 45 deterministic full-state observations and eight explicitly
labeled real-clock assertion observations. Successful persistence timestamps and
the separate profile guard use before/after bounds rather than fabricated exact
times. Three serial combined-output captures, retaining expected injected-failure
logs, are byte-identical: 2,830,657 bytes, SHA256
`07276af68e8633a9e8fd0ac92f5af1c1ff138c01a90dfabf4d8f14f827e7e065`.
Existing Host test changes are insertions only. Collection order/expiry/profile,
pickup ownership/range/quota/order/failures/IIDs/RNG and autosave scheduling/store
failure behavior are characterized before moving production statements.

All 157 enabled Debug tests pass (33.94 seconds), with 17 optional-data cases
disabled. The unchanged real `talent_tree_server` scheduler witness passes here
and in the GL/PostgreSQL build; that witness uses ordinary drops and the first
five-second scheduled pickup pulse, then checks persisted quantity six. It directly
witnesses autopick reachability; collection/autosave boundaries use the pinned
adapter and the later coordinator-source reconstruction. The focused GL Host plus
talent checks pass (2/2, 9.75 seconds); the complete Host guard passes under
ASan/UBSan/leak detection without suppressions (2.85 seconds).

Fresh coverage is valid. R15 records actual production sites at 37/48
lines and 48/98 raw arcs, including 12 exception arcs. Executed generated-adapter
lines are not attributed to those production locations. The stronger boundary
characterization is separate evidence; it is not a claim of full old-thread
coverage. Independent generator/fixture review, changed-source rejection,
source-only generation, pinned formatting and diff checks pass.

Evidence: `/tmp/openkal-h1-baseline/{baseline-report,parent-report,source-proof,generator-validation}.json`,
`snapshot-{single,repeat,parent}.log`, `debug-full-ctest.log`,
`{gl,sanitize}-parent-ctest.log` and `parent-coverage.log` in the same directory.

## Remaining H1 — Explicit collection, pickup and autosave phases

Extracted `tick_collection_expiry_and_profile`, `tick_talent_autopick` and
`autosave_player_if_due` into narrow file-local helpers. The Playing gate,
revival, resource/transform coordinator and intervening maintenance remain in
their previous order. The pickup candidate vector still dies before its lock;
no additional allocation, cached clock or scheduler abstraction was introduced.
The collection helper defaults to the initial `unix_seconds_now`; downstream
profile/persistence clock reads are unchanged.

The fixture and generator bytes from `e65bcb56` remain frozen. The temporary
baseline-build bridge is removed; the same scenarios now call actual helpers.
Both merged-stream captures exactly match the old 2,830,657-byte snapshot,
SHA256 `07276af68e8633a9e8fd0ac92f5af1c1ff138c01a90dfabf4d8f14f827e7e065`.
The 45 deterministic and eight explicitly bracketed real-clock observations
retain their distinction. Independent source reconstruction restores
the entire old Host byte-for-byte after reversing only the declared parameter
bindings and collection clock name (148/316/54 effect tokens).
Only trivial closure/bool/pickup-count lifetimes now end at helper return;
no escaped reference or observable destructor changes. Independent review is clear.

All 157 enabled Debug tests pass (33.72 seconds), with the same 17 optional-data
cases disabled. Fresh R15 coverage is valid: 56/56 executable lines and 84/96 raw arcs.
The uncovered arcs include 11 compiler exception arcs and one collection-slot
expiry alternative; no exclusions or generated-body attribution are applied.
All 163 enabled GL/Qt non-PostgreSQL tests pass (32.94 seconds); the Host
guard passes under ASan/UBSan/leak detection without suppressions (2.89 seconds).
All six configured PostgreSQL/TLS integration tests, including the mixed load
smoke, pass (96.57 seconds). A final parent snapshot capture again matches
the frozen baseline exactly. This extraction makes responsibilities
explicit; it does not claim a measured throughput improvement.

Evidence: `/tmp/openkal-h1-extraction/{result,source-audit}.json`,
`snapshot-{single,repeat}.log` in the same directory, and
`/tmp/openkal-h1-final-{debug,gl,sanitize}-build.log`,
`/tmp/openkal-h1-final-debug-ctest.log`,
`/tmp/openkal-h1-final-gl-ctest.log`,
`/tmp/openkal-h1-final-{sanitize,postgres}-ctest.log`,
`/tmp/openkal-h1-final-coverage.log` and
`/tmp/openkal-h1-extraction/parent-report.json`.

## Remaining H2 prerequisite — Account-storage characterization

Production Host is pinned to `798d001c`, SHA256
`a870ce5111a1cd4a7d053537600b741d8de11d06c8603bc092dd6b092f62ead5`.
The three old request bodies are lifted mechanically without member or clock
substitution: 123/844/652 tokens. A single-iteration adapter preserves all 20
frame-loop continues and the withdrawal search's inner break. Source-only
creation, GPL-header retention and changed-source rejection are verified; no
normal build depends on Git history.

The direct driver records all 30 item and 40 character fields, bag state, keeper,
IIDs, money, packets and ordered committed SQL effects. It pins selection and
parsing, quantity/fee/destination rules, full/partial transfers, metadata,
independent account ownership and failed insert/update/compensating-delete/save
behavior. SQL AFTER triggers describe committed effects, not rejected API attempts.
Successful autosave timestamps are explicitly bounded real-clock observations.
Selected-body dead/teleport/activity cases do not claim to bypass outer phase or
trade gates. Existing H1 fixtures remain unchanged; Host-test edits are insertions.

Two complete combined-output captures match: 6,750,540 bytes, SHA256
`9ab0c44dd31bdffc8d8de9f4b6fd718140355e7d010955ffd6878a27dc8ddbc4`.
Snapshot mode explicitly sets logging to WARN before fixture creation, avoiding
PID-dependent informational paths while retaining raw state, rows, bytes and
all warnings/errors. Normal tests retain their logging configuration. No capture
is post-filtered. The existing 215-entry storage reply versus empty 216-entry
reply at the 8,000-byte frame limit is characterized, not corrected here.

The new `storage_server` real Unix/SQLite witness passes (0.21 seconds), as does
`storage_server_postgres` with distinct authenticated PostgreSQL/TLS accounts
(1.31 seconds). They check transfers/fees/metadata, unauthorized foreign IIDs,
NPC authorization, successful save acknowledgement and actual gameplay reconnect.
Zero money on login remains implicit: assertions require the durable zero and
zero received money frames; positive balances require the actual update. An
initial contrary test assumption and a direct-fixture directory-lifetime clash
were corrected against unchanged source before freezing expectations.

Independent review is clear. The Host and storage guards pass under
ASan/UBSan/leak detection without suppressions (2/2, 10.86 seconds); GL Host,
storage and protocol guards pass (3/3, 7.70 seconds). All 158 enabled Debug tests pass (38.76 seconds), with 17 optional-data cases
disabled. The final parent capture matches both frozen snapshots exactly.
Fresh R16 coverage is valid: 259/289 production-site lines and 227/412 raw arcs,
including 88 exception arcs. The source-derived failure replay remains separate
evidence; its generated-body execution is not attributed to production request
lines. The subsequent implementation will run that same driver against actual
helpers. No exclusions or artificial full-coverage claim are applied.

Evidence: `/tmp/openkal-h2-baseline/{baseline-report,parent-report,source-proof,unchanged-source,generator-validation}.json`,
`snapshot-{single,repeat,parent}.log`, `{debug-full,gl-parent,sanitize-parent}-ctest.log`,
`parent-coverage.log` and `parent-generator/validation.json` in the same directory;
`/tmp/openkal-h2-transport-{sqlite,postgres}.log` and
`/tmp/openkal-h2-independent-review.json`.

## Remaining H2 — Explicit account-storage handlers

Extracted info, deposit and withdrawal into three file-local handlers with
borrowed dependencies. Host retains the Playing/opcode gates and frame-loop
policy. The return value describes branch-end versus continue flow,
including successful-looking output after an ignored save failure; it is not a
transaction-success flag. The 20 converted outer continues and the one retained
withdrawal search break are independently verified. No new clock, state owner,
allocation, validation rule or transaction behavior is introduced.

The baseline driver and both actual-transport guards from `267fafe2` are byte-
identical. Only the temporary 16-line generator build bridge is removed; the
same direct scenarios now execute the real handlers. Two raw storage captures
match the complete frozen 6,750,540-byte reference, and the H1 snapshot remains
identical. Independent reconstruction restores the entire old Host byte-for-byte
under explicitly audited parameter bindings and continue/return conversion
(123/844/652 body tokens). Other request, auth/trade and cleanup code
is unchanged. Bag pointers do not escape and source is not read after compaction.

All 158 enabled Debug tests pass (38.88 seconds), as do all 164 enabled GL/Qt
non-PostgreSQL tests (36.60 seconds). Host and real SQLite storage guards pass
ASan/UBSan/leak detection without suppressions (2/2, 10.94 seconds). The affected
real PostgreSQL/TLS storage, existing multiplayer/store contract and mixed load
smoke pass (3/3, 25.60 seconds). Optional-data cases remain disabled.

Fresh R16 coverage is valid: 295/301 lines and 290/422 raw arcs, including 91
exception arcs. Every executable line in the three changed handlers is covered.
The six remaining lines belong to the unchanged stored-row filter and storage
serializer's defensive paths; they remain visible. All eight previous Host
coverage sites retain exactly their old selected source lines; R08 is split
around the insertion to avoid attributing H2 helpers to the earlier scope.
This is a responsibility extraction, not a measured performance improvement.

Evidence: `/tmp/openkal-h2-extraction/{report,parent-report,source-audit,independent-review}.json`,
`snapshot-{single,repeat,parent}.log` in the same directory, and
`/tmp/openkal-h2-final-{debug,gl,sanitize,postgres}-ctest.log`,
`/tmp/openkal-h2-final-{debug,gl,sanitize}-build.log` and
`/tmp/openkal-h2-final-coverage.log`.

## Remaining H3 prerequisite — NPC-shop characterization

Production Host remains byte-identical to `cbbda99b`, SHA256
`5a6e64218e64cfb7b3d9f787d5961d74414eff551ad229eee71d18193f437b0f`.
All three shop bodies match the preceding source exactly before the deliberate
full-file repin. The checked-source generator lifts 338/580/590 tokens without
body substitution. Its list wrapper retains the inner offer-filter continue;
outer continues keep their frame-loop disposition. Changed-source rejection is
verified and ordinary CI needs no Git history.

The frozen driver has 76 cases for ordered ordinary/skin offers, context/parsing,
quantity/capacity/pricing/binding rules, all item metadata, IID/RNG consumption,
partial/full sale, independent sessions and ignored persistence failures. The
reachable partial result is durable row insertion; valid in-memory insertion
remains all-or-none. Positive int prices times valid quantities cannot overflow
int64; money-add overflow is tested without inventing impossible inputs.
SQL audits retain their committed-effect limits. Successful autosave uses
explicit measured clock bounds, not a fabricated deterministic timestamp.

Three final raw combined captures match: 4,528,388 bytes, SHA256
`e059ddf2a4cc7ebbc3ec6ec53782c090c4bcdce7432d33698cc5bee4d20fbbac`.
Snapshot CLI logging is explicitly WARN; expected errors and every observation
are retained without postcapture filtering. Removing only the H3 insertion
blocks restores the previous Host test bytes exactly. Existing H1/H2 generators
and storage-transport cases remain untouched.

The existing real Unix `starter_kit` witness now checks successful partial/full
sales, money, saved acknowledgements and reconnect while preserving its initial
starter-once/bound-item assertions. Together with the unchanged `skin_shop_server`
and complete Host guard, it passes. Independent review is clear.
All 158 enabled Debug tests pass (44.84 seconds), with 17 optional-data cases
disabled. The GL Host/starter/skin/protocol guards pass (4/4, 14.01 seconds), as
do Host and starter under ASan/UBSan/leak detection without suppressions
(2/2, 18.17 seconds). R17 coverage measured before the test-only clock correction
is valid: 350/376 lines and 386/612
raw arcs, including 119 exception arcs. Generated old-body execution is separate
evidence and is not attributed to production request lines.

An earlier parent capture found a timing-dependent conditional PASS diagnostic
when two successful saves share a millisecond; state/rows/packets matched. The
final driver asserts every invocation: successful character saves use inclusive
measured bounds, while rejects, skin exchanges and injected character-save
failures preserve the prior timestamp. Independent review and all tests above
use these final bytes. The failed capture and initial sandbox socket denial are
preserved; no production timing change or postcapture filtering was used.

Evidence: `/tmp/openkal-h3-baseline/{repin-source-proof,clock-fix-preservation,generator-validation,snapshot-clock-fix-equality,parent-clock-fix-snapshot,parent-report}.json`,
`snapshot-clock-fix-{first,second,parent}.log`, `{debug,gl,sanitize}-clock-fix-ctest.log`,
`parent-coverage.log`, `coverage.json` and
`/tmp/openkal-h3-independent-review.json`.

## Remaining H3 — NPC-shop request handlers

The ItemList, BuyItem and SellItem bodies now have three file-local helpers with
explicit borrowed dependencies. Their return value preserves previous frame-loop
continue versus branch-end flow; it does not promise transaction success. The
ordinary shop and skin exchange keep their different restrictions and persistence
behavior. Fifteen outer continues become false returns; the inner offer-filter
continue remains inside its loop. Phase/opcode gates stay in thread_main.

The driver, generators and actual transport fixtures from `718bbc9d` remain
byte-identical. Two worker captures and an independent parent capture exactly
match the frozen 76-case, 4,528,388-byte baseline, SHA256
`e059ddf2a4cc7ebbc3ec6ec53782c090c4bcdce7432d33698cc5bee4d20fbbac`.
The older H1 and H2 snapshots also remain exact. Independent normalization of
338/580/590 tokens reconstructs the entire old Host byte-for-byte;
actual repository review is clear. No cache, scheduler, transaction, clock, RNG
or presentation change is included. No performance speedup is claimed.

All 158 enabled Debug tests pass (44.18 seconds), as do all 164 non-PostgreSQL
GL tests (42.02 seconds); 17 optional-data tests remain disabled in each build.
Host and the actual Unix starter/shop witness pass under ASan/UBSan/leak detection
without suppressions (2/2, 18.14 seconds). Actual PostgreSQL/TLS multiplayer and
mixed-load regressions pass (2/2, 24.27 seconds); these do not claim to replay
shop requests on PostgreSQL.

After clearing Debug profiles, the fresh valid R17 report covers 372/386 lines
and 438/626 raw arcs, including 122 exception arcs. The only uncovered line in
the three new request helpers is the int64 purchase-multiplication guard:
positive int prices and valid quantities up to 10,000 cannot reach it. Remaining
shared-helper and protocol gaps remain visible, including defensive paths,
collection-stat initialization and compiler closing-brace accounting. No coverage
exclusions were added. The selected source of all nine earlier Host areas is
byte-exact after range mapping; new handlers are credited only to their own area.

Evidence: `/tmp/openkal-h3-extraction/{production-report,production-source-audit,independent-review,frozen-fixtures,parent-snapshot,scope-preservation,coverage,parent-report}.json`,
`/tmp/openkal-h3-final-{debug,gl,sanitize,postgres}-ctest.log`, matching build logs
and `/tmp/openkal-h3-final-coverage.log`.

## Remaining H4 prerequisite — NPC, quest and activity routing

Production Host remains byte-identical to `1be692c6`, SHA256
`d62a90403673f0d805efdca79665c1cf8a126a76acf3514b1d730c1e2b863fe7`.
A checked-source generator lifts only the seven selected boundaries, without a
maintained copy of the complete four-route coordinator. Independent reversal
verifies 215/213/41/92/200/260/44 tokens and ten outer continuation
outcomes. Source-only generation, GPL retention and changed-source rejection
pass; normal checkout builds require no Git history.

The new driver adds 79 observations for dialog context, portal confirmation,
real quest processing, donation validation/consumption, cooking and cancellation.
It records complete relevant state, all item/character fields, committed SQL effects
and ordered owner/observer frames. Fresh fishing/cooking clocks use measured bounds
and disclosed markers. Three raw merged captures are byte-identical: 4,683,045 bytes,
SHA256 `fa85c5e94b93f8fe5ae9c07cff15676da58655175752611a3976bf5a46f9eb01`.
The prior H1/H2/H3 snapshots remain exact; removing only the new insertions restores
the old Host test bytes.

One new real SQLite/Unix case extends ConsumableCraftingServerTest while preserving
its initial 15 cases and complete run_case body. Fresh saved=true barriers verify
HTML/quest/crafting precedence, accepted trailing requests, portal confirmation
through malformed/missing Asks, authoritative teleport ACK, EXP donation, cooking/
fishing start and quit, durable reauthentication and a fresh Host without a quest
engine. Independent source review is clear and the complete target passes (2.71
seconds). Its first run exposed an incorrect empty-quest assumption: login grants
9001/1/0. The final test explicitly requires that initial singleton and the exact
three additional quest flags through both profiles; no rows are ignored. Initial
compile/fixture failures are preserved, with no production workaround.

All 158 enabled Debug tests pass (51.86 seconds), with 17 optional-data tests disabled.
The GL Host guard passes (21.46 seconds), plus seven actual-routing/domain/protocol
guards (2.66 seconds). Separate Host and real-routing runs pass ASan/UBSan/leak
checks without suppressions (27.84 and 3.63 seconds). Fresh R18 coverage is valid:
321/409 lines and 340/636 raw arcs, including 159 exception arcs. Generated old-body
execution is separate evidence and is not credited to production request lines.

Retained ritual activation and request-specific EXP settlement remain explicitly
source/domain evidence, rather than claimed socket coverage. SQL AFTER audits show
committed effects, not rejected attempts. The broad unchanged quest-sink callbacks
and defensive branches retain visible raw coverage gaps. No raw 100-percent claim,
new clock API, coordinator clone or transaction fix is included.

Evidence: `/tmp/openkal-h4-baseline/{final-handoff,test-preservation,generator-validation,snapshot-equality,parent-snapshot,coverage,parent-report}.json`,
`/tmp/openkal-h4-independent-review.{md,json}`, the transport reviews under
`/tmp/openkal-h4-{baseline/transport-independent-review,transport-independent-review}.json`,
and `/tmp/openkal-h4-baseline/{debug-full,gl-host,gl-domains,sanitize-host,sanitize-transport}-ctest.log`.

## Remaining H4 — NPC, quest and activity helpers

Seven file-local helpers now select dialog NPCs, handle portal confirmation,
process a quest, validate and collect EXP donations, start medicine cooking and
cancel activities. Dependencies remain explicit borrowed objects; the donation
result owns only scalar values. Ritual routing, the lazy Unix read and EXP
settlement, the complete announce-versus-progress presentation and all outer
gates/continuations remain visible in thread_main. No general context or handler
framework is introduced.

The `b9c5f883` driver, actual transport case and generators remain byte-identical.
Two worker captures and an independent parent capture exactly match the frozen
79-observation, 4,683,045-byte stream, SHA256
`fa85c5e94b93f8fe5ae9c07cff15676da58655175752611a3976bf5a46f9eb01`.
H1/H2/H3 snapshots also remain exact. Independent reversal reconstructs the complete
old Host after the declared bindings and ten outer outcomes, retaining all seven
215/213/41/92/200/260/44 token bodies and three quest-call bindings. Actual repository
review is clear. Only trivial donation-snapshot lifetime and the quest sink's
immediately preceding branch-end destruction move to helper return.

All 158 enabled Debug tests pass (51.80 seconds), as do all 164 non-PostgreSQL GL
tests (49.56 seconds); 17 optional-data tests remain disabled in each build.
Host and actual SQLite/Unix routing pass ASan/UBSan/leak checks without suppressions
(2/2, 31.54 seconds). Actual PostgreSQL/TLS multiplayer and mixed-load regressions
pass (2/2, 24.27 seconds), without claiming PostgreSQL NPC-request replay.

Fresh R18 coverage is valid: 359/436 lines and 378/646 raw arcs, including
159 exception arcs. The seven helpers have two uncovered defensive donation
removal-failure lines; a normal unchanged bag has already been counted and SQL
write failures intentionally retain the live-removal success. Unchanged
quest callbacks, retained ritual/settlement and some outer rejection continuations
retain visible gaps. These are not all unreachable; detailed limits remain explicit.
The selected source of all ten earlier Host areas is byte-exact after remapping.
No exclusions, gameplay/transaction changes or performance claims are included.

Evidence: `/tmp/openkal-h4-extraction/{final-handoff,source-audit-actual,scope-preservation,parent-snapshot,coverage,coverage-limits,parent-report}.json`,
`/tmp/openkal-h4-actual-independent-review.json`,
`/tmp/openkal-h4-final-{debug,gl,sanitize,postgres}-ctest.log` and matching build/coverage logs.

## Remaining C1 prerequisite — online client packets

Production NetClient.cpp and its header remain unchanged, SHA256
`9691c38d671c9c58f540d05225309f19770b182ce71ec3f250f5c2b4fc36beeb` and
`3238d4d23d74a77c2a91c507857f32b141726ace71b376d32508a904b1eed7a6`.
The new public BufferedSession driver covers nine online packet groups without
private access, a decoder clone, live server or database. Typed observations include
all exposed trade/market/party data, guild views/displays, options, serials and
complete bounded FIFO drains. It pins malformed close-versus-ignore behavior,
valid suffix handling and cleanup, all pickup truncations, repeated receipts,
legacy market packets, high u64 identities, dynamic party filtering and two-client
isolation. Receipt-only frames explicitly produce no inventory insertion.

Three raw merged captures are byte-identical: 108,906 bytes, SHA256
`1fd173f88df2bce01a661a47082198b3a5c17f46007ce4ef967d311fa406f49a`,
with 82 valid, unique JSON checkpoints each. The first successful-assertion capture
had incidental INFO output interrupting a JSON line; it is preserved. Snapshot mode
now sets WARN before execution, retaining warnings/errors and all raw observations,
without postcapture filtering. Normal/default logging remains unchanged. Removing
only declared insertions restores the entire previous test file byte-for-byte.
Independent draft and actual-source reviews are clear.

All 13 focused Debug guards pass (24.35 seconds). Parent verification passes all
158 enabled Debug tests (42.40 seconds), 14 GL client/protocol/presentation/UI guards
(25.00 seconds) and the client guard under ASan/UBSan/leak detection without
suppressions (3.29 seconds). The 17 optional-data Debug tests remain disabled.
Fresh R19 coverage is valid: 338/349 lines and 542/800 raw arcs, including 160
exception arcs. Gaps remain visible; no exclusions or raw 100-percent claim.
The baseline serializes C1's public state and queues, not all private client state.

Evidence: `/tmp/openkal-c1-baseline/{handoff,source-preservation,snapshot-equality,parent-snapshot,coverage,parent-report}.json`,
`HANDOFF.md`, raw `snapshot-{first,repeat,parent}.log`,
`{debug-full,gl-focused,sanitize-client}-ctest.log`, and
`/tmp/openkal-c1-host-actual-review.json`.

## Remaining C1 implementation — online packet dispatch

The private `handle_online_packet` now owns the nine characterized online packet
groups. Its boolean result denotes a recognized packet, including malformed
packets already handled by the branch. Ten outer continuations become
handled returns; the inner trade-item continuation remains unchanged. Framing,
payload reuse, invalid/incomplete drains, exceptions and the other packet groups
remain in `process_frames`. No state ownership, clock or protocol behavior changes.

The complete frozen test driver remains byte-identical. Two author captures and
the independent parent capture match all 82 reference observations exactly:
108,906 bytes, SHA256
`1fd173f88df2bce01a661a47082198b3a5c17f46007ce4ef967d311fa406f49a`.
Independent source reversal reconstructs the old implementation and header after
the declared movement and ten handled returns; actual-source review is clear.

All 158 enabled Debug tests pass (42.00 seconds), all 164 non-PostgreSQL GL tests
pass (39.02 seconds), and the client guard passes ASan/UBSan/leak detection without
suppressions (3.28 seconds). The 17 optional-data tests remain disabled in each
full build. This client-only change does not claim a new PostgreSQL routing run.

Fresh R19 coverage is valid: 340/351 lines and 544/800 raw arcs, including 159
exception arcs. Every executable line of the new packet helper is covered. Gaps
remain in unchanged item-name lookup and shared protocol parsers; these are not
all unreachable. No exclusions or raw 100-percent claim are introduced.
Earlier R01/R08 selected source is byte-exact after remapping. R05 preserves its
queue-cap statements with four-space dedentation and one outer continuation
converted to a handled return; that normalization is explicit rather than a
byte-exact claim. No performance improvement is claimed for this extraction.

Evidence: `/tmp/openkal-c1-extraction/{final-handoff,source-audit,scope-preservation,parent-snapshot,coverage,parent-report}.json`,
`/tmp/openkal-c1-extraction-independent-review.actual.json`, and
`/tmp/openkal-c1-final-{debug,gl,sanitize}-ctest.log` with matching build logs.

## Remaining C2 prerequisite — inventory and item-service packets

Production remains at C1 commit `40bdea6f`, with NetClient.cpp SHA256
`e97af6ce278e1712957bf21c0782fd2bc35a2a4129a3f8608b198b59944802ca`.
The public BufferedSession driver now covers the nine inventory/equipment/service
groups: all InsertItem prefix boundaries and optional metadata, independent mail
dirty/claim state, immediate animal fields versus FIFO events, wear/trash tolerance,
strict riding/cache/shop/storage replies, cleanup and two-client isolation.
No private access, decoder clone, production change or user-supplied asset is used.

The frozen test SHA256 is
`7e7c59a32114bf0d462665c6224c19130c7c5e07c62994009982e0a2cf9ff1e9`.
Removing declared insertions restores the complete previous driver byte-for-byte.
Three raw captures match: 128 observations, 88,826 bytes, SHA256
`4c13900489f01d46dfdc536088e40066b50ac7a4020fd2cbf744327897591e0b`.
The C1 reference also remains byte-exact. An initial passing 125-case
baseline was strengthened with three reachable short-header guards after coverage
review; every prior observation and assertion remains unchanged. Both evidence
sets and the independent actual-source reviews are preserved.

All 158 enabled Debug tests, 164 non-PostgreSQL GL tests and the client guard under
ASan/UBSan/leak detection pass. Each full build retains 17 disabled optional-data
tests. The focused run passes 14 enabled tests; optional `inventory_hud` is disabled
and is not counted as passing. Fresh R20 coverage is valid: 343/353 lines and
392/576 raw arcs, including 104 exception arcs. All selected NetClient handler,
consumer and inline getter lines are covered. Remaining helper read-failure guards
follow fixed-size checks; InsertCharm's early guard is bypassed by its caller's
minimum-size condition. These are packet-path limits, not claims about all direct
helper API inputs. No coverage exclusions are introduced.

The declared-count storage rejection does not claim an isolated full-capacity wire
test: 216 complete rows exceed the 8,000-byte frame limit. Animal added EXP is
unsigned; 65,535 is tested as positive. Existing close-versus-EOF retention is
characterized as it stands. No gameplay, UI or parser correction is bundled.

Evidence: `/tmp/openkal-c2-baseline/{handoff,source-preservation,header-guard-strengthening,snapshot-equality,coverage,parent-report}.json`,
`HANDOFF.md`, `pre-guard-strengthening/`, `parent-before-entry-guards/`,
`/tmp/openkal-c2-independent-review.actual-strengthened.json` and
`/tmp/openkal-c2-baseline-guard-parent-{debug,gl,sanitize}-ctest.log`.

## Remaining C2 implementation — inventory packet dispatch

The private `handle_inventory_packet` owns the nine characterized groups and ten
opcodes. Eleven outer frame continuations become handled returns. All 1,137
tokens, inner control flow, parser tolerances and immediate/FIFO effects
are preserved. The recognized return includes malformed ignored packets; foreign
opcodes return false. Framing, all other groups, state ownership and callers remain
unchanged. No allocations, clocks, validation policy or gameplay rules are added.

The committed baseline driver remains byte-identical. Two author captures and the
parent capture exactly match its 128-observation, 88,826-byte raw reference, SHA256
`4c13900489f01d46dfdc536088e40066b50ac7a4020fd2cbf744327897591e0b`.
The C1 raw reference is also exact. Independent AST selection and literal-preserving
token comparison reconstruct the entire old CPP/header, preserving all 34 remaining
groups and the complete C1 helper. The same audit passes against actual repository
source, whose fingerprints equal the reviewed drafts.

All 158 enabled Debug tests, 164 non-PostgreSQL GL tests and the client guard under
ASan/UBSan/leak detection pass. Both full profiles retain 17 disabled optional-data
tests. Fresh R20 coverage is valid: 337/347 lines and 395/580 raw arcs, including
105 exception arcs. Every selected NetClient executable line, including the entire
new helper, is covered. The line total changes with compiler attribution after
movement; no selected behavior or coverage exclusion is removed. The same ten
shared-helper defensive lines remain visible. All earlier selected client source
and the retained public-consumer source are byte-exact after scope remapping.
This is a maintainability extraction without a performance claim.

Evidence: `/tmp/openkal-c2-extraction/{final-handoff,source-audit-actual,scope-preservation,replay-results,coverage,parent-report}.json`,
`HANDOFF.md`, `/tmp/openkal-c2-extraction-independent-review.actual.json` and
`/tmp/openkal-c2-final-{debug,gl,sanitize}-ctest.log` with matching build logs.

## Remaining C3 prerequisite — quest, skill and shortcut packets

Production remains at tested C2 commit `b9b6fc53`. The public BufferedSession
baseline adds HTML processing-time correlation, successful/failed Ask sends,
first-duplicate quest replacement, independent retained quest latches, latest
learned/shortcut snapshots, separate SkillUp FIFO, malformed/tolerated-tail
neighbors, two-client isolation and existing close-versus-EOF retention.
It uses typed public observations, all 20 shortcut slots and bounded full drains,
without private access or a decoder clone. Four insertions restore every byte of
the initial/C1/C2 test driver. The frozen test SHA256 is
`f277e649823b57b4005e6e3b2ac74b3a64049e18b6dc80d6cb6bda3a7123f893`.

The first compile and C3-only run pass without expectation corrections. Two raw
captures and an independent parent capture match all 54 unique checkpoints:
43,489 bytes, SHA256
`61993549f42d507c3095f204ced69a8f0bd8ea4f2f6890e6278bd1ddcd7b864e`.
Both earlier C1/C2 raw references remain exact. Explicit record flushing and an
ordinary successful exit acknowledgement keep snapshot/teardown deterministic.
An initial stale test-object gcda timestamp diagnostic is retained; only that
outdated counter file was removed before capture. Fresh parent coverage resets
all counters and runs the complete suite. Independent proposal/actual reviews pass.

All seven focused guards, 158 enabled Debug tests, 164 non-PostgreSQL GL tests and
the client guard under ASan/UBSan/leak detection pass. The full profiles retain
17 disabled optional-data tests. Fresh R21 coverage is valid: 189/197 lines and
168/232 raw arcs, including 39 exception arcs. One wrapped SkillUp call is assigned
zero executions on its first source line while its continuation executes 66 times;
the raw GCC JSON and actual FIFO assertions retain that distinction. Seven other
read-failure lines follow parser size checks. Raw gaps remain visible; no coverage
exclusions or complete-private-state claim are introduced.

Evidence: `/tmp/openkal-c3-baseline/{handoff,source-preservation,snapshot-equality,coverage,parent-report}.json`,
`HANDOFF.md`, `gcov-line-diagnostic/skill-up-lines.json`,
`/tmp/openkal-c3-full-independent-review.actual.json` and
`/tmp/openkal-c3-baseline-parent-{debug,gl,sanitize}-ctest.log`.

## Remaining C3 implementation — quest packet dispatch

The private `handle_quest_packet` owns the six contiguous quest/learned-skill/
shortcut groups. Six outer continuations become handled returns; the inner
first-matching-quest loop and break remain exact. The initial 614 tokens, parser
tolerances, latches, FIFO behavior and HTML fallback are preserved. No state,
public API, validation or cleanup policy is added. Independent AST/token reversal
reconstructs the complete old CPP/header and verifies both earlier family helpers
and all drain callers. The audit passes against the applied reviewed source.

The frozen baseline test remains byte-identical. Two author captures and the
parent capture match its 54-observation, 43,489-byte raw stream exactly, SHA256
`61993549f42d507c3095f204ced69a8f0bd8ea4f2f6890e6278bd1ddcd7b864e`.
Both earlier C1/C2 raw references remain exact. All seven focused guards, 158
enabled Debug tests, 164 non-PostgreSQL GL tests and the client guard under
ASan/UBSan/leak detection pass. The full profiles retain 17 disabled optional-data
tests. Initial recompiled-object counter diagnostics are preserved; fresh parent
coverage resets all counters before the complete suite.

Fresh R21 coverage is valid: 186/193 lines and 171/236 raw arcs, including 40
exception arcs. All 95 selected NetClient executable lines, including the new
helper, are covered. Compiler attribution changes the line total; the former
split-call zero counter disappears after formatting, with no removed statements
or exclusions. The same seven parser read-failure lines remain visible. Earlier
selected client source and the retained public-consumer source remain byte-exact
after scope remapping. No performance improvement is claimed.

Evidence: `/tmp/openkal-c3-extraction/{handoff,actual-source-audit,scope-preservation,snapshot-equality,coverage,parent-report}.json`,
`HANDOFF.md`, `/tmp/openkal-c3-extraction-independent-review.actual.json` and
`/tmp/openkal-c3-final-{debug,gl,sanitize}-ctest.log` with matching build logs.

## Remaining C4 prerequisite — session, progression and time packets

Production remains at tested C3 commit `4bb47f56`. The public BufferedSession
baseline characterizes Code/exit acknowledgements, property destinations and
HP/MP deltas, EXP/level latches, clock/event snapshots, malformed neighbors,
buffered suffix processing and existing reconnect retention. Explicit stable
clock queries and brackets around real receive times avoid a production clock
seam or private-state access. Integer observations preserve their full precision.

The frozen test SHA256 is
`5ee8e58bce5b6e8d038e8796494dc29cbf61199d711f0160cf4c5823e080d95b`.
Declared insertions reconstruct the complete previous driver. Two author captures
and the parent capture match 121 unique observations: 98,292 bytes, SHA256
`e6edd3c8224c2b4b4c3c61a82ef9587373e2eee8f902117739f05fb5b999323b`.
Every earlier C1/C2/C3 raw reference remains exact. Coverage review added one
reachable excessive-clock-event-count case; all 120 preceding record bytes and
assertions remain unchanged. Independent full and narrow strengthening reviews
are clear. An initial unsynchronized-clock placeholder expectation was corrected
from the existing model contract; its failed run is retained.

All seven focused guards, 158 enabled Debug tests, 164 non-PostgreSQL GL tests and
the client guard under ASan/UBSan/leak detection pass. Both full profiles retain
17 disabled optional-data tests. Fresh R22 coverage is valid: 484/491 lines and
381/542 raw arcs, including 100 exception arcs. All six receiver bodies and all
ServerClock and ClockSync selected lines are covered. Seven remaining zero-count
lines include an empty combat-float guard, a closing-brace attribution and five
parser read-failure defenses after size checks. Other raw arc gaps remain visible;
they are not all classified as unreachable. No exclusions are introduced.

Evidence: `/tmp/openkal-c4-baseline/{handoff,source-preservation,snapshot-equality,coverage,parent-report}.json`,
`HANDOFF.md`, `pre-count-guard/`, `parent-before-count-guard/`, the independent
full/narrow reviews and `/tmp/openkal-c4-baseline-count-parent-{debug,gl,sanitize}-ctest.log`.

## Remaining C4 implementation — session and scalar packet dispatch

The private `handle_snapshot_packet` owns the six characterized session, clock,
event, map and property groups. Eight outer continuations become handled returns;
all 2,099 tokens, parser behavior, scalar/latch updates and queue ordering
are preserved. No state, public clock API, cleanup policy or allocation is added.
The exact synchronize expression still obtains its receive time lazily after a
successful parse. Invalid clocks retain close, buffered-suffix and caller cleanup
behavior. No nanosecond timing or performance improvement is claimed.

Independent AST/token reversal reconstructs the entire old CPP/header. The later
groups cross inventory/quest dispatch calls: their complete unchanged helpers are
proved to execute only disjoint opcode comparisons and false returns for these
six numeric opcode values. This is a bounded foreign-path proof; their recognized
paths remain stateful. All other crossed guards and all drain callers are checked.

The baseline test remains byte-identical. Two author captures and the parent
capture match all 121 observations and 98,292 raw bytes, SHA256
`e6edd3c8224c2b4b4c3c61a82ef9587373e2eee8f902117739f05fb5b999323b`.
All earlier C1/C2/C3 raw references remain exact. Seven focused guards, all 158
enabled Debug tests, all 164 non-PostgreSQL GL tests and the client guard under
ASan/UBSan/leak detection pass. The full profiles retain 17 disabled optional-data
tests. This client-only extraction does not claim another PostgreSQL routing run.

Fresh R22 coverage is valid: 482/489 lines and 384/546 raw arcs, including 101
exception arcs. Every executable line in the new helper is covered. The same
seven gaps remain in retained consumers/parser defenses; no exclusions or raw
100-percent claim are introduced. Earlier selected client source and retained
public-consumer source remain byte-exact after remapping. Formatting changes
compiler line attribution, with no removed behavior.

Evidence: `/tmp/openkal-c4-extraction/{implementation-handoff,source-audit-actual,scope-preservation,replay-results,coverage,parent-report}.json`,
`HANDOFF.md`, `/tmp/openkal-c4-extraction-independent-review.actual.json` and
`/tmp/openkal-c4-final-{debug,gl,sanitize}-ctest.log` with matching build logs.

## Remaining C5 prerequisite — world entities and movement

Production remains at tested C4 commit `03d59558`. The public BufferedSession
baseline covers the 13 world/entity groups and 15 opcodes: complete player/NPC/
ground/monster observations, remote-only removal cancellation, motion corrections,
deferred death/removal, item origins, observer movement, selective teleport reset
and acknowledgement, malformed/fragmented neighbors, two-client isolation and
handshake/close dispatch. The 383 unique checkpoints include the initial
268-case short-prefix matrix. No private or rendering seam is added.

The frozen driver SHA256 is
`be8793cbf7cf5aedbfaab75b02485226d14983f8be0fe6bc9587f3251dcef3cb`.
Independent insertion reversal restores the entire preceding C4 driver. The first
compile found an incorrect fixture output-container type; the assertion now checks
the complete public skill map. All other reviewed inputs and assertions are exact,
and the first diagnostic is retained. C5-only and eight focused guards pass.
Two author raw captures match 383 records, 570,738 bytes,
SHA256 `b18d01e3bd5dbe8144a1b7e871e11ae4d1e7aa3cdf40a64edd00bf07f8dfd4bd`.
All earlier C1/C2/C3/C4 raw references remain byte-identical. Numeric projections
and fixed interpolation steps retain their stated limits. No C5 deadline/clock
output or production time seam is used.
Coverage review strengthened an already passing 375-case baseline with eight
reachable knee/GState, input/ACK, movement-baseline/throttling and empty-first-frame
observations. Every preceding assertion and the complete 375-record raw stream
remain exact. The initial full-suite and source evidence are archived separately.

The independent parent capture also matches. All 158 enabled Debug tests, 164
non-PostgreSQL GL tests and the client guard under ASan/UBSan/leak detection pass;
17 optional-data tests remain disabled in each full profile. Independent actual
and additive-case reviews are clear. Fresh R23 coverage is valid: 882/890 lines
and 567/774 raw arcs, including 144 exception arcs. All 477 selected NetClient,
213 inline model/getter and 49 remote-motion lines are covered.

Six parser read-failure defenses remain zero in the scoped report. Two additional
zero lines in `coords::to_server` reflect object selection: the client executable
links its instrumented inline definition from `Player.cpp`, whose gcov notes are
outside the reporter's filename selection. The raw counters there are 20/20 and
the public XYZ/wire assertions pass; the unchanged scoped report still shows
882/890. No exclusions or artificial calls are added to alter that number.

Evidence: `/tmp/openkal-c5-baseline/{handoff,source-preservation,reachable-guards-preservation,snapshot-proof,coverage,parent-report,coords-inline-diagnostic}.json`,
`before-reachable-guards/`, `parent-before-reachable-guards/`, the independent
actual/strengthened reviews and `/tmp/openkal-c5-baseline-guards-parent-{debug,gl,sanitize}-ctest.log`.

## Remaining C5 implementation — world/entity packet dispatch

The private `handle_world_packet` owns the 13 characterized groups and 15 numeric
opcodes, borrowing the payload and unchanged origin-patch value. Twenty-seven
outer continuations become handled returns. All 3,149 noncomment AST tokens,
ordinary literals/comments, parser tolerances, the removal predicate lambda and
entity/queue ordering are preserved. No state, model/render API, cache or clock
policy is added. CreateItem still starts at age zero; it does not read a clock.

Independent actual-source reversal reconstructs the complete prior CPP/header.
All four earlier helpers and three drain callers remain exact. The last five
groups cross the quest-helper call; each selected numeric opcode takes only
that helper's disjoint comparisons and literal-false path. Its recognized paths
remain stateful. Other world groups retain their relative order, and foreign
opcodes continue through the remaining dispatcher. Header declarations document
the synchronous payload borrow and handled-result contract.

The committed baseline driver remains byte-identical. Two author captures and
the independent parent capture match all 383 records and 570,738 raw bytes,
SHA256 `b18d01e3bd5dbe8144a1b7e871e11ae4d1e7aa3cdf40a64edd00bf07f8dfd4bd`.
All C1/C2/C3/C4 raw references remain exact. Eight focused guards, 158 enabled
Debug tests, 164 non-PostgreSQL GL tests and the client guard under
ASan/UBSan/leak detection pass. Both full profiles retain 17 disabled optional-data
tests. No new PostgreSQL routing run is claimed for this client-only extraction.

Fresh R23 coverage is valid: 867/875 lines and 570/778 raw arcs, including 145
exception arcs. All 462 selected NetClient, 213 model/getter and 49 remote-motion
lines are covered. The same six parser defenses and two scoped coordinate-counter
zeros remain; fresh raw linked-object counters again show 20/20 for the latter.
Formatting changes line attribution without removed behavior or exclusions.
All earlier selected client source and retained public consumers are byte-exact
after remapping. This is a maintainability extraction without a speedup claim.

Evidence: `/tmp/openkal-c5-extraction/{handoff,actual-derived-proof,scope-preservation,replay-results,coverage,parent-report,coords-inline-diagnostic}.json`,
`/tmp/openkal-c5-extraction-independent-review.actual.json` and
`/tmp/openkal-c5-final-{debug,gl,sanitize}-ctest.log` with matching build logs.


## Remaining C6 prerequisite — status, talents and system messages

Production remains at tested C5 commit `5779ee77`. The public BufferedSession
baseline covers talent replacement, native and catalog buff lifetimes, register
replacement/removal, monster state/property updates, battle/system feedback,
optional sources, malformed/fragmented payloads and session isolation. The final
246 checkpoints include 44 literal native/pay buff goldens, owned missing-skill
fallback, icon filename variants, loaded/clamped job titles and enabled chest
diagnostics. No renderer or private state seam is introduced.

The frozen test SHA256 is
`5525ed9a8d64146a1d81024d17d21481b03e466fa257b77f06089ccdf488d003`.
Independent full insertion reversal restores the previous driver. Compilation
first exposed a nonexistent temporary-directory helper; the fixture now uses the
existing owned filesystem pattern. Initial assertions incorrectly treated bit 63
as unknown: it is the native Spirit Resistance buff and correctly resolves to
`active108.bmp`. That expectation was corrected, with every slot and retained
lifetime still checked. All initial diagnostics remain archived. Subsequent
coverage review added 59 reachable observations and strengthened exact register
deadline retention. The complete earlier 187-record stream remains an exact
prefix. Job-title, debug-overlay and snapshot logging state are restored.

Two author captures and the independent parent capture match 246 records and
308,926 raw bytes, SHA256
`295e3675507d03e1cbe7c2e4ae60be1a1fc9bba3cd14c1d2f2c695d867528bee`.
All five earlier client references remain byte-exact. Eleven focused guards,
158 enabled Debug tests, 164 non-PostgreSQL GL tests and the client guard under
ASan/UBSan/leak detection pass. Both full profiles retain 17 disabled optional
user-data tests. Independent actual and strengthened-case reviews are clear.

Fresh R24 coverage is valid: 702/719 lines and 618/788 raw arcs, including 135
exception arcs. The 13-path runtime scope retains public consumers and excludes
fixture-loading support. Remaining zeros include defensive/default paths outside
these packet callers, untested shared setup and compiler line attribution; they
are disclosed individually, with unchanged raw counters and no exclusions or
100-percent claim. This client-only prerequisite does not claim PostgreSQL or
visual-asset validation.

Evidence: `/tmp/openkal-c6-baseline/{handoff,reachable-cases-preservation,snapshot-proof,coverage,coverage-limits,parent-report}.json`,
`before-reachable-cases/`, `parent-before-reachable-cases/`,
`/tmp/openkal-c6-independent-baseline-review.strengthened.json` and
`/tmp/openkal-c6-baseline-guards-parent-{debug,gl,sanitize}-ctest.log`.


## Remaining C6 implementation — status packet dispatch

The private `handle_status_packet` owns the four characterized groups and five
numeric opcodes. Thirteen outer continuations become handled returns; all three
inner buff-loop continuations remain intact. All 2,122 AST tokens,
comments/literals, parser and consumer effects are preserved. The two lazy steady
clock reads remain in their accepted branches. No state, public API,
cache, clock seam, rendering behavior or new hot-path allocation is introduced.

Independent actual-source reversal restores the entire prior CPP/header. All five
earlier helpers and all three drain callers remain byte-exact. The later status
groups cross three helpers and five direct combat guards; the selected numeric
opcodes take only disjoint comparisons and foreign-false paths there. The
recognized helper paths remain stateful. Online/snapshot prefix order, borrowed
payload lifetime, nested loops and FrameDrain exception behavior remain unchanged.

The committed baseline test remains byte-identical. Two author captures and the
parent capture match all 246 records and 308,926 raw bytes, SHA256
`295e3675507d03e1cbe7c2e4ae60be1a1fc9bba3cd14c1d2f2c695d867528bee`.
All C1–C5 raw references remain exact. Eleven focused guards, all 158 enabled Debug
tests, 164 non-PostgreSQL GL tests and the client guard under ASan/UBSan/leak
detection pass. Both full profiles retain 17 disabled optional user-data tests.
No new PostgreSQL routing or visual-asset validation is claimed.

Fresh R24 coverage is valid: 695/712 lines and 621/792 raw arcs, including 136
exception arcs. Every executable line in the extracted helper is covered. The
same 17 retained-consumer/parser/default gaps remain visible, including the old
closing-brace attribution at NetClient.cpp:647. Earlier selected client source
and retained public consumers are byte-exact after remapping. Formatting changes
line attribution without removed behavior or coverage exclusions. This is a
maintainability extraction without a speedup claim.

Evidence: `/tmp/openkal-c6-extraction/{handoff,actual-derived-proof,scope-preservation,runtime-capture-proof,coverage,parent-report}.json`,
`/tmp/openkal-c6-extraction-independent-review.actual.json` and
`/tmp/openkal-c6-final-{debug,gl,sanitize}-ctest.log` with corresponding build logs.


## Remaining C7 prerequisite — combat and action packets

Production remains at tested C6 commit `c1d17988`. The public BufferedSession
baseline covers Attack, strict PlayerAnimation, Skill, Effect and Action: parser
boundaries/fallbacks, animation and deferred arrival, signed HP/components, facing,
class-dependent effects, cast acknowledgement, delayed damage, FIFO exhaustion,
latest-value latches, death/removal/transform state, fragmented/mixed frames and
two-client isolation. The initial 180 observations are unchanged; two additional
public cases cover missing-monster action-damage output and a matching ground-skill
self echo. The latter checks the ordinary outbound request and complete literal
HP, feedback, flags and delay fields. No private state or renderer seam is added.

The final driver SHA256 is
`7226de2a7dd6acfff47b4cc0cd4edee4781711aeec2bf1f419787239eff3d2c5`.
The first compile and first C7-only run passed, including the later additive
cases. Independent rebase/additive reversal restores the entire prior driver and
every earlier assertion. The initial 180-case full-suite/coverage evidence is
archived separately. Its complete 472,349-byte raw stream remains an exact prefix.
The initial bring-up log retains a nonfatal stale libgcov profile diagnostic;
authoritative full measurements use freshly cleared counters.

Two author captures and the parent capture match 182 records and 478,043 raw bytes,
SHA256 `7e6f30768e22bcc1de69b7f5d7be0af077d43dc650f67c1dd8f2cef2358edf22`.
All six prior client references remain byte-exact. Twelve focused/default guards,
158 enabled Debug tests, 164 non-PostgreSQL GL tests and the client guard under
ASan/UBSan/leak detection pass. Two optional guards are disabled in the focused
selection; both full profiles retain 17 optional user-data tests disabled.

Fresh R25 coverage is valid: 612/616 lines and 559/756 raw arcs, including 134
exception arcs. Every selected handler line is covered. The remaining zeros are
an empty combat-text defense whose selected callers produce nonempty text, and
three parser read-failure defenses behind prevalidated bounds. Raw counters and
exception arcs remain visible without exclusions or a 100-percent claim. The
11-file scope covers changed receiver paths and direct parser/output consumers;
request construction is input support, recorded separately. Private expectation
clearing is source-proved rather than inferred from a public acknowledgement.
These tests do not claim audiovisual asset correctness or another PostgreSQL run.

Evidence: `/tmp/openkal-c7-baseline/{handoff,actual-source-preservation,reachable-cases-preservation,snapshot-proof,parent-strengthened-review,parent-scope-policy,coverage,parent-report}.json`,
`before-reachable-cases/`, `parent-before-reachable-cases/`, independent proposal/
rebase reviews and `/tmp/openkal-c7-baseline-guards-parent-{debug,gl,sanitize}-ctest.log`.


## Remaining C7 implementation — combat packet dispatch

The private `handle_combat_packet` owns the final five contiguous groups, borrowing
the payload and unchanged origin patch. Eleven outer continuations become handled
returns. All 2,771 AST tokens, literals/comments, arithmetic and five
facing calls retain their order. The family is clock/RNG-free. No state, public
API, rendering policy, cache, new allocation or exception boundary is introduced.

Independent actual-source reversal reconstructs the entire preceding CPP/header.
All six earlier helpers and the update/handshake/close callers remain byte-exact.
The replacement call occupies the first-Attack position after those six
handlers; no earlier helper or direct guard is crossed. The foreign path performs
five disjoint opcode comparisons and returns false. FrameDrain, borrowed payload,
malformed handling, cleanup and suffix/exception ownership remain unchanged.
`process_frames` now keeps framing and the seven ordered family calls visible.

The committed baseline driver remains byte-identical. Two author captures and the
parent capture match 182 records and 478,043 raw bytes, SHA256
`7e6f30768e22bcc1de69b7f5d7be0af077d43dc650f67c1dd8f2cef2358edf22`.
All C1–C6 references remain exact. Twelve focused/default guards, 158 enabled Debug
tests, 164 non-PostgreSQL GL tests and the client guard under ASan/UBSan/leak
detection pass. Both full profiles retain 17 optional user-data tests disabled.
No new PostgreSQL or audiovisual asset validation is claimed.

Fresh R25 coverage is valid: 603/607 lines and 562/760 raw arcs, including 135
exception arcs. Every extracted handler line is covered. The same empty-text and
three prevalidated parser defenses remain visible. Earlier selected client source
and retained consumers are byte-exact after remapping, with no exclusions or raw
100-percent claim. Formatting changes compiler line attribution. This finishes
the seven client-family maintainability extractions without a speedup claim.

Evidence: `/tmp/openkal-c7-extraction/{handoff,actual-derived-proof,scope-preservation,runtime-capture-proof,coverage,parent-report}.json`,
`/tmp/openkal-c7-extraction-independent-review.actual.json` and
`/tmp/openkal-c7-final-{debug,gl,sanitize}-ctest.log` with matching build logs.


## Remaining H5 prerequisite — Doggebi encounter phases

Production remains at tested C7 commit `53a9ce6f`; Host code is unchanged. The
111-case baseline covers Lord ascension, combat/controller scheduling and ritual
transitions, complete session/world state, ordered audiences, RNG, summon-vector
relocation and reset/presentation behavior. Four practical additions cover loaded
ascension magic endpoints, Fury's INT term and ordered SpiritAbsorb eligibility.
Independent source reversal preserves the complete preceding driver and every
assertion. The initial 107-record stream remains an exact raw prefix.

Two author captures and the fresh parent capture match 111 records and 13,917,761
raw bytes, SHA256
`0ee22df827e19c1b54f55f152d6fa983291d28de3eccface2297ab6cf0524ffa`.
All H1–H4 references remain byte-exact. The owned ordinary SQLite/Unix witness
checks eight monuments, both readers observing the same Lord's ordered prepare/
fire sequence, map-state/countdown output and saved exits. A missing-monument
negative case also passes. This does not claim long-duration encounter timing,
rendering/audio, PostgreSQL encounter identity or encounter persistence.

Fresh parent validation passes 159 enabled Debug tests, 165 non-PostgreSQL GL
tests, the Host and encounter guards under ASan/UBSan/leak detection and two
actual PostgreSQL multiplayer/load guards. Full profiles retain 17 optional
user-data tests disabled. Earlier eight focused guards and the final default
Host guard also pass. Fixture bring-up corrections are retained with their failed
sources/logs: contact damage is 2 after resistance, and the live monster's derived
HP is 1132. No production behavior or assertion was weakened.

Fresh R26 production coverage is valid: 808/1137 lines and 496/1140 raw arcs,
including 233 exception arcs. Boundary cases also execute the mechanically
source-proved old-source adapter; its separate coverage is 503/508 lines and
459/580 raw arcs, including 96 exception arcs. These denominators are not combined.
The ordinary witness executes real Host scheduling. Three adapter null-ritual
closure guards are bypassed by the selected caller's outer guard; two post-tick
switch arms are not emitted by the current domain transition function. Other
shared-consumer gaps remain visible rather than being declared unreachable.

Non-executable enum/value declarations are retained as explicit source/type/default
verification after the initial coverage tool rejected their absent gcov records.
No executable line was excluded or counter rewritten. Initial reference-path and
CTest transcript-copy mistakes are recorded and corrected from exact frozen raw
artifacts or an unchanged-binary run. The final 111-case parent pipeline ran fresh
without resume. Raw coverage remains below 100 percent within the documented scope.

Evidence: `/tmp/openkal-h5-baseline/{handoff,parent-report,coverage,adapter-coverage,coverage-limits,parent-actual-baseline-review,parent-actual-live-review,parent-strengthened-review,bringup-corrections}.json`,
independent reachable-case review, both initial-107 archives and
`/tmp/openkal-h5-baseline-guards-parent-{debug,gl,sanitize,postgres}-ctest.log`.


## Remaining H5 implementation — explicit encounter phases

The coordinator now calls three ordered private free functions for Lord ascension,
Lord combat and ritual progression. Six existing reset/presentation closures become
narrow free functions. A simple aggregate owns the twelve controller
values in the same declaration/default/destruction order and at the initial
thread lifetime. World, sessions, mutex, next ID and ritual are borrowed explicitly.
The presentation function-pointer clock defaults to the steady clock and
is read separately for each eligible recipient; downstream contact clocks remain
independent. No general scheduler, service context or gameplay policy is added.

Independent actual-source reversal restores the entire preceding Host byte-for-byte
under only the explicit name/member/argument bindings. Existing locks, erase scopes,
RNG expressions, lazy clocks, summon relocation/reacquisition and packet order are
preserved. Of 28 earlier selected Host ranges, 27 remain byte-exact and one retained
R18 coordinator range has one state and three presentation-call bindings. The root
scope proof preserves 59 retained ranges and maps all nine helpers. Earlier H1–H4
source-only generators retain their strict historical pins and need no adaptation;
only the current H5 baseline-generation CMake wiring is removed.

The initial reviewed temporary proposal contained four unbound old mutex names;
independent review caught them before application. The exact four bindings and a
nine-comment parameter-documentation followup are archived and independently
verified. The first actual build and direct 111-case run passed. Both the direct
capture and full parent capture match all 13,917,761 baseline bytes, SHA256
`0ee22df827e19c1b54f55f152d6fa983291d28de3eccface2297ab6cf0524ffa`.
The frozen direct and ordinary live test files are unchanged; all H1–H4 raw
references remain exact.

Validation passes 159 enabled Debug tests, 165 non-PostgreSQL GL tests, two affected
guards under ASan/UBSan/leak detection and two actual PostgreSQL multiplayer/load
guards. All affected builds pass. Full profiles retain 17 optional user-data tests
disabled. The ordinary encounter witness runs through actual Host scheduling.
No long-duration, audiovisual or PostgreSQL encounter-identity claim is added.

Fresh R26 production coverage is valid: 1049/1113 lines and 801/1156 raw arcs,
including 236 exception arcs. The direct baseline now invokes the real helpers,
so their counters are attributed to production rather than the old adapter; these
figures do not combine adapter and production denominators. Five helper lines
remain: three null-ritual returns bypassed by selected scheduled callers and two
post-tick phase arms not emitted by the current domain transition. Retained spawn/
activation and shared-consumer gaps remain visible and are not all called
unreachable. The new aggregate declaration has no gcov records; its fields/defaults
remain source/type/order verified, while actual initialization and every executable
helper/caller remain measured. The reporting diagnostic is retained; only this
non-executable metadata was corrected, reusing successful tests and their counters.
No exclusions, counter rewriting or raw 100-percent claim are introduced. This is
a maintainability change without a speedup claim.

Evidence: `/tmp/openkal-h5-extraction/{HANDOFF,actual-derived-proof,scope-preservation,first-runtime-proof,coverage,coverage-limits,parent-report,parent-nonexecutable-state-correction}.json`,
`/tmp/openkal-h5-independent-production-review.{py,json,md}` and
`/tmp/openkal-h5-final-{debug,gl,sanitize,postgres}-ctest.log` with build logs.


## Remaining H6 prerequisite — Archer persistent damage and delivery

Production remains at tested H5 commit `44056545`. The 76-case baseline covers
status timing/grace and nonlethal boundaries, Cloud refresh/reownership, Rain
movement cancellation and cleanup, explosion fuse geometry, target/owner filters,
PvP mitigation, dummy fallback, cross-producer death/reward and ordered delivery.
The last two additions cover a one-HP real monster under Cloud and a real player
whose fuse follows its current position. Each addition independently reconstructs
the preceding complete driver. initial 74 and 75 raw streams remain exact prefixes.
No assertion or expected result was weakened.

The frozen driver SHA256 is
`fce642e989234b785a8dcb0b7be3a9914b2037b3d63d461f653a5e95b153e0a9`.
Two author captures and the fresh parent capture match 76 ordered observations,
456 typed JSON records and 9,468,170 complete raw bytes, SHA256
`7b6bd836e7c7a8083e8dd78abe3b751610b8214cbf5587e1d4fcebd9e6aff87d`.
All H1–H5 references remain byte-exact. Affected absolute PvP deadlines have an
explicit labeled representation only after before/after bounds assertions;
other state/wire bytes remain exact and no global clock is frozen.

The ordinary SQLite/Unix witness exercises learned skills 42, 47 and 49 through
two readers, controller expiry, retained Cloud damage, movement interruption,
initial versus delayed explosion damage, authoritative HP conservation and
actor-last durable saves with all 30 inventory fields and learned ranks. Both
runtime actors share one persisted local character; this is not a PostgreSQL
identity test. The direct reward case has a null store and sublevel three-EXP
reward, with no unrelated event controller, and makes no persistence claim.

Bring-up diagnostics are retained. The standalone test needed explicit Attack.h;
a mistaken Constants.h trial was removed. Bounded ordinary connection retries
handle asynchronous socket readiness. Local-mode movement is echoed only to the
actor; both damage/removal streams remain checked, and Rain removal must precede
the source-derived earliest uninterrupted fifth pulse with a one-ms quantization
margin. This is an explicit causal timing precondition, not a performance limit.
Early revision/stale-gcda and sandbox-socket diagnostics remain in their logs.
The final measurements use fresh counters and unfiltered combined output.

All 160 enabled Debug tests, 166 non-PostgreSQL GL tests, two affected guards under
ASan/UBSan/leak detection and two actual PostgreSQL multiplayer/load guards pass.
The six earlier focused/default guards and all three ordinary skill profiles also
pass. Full profiles retain 17 optional user-data tests disabled. Complete initial
74- and 75-case broad validation is archived; final 76 validation ran fresh.

Fresh R27 production coverage is valid: 1027/1389 lines and
782/1471 raw arcs, including 233 exception arcs. The separate mechanically
source-proved old-source adapter covers 389/393 lines and 328/440 raw arcs,
including 84 exception arcs. These denominators are not combined. Remaining
adapter paths are valid-kind or locked-query/geometry defenses; shared consumer,
conditional reward/stat and const-overload gaps stay visible. No blanket
unreachability or raw 100-percent claim is made. Non-executable value/member
declarations retain source/type/default/order verification; private copies of both
production and test gcno independently confirm their absent line attribution.
Runtime initialization, both lookups and executable producer/delivery paths remain
measured, without instrumentation exclusions or counter rewriting.

Evidence: `/tmp/openkal-h6-baseline/{handoff,additive75-handoff,additive76-handoff,parent-report,root-R27-scope-policy,coverage,adapter-coverage,coverage-limits}.json`,
`runtime/`, both parent archives, independent rebase/live/additive reviews and
`/tmp/openkal-h6-baseline-guards-parent-{debug,gl,sanitize,postgres}-ctest.log`.


## Remaining H6 implementation — Archer scheduling phases

Baseline commit `0038f04d` precedes production changes. Six narrow functions now
separate player lookup, player/dummy lookup, timed damage, persistent areas,
explosion fuses and ordered hit delivery. Lookup closures forward at
the same caller positions; the hit vector remains owned by the tick scope through
the later Mage/world phases. No scheduler, clock injection or extra allocation is
introduced. Locks, erase control flow, RNG/arithmetic and reward timing remain
in their previous order. Borrowing the stable store once at delivery entry is the
only store-pointer read timing change and does not change ownership or waits.

The independently reviewed complete-source inverse reconstructs tested Host
`8ac11793` from final `d6cd93b9`. All six helper bodies and documentation match the
reviewed proposal. Scope translation verifies 101 retained runtime/declaration
ranges; the extracted functions, both forwarders and caller-owned hit setup remain
covered. The old-source generated bridge is no longer compiled; the frozen test
source `fce642e9` and ordinary live witness `cf78552d` are unchanged.

All 76 H6 observations and the entire 9,468,170-byte reference stream remain exact,
as do all H1–H5 references. All 160 enabled Debug tests, 166 non-PG OpenGL tests,
two affected ASan/UBSan/leak guards and two actual PostgreSQL multiplayer/load
guards pass. All 17 optional user-data tests remain disabled.

Fresh production R27 coverage is 1180/1405 lines and
940/1479 raw arcs, including 237 exception arcs.
Remaining helper line gaps are recorded literally as `{"tick_archer_areas": [6761, 6862], "tick_archer_explosions": [7013, 7021]}`;
unchanged shared-consumer and compiler/exception paths remain visible. No coverage
counter rewriting, instrumentation exclusions or raw 100-percent claim is used.

Evidence: `/tmp/openkal-h6-extraction/post-H5/{source-audit,supplemental-rebase-proof,scope-preservation,graph-actual-verified,parent-report,coverage,coverage-limits}.json`,
independent post-H5 production review and
`/tmp/openkal-h6-production-guards-parent-{debug,gl,sanitize,postgres}-ctest.log`.


## Remaining H7 prerequisite — Mage persistent effects and marks

Production remains at tested H6 commit `c1a6bd57`. Ninety unchanged cases cover
mark cleanup, timer/grace boundaries, owner/target/map/death and movement gates,
channel and controller variants, geometry, discarded RNG draws, slow/palsy/moves,
paid HP drain including SQL failure and dead-owner revival, deferred output passes
and a single reward after overlapping effects. Every declared Mage area/mark/hit
and move-result field is represented; relevant monster/session fields are observed,
without a claim of all 70 monster or all character fields. Actual SQL rows are
checked where used; there is no SQL-attempt audit seam.

The exact driver `f90ceddc` includes all preceding H1–H6 fixtures. An independent
four-insertion inverse recovers the complete frozen H6 driver `fce642e9`, including
its final two boundaries. The 90-case block remains byte-exact `7bd0105c`. Source-only
generation pins actual Host `d6cd93b9`, retains phase `763d6913` and uses the real H6
lookup forwarder with its existing lifted function. Changed input/phase rejection
and whole historical bridge reconstruction pass. The historical precompile macro
selection defect was corrected to MAGE without changing fixture expectations.

Two author raw captures and a fresh parent capture match 90 ordered observations,
540 typed JSON records and 11,273,543 complete raw bytes, SHA256
`93a2b4eebb5e779f7891cd53f582a43ad27f85e2aad91572186e482d9265ec06`.
All H1–H6 raw references remain exact. Only already asserted real Unix expiry and
steady PvP deadline fields have explicit labeled representations; lazy clock/RNG
source sites remain exact, with no global clock freeze or postcapture filtering.

The ordinary SQLite/Unix guard exercises learned 43, 66 and 67 through two readers.
Skill 66 has no controller packet; actual slow state, accepted actor movement,
a fresh observer save FIFO boundary and later 43 pulses establish cancellation.
The mark lockout response must precede 499 ms to reserve one integer-ms floor unit
within the actual 500-ms gate; invalid timing is explicit and is not a performance
benchmark or silently retried. Both actors share one persisted local character;
two final actor saves and full 30-field inventory/rank/pose/resource checks retain
that qualified persistence claim. Attack.h/readiness/local-echo corrections were
independently source-proved before compilation; final direct tests first passed
without changing expectations. Bring-up and stale-counter diagnostics are retained.

All 161 enabled Debug tests, 167 non-PG OpenGL tests, two affected ASan/UBSan/leak
guards and two actual PostgreSQL multiplayer/load guards pass. All three ordinary
skill profiles pass; 17 optional user-data tests remain disabled. Fresh production
R28 covers 1266/1661 lines and 960/1748 raw arcs,
including 280 exception arcs. The separate generated adapter covers
309/316 lines and 276/362 raw arcs, including
65 exception arcs. These denominators are not combined. Remaining raw
paths and source/type contracts stay explicit. The seven adapter gaps are Curse/
Palsy storm arms unused by ordinary storm producers, a fixed monster-query guard
and a null-owner delivery defense whose hit producers already validate the owner.
Independent source review found no omitted ordinary case worth manufacturing;
no exclusions, rewritten counters or 100-percent claim are used.

Evidence: `/tmp/openkal-h7-baseline/post-H6/{rebase-proof,scope-facts,root-R28-scope-policy,parent-report,coverage,adapter-coverage,coverage-limits}.json`,
`runtime/`, independent rebase/live reviews and
`/tmp/openkal-h7-baseline-guards-parent-{debug,gl,sanitize,postgres}-ctest.log`.


## Remaining H7 implementation — Mage scheduling phases

Baseline commit `d730f308` precedes production changes. Three narrow functions
now prune lightning marks, tick Mage areas and deliver all damage, then moves,
then deaths. Caller-owned hit and move vectors retain their lifetime
through world production/delivery. Lazy Unix reads, discarded RNG draws, integer
arithmetic, erase/continue/break targets, locks, paid drain and ignored SQL failure
behavior remain source-exact. Stable store pointers are borrowed at helper entry;
this qualified scalar-read timing change adds no ownership, waits or allocation.
The now-unused existing live-player forwarder remains source-exact at its initial
caller position; actual lookup execution uses the previously tested H6 function.

Independent whole-file inversion restores tested `d6cd93b9` from final `e45b2a1f`.
All six H6 helper/documentation blocks and both preceding forwarders remain exact;
152 earlier runtime/type ranges are translated with source proofs. The frozen
`f90ceddc` driver and `3405e28d` ordinary witness are unchanged. The old generated
Mage bridge is no longer compiled; the same 90 cases now invoke real functions.

All 90 observations and the complete 11,273,543-byte raw reference remain exact,
as do all H1–H6 references. All 161 enabled Debug tests, 167 non-PG OpenGL tests,
two affected ASan/UBSan/leak guards and two actual PostgreSQL multiplayer/load
guards pass. All 17 optional user-data tests remain disabled.

Fresh production R28 coverage is 1386/1664 lines and
1108/1754 raw arcs, including 283 exception arcs.
Remaining helper gaps are `{"tick_mage_areas": [7286, 7287, 7288, 7289, 7290, 7395], "deliver_mage_area_effects": [7484]}`; conditional shared consumers and
compiler/exception paths remain visible, without counter rewriting, instrumentation
exclusions or a raw 100-percent claim.

Evidence: `/tmp/openkal-h7-extraction/{actual-tested-H6-gate,source-audit,scope-preservation,parent-report,coverage,coverage-limits}.json`,
independent production review and
`/tmp/openkal-h7-production-guards-parent-{debug,gl,sanitize,postgres}-ctest.log`.


## Remaining H8 prerequisite

Production remains at tested H7 commit `cafa50d9`. The frozen driver
`03834cb7` adds 57 owned
observations while preserving every H1–H7 assertion. The four source-generated
bodies retain 113/86/343/129 tokens and phase `a388bfa1`; the strict full-source pin
is actual Host `e45b2a1f`. Source-only
regeneration, changed-source rejection and whole earlier-driver/CMake inverses
pass. The generated adapter is measured separately from production.

The first direct run exposed a setup defect in the intended blocked-spawn
case: ensure_blocked_mask did not replace the already allocated open mask. The
independently reviewed correction explicitly blocks the four owned spawn tiles;
all 57 assertions/names and timer/placement expectations remain byte-exact. The
initial failed log/snapshot and whole-initial-driver inverse are retained. The
corrected run passed before final raw captures and fresh broad validation.

The finite matrix covers distinct prey/refill/index/sync filters, refill sentinel
and timing/radius/cap boundaries, complete 70-field monster and 30-field item
projections plus declared pose/event/ground/NPC fields, mixed source membership,
ordered dirty-state/corpse packets, corpse removal before borrowed index rebuild,
2→1→2 viewer transitions, mutations between batches and seeded owned plant RNG.
Per-peer output is completely drained with success and progress assertions. The
previous world lock and caller-owned vectors remain explicit contracts through
later event consumers; this is not coverage of every world-AI branch.

Two complete author captures and a fresh parent capture match 57
ordered observations and 9,443,088 raw merged bytes, SHA256
`ac2fc76838751c5c1d202b512a85bbe6127c1cd00e891f5cdeecbace0212e64f`. All H1–H7 raw references remain byte-exact. Counts and hashes
come from the actual runtime proof/configuration, without filtering or guessed
capture constants.

All 161 enabled Debug tests, 167 non-PG OpenGL tests,
2 affected ASan/UBSan/leak guards (Host and multiplayer presentation)
and 2 actual PostgreSQL multiplayer/load guards pass without skips;
17 optional user-data tests remain disabled. No new ordinary fixture is added:
the existing multiplayer-presentation harvest guard exercises the real scheduler,
with existing harvest/refill/index domain guards reused. Its production random
plant seeding is distinct from the deterministic direct fixture. No independent
SQLite identity, whole-world AI or performance claim follows from these tests.

Fresh production R29 is 345/369 executable lines and
352/552 raw arcs, including
141 exception arcs. The separate generated adapter is
71/71 lines and
90/112 raw arcs, including
17 exception arcs. Raw missing paths remain visible;
these denominators are not combined and no counter rewriting, exclusion or
100-percent claim is used.

Evidence: `/tmp/openkal-h8-baseline/post-H7/{rebase-proof,scope-facts,parent-report,coverage,adapter-coverage,coverage-limits}.json`,
`runtime/snapshot-proof.json`, independent baseline/rebase reviews and
`/tmp/openkal-h8-baseline-guards-parent-{debug,gl,sanitize,postgres}-ctest.log`.


## Remaining H8 implementation

Baseline commit `0105d4a7` precedes production changes. Four narrow free
functions collect prey poses, refill eligible players, publish monster status and
remove corpses, and synchronize world interest. The 113/86/343/129-token bodies,
comments, literals and internal control flow retain their exact bindings. The
complete source inverse recovers tested H7; all earlier H5/H6/H7 helpers and both
lookup forwarders remain unchanged. No scheduler/context framework or new cache
is introduced, and this maintainability change makes no performance claim.

The complete previous world lock, viewer snapshot, Harvest/world/ground call
order, caller-owned poses and five world-event vectors remain in the coordinator.
Dirty-state packets precede corpse erase; erase and clear precede borrowed-index
rebuild; monster/item/NPC consumers finish under the same lock before return.
Index sources remain stable throughout that batch. Viewer-count and recipient
predicates deliberately remain distinct, including zero/one-viewer fallback.
Later H9/metrics consumers keep the same event-vector ownership and lifetime.

Two qualified observation changes are explicit: immutable `max_monsters` is read
once at helper entry rather than once per eligible session, and trivial viewer
count/index-pointer locals end at helper return just before the old lock scope
ends. These introduce no new ownership, callbacks, waits or clock/RNG reads.
The frozen `03834cb7` driver keeps all 57 cases and H1–H7 assertions unchanged;
the old generated WorldStep bridge is no longer compiled and those same calls
now exercise real production functions.

All 57 observations and 9,443,088 complete merged raw bytes
remain exact, SHA256 `ac2fc76838751c5c1d202b512a85bbe6127c1cd00e891f5cdeecbace0212e64f`, as do all H1–H7 references. All
161 enabled Debug tests, 167 non-PG OpenGL tests, the
2 affected ASan/UBSan/leak guards and 2 actual
PostgreSQL multiplayer/load guards pass without skips; 17 optional user-data tests
remain disabled. The existing multiplayer-presentation harvest witness and domain/
index tests are reused with the same bounded scope; no new ordinary fixture or
claim about independent SQLite identities or every world-AI path is added.

Fresh production R29 is 379/381 executable lines
and 398/556 raw arcs, including
143 exception arcs. All instrumented lines in the four extracted functions are covered. Shared conditional consumers and compiler/exception paths
remain visible without exclusions, rewritten counters or a 100-percent claim.
The pre-extraction generated adapter remains a separate historical denominator.

Evidence: `/tmp/openkal-h8-extraction/post-H7/{source-audit,scope-preservation,parent-report,coverage,coverage-limits}.json`,
independent production reviews, the baseline `runtime/snapshot-proof.json`, and
`/tmp/openkal-h8-production-guards-parent-{debug,gl,sanitize,postgres}-ctest.log`.


## Remaining H9 prerequisite — World event consequences

Production remains at tested H8 commit `047a9207`. The frozen `6826bb14` driver
adds 77 characterized observations after all H1–H8 cases, including 76 Archer, 90 Mage
and 57 world-step cases. Four insertion removals recover the entire prior driver;
source-only generation pins the actual tested H8 source `81097a2e`.
The raw 447-line phase and its one counted H5 Lord field binding reconstruct the
historical delivery phase and the entire old bridge. Changed complete-source and
phase inputs are rejected; the baseline precedes any production extraction.

The first 41a baseline compile failed on the nonexistent `protocol::S2COpcode` name;
that failed log and source remain archived. The sole correction changes that code
qualifier to the existing `protocol::S2C`, preserving InsertItem wire value 7.
The final 6826 driver and 9d42 block recover entire 41a/B1aa under precisely that one
identifier inverse, and recover the complete 03834 H8 driver through four insertion
removals. All 77 case names, inputs, literals and assertion structure apart from that
compile-only qualifier remain exact. No test ran with the rejected 41a candidate.
The review and separate compiler-fix supplement are both required gates.

Cases retain harvest failure/retry/full-bag/IID behavior, ordinary chest drop
before death, item despawns, known-recipient movement, Prepare/Fire/basic attack,
Lord secondary victims, paid defense and ignored SQL failure, counter/reward/death/
HP ordering and the existing ordinary lethal-motion behavior. Every declared 70-field
MonsterEntity, 15-field GroundItem, 30-field ItemRecord, 40-field CharacterRecord and
28-field BagSlot projection is represented, along with the complete chest/move/
attack, pay-buff/remain and inventory-unlock values. Real committed SQLite rows and
audit effects are checked; rolled-back SQL attempts and all private state are not
claimed. No production formula, clock or rendering API is introduced.

The actual raw pair and fresh parent replay match 77 ordered observations,
462 valid whole JSON records and 9,786,392 complete merged bytes,
SHA256 `031b19c1da04986ed2e06b24d27d7de50579cbdb6bbee7f1d4bb82abb3ec80a5`. All eight earlier family streams remain exact. These
numbers are read from validated runtime files, without postcapture filtering or
guessed sizes; all diagnostics and scoped real-clock policies remain.

The unchanged 821-line ordinary SQLite/Unix witness exercises two real readers in
three profiles: positive basic incoming damage, AI4 Skill 29 Prepare/Fire/lethal HP,
and ordinary learned Counter 20 followed by authoritative monster death. Counter requires
the natural 351/1000 gate plus a normal hit, with a 120-second watchdog and ordinary
20-second recasts. It remains probabilistic and can legitimately fail that watchdog;
passing is not a deterministic timing or reliability benchmark. Shared-character
persistence is qualified: observer saves/closes first, then the connected actor gets
two fresh saved=true replies and all 30 item fields/pose/HP are verified BEFORE actor
close and Host.stop, then reopened again afterward.

All 162 enabled Debug tests, 168 non-PG OpenGL tests, two affected ASan/UBSan/leak
guards (Host and world_event_server), and two actual PostgreSQL multiplayer/load
guards pass. All 17 optional user-data guards remain disabled. The PostgreSQL gates
are separate from the three SQLite live profiles; no H9 distinct-account/TLS claim
is inferred from those local profiles.

Fresh actual production R30 covers 750/801 lines and
570/966 raw arcs, including 231 exception arcs.
The separate generated adapter covers 313/313 lines and
372/494 raw arcs, including 104 exception arcs.
These denominators are not combined. All remaining raw branches, compiler/exception
arcs and explicit source/type contracts remain visible; neither full SQL-backend/
world-AI coverage nor raw 100 percent is claimed.

Evidence: `/tmp/openkal-h9-baseline/post-H8/{parent-report,coverage,adapter-coverage,coverage-limits,rebase-proof,scope-facts}.json`,
`/tmp/openkal-h9-baseline/post-H8/runtime/snapshot-proof.json`, independent direct/rebase/live reviews and
`/tmp/openkal-h9-baseline-guards-parent-{debug,gl,sanitize,postgres}-ctest.log`.


## Remaining H9 implementation — World delivery phases

Baseline commit `31730c4c69571986bb3fb4bbbcdfb48c4e716717` precedes production changes on tested H8 `047a9207`.
Four narrow file-local functions now deliver chest/harvest events, item despawns,
monster moves and monster attacks. The frozen corrected 6826/9d42 baseline's 77-case
driver and 821-line ordinary witness are unchanged by this extraction; the same cases
now invoke real helpers and the generated
WorldEvents bridge is no longer compiled. H8's caller-owned five event vectors and
the scheduled-block/metrics boundary retain their lifetime and order.

The earlier 41a compiler failure and archived B1aa review remain visible. Its sole
compile correction renamed `S2COpcode` to the existing `S2C`, with wire value 7 and
all other 77-case bytes preserved. Both the review and compiler-fix
supplement, full 41a inverse and prior 03834 insertion inverse remain gated here.
No rejected 41a test run is claimed.

Whole-file inversion restores actual tested H8 `81097a2e` from final
`c7251d89`. Historical 878/67/120/1857 effect leaves, all literals/comments,
internal continue/break/return targets, seven world-lock sites, lazy clocks and RNG
consumption remain exact under named dependency bindings. 249 prior parsed function
and documentation blocks, including H5–H8, and both H6 lookup forwarders remain
unchanged. The caller-local move adapter remains; one small nonescaping attack-local
forwarder borrows only clients and sends death-generated moves through the same
shared move operation. No copied context, interface or new owning allocation is added.

Stable plant config and store are borrowed once at helper entry and the stable
Doggebi Lord scalar is captured for the attack batch instead of reread within it.
Those qualified read-timing changes add no clock, wait, ownership or state-policy
change. Harvest and ignored SQL failures, drop-before-death ordering,
recipient known sets, paid charges, counter rewards, death/HP and motion behavior
are retained without formula/rollback/gameplay changes.

All 77 observations and the complete 9,786,392-byte raw reference remain exact,
SHA256 `031b19c1da04986ed2e06b24d27d7de50579cbdb6bbee7f1d4bb82abb3ec80a5`, as do all eight preceding family streams. All 162 enabled
Debug tests, 168 non-PG OpenGL tests, two affected ASan/UBSan/leak guards and two
actual PostgreSQL multiplayer/load guards pass. All 17 optional user-data guards
remain disabled. Ordinary Basic/AI4 Skill 29/Counter 20 use two SQLite readers; the
natural 351/1000 plus hit chance, 120-second watchdog and ordinary 20-second recasts remain
explicitly probabilistic. Two fresh connected actor saves and preclose/prestop row
checks preserve the qualified shared-character durability contract.

Fresh production R30 coverage is 799/813 lines and
691/972 raw arcs, including 234 exception arcs.
Every instrumented line in all four extracted functions is covered. The 14
uncovered physical lines belong to unchanged shared consumers, with their exact
conditions reviewed separately. Raw branch and exception gaps remain visible. Full declared 70/15/30/40/28-field projections
and event/pay/unlock values do not imply complete private state, full SQL backend
or world-AI coverage. No exclusions, counter rewriting or raw 100-percent promise is used.

The finite 16-package refactoring queue has passed implementation validation. This
queue status does not declare the overall authorized work finished: final 64-client
Release PostgreSQL load validation and owned-cluster cleanup remain separate root
gates, followed by final local commit/worktree verification. No push is authorized.

Evidence: `/tmp/openkal-h9-extraction/{source-audit,scope-preservation,parent-report,coverage,coverage-limits}.json`,
`/tmp/openkal-h9-baseline/post-H8/runtime/snapshot-proof.json`, independent production review and
`/tmp/openkal-h9-production-guards-parent-{debug,gl,sanitize,postgres}-ctest.log`.

Additional H9 evidence: `/tmp/openkal-h9-actual-rebase-review.json`,
`/tmp/openkal-h9-extraction/graph-actual-verified.json` and
`/tmp/openkal-h9-extraction/shared-gap-carryforward.json`. The shared gap review
qualifies the actual H9 producers; those paths are not globally called unreachable.


## Final refactoring acceptance

The finite H1–H9 and C1–C7 queue is complete: all 16 implementations have tested
local commits on `codex/codebase-optimizations`. Final production commit
`ab13822d` follows H9 baseline `31730c4c`. Earlier pending-work statements in
this chronological record are superseded by this acceptance entry and the current
[execution plan](RemainingRefactoringPlan.md).

On the final source, all 162 enabled Debug tests, 168 non-PostgreSQL OpenGL tests,
two affected ASan/UBSan/leak guards and two actual PostgreSQL integration/load
guards pass. All nine Host reference families remain byte-exact. Every instrumented
line in the four H9 functions executes; the wider R30 scope remains honestly
799/813 lines and 691/972 raw arcs, including 234 exception arcs. The 14 unchanged
shared line gaps and their source conditions remain documented. All 17 optional
user-data tests remain disabled; whole-codebase or raw 100-percent coverage is not
claimed.

The final Release PostgreSQL/TLS profile passes with 64 clients, including one
slow reader and 63 healthy peers during pressure, two warmup rounds and ten
measured barrier rounds. Its runtime report is
`/tmp/openkal-final-refactoring-load64-postgres.json` and unfiltered output is `/tmp/openkal-final-refactoring-load64-postgres.log`.
This synthetic profile provides acceptance evidence, not a general speedup or
reliable tail-latency claim. The owned temporary PostgreSQL cluster is stopped;
its PID file is absent and its socket no longer accepts connections.

Final gate evidence: `/tmp/openkal-h9-extraction/final-release-load64-result.json`,
`/tmp/openkal-h9-extraction/postgres-cleanup.json` and `/tmp/openkal-h9-extraction/final-acceptance.json`.
Delivery remains local; no push or pull request is part of this work.


## Build-warning follow-up

The reported Host coverage build warnings are resolved. `WornGear` explicitly
initializes its defaulted charm state; both immutable pair loops bind by const
reference. The transform broadcaster no longer accepts an unused sender. The
unused equipment IID copy, uninvoked player-lookup lambda and obsolete undefined
ten-argument drop declaration are removed; the correct eleven-argument drop
contract and its calls are retained. Independent complete-source reversal checks
confirm only these warning cleanups, with the existing behavior preserved.

Both the server and Host coverage translation units pass with all six reported
warning categories treated as errors. Full Debug and affected sanitizer builds
emit no warnings. All 162 enabled Debug tests and the Host ASan/UBSan/leak guard
pass; all nine complete Host reference streams remain byte-exact. Fresh coverage
validation passes for all 30 areas after remapping source coordinates and hashes.
Only the removed unused lambda disappears from selected source; no coverage
exclusion or counter rewrite is added. The 17 optional user-data tests remain
disabled.

Evidence: `/tmp/openkal-warning-cleanup/{strict-compile,independent-review,scope-preservation,validation-report,coverage}.json`
and the adjacent unfiltered build/test logs. These results supersede the earlier
source pins only for this warning follow-up; historical validation remains intact.

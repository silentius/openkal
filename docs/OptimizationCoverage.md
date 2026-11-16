# Regression preparation for the optimization plan

Date: 2026-11-02. Production source baseline:
`6d1f5c5222530ad155ea525df42cac4395bdf055`.

This records the preparation commit `27b6251f` for
[CodebaseReviewPlan.md](CodebaseReviewPlan.md). That commit included no
optimization or application behavior fix. Subsequent implementation and
validation are tracked in [OptimizationProgress.md](OptimizationProgress.md).
The percentages below describe the preparation baseline; the generated report
uses the current manifest's source fingerprints and ranges. The scope covers concrete planned change sites and relevant call paths, aiming
for useful reachable-path tests rather than an unqualified 100% claim.
Raw compiler counters remain unfiltered; no coverage exclusions were added.

## Scope and implementation

`tests/OptimizationCoverageScope.json` defines the measured source ranges for
R01–R10 and R12, with SHA-256 fingerprints of the complete source files. R11
changes CMake compilation structure and requires build verification instead of
an invented executable coverage percentage. The measurement is deliberately
about these ranges, not whole-repository coverage.

R05 includes every planned genuine vector-front FIFO drain. Create/remove
collections with cancellation and buff-slot filtering retain their separate
semantics. R06 includes the generic layout and FreeType measurement plus the
concrete multiline `UiView` and announcement `GuildWindow` call sites. The R08
first extraction is bounded to consumable crafting and TalentTree requests;
other opcode families need their own tests before extraction.

Only one production header changes: `GameWorldCoveragePeer` is a narrow friend
for characterization of private occupancy predicates and combat inputs. There
is no runtime implementation change. Other changes add tests, opt-in GCC
instrumentation, a source-range manifest, a reporter and this documentation.
The tests remain available with coverage disabled. Qt-specific tests follow the
existing tool build options.

Fixtures are independently written synthetic catalogs, temporary saves, owned
protocol packets and a generated solid-color BMP. Text tests use an installed
system TrueType font; they do not bundle a new font. No proprietary game data,
real user saves or external implementation material is required.

## Added behavioral checks

- **R01 / framing and lifecycle:** every header/payload split, coalesced bursts,
  incomplete suffix completion, malformed lengths, unknown opcodes, buffer
  limits, receive failures, disconnect/reconnect and the actual world and
  spectator worker paths. Assertions check exact packets, event order and
  current cleanup behavior. A separate protocol target checks parser contracts.
- **R02 / output:** deterministic test-only linker wrappers script POSIX
  partial writes, EINTR, readiness, timeout and fatal errors; TLS tests exercise
  both unchanged internal BIO helpers and the linked public implementation.
  Checks include exact bytes, unsent suffixes, bounded receives, WANT_READ /
  WANT_WRITE and descriptor cleanup. Existing real TLS tests remain enabled.
- **R03 / interest:** exact monster/item/NPC appearance and removal deltas,
  map and distance boundaries, unchanged repeated updates, and removed objects.
- **R04 / occupancy:** an independently written predicate oracle covers maps,
  overlaps, self-exclusion, dirty source/target reservations, negative and cell
  boundary coordinates, corpses, nonblocking races and changing player poses.
- **R05 / FIFOs:** empty, ordered distinct events, complete drain, alternating
  refill/drain, structured payload preservation, capped oldest-event eviction,
  and the existing create/remove cancellation behavior. The remaining effect,
  HTML, shop, storage, item, mount, map, party and information queues are included.
- **R06 / text:** exact byte spans, whitespace, blank lines, trailing newlines,
  UTF-8 boundaries, fallback punctuation/glyphs, malformed UTF-8, narrow/zero
  widths, nonadditive callbacks, callback exceptions, seeded generated inputs,
  font reload and scale clamping. Integration tests exercise real multiline
  widget rendering and the existing guild announcement editor.
- **R07 / combat:** all four playable classes, primary and random stats,
  weapons/armor and fallback equipment, mix/prefix bonuses, full Qigong and
  Dragon effect matrices, riding kinds/grades/satiety, transform stages/races,
  Bof, curses, passive skills, guild bonuses, resources and profile snapshots.
  Assertions compare composed outputs and preserve unrelated session state.
- **R08 / small request families:** real Unix/SQLite round trips for malformed,
  unauthorized, unknown, wrong-NPC, dead, busy, teleport, map and distance
  crafting guards; exact inventory, binding and money persistence for success
  and rejection. Talent tests cover malformed input, persistence rollback,
  rest, Beacon and teleport behavior, plus real Earth Boom control rejection
  and recovery after the control deadline.
- **R09 / quest target selection:** observed attack/travel choices, stable ties,
  missing catalogs/groups, chance filtering, unrelated drops, level changes,
  objectives, reload and reachability through an owned portal. Worker integration
  also exercises the real login timeout and transport closure.
- **R10 / item picker:** all eleven tabs, ordering/filter combinations,
  preselection/selection restoration, mouse/Ctrl/Shift/Return/double-click,
  stale IDs, occupied/overlay/placeholder pixels, owned BMP decode and 2x DPI.
- **R12 / inventory:** an independent classification oracle, all 216 slots,
  all 64 filter masks and 64 writable masks at three scales, shortened bags,
  expiry, catalog reload, zero-count entries and public interactions.

`tests/test_coverage_report.py` separately guards the measuring tool: uncovered
branches cannot be hidden by executed lines, compatible object counters merge,
different functions/topologies remain distinct, exception arcs remain counted,
missing ranges fail, source drift/stale objects fail, and errors invalidate an
older successful report.

These tests characterize current behavior. They do not yet assert the future
algorithms' linear work bounds, queue overflow policy, cache invalidation or
new index lifecycle. Add those checks with each implementation. No performance
gain or complete concurrency/graphics validation is claimed here.

## Reproduction

Use GCC with its matching `gcov` and Qt 6.9+ for the optional tool tests. The
local run uses GCC/gcov 11.4.0 and Qt 6.9.0. Replace the Qt prefix as appropriate.

```bash
cmake -S . -B /tmp/openkal-coverage -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DOPENKAL_ENGINE=null -DOPENKAL_DATA_ROOT= \
  -DOPENKAL_BUILD_TESTS=ON -DOPENKAL_BUILD_TOOLS=ON \
  -DOPENKAL_BUILD_AREA_EDITOR=OFF -DOPENKAL_BUILD_PROFILE_CLIENT=OFF \
  -DOPENKAL_RUN_USER_DATA_TESTS=OFF -DOPENKAL_WITH_POSTGRES=OFF \
  -DOPENKAL_BUILD_TEST_PLAYERS=ON -DOPENKAL_BUILD_LOOT_EDITOR=ON \
  -DCMAKE_PREFIX_PATH=/absolute/path/to/Qt/6.9.x/gcc_64 \
  -DOPENKAL_ENABLE_COVERAGE=ON
cmake --build /tmp/openkal-coverage --parallel 8
```

Before a clean measurement, clear only generated counters in that dedicated
build directory. Keep source files frozen throughout build, tests and collection.

```bash
python3 - <<'PY'
from pathlib import Path
for profile in Path('/tmp/openkal-coverage').rglob('*.gcda'):
    profile.unlink()
PY
ctest --test-dir /tmp/openkal-coverage --output-on-failure
cmake --build /tmp/openkal-coverage --target openkal_coverage_report
```

Local Unix/TLS integration needs permission to create local sockets. Qt picker
tests set their own offscreen environment. The report is written to
`/tmp/openkal-coverage/coverage/index.html` and `coverage.json`; it shows each
area's raw line/branch counts and exact uncovered source lines/arcs.

The CMake reporting target uses `--report-only`: feasible residual gaps do not
fail the build. Invalid/missing/stale measurement data still fails. For the
optional diagnostic strict 100% check, invoke `tools/coverage_report.py` with the
same arguments and omit `--report-only`. Its `all_runtime_areas_complete` flag
means literal raw 100%, not acceptance of this test-preparation work.

Counters are combined across compatible compiled instances, including test-only
inclusions of unchanged source. The reporter keeps distinct function/template
and branch-topology variants separate. Line coverage is the union of execution
across these instances. Branch totals include exception and compiler cleanup
arcs; `throw: false` alone does not identify a business decision.

## Verification results

The initial review baseline was 132 enabled passing tests and 17 disabled
optional-user-data tests. The added preparation consists of 12 C++ test files,
14 C++ CTest entries and one Python CTest entry containing 11 reporter checks.

- **Instrumented Debug/null plus Qt tools:** all **150 enabled tests passed**
  in 113.60 seconds; 17 optional-user-data tests remained disabled (167 registered).
- **Ordinary Debug/null, coverage OFF and Qt tools OFF:** all **144 enabled
  tests passed** in 67.86 seconds; 17 optional-user-data tests remained disabled
  (161 registered). All 990 compile commands lack `--coverage`; the opt-in build
  has it on all 1,049 compile commands.
- The last added single-line fallback case passed on a targeted rebuild/rerun
  in both configurations. The final report retains the clean full-suite
  counters plus this target's fresh rerun. C++ formatting, Python compilation,
  the 11 reporter checks and `git diff --check` also passed.
- No measured source range lacks instrumentation. Source hashes and object
  freshness checks passed. No PostgreSQL or real GL rendering run is claimed.

Raw per-area results are recorded below. These percentages include exception
arcs and compiler bookkeeping, with no exclusions. R11 has the successful
build/test verification above instead of an executable coverage percentage.

- **R01:** lines 139/139 (100%); branches 146/186 (78.49%).
- **R02:** lines 104/104 (100%); branches 118/144 (81.94%).
- **R03:** lines 60/60 (100%); branches 109/140 (77.86%).
- **R04:** lines 47/47 (100%); branches 50/52 (96.15%).
- **R05:** lines 185/185 (100%); branches 106/138 (76.81%).
- **R06:** lines 63/64 (98.44%); branches 67/86 (77.91%).
- **R07:** lines 848/856 (99.07%); branches 465/623 (74.64%).
- **R08:** lines 188/188 (100%); branches 139/170 (81.76%).
- **R09:** lines 46/46 (100%); branches 71/80 (88.75%).
- **R10:** lines 398/404 (98.51%); branches 425/674 (63.06%).
- **R12:** lines 87/87 (100%); branches 136/148 (91.89%).

Logs: `/tmp/openkal-coverage-ctest-final.log`,
`/tmp/openkal-noncoverage-ctest-final.log`,
`/tmp/openkal-multiline-last-test-0.log`,
`/tmp/openkal-multiline-last-test-1.log` and
`/tmp/openkal-coverage-report-final.log`.

## Remaining reachability limits

These explanations do not remove any line or branch from the raw report.

- **Linux errno aliases:** `EAGAIN == EWOULDBLOCK == 11` on the measured platform.
  In `Transport.cpp:75,109` and TLS BIO callbacks `TlsSession.cpp:36,46`, the
  second equality cannot be true after the first equality was false. Both errno
  names and actual retry/error outcomes are tested. This proof is platform specific.
- **Client owner guards:** real single-threaded lifecycle paths enter
  `handshake`, `update`, `close`, `pump_rx` and `process_frames` with a nonnull
  session owner; transport failures close the descriptor without resetting that
  owner. Defensive null branches at `NetClient.cpp:232,263,1467,1553,1570,1619`
  therefore need reentrant/concurrent state mutation absent from the real
  implementation. Dead-session outcomes are separately tested.
- **TextWrap loop:** at `TextWrap.cpp:93`, `start <= text.size()` is always true
  when evaluated: initially zero, later at most a real newline plus one or a
  cursor bounded by the input. Termination uses the explicit break. Invalid
  storage/overflow is not a valid `string_view` test case.
- **Combat gear:** `Host.cpp:1843–1902` appends `WornGear` only after checking the
  item definition; no later code clears those pointers. Null guards/ternaries
  at `2105,2115–2121,2128,2188,2472,2482–2489` have no valid calculator input.
  Both weapon/armor and all actual bonus paths are tested.
- **Combat finite domains:** Qigong/Dragon helpers only yield primary kinds
  0–4, making `Host.cpp:2024–2025` unreachable. Validated riding IDs yield kind
  0–5, so the last `== 5` check at `2589` cannot be false after 0–4 were rejected.
  Transform defense table columns are either both zero or both positive; the
  second true operand at `2638` cannot occur after the first was false.
- **Talent datastore:** gameplay workers start only after successful store
  creation and stop before it is reset; null-store guard outcomes at
  `Host.cpp:12724,12746` are outside a running server lifecycle. Real save
  failures and rollback with a present store are tested.
- **Crafting NPC identity:** `Host.cpp:10579–10580` stores an active crafting
  NPC ID only when that immutable NPC already has the crafting dialog ID;
  all other interactions clear it. Once lookup succeeds at `10674`, the wrong-
  HTML outcome at `10676` cannot occur without changing the catalog during
  dispatch. Missing/invalid NPC, map, distance and activity guards are tested.
- **Quest catalogs:** public loaders discard nonpositive outer/inner itemgroup
  chances and drop attempts. The corresponding defensive outcomes at
  `TestPlayerAgent.cpp:1121,1125,1159` cannot arise from loaded catalogs.
- **Worker receive cap:** the concrete spectator `PosixSession` caps input at
  32,000 bytes, with frames at most 8,000 bytes. It cannot reach the helper's
  separate >1 MiB guard at `TestPlayerWorker.cpp:33`; a synthetic-session helper
  test covers that contract failure without claiming the concrete variant did.
- **Picker object lifetime:** lists captured by Qt signals originate from the
  nonnull factory; QApplication must exist before constructing the QWidget.
  Icon lookup guarantees a decoded icon or placeholder. Thus null-list/qApp/
  base-icon outcomes at `ItemPickerDialog.cpp:179,192,207,264,280` are not
  ordinary valid UI actions.
- **Picker synchronous catalog invariants:** sorting receives IDs just found
  in the immutable catalog (`320–321,491`); every `class_small` substring is
  already contained in `clazz` (`457–458`); constructor-created tabs remain
  in 0–10 (`385`). Stale catalogs, no-match filters and every real tab are tested.
- **Compiler records:** picker argument lines `278,295` have zero attributed
  counts despite executed drawing calls and verified pixels; closing brace
  `311` belongs to exception cleanup. Extra delete-null arcs at `479,481` sit
  inside proven nonnull source guards. QString temporary cleanup at
  `500,501,572` flows to `_Unwind_Resume`. Similar combat expression blocks at
  `1878,2113,2118,2480,2486` remain raw zero-count records despite real effect
  factory calls and output assertions. They are retained, not reported executed.
  `GuildWindow.cpp:620` similarly records zero for the literal `1` argument of
  `std::max`, with `unexecuted_block: false`; the surrounding maximum/page
  calculation executed 62 times. That raw line remains uncovered in R06.
- **Exception paths:** remaining allocation, virtual-call and temporary cleanup
  arcs are included. Some are feasible fault conditions and remain untested;
  they are not all called impossible. Targeted client allocation and send
  failures plus text-measure callback exceptions are tested. Concrete default-
  allocator FIFO erase cannot allocate and uses nothrow movable/destructible
  values, but this does not establish a blanket no-throw property for enqueues.

## Existing behavior requiring separate decisions

The tests explicitly label these observations as current behavior, not desired
requirements for future correctness work:

- Client EOF cleanup increments serials twice and repeats on later updates.
  `close`/`adopt_session` retain partial receive bytes and pending chat; an old
  partial frame can complete after adoption. A malformed semantic packet closes
  transport while later frames in the buffered batch can still dispatch.
- Picker flat-tab constructor preselection is lost while the dialog is hidden
  (`ItemPickerDialog.cpp:139–143,528–552`). Its private flat success branch cannot
  be reached through that sole production caller until this defect is fixed.
- Picker filtering/restoration can duplicate or lose selected IDs because
  selection signals reenter collection (`202–224,602–629`).
- Qt double-click emits both doubleClicked and activated. Both handlers accept;
  the second collects after hide and clears the chosen IDs (`166–169,635–641`).
  Return activation is separately checked and retains the selection.

Do not silently preserve these defects as a new product specification. A pure
performance patch should keep its scope clear; authorize and test a separate
behavior fix if these policies are changed.

Before changing an area outside the manifest, extend its scope and regression
cases first. Recheck source ranges/fingerprints intentionally after each future
patch; never update fingerprints merely to bypass a drift failure. Run the
relevant regression tests and the appropriate full configuration again.

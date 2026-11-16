# Open engineering work

This is the canonical implementation and technical-delivery backlog, updated
2026-11-16. The [source-release review](docs/OpenSourceReleaseChecklist.md)
tracks licensing, redistribution, repository contents and public statements.
These engineering tasks do not determine whether the repository has adequate
rights to publish its contents. An emulator source release may contain bugs,
unfinished features and known limitations.

The existing F01–F26 IDs and acceptance criteria are preserved. Schedule by
feature scope; F01 is the first priority before promising durable Battle Riding
progression. Do not reopen completed optimization/refactoring packages without
fresh evidence of a defect. Optional game files remain separately supplied by
users; obtaining or licensing their files is outside this backlog.

## Implementation and gameplay

- [ ] **F01 — Persistent Riding transactions [P1 for stable persistent play].**
  Make Cache opening and Stablemaster/combine/feed/enchant/upgrade operations
  atomic across inventory, currency, expiration and mount progression, with
  durable retry protection and failure injection. `open_riding_cache` in
  `Host.cpp` publishes the staged bag before persisting individual rows and
  ignores the deletion result. Later mutation handlers also perform separate
  writes. See [Battle Riding Cache](docs/BattleRidingCachePlan.md). Done: failed writes/restarts
  cannot lose or duplicate items/progression; grade changes preserve or return
  gear references according to an explicit tested rule.
- [ ] **F02 — Battle Riding JSON rules [P2].** Load grades, abilities, equipment
  and restriction rules rather than relying on matching C++ defaults. Done:
  owned tests prove valid edits change behavior and invalid rules fail clearly.
- [ ] **F03 — Battle Riding abilities [P2].** Implement unique server-side rolls,
  grade-specific initial slots, effects and six-slot presentation. Done:
  deterministic owned RNG tests cover uniqueness, slot limits and effect removal.
- [ ] **F04 — Riding rejection feedback [P2].** Define stable results for every
  rejected panel mutation. Done: malformed, unauthorized, stale, full-bag and
  unavailable-service requests receive tested, consistent feedback.
- [ ] **F05 — Riding restrictions and permanent collection [P3].** Complete
  fishing/drop restrictions and permanent account history/bonuses separately from
  timed mount ownership. Done: action/map/PvP rules and dismounted effect removal
  have owned coverage.
- [ ] **F06 — Mounted presentation and lifecycle coverage [P3].** Complete
  mounted combat/observer synchronization and panel/gear/forced-dismount tests;
  cover logout/restart expiration, deletion, catalog changes, satiety, destruction
  and competing mutations. Keep unavailable optional meshes explicitly unsupported
  rather than fetching replacements. Done: supported paths have synthetic tests
  and presentation claims identify their actual tested scope.
- [ ] **F07 — Riding acquisition and species policy [P3].** Decide cache/drop
  acquisition, costs and additional species mappings as project policy.
  Done: documented budget and tests preserve selected species and distinguish
  travel mounts from Battle mounts. See `docs/BattleRidingCachePlan.md`.
- [ ] **F08 — Knight costume clipping [P2].** Resolve 25003/25004 across complete
  garment boundaries, intermediate motion, self-contact and temporal continuity.
  Done: no new distortion or neighboring-surface intersections across the stated
  tests. Keep rejected experiments disabled. See `docs/SkinMotionReview.md`; changes
  to player rendering require an explicitly scoped rendering task.
- [ ] **F09 — Equipment appearance verification [P3].** Finish supported animation,
  effects and mixed-outfit checks. Done: record tested combinations and limits
  without converting static previews into full-motion claims. Missing optional
  textures for 25160/25313/25396 and unresolved classifications remain
  documented limitations; acquiring/researching them is excluded from this queue.
- [ ] **F10 — BOF verification [P3].** Complete class-specific attachments,
  timing/looping, materials and observer verification for supported sets. Done:
  owned checks and bounded visual observations establish more than a visible
  particle or catalog name. See `docs/BOF.md`.
- [ ] **F11 — Mitigation playtesting [P3].** Evaluate high avoidance together
  with healing before changing Defense/Absorb/evasion constants. Done: document
  scenarios and measured outcomes; retain current integer-boundary guards.
- [ ] **F12 — EXP noticeboard placement [P3].** Verify Temporary Fort placement,
  clearance and two-sided readability. Done: record supported world placement
  separately from the procedural preview. See `docs/TASKS.md` and `docs/world/ExpEventBoard.md`.
- [ ] **F13 — Doggebi encounter validation [P2].** Exercise all skills, teleport,
  Spirit Absorb, 10-second spawn immunity and the one-hour Ascension/despawn
  lifecycle. Done: owned integration evidence plus explicitly bounded live checks.
- [ ] **F14 — Extend Doggebi controller regression [P2].** Extend the existing
  `tests/DoggebiLordServerTest.cpp` and phase fixtures for remaining encounter
  transitions and failure boundaries. The current real-transport test covers
  complete/incomplete monument layouts, countdown and shared prepare/fire
  observation; it passed. Done: the still-missing full encounter/expiry coverage
  has explicit owned cases and detects a deliberate transition regression.
- [ ] **F15 — Doggebi audio [P3].** Supply project-created synthetic cues
  for ritual, earthquakes and Lord skills, document origin and verify timing.
  External sound searches and asset-pack imports are outside this queue.
- [ ] **F16 — Doggebi tuning [P3].** After F13/F14, tune damage, cooldowns, spirit
  count and supported hitboxes. Done: changes have explicit scenario evidence
  rather than relying on names or sound playback. See the Doggebi packages F13–F16 below.
- [ ] **F17 — Pet mixing design decisions [P3].** Resolve the nine open questions
  in `docs/PetMixing.md`: result IDs, outcome probabilities, recipe matrix, reuse cap,
  reroll costs/results, option creation time, persistence/history, protocol and
  five-minute suppression semantics. Use an explicit OpenKal design rather than
  external-source research. Done: the spec has concrete testable decisions.
- [ ] **F18 — Pet mixing implementation [P3; after F17].** Implement Crossbreed
  and Monster Pet Creation with separate transactional services, instance state,
  protocol/UI and failure/retry tests. Done: costs/results/rollback match the
  approved project design with synthetic fixtures.
- [ ] **F19 — Guild expansion [P3].** Scope broader alliance, flag, territory,
  treasury and raid features beyond the implemented Castle War rules. Done:
  each new capability has authoritative rules, persistence, authorization and
  tests. Track the deployed siege layout and remaining siege interactions in
  [Castle War](docs/CastleWar.md#follow-up-work-for-the-original-castle).
- [ ] **F20 — High-value transactions [P3].** Choose a currency limit and mechanism
  before implementing larger transactions. Done: overflow, limits and atomic
  transfers are specified and tested. See `docs/FeatureRequests.md`.
- [ ] **F21 — Remaining inventory expansion [P3].** Decide whether the request
  requires capacity beyond the current 216 General slots or different tabs from
  the existing sections and six category filters. Wheel scrolling already exists.
  Done: any additional capacity/tab change has explicit full-bag, moved-slot and
  existing-save behavior with persistence/protocol tests; do not duplicate the
  current scrolling/filter implementation.
- [ ] **F22 — Multiple enhancement operations [P3].** Define any batch behavior
  before implementation; preserve per-operation resource, chance and destruction
  rules. No guaranteed +15/+16 process is currently specified. Done: a complete
  rule set and atomic/failure tests precede enabling the action.
- [ ] **F23 — Optional emote chat commands [P3].** Decide whether to add command
  aliases to existing actions. Done: any enabled aliases retain server validation
  and match Action-tab/hotbar behavior. See `docs/Emotes.md`.
- [ ] **F24 — Transform progression policy [P2, before claiming complete rules].**
  `libs/protocol/include/openkal/protocol/Transform.h:346` still records unresolved
  worn-egg stat/EXP behavior around levels 40 and 68. The active
  `worn_transform_bonus` at line 353 applies the factor-based formula; the pending
  comment is not evidence that this formula is wrong. Define an explicit OpenKal
  policy using current code and synthetic requirements, document its limits and
  replace the unresolved instruction with that decision. Done: owned boundary
  cases cover levels 39/40/41 and 67/68/69, invalid races and maximum supported
  levels, with consistent UI/server results. External formula/source research is
  excluded; preserve current values unless the policy intentionally changes them.
- [ ] **F25 — Thief base-stat policy [P3].**
  `libs/server/src/Combat.cpp:34` still labels the active Thief base-stat row as a
  placeholder. Decide whether the current DEX-focused values are the intended
  OpenKal defaults, document that decision and its effect on creation/reset, and
  add meaningful stat/allocation boundary cases if behavior changes. Done: the
  supported class policy is explicit and consistent across creation and combat;
  no unsupported claim of initial-game fidelity or unrequested rebalance.
- [ ] **F26 — Absorb rating explanation [P3].**
  `docs/DamageReduction.md:97` explicitly defers a Character Info tooltip that
  explains raw rating, effective reduction and the PvP bonus. Implement this in
  the existing TrueType/data-driven UI when scheduled. Done: examples at the
  rating-40 boundary and raw rating 100 agree with server rounding, distinguish
  PvP, and never describe raw rating as a direct immunity percentage. Read the
  UI-schema skill for that implementation; no mitigation constants change.


## Technical verification and delivery

These tasks were moved out of the legal/source-release plan. IDs R13, R15 and
R16 remain stable for existing references. Their completion does not establish
permission to redistribute code or content.

- [x] **R13 — Final technical candidate verification.** Build from a Git-free
  extraction of the candidate archive after code or content corrections. Run
  the owned suite and the relevant graphics and database tests. Use the exact
  candidate bytes and record versions, commands,
  failures/skips and checksums. Done: the claimed technical support matches the
  actual results. A source-only legal review does not require all game features
  or these capability checks to pass.
  The local 0.1.0 candidate verification is recorded below. Repeat the checks
  if the selected source or build inputs change.
- [ ] **R16 — CI results [P1 for repository delivery].** Inspect `cpp-style`,
  `build-null`, `release-client` and `postgres` for the actual delivery revision
  in `.github/workflows/ci.yml`; resolve failures required by repository policy.
  Reject skips as evidence of graphics/database functionality. Done: record the
  revision and required job results. PR #331 passed all four jobs before the
  merge. Verify CI again for the revision containing the local release changes.
- [ ] **R15 — Publication and release assembly [P1 when publication is
  requested].** OpenKal 0.1.0 selects the versioned source archive and its
  SHA-256 checksum for publication. Both are staged locally. Keep release notes
  and the [source-release review](docs/OpenSourceReleaseChecklist.md) aligned
  with the selected files. Publication and tagging await explicit instruction.

## Local verification on 2026-11-16

The 0.1.0 candidate uses the tree at `13a6c43e` plus local release fixes.
The build input was a Git-free extraction of the source manifest with
`OPENKAL_DATA_ROOT=` and user audio disabled. The environment was Ubuntu
22.04.5 amd64, GCC 11.4.0, CMake 3.22.1, Python 3.10.12 and OpenSSL 3.0.2.

- Headless build and CTest: 160 passed, zero failures. Seventeen tests labeled
  `optional-user-data` were disabled.
- Release/OpenGL on the local display: all five CI-selected regressions passed
  without skips. The Debian installation contents and runtime dependencies
  passed the package checker, and all five isolated launcher tests passed.
- PostgreSQL 18.6: all five CI-selected integration tests passed without
  skips against a temporary database bound to localhost.
- Clang-format 18.1.8 passed for all 875 project C/C++ files. The source archive
  repackaged byte for byte. The source archive hash is in `SHA256SUMS`.

## Earlier technical evidence

The 2026-11-04 earlier recheck rebuilt Debug/null and PostgreSQL targets. Its
owned suite had 157 passed, zero failures/skips and 17 optional-user-data tests
disabled (178.19 seconds). Four isolated PostgreSQL 18 tests passed without
skips (32.26 seconds). Five existing Release/OpenGL regression executables passed
with local display access; those graphics binaries were not rebuilt in that
recheck. The disposable database container was removed.

The existing Debian artifact passed its content/dependency/library check; five
launcher cases, two source-packaging cases and eleven skin-preview cases passed.
All 863 project C/C++ files passed clang-format 18.1.8 and had the expected
project header. These are earlier bounded engineering results, not a rights
clearance or evidence for future changed bytes. No new builds or gameplay tests
were needed for the documentation reorganization.

## Scope and work references

The detailed feature specifications remain in [the work index](docs/TASKS.md)
and [feature requests](docs/FeatureRequests.md). F08/F09 cover the former cosmetic
TODOs, and F13–F16 cover the former Doggebi TODOs; they are not duplicate tasks.
Keep three unavailable appearance textures explicitly unsupported until usable
inputs exist, without turning asset acquisition into a source-release gate.
No player-rendering, terrain, camera or culling change is authorized by this list.

# Open-source release: repository rights and content review

Execution record: **2026-11-06**. L01–L08 have been worked through on the current
source tree within the boundary below. The local content checks and concrete
editorial corrections are complete. A follow-up on **2026-11-06** removed external
name-reference annotations and additional source-file/formula-table references in
comments, preserving the actual names, values and behavior. The declared current
project understanding is that implementations were generated and formulas
were examined, with no known copied implementation. This is a qualified development
record, not independent verification of every implementation's history or an
unconditional legal release clearance.

The post-merge full-tree rescan is recorded under **L09** below. It identifies
the requested dialog-comment correction. The subsequent documentation update
replaces effect-table origin wording with the client asset reference and records
the repository-wide wording review under L09-A.

Technical TODOs, feature behavior and release delivery were not changed by this
execution. [TODO.md](../TODO.md) and [docs/TASKS.md](TASKS.md) are byte-for-byte
unchanged from the start of this turn. No commit, push, tag or publication was
performed. Known gameplay bugs do not determine this source-content review.

## Boundary and exclusions

The publication set reviewed on November 6 contained **2,277 tracked files**, exactly
matching `SourceFiles.txt`. The review includes code, configuration, fixtures,
artwork masters/exports, audio, documentation, authoring records and vendored
software, rather than only what happens to enter a runtime package.

Compatible game files remain separately supplied by users and outside
this review. No external game directories, private packs, Git history, private
chats or remote reference/legal sources were opened. Existing repository origin
records were read as records, not reauthenticated through their historical links.
Normal use of compatible filenames, item IDs, coordinates and format descriptions
is not classified as infringement merely because it refers to another game.

The resource packer's external-input support remains unchanged; former R02 stays
excluded. External images/audio present only as reference descriptions are not
included source artifacts or an independent permission-collection task. This
execution selects a **source-only candidate**. Binary-distribution obligations
have not been cleared for any new binary release and do not block reviewing the
source set.

## L01 — Publication boundary [Completed]

- [x] Compare `git ls-files` with `SourceFiles.txt`: 2,277 tracked and selected
  files, no omissions/extras, regular in-tree files only. No build tree, save,
  private credential file or separately supplied installation is selected.
- [x] Classify actual bundled bytes: 170 images (56 PNG, 99 BMP, 12 SVG, three
  DDS), three plant models, three synthesized MP3s and one synthetic area fixture.
  The image count already includes the three plant textures. No font files,
  executable/library binaries or runtime game installation are listed.
- [x] Select source-only verification; retain optional binary recipes as recipes,
  without asserting that a new binary artifact has passed license review.

Decision: **retain the existing source file set**. No runtime data was imported,
removed or replaced. Exact final source hashes and a source-only archive are
recorded in the ignored local `build-legal-review-2026-11-06/` directory under L08.
That directory is not part of the public source set.

## L02 — Project and third-party license records [Completed with stated scope]

- [x] Check root `LICENSE`, contributor guidance, project header declarations,
  `THIRD_PARTY_NOTICES.md` and `debian/copyright` for the stated GPL-3.0-or-later
  project license. All 863 project C/C++ files have the expected project header.
  That checks declarations, not authorship of each implementation.
- [x] Inspect the complete `third_party/` file set: only miniaudio 0.11.22 and its
  MIT-0 license text are vendored. The standalone MIT-0 text matches the embedded
  alternative after whitespace normalization; attribution is preserved.
- [x] Clarify the notices and Debian copyright comment: project licensing applies
  to OpenKal contributions and is not a blanket assertion of exclusive authorship
  or permission for external inputs and retained community labels.
- [x] Separate build/system dependencies from copied source. No dependency
  implementation other than miniaudio is listed as vendored. Optional binary
  license/relinking/source obligations remain outside this source-only result.

Decision: **retain GPL and miniaudio notices**, with the clarified scope. No
missing mandatory notice for the identified vendored component was found. The
community annotation and historical-origin limits below prevent treating this
formal license check as proof that every file has a complete chain of rights.

## L03 — Catalog reference cleanup [Completed; name choices applied]

- [x] Inspect the four flagged current files without opening external inputs.
- [x] Classify their actual content without treating technical identifiers as
  copied implementations or changing existing names merely because of their origin.
- [x] Remove the requested reference annotations from `config/skins/names.json`
  and `config/skins/weapon-templates.json`; preserve all other curated content.

**Skin Artisan — retain technical configuration.**
`config/npcs/60002_skin_artisan.json` remains numeric placement/appearance
configuration with service identifiers and a short display label. Its transfer
record is unchanged; it includes no external mesh, texture or implementation.

**World objects — retain technical mappings.**
`config/world_objects.json` retains 82 model-path rows, particle identifiers, bone
numbers and offsets. No external model, texture or particle implementation is
included in this file. Neither configuration was regenerated or modified here.

**Name and weapon catalogs — remove reference annotations, retain their values.**
Both catalogs now use a neutral `source.scope` description. External origin paths,
input hashes, author/permission assertions and confirmation dates have been removed
from those source blocks. All 77 `source_lines` fields and the three separate
`reference_names` arrays (eight reference labels) have been removed. The audit no
longer requires or exports the removed source-line fields.

The name-selection follow-up excludes candidates beginning with Returned,
Artificial or GM, including Imperial GM variants. Forty candidates are removed.
Ten unique surviving names are applied to the item definitions, including 25044
as `Blood Dragon's Sword`; their descriptive fallback entries/notes are removed.
Skins have no time-limited variants. A further 28 candidates with duration
annotations are removed, and seven additional timeless names are applied.
The final thirteen names are explicitly selected and synchronized with the item
definitions, including `Blood Dragon Stick`, `Blood Dragon Imperial Stick` and
the two existing Violet Wing descriptive labels. The latter retain their initial
name origin. Their descriptive fallback entries/notes are removed as well.
There are now 74 resolved names, including the eight existing project overrides,
and no pending name records.
No ID, class, model coordinate, weapon
category, geometry fingerprint or gameplay value changed. The 27 model-comparison assignments and three
additional technical category mappings remain functional. They compare model
coordinates/geometry; they are not retained name-reference annotations.

Decision: **apply the explicit naming rules and final name choices; the name
selection is complete**. Tests no longer derive weapon categories
from the wording of a name. Actual category/model guards and catalog tests remain.
No new ownership or permission claim follows from removing metadata, and no finding
of copied implementation follows from an ordinary display name. This record does
not erase or reauthenticate the earlier review; it describes the current cleanup.

## L04 — Distributed resources [Inventory complete; recorded basis retained]

- [x] Validate all 170 image entries and SHA-256 hashes in
  `docs/provenance/Images.json`; all current image files have an entry and a source
  family. Retain the existing per-file records and declared distribution license.
- [x] Reproduce all three plant models and three DDS textures byte for byte from
  `tools/generate_aloe.py` and `tools/generate_crafting_plants.py` in a temporary
  output directory. Their geometry/colors are procedural; no game files are read.
- [x] Reproduce the synthetic area fixture byte for byte with
  `tests/map/GenerateAreaFixture.cpp`; its local README describes the owned test
  pattern. Keep the fixture separate from actual game maps.
- [x] Inspect the three MP3 files' presence and the synthetic-origin record in
  `RuntimeData/sounds/README.md`. Their stated basis is mathematical synthesis,
  not inclusion of reference recordings. Audio synthesis was not independently
  reconstructed during this execution.
- [x] Check the publication set for fonts and the twelve SVGs for embedded/remote
  images, script or font declarations: none found. System fonts are not bundled.

All 30 image families below are **retained on their existing local project-origin
records and declared GPL-3.0-or-later distribution basis**. The listing describes
those records, not a new historical or legal authentication. The per-file hashes,
inputs and evidence limitations remain in the image manifest.

- `logo`: 1 files; image_generation.
- `unlock`: 1 files; image_generation.
- `riding`: 4 files; image_generation.
- `bound`: 1 files; image_generation.
- `coin`: 1 files; image_generation.
- `exp`: 1 files; image_generation.
- `potions`: 8 files; image_generation.
- `auto-hp`: 4 files; image_generation.
- `hud`: 20 files; image_generation.
- `minimap`: 5 files; image_generation.
- `menu`: 15 files; image_generation.
- `dialogs`: 24 files; image_generation.
- `long-controls`: 7 files; image_generation.
- `collection`: 7 files; image_generation.
- `messages`: 2 files; image_generation.
- `paperclip`: 2 files; original_vector.
- `inventory-vectors`: 15 files; original_vector.
- `cursors`: 3 files; procedural_artwork.
- `talents`: 13 files; image_generation.
- `polishing`: 1 files; image_generation.
- `revision`: 1 files; image_generation.
- `talismans`: 6 files; image_generation.
- `aloe`: 1 files; image_generation.
- `ingredients`: 2 files; image_generation.
- `plant-textures`: 3 files; procedural_artwork.
- `skin-icons`: 2 files; procedural_artwork.
- `skin-ticket`: 1 files; image_generation.
- `party`: 6 files; original_vector.
- `compact-chrome`: 9 files; project_procedural_artwork.
- `trading-post`: 4 files; retained_project_artwork.

No image, audio or model bytes changed. Some family records describe earlier
external style/layout references; those reference files are absent from the
publication set and were not opened. Their mention alone establishes neither
copying nor infringement. Generated origin and matching hashes do not establish
copyright ownership, originality or the absence of protected similarities.
The current source records are the accepted bounded inventory evidence; a new
external-reference rights investigation is excluded.

## L05 — Implementation-origin comments [Completed within the current-tree scope]

- [x] Keep the earlier six C++ comment-block corrections and project license headers.
- [x] Check current project C/C++ text for explicit references to foreign source
  filenames, implementation-copying claims and internal formula-table names.
- [x] Clarify further comments in seven headers: outdoor attribute bits,
  BState/monster/property identifiers, monster race values, worn-egg formula limits
  and Defense Strengthening. Describe the current data or calculation rather than
  implying that an implementation was copied from a particular source file.
- [x] Format changed headers with clang-format 18.1.8 and compare non-comment C++
  tokens with the turn-start bytes: identical. No formula, constant, ID, runtime
  algorithm or gameplay behavior was changed.

**Declared development approach:** the project's current understanding is that
implementations were generated, formulas were examined for compatibility,
and no implementation is known to have been copied. That statement retains its
qualification; it is not an independently reconstructed creation history or a
personal approval substituting for permissions.

Decision: **retain the implementations and keep comments factual**. No concrete
copied implementation was identified in this bounded review, so no algorithm was
rewritten merely because an old comment named a source header or formula table.
Future concrete copying findings require a targeted implementation decision.
Comments describing ordinary copying of data from an appearance rig are unchanged;
they describe an in-memory operation, not source-code provenance.

History, externally supplied game files and private source were not consulted.
Historical independent authorship is not proved by comment cleanup, token equality,
project headers or passing tests. No unqualified independent-authorship assertion
has been inserted into the code.

## L06 — Personal authority narratives and public metadata [Completed]

- [x] Replace all sixteen `user_confirmed` values in `config/skins/catalog.json`
  with `manual_check_recorded`, keeping every observation and its verification
  limits. The separate preview catalog remains unchanged.
- [x] Add an asset-free regression that rejects personal-authority status values
  in both catalogs. Extend the existing synthetic audit-export case to verify
  neutral status output in JSON, CSV and Markdown, including unverified behavior.
- [x] Confirm the guard fails if a `user_confirmed` value is reintroduced.
- [x] Scan the current manifest text for personal authority/request narratives and
  personal absolute paths, and scan selected bytes/filenames for private keys,
  credentials, databases and obvious credential tokens. No actual credential,
  private-file entry or personal absolute path was found by these bounded checks.

Decision: **neutral factual public review metadata**. Repository URLs, maintainer
email/copyright fields and relevant provenance identifiers remain. The name in an
unchanged authentic generation prompt is not an approval statement. Regression
fixtures and this record may name rejected status values as examples; active
catalogs and generated reports no longer use them as review authority.
These editorial corrections are not evidence of a rights grant.

## L07 — Public descriptions and license scope [Completed]

- [x] Clarify in `README.md` that OpenKal is an independent compatibility/emulator
  project, claims no official affiliation/endorsement, and needs separately
  supplied compatible game files.
- [x] Align notices, Debian copyright comment, release notes and skin documentation
  on project contributions versus retained compatibility metadata. Avoid claiming
  that all retained annotations were exclusively authored or individually cleared
  by OpenKal. Contributor guidance already forbids copying proprietary code.
- [x] Preserve factual provenance, relevant authorship/contact information and
  documented gameplay limitations. Leave both technical TODO files unchanged.

Decision: **publish bounded descriptions of actual contents**, with no guarantee
of universal legality under German law or of permission for arbitrary user packs.
Compatibility naming remains; no automatic project/game renaming was introduced.

## L08 — Final source-content checks [Completed locally; no blanket clearance]

- [x] Recheck the tracked source set, inventory consistency, license texts/notices,
  intended exclusions and changed-file formatting. No source file was added or
  removed; the selected set remains 2,277 files.
- [x] Generate `SourceFiles.sha256` and `Review.json` in the ignored local
  `build-legal-review-2026-11-06/` directory. Record per-file hashes, source family
  dispositions, the retained L03/L05 limits and the two protected TODO hashes.
- [x] Generate a manifest-only source archive there; verify every member against
  the selected current bytes, recheck inventory/license files from its Git-free
  extraction, and record archive SHA-256. No build or save files enter the archive.

The local archive is a **review snapshot**, not an authorized release, a cleared
binary artifact or an assertion that unresolved origin questions have vanished.
L01–L08's requested current-tree inspection and feasible corrections are complete.
The final name choices are applied; no pending name records remain. Historical
authorship is not independently reconstructed; L05 records the qualified current
development understanding instead of claiming proof. If an unconditional legal clearance is
required, L08 cannot supply it from the evidence allowed here. Changing comments,
license scope wording or status values does not cure genuinely unauthorized
material if any is later established.

## Verification and reproducibility

Five selected CTest entries passed without skips: `skin_catalog_audit`,
`skin_preview_selection`, `area_map_fixture`, `source_packaging` and
`image_inventory`. The first two ran 24 and eleven Python test cases respectively.
The personal-authority guard also rejected a deliberately reintroduced value.
The catalog-reference guard and a synthetic audit export verify that reference
annotations are absent and current labels still export without source-line fields.
Six plant files and the owned map fixture reproduced byte for byte. No full
playability, graphics or database gate was added to this rights/content review.

The checks used the existing local build, project-owned synthetic fixtures and
local records. Graph discovery used Tier 2 with all relevant coverage pages and
source fallback; no graph result establishes copyright authorship. No remote
source, Git history or externally supplied game data was consulted.

Repeat the source checks with:

```sh
python3 tools/package_source.py --check-tracked
python3 tools/check_image_inventory.py
ctest --test-dir build-oss-audit-null --output-on-failure \
  -R '^(skin_catalog_audit|skin_preview_selection|area_map_fixture|source_packaging|image_inventory)$'
python3 tools/package_source.py --version 0.1.0 \
  --output build-legal-review-2026-11-06/openkal-0.1.0-source-review.tar.gz
```

Exact final hashes belong to the local verification record after this document's
bytes are frozen. Re-run the record/archive step if any selected file changes.
Technical packages F01–F26 and R13/R16/R15 remain in [TODO.md](../TODO.md), with no
edits or delivery actions from this execution. Former R03 is recorded under L06.

## L09 — Post-merge full-tree rescan, 2026-11-06

Baseline: current `main` revision
`32e614f6033aee594186677e17204db1393eb92e`, with the local comment correction
and this report update described below. Git was used only for the current
revision, tracked-path inventory and working-tree diff. No history, external
game data, private source trees, remote pages or legal sources were consulted.

### Scope and method

- Read and hash every one of the 2,277 manifest-listed current files; compare the
  manifest and tracked set in both directions. The baseline contains 2,112 UTF-8
  text files and 165 binary files. Its 516,314 text lines include the vendored
  header; the 863 project C/C++ files contain 318,060 lines.
- Screen all text for restrictive notices, origin/copying claims, source-file
  references, personal-authority narratives and source URLs. Review candidate
  matches in context, including a broader set of 434 compatibility comments.
  Inspect all eleven dialog definitions, all three bundled quest definitions,
  and the item-description inventory, grouping repeated cosmetic descriptions.
- Check every selected path and all three pages of root-scope graph coverage.
  Generation: `2026-11-06T18:19:41Z`; 2,040 paths have no recorded coverage issue,
  70 have partial parsing and 167 are excluded from graph indexing. Read all
  file bytes directly, including the 146 reported partial ranges and excluded
  assets. Graph traces contain ambiguous same-name resolutions, so exact source
  determines the loader and table findings; call-graph completeness is not claimed.
- Check byte-level private-key/token/personal-path markers across text and
  binaries, all twelve SVGs for embedded/remote images or scripts, the complete
  selected third-party file set, local dependency declarations and license texts.
  Reproduce the six plant model/texture files and owned area fixture in temporary
  output directories, without changing distributed assets.

This is a whole-tree automated screening with contextual source review, not a
manual line-by-line authorship audit or an external code-similarity comparison.
Coverage and hashes establish the inspected bytes, not their legal status.

### L09-A — Skill presentation documentation [Corrected locally]

`SkillEffectTable.h` now documents each effect identifier by its actual client
asset entries, model/texture files, attachment and lifecycle. The per-skill
reference is [SkillEffects.md](SkillEffects.md): 129 lookup keys, 172 phase rows,
all 134 effect identifiers, and the non-table channel/state paths. Unsupported
dedicated visuals are stated explicitly instead of assigning an invented asset.

OpenKal's rendering paths were built and refined through iterative implementation
and visual testing. Comments now describe those implementations directly, without
claims that they came from a foreign rendering implementation or named private
source table. The repository-wide review also replaces ambiguous implementation
origin labels, external method citations and personal-request comments with
current behavior descriptions. Runtime data lookups, formulas, IDs and dispatch
values are preserved.

The review covers project code, maintained documentation and descriptive metadata.
Technical identifiers, retained verbatim generation records and third-party
license texts retain their exact spelling. Those records must not be rewritten
as if their historical text had changed. Word choice is not a new rights grant;
the source-content review retains its stated historical/external evidence limits.

Validation for the wording update: all 863 project C/C++ files were scanned;
140 modified C/C++ files preserve every non-string, non-comment token. Of those,
101 also preserve all string tokens; the remaining changes are diagnostics,
fixture text and the procedural-viewer label. Sixteen Python files preserve their
AST structure apart from documentation and text/byte literals; 84 JSON files
preserve keys, structure and all non-text values. The eight SVG changes affect
comments only, and their current manifest hashes are updated. Gameplay and
rendering logic are unchanged.

Eight relevant CTest entries passed (`skin_catalog_audit`,
`skin_preview_selection`, `multiplayer_presentation`, `player_skill`,
`sprite_fx_parse`, `skill_buff`, `source_packaging`, `image_inventory`). The
presentation test required a rerun outside the sandbox because local Unix socket
binding was denied in the sandbox. All six runtime-icon Python tests also passed;
`SkillEffectTable.cpp` passed a C++17 syntax check. CTest used the existing local
build; token equality supports its use for these editorial changes. The static
reference check covers all 134 effect identifiers, 172 phase rows and 129 skill
keys. No new data-pack visual verification or full rebuild is claimed.

Current verification records and a source-only snapshot are stored locally under
`build-legal-review-2026-11-06/wording-update/`.

### L09-B — Dialog content description [Corrected locally]

The `DialogDef` comment described all dialog data as converted external scripts,
whereas `DialogCatalog::load_dir` loads JSON and nine of the eleven definitions
carry project-content notes. The other two, `000277.json` and
`000690.json`, contain mixing instructions and the community EXP-event service;
their lack of an individual origin note is not evidence of copied prose.

At the requested location, the comment now reads: "NPC dialog content loaded
from JSON definitions." Only this comment changed. Non-comment source is
identical; dialog files, identifiers and runtime behavior are unchanged. This
corrects the blanket description without asserting a newly verified authorship
history for the data.

### Other dispositions and verification

No additional concrete copied-implementation finding, unidentified vendored
library, missing notice for the identified vendored component, actual private
key/token, personal absolute path or active personal-approval narrative was
identified by these checks. All 863 project C/C++ files retain the expected GPL
header, and miniaudio's standalone MIT-0 text matches its embedded alternative
after whitespace normalization. No font or executable/library artifact is in
the manifest.

The public-guide links in `DragonFusing.cpp`, `Qigong.cpp` and
`MinimapAreaName.cpp` describe numerical compatibility values and short labels;
they are retained as provenance, without opening or licensing the remote pages.
The existing NPC/world-object transfer records remain classified under L03.
The Shinsu database description concerns JSON metadata, not evidence of copied
loader code. Anti-cheat name matches concern policy, unused answer identifiers
and a packet-layout comment, not a bundled anti-cheat library. Script matches
concern owned HTML reports and escaping tests, not embedded game assets.

All 170 image hashes still match the existing inventory. Six plant files and the
area fixture reproduce byte for byte. The three MP3s have only encoder metadata;
their synthesis-origin statements remain local records, not a newly reproduced
audio-generation workflow. Existing image-reference and historical-origin limits
remain; this rescan does not reopen excluded investigations.

The five selected tests passed without skips: `skin_catalog_audit`,
`skin_preview_selection`, `area_map_fixture`, `source_packaging` and
`image_inventory`. The comment correction was formatted with clang-format
18.1.8 and checked for non-comment equality. A fresh manifest-only source snapshot
is checked member by member and revalidated after Git-free extraction.

Local machine-readable evidence, hashes and the snapshot are under the ignored
`build-legal-review-2026-11-06/full-rescan/` directory. The scan script and its
pattern definitions are retained there for reproducibility. That initial snapshot
preceded the broader wording update under L09-A. The latter also adds the skill
reference to SourceFiles.txt (2,278 selected files), updates descriptive metadata
and eight SVG comments with their manifest hashes, and revises test diagnostics.
Gameplay data, SVG geometry and runtime logic are unchanged; edits remain local
and uncommitted. Earlier scan/archive hashes describe their frozen snapshots,
not a later working tree.

**Result:** no confirmed legal violation or definite source-release blocker was
established within the allowed evidence. L09-A records the completed presentation
documentation correction. Historical authorship and excluded external inputs
remain outside the evidence collected here.
The result is not unconditional legal clearance, and does not clear any binary
release or the excluded historical/external inputs.

## L10 — Current source candidate, 2026-11-16

The post-merge review started at `13a6c43e` from PR #331. The current tracked
set and `SourceFiles.txt` agree on all 2,296 paths. Screening covered the whole
set, with direct review of the packaging, startup, transport and persistence
paths and public release documentation. The review used the current files
and their existing provenance records. OpenKal 0.1.0 selects the source archive
and its checksum for publication.

- The source archive round-trips byte for byte from a Git-free extraction. The
  local Debian recipe and CI use that extraction as their build input.
  The manifest check rejects both missing and untracked entries.
- All 875 project C/C++ files have GPL headers. The vendored miniaudio header
  matches the [upstream 0.11.22 file](https://raw.githubusercontent.com/mackron/miniaudio/0.11.22/miniaudio.h).
  The source archive includes the project's license and third-party notices.
- All 170 image hashes match. The three procedural plant models, their three
  textures and the synthetic area fixture reproduce byte for byte.
- The selected files contained no private-key, common access-token or personal
  absolute-path matches. All 1,021 JSON files parsed and internal documentation
  links were checked.

All 170 repository images were created for OpenKal using AI. The
[image provenance inventory](ImageProvenance.md#authorship-distribution-and-references)
documents their creation and processing. The artwork provenance item is closed
for the current source candidate. The publication decision for internal
authoring chat titles and identifiers remains open.
The [technical results and remaining delivery steps](../TODO.md#technical-verification-and-delivery)
apply to the local candidate.

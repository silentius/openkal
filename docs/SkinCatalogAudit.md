# Skin catalog audit

Audit the curated definitions against the data root selected by the running
client. A different preview pack does not establish runtime asset availability.
The tool reads definitions and optional assets without changing either.

```sh
cmake --build build --target skin_asset_probe
python3 tools/audit_skins.py --data-root DATA_ROOT --output build/skin-audit/RUN_NAME
```

Use an empty output directory. No running client/server or OpenGL context is
needed. Do not regenerate curated IDs from a different model inventory:
`tools/create_skin_items.py` allocates initial candidates from sorted filenames,
and different available files can shift the allocation. Its guards reject stale
ID/model associations rather than overwriting curated definitions.

## Reports and interpretation

- `report.json`: resolved models, bounds, bones, textures, fingerprints, name
  provenance and findings for each item.
- `catalog.csv`: searchable names, IDs and model paths.
- `review.md`: technical findings and recorded gameplay observations.
- `review-groups.json`: class variants grouped for comparison previews.

Exit 0 means no technical errors; 1 means reports were written with findings;
2 means invalid input, incompatible catalog or an unavailable probe stopped the run.

Checks cover allocated/withdrawn IDs, numeric filename prefixes, classes, slots,
wear flags, model coordinates, geometry, materials and bone palettes. Body
coverage heuristics flag candidates for inspection; they do not change slots.
Normal fingerprints include geometry, UVs, skinning, render flags and decoded
texture pixels. Equal fingerprints do not establish equivalent animated behavior.

The texture resolver searches GTX, DDS, TGA and PNG in that order within each
directory, case-insensitively. It preserves TGA origins and PNG transparency.
Missing files and unsupported extensions are distinct findings. Decode failures
do not trigger conversion or replacement. The diagnostic filename search is
bounded to the clothing and weapon trees.

`technical_status: passed` establishes technical loading checks, not complete
appearance, motion, clipping, face or attachment correctness. Static previews,
sampled poses and gameplay results have separate scopes.

## Catalog mappings

The allocated IDs and model coordinates are in [SkinItems.md](SkinItems.md).
Runtime rules and current catalog totals are in [Skins.md](Skins.md); equipment
grade/Darkness mappings are in [EquipmentSkinCatalog.md](EquipmentSkinCatalog.md).

The curated mapping files have distinct responsibilities:

- `config/skins/catalog.json`: allocation, exclusions, model overrides and explicit
  supplemental definitions.
- `names.json`: community name annotations guarded by ID, class, slot and model.
- `descriptive-names.json`: project-authored English visual names with the same guards.
- `weapon-templates.json`: 27 Knight type assignments from exact reference geometry.
- `weapon-compatibility.json`: 60 OpenKal type choices, 23 one-hand and
  37 two-hand, with grip/pose reasons and attachment-shape guards.
- `preview-review.json`: static family observations and focused gameplay cases.

The initial 101 Knight costume weapon appearances have explicit types: twelve
use community sword/giant-sword labels, two use explicit material hand-count
labels and 27 use reference geometry; the other 60 use OpenKal compatibility
choices. Later equipment additions have explicit class-compatible types.
Magician, Archer and Thief use staff, bow and dagger defaults where no override
is needed. Unknown future Knight types remain unavailable.

Template comparisons use identical decoded non-effect XYZ positions and triangle
indices, ignoring mesh order. `attachment_shape_fingerprint` and
`attachment_shape_meshes` detect changes to reviewed target and reference shapes.
They ignore UVs, materials, skin weights and effects and do not infer a type.
The audit checks both sides against the stored expected shape, so changing both
models together does not bypass the guard. Exporter mesh names, approximate
length and odd/even variants are insufficient classification evidence.

OpenKal compatibility choices retain separate provenance and do not replace
reference types. The audit exposes `weapon_type_origin` and `weapon_compatibility`;
item descriptions identify OpenKal choices whose external type is
unverified. Use `--weapon-templates FILE` or `--weapon-compatibility FILE` for an
explicit alternate catalog. Runtime matching uses the definition's type rather
than inferring it from a fingerprint.

## Names and provenance

`names.json` retains 74 resolved display names and their model associations.
No pending name records remain after the final thirteen naming choices.
Display names are labels, not evidence of implementation origin or weapon type.
External reference names, source-line annotations and external source metadata
have been removed from this catalog and `weapon-templates.json`. Candidate names
beginning with `Returned`, `Artificial` or `GM` are excluded; `Imperial GM` is
also treated as a GM variant. This removes 40 candidates and resolves ten names,
including 25044 as `Blood Dragon's Sword`. Skins have no time-limited variants:
28 candidates with duration annotations are removed, resolving seven more names.
The remaining names are explicitly selected, including `Blood Dragon Stick`,
`Blood Dragon Imperial Stick` and the existing Violet Wing descriptive labels.
The Violet Wing labels retain their `openkal_original` origin. Resolved names are
synchronized with their item definitions and removed from the descriptive
fallback catalog. Numeric guards and geometry comparisons remain unchanged.
The current review scope is recorded in [the content review](OpenSourceReleaseChecklist.md).

Eight former Game Master display labels use Stormforged, Starfall and
Moonveil names with Sovereign variants. The name catalog retains stable item IDs
and model guards without separate historical name fields. Project-authored descriptive names also cover unresolved
visual themes and later equipment additions. Three missing-texture appearances
retain technical names until they can be inspected. A display name never assigns
a weapon type or resolves an ambiguous external name.

Name statuses include `community_mapping`, `ambiguous`, `unresolved`,
`test_sample` and `local_reference_match`. `openkal_descriptive` is a separate
display-name status; `reference_name_match` retains the underlying reference
status. Conflicting rental/returned/alternate names sharing a model remain
ambiguous. Generators reject stale guards and replacement of resolved community
names. `--descriptive-names FILE` selects an alternate descriptive catalog.

Additional local item-name references can be inspected without importing tables:

```sh
python3 tools/audit_skins.py --data-root DATA_ROOT \
  --legacy-items LOCAL_ITEM_CONFIG --named-items --output build/skin-audit/NAME_CHECK
```

`--legacy-items` may be repeated. The parser reads records and preceding comments,
skips commented records and never executes input. Matches record source basename,
hash, line and read time, retaining unresolved author/license fields.
`--named-items` creates a separate skin-only overlay; it is not a complete item
catalog. Keep reference data and generated output outside the source distribution.

The public [Costume Crafting guide](https://gameagit.com/kalonline/CostumeCrafting)
provides naming context without a local model-coordinate crosswalk. The PNG
integration uses the public [libpng API](https://www.libpng.org/pub/png/libpng-manual.txt);
see [third-party notices](../THIRD_PARTY_NOTICES.md). Inspection code, synthetic
fixtures, names and review notes are OpenKal work under GPL-3.0-or-later.

## Comparison previews

With an OpenGL context and the tools build enabled:

```sh
cmake --build build --target skin_preview_renderer
python3 tools/generate_skin_previews.py \
  --report build/skin-audit/RUN_NAME/report.json --data-root DATA_ROOT \
  --output build/skin-previews/RUN_NAME
```

The empty output directory receives a searchable offline `index.html`, family
`sheets/`, individual `renders/`, `previews.json`, `index.md`, a render manifest
and renderer log. Pillow uses TrueType captions; `--font FONT.ttf` overrides the
font. `--review FILE` overrides the guarded notes in `preview-review.json`.

Whole-body models supply hair/headwear and retain the ordinary face unless
`skin.replace_face` is set. `skin.keep_hair` supports explicit exceptions.
Head-only costumes retain ordinary clothing. Missing variants and audit errors
produce placeholders; another class or texture is never substituted.

These are fitted static bind poses. They compare themes, coverage and textures;
use [Skin rendering findings](SkinMotionReview.md) for motion and gameplay results.
All generated art, absolute asset paths and detailed diagnostics stay in ignored
build output.

## Remaining asset findings

The initial 443-item audit finished with 398 technical passes, 42 review findings
and three missing textures. That snapshot predates the later catalog expansion:

- 25160, Magician `Cr_15_c05.gb`: missing `CR_1_c05.dds`.
- 25313, Archer `PB_S21_11.gb`: missing `PB_S21_11.png`.
- 25396, Thief `Ct_15_c05.gb`: missing `CT_1_c05.dds`.

The body previews rendered 123 appearances in 32 family sheets, with two
missing-texture placeholders and three absent class variants. The Archer texture
belongs to a weapon outside those sheets. Model-path decoding for the expanded
catalog is a separate check and does not clear missing textures.

One local `PB_S22_03.png` contained JPEG data. A local pixel-preserving conversion
repaired that file with its retained as a backup; the runtime does not
silently treat JPEG as PNG. That repair and the active/preview-pack reconciliation
are local diagnostics, not distributed resources or automatic tool behavior.
Existing runtime files take precedence over different preview fingerprints.

Six weapon fingerprint pairs are identical: 25090/25094, 25322/25326,
25324/25335, 25345/25347, 25355/25364 and 25448/25450. Their IDs remain distinct.
These comparisons do not establish motion/effect equivalence. Family 15-05's blue
skin and the Thief gold body's costume category remain unresolved.

## Tests

```sh
ctest --test-dir build \
  -R '^(skin_catalog_audit|skin_asset_inspection|texture_formats|skin_preview_selection|inventory_hud)$' \
  --output-on-failure
```

Synthetic fixtures cover name ambiguity, stale mappings, reserved IDs, body
coverage warnings, missing textures, geometry/bones, PNG/TGA decoding and safe
gallery captions. The audit/inspection/preview-selection checks need no game art.

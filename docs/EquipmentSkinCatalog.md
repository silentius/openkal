# Equipment appearance skins

The catalog covers ordinary equipment appearances through G55, the Darkness
family and G56/G59 weapons. These are OpenKal cosmetic definitions,
not ordinary armor or weapons: they have level 1 requirements, no combat values,
no fixed gold price and the usual two-ticket Skin Artisan price. They remain
tradable skin items and require the corresponding real weapon type when equipped.

## Scope and allocation

IDs 25487–25770 add 284 appearances: 140 normal armor parts, 15 Darkness armor
parts, 109 normal weapons, eight Darkness weapons, eleven normal shields and one
Darkness shield. The twenty existing G8 parts are reused. Costume and
weapon-skin definitions and retired IDs keep their allocations. The next cosmetic
ID after that initial expansion was 25771. The later weapon additions contain
sixteen G56 appearances at 25771–25786 and twenty-one G59 appearances at
25787–25807. The next available cosmetic ID is now 25808.

Every class has helmet, upper armor, gauntlets, pants and boots for appearance
grades 8, 16, 24, 32, 42, 46, 50 and 55. These correspond to ordinary clothing
model groups 1–8, variant 1. They are individual-part skins; ordinary armor meshes
are not reclassified as whole-body costume meshes. Existing group 14 sets remain
available independently.

Normal weapon appearances through G55 use the observed low model families:

- Knight: groups 1–10, with groups 8 and 10 assigned to greatswords; the other
  selected normal models use one-hand swords. Groups 7/8 are the G50 families,
  and groups 9/10 are the G53 families.
- Magician and Archer: groups 1–8, including the G50/G53 appearances.
- Thief: groups 1–6 plus G53 variants 01, 02, 03 and 07 in group 7. Later group 7
  and group 8 appearances are outside the selected grade range.
- Knight shields: selected variants in groups 1–4 through G55. Similar-looking
  forged variants retain their separate observed models.

The G56 family uses Knight sword group 13 and greatsword group 14, variants 01–04
in each; Magician staff group 10 and Archer bow group 10, variants 01–04 in each.
These sixteen items were initially mislabeled G59. Direct comparison with the
official guide images corrected their display names while preserving IDs and models.

The G59 family uses Knight sword group 11 and greatsword group 12, variants 01–04
in each; Magician staff group 9, variants 01–06; Archer bow group 9, variants 01–04;
and Thief dagger group 8, variants 01–03. The Thief items were already correctly
classified. Eighteen additional models complete the twenty-one G59 offers. All
use the same two-ticket price. Model-group numbers do not increase with grade;
the reviewed mappings must be checked against the official appearance images.
The reference image URLs are recorded in `catalog.json` alongside both families.

Grade labels describe appearance families, not requirements, stats, official
costume categories or a new enhancement system. Alternate weapon appearances use
their model pair in the display name to distinguish styles without importing
legacy item names or asserting additional bonuses. The guarded mapping is in
`config/skins/catalog.json` under `supplemental_items`.

## Darkness availability

The complete 09-01 armor family is present for Knight, Magician and Archer.
Knight's alternate group 9 upper/head variants showed missing coverage or unrelated
head geometry in the inspected local pack, so only the complete base set is used.
Thief group 9 is a golden appearance; it is not labeled Darkness.

Weapon group 20 provides four Knight appearances (one-hand and two-hand swords,
each with a Treasure variant), two Archer bows and two Thief dagger appearances.
Knight shield 05-01 supplies the matching shield appearance. No Magician group 20
weapon model was present in the inspected inventory. Other staff models remain
unclassified rather than being substituted for the missing Darkness staff.

These are statements about the inspected local model inventory, not proof that
other client versions lack additional appearances. Darkness names omit a numeric
grade because legacy sources use inconsistent grade labels.

## Provenance and review

Definitions, generic English labels, mappings and tests were authored by OpenKal
contributors under GPL-3.0-or-later on 2026-10-30. Model coordinates come from
observing optional local filenames, decoded mesh/texture metadata and previews.
No external art, item tables, reference item IDs, source code or combat values are
redistributed. Detailed local renders remain outside the source distribution.

Public INIXSOFT guide pages were consulted on 2026-10-30 for equipment appearance
and grade context: [Knight armor](https://gameagit.com/kalonline/Armor01),
[Archer armor](https://gameagit.com/kalonline/Armor02),
[Magician armor](https://gameagit.com/kalonline/Armor03),
[Thief armor](https://gameagit.com/kalonline/Armor04),
[Knight weapons](https://gameagit.com/kalonline/Weapon01),
[Archer weapons](https://gameagit.com/kalonline/Weapon02),
[Magician weapons](https://gameagit.com/kalonline/Weapon03),
[Thief weapons](https://gameagit.com/kalonline/Weapon04) and
[Knight shields](https://gameagit.com/kalonline/Weapon01_shield).
The public guide is copyrighted by INIXSOFT; its artwork and stat tables are not
included. The mappings and short generic appearance labels are independently
authored compatibility data.

Matching armor sets were rendered from the front and back. Weapons and shields
were rendered individually with the existing class skeleton and attachment
settings. These are static appearance checks, not complete animation, effects,
multiplayer or every mixed-outfit verification. Existing known
catalog issues remain tracked separately.

Data-only regression tests check complete grade sets, stable IDs, unique model
coordinates, class-compatible weapon types, missing-family exclusions and the
absence of combat-value fields. Shop tests check class filtering, two-ticket
prices and shield placement. The generator appends these explicit allocations
and does not overwrite curated directories or infer new IDs from this expansion.

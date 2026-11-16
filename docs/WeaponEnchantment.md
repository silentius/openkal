# Talisman of Enchantment

The five stackable Talismans of Enchantment share the scroll artwork,
with gold Roman numerals painted directly into the violet magic sigil:

- **23022 — I:** weapon grades 40–49.
- **23023 — II:** weapon grades 50–59.
- **23024 — III:** weapon grades 60–69.
- **23025 — IIII:** weapon grades 70–79 (additive Roman spelling).
- **23026 — V:** weapon grades 80 and higher.

Drag the matching talisman onto a single unequipped weapon of grade 40 or higher.
The grade band is checked against the weapon before rolling. A mismatched
variant consumes nothing and leaves all existing stats and protection intact.
Only loaded weapon definitions are used; the bands do not create missing grades.
A grade change can cross a talisman band boundary and retains its newly rolled
stats. The server consumes one matching talisman, resets OTP, physical
Attack, Magic, Explosive Blow (EB) and EB revision progress, and applies freshly
rolled values. Existing values can decrease. The instance ID, prefix, endurance,
account binding, mixing, Qigong, Dragon Fusing and unrelated bonuses are retained.
An accepted application consumes one Polishing Stone protection charge; the
intentional reset and grade downgrade are not failure damage. Invalid requests
consume nothing. Weapons below G40 retain their enchantments and protection when
an application is rejected. A valid G40 weapon can still roll down to G37; the
resulting G37 weapon retains all freshly rolled OTP, Attack/Magic and EB values
but cannot receive another talisman. This feature does not damage or destroy
weapons.

## Independent rolls

The grade roll has a 10% chance to request the next lower grade, a 5% chance to
request the next higher grade and an 85% chance to retain the current grade.

The server selects the nearest lower or higher grade actually present in the
loaded item catalog with the same weapon subtype, player-class restriction and
Imperial status. Sword and two-handed sword are separate subtypes. Imperial
weapons remain Imperial; normal weapons cannot become Imperial. Imperial status
comes from replace-weapon outputs in the loaded Mixing recipes or the whole word
`Imperial` in the item definition name, matched without regard to case. If several
definitions match the next grade, the server chooses uniformly among their distinct
item IDs. Every eligible variant has the same chance, independently of catalog
iteration order. This separate variant draw does not change the 10% downgrade and
5% upgrade chances. Multiple valid targets do not cancel a successful grade roll.
At a catalog boundary the current definition is retained.
Other enchantment rolls still take effect.

OTP stages are rolled separately in this order: +1 at 50%, +2 at 40%, +3 at 20%,
+4 at 10%, +5 at 5%. A successful stage replaces the preceding OTP result.
Every stage is attempted even when a preceding stage failed. If all fail, OTP
remains zero.

Attack/Magic stages are rolled separately in this order. Wands receive Magic;
other weapons, including swords, two-handed swords, bows and daggers, receive
Attack. Each success replaces both the plus value and its paired EB value:

- +1 / 1 EB: 100%
- +2 / 2 EB: 90%
- +3 / 3 EB: 80%
- +4 / 4 EB: 70%
- +5 / 5 EB: 60%
- +6 / 6 EB: 50%
- +7 / 7 EB: 40%
- +8 / 8 EB: 30%
- +9 / 9 EB: 20%
- +10 / 10 EB: 18%
- +11 / 10 EB: 16%
- +12 / 10 EB: 14%
- +13 / 10 EB: 12%
- +14 / 10 EB: 10%
- +15 / 10 EB: 6%
- +16 / 10 EB: 2%
- +16 / 16 EB: 1%

The final +16 / 16 EB stage is intentional. Later stages are always attempted,
including after failures. These percentages are per-stage success chances, not
mutually exclusive final-result probabilities. Grade, OTP and Attack/Magic use
separate random draws. Single-player uses the same shared rules as the server.
The existing enchantment request and absolute item-update packets carry the
result; no database or packet-format change is needed.

## Verification and provenance

`WeaponEnchantmentTest` checks every percentage threshold, reset behavior,
independent stages, both +16 outcomes, catalog boundaries, subtype/class filtering,
Imperial preservation, equal variant selection chances in both grade directions
and order-independent selection among duplicate grades,
including the complete 10% downgrade / 5% upgrade / 85% unchanged distribution.
`WeaponEnchantmentServerTest` uses project-authored synthetic item definitions,
a Unix socket and an isolated SQLite database to verify target validation,
consumption, complete reset, unrelated-field preservation, supported weapon
families, replay rejection and relogin persistence. `InventoryHudTest` covers
accepted and rejected drag targets for all five talisman bands, including every
40/50/60/70/80 boundary. `RuntimeIconsTest` verifies that all five distinct painted icon variants are packaged with transparency. The inventory HUD capture mode `--capture-enchantment-icons <output.ppm>`
shows the painted numerals at 32, 48 and 64 pixels.

Implementation and fixtures are OpenKal work under GPL-3.0-or-later,
integrated with the existing enchantment rules. Icon provenance and its exact
generation prompt are recorded in `RuntimeData/templates/TalismanOfEnchantment.md`.

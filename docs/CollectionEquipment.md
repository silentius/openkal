# Collection equipment

The inventory handle opens a left drawer containing Taegeuk, Hanin's Mirror
and Yeoiju. Drag an item into its matching slot or double-click it in the bag.
Double-click a drawer item to return that exact instance to the inventory.
Equipped collection items live in a separate collection container and free their
inventory cells. Removal requires a free writable inventory cell; a full bag
leaves the item in the collection. All transfers are acknowledged by the server
and preserve the instance ID, upgrades, rolled abilities, binding and expiry.
Previously saved equipped collection items are restored into the separate
container on login, including when the normal inventory is full.

## Supported equipment

Yin-Yang Mirrors 24000–24002, raw trigrams 24003–24026, enchanted trigrams
24027–24050 and Taegeuk 24051–24053 cover Apprentice, Novice and Senior grades.
All eight enchanted trigrams must share a grade. The mirror must be at least
as high as Taegeuk, and Taegeuk at least as high as the trigrams. Remove mirror
and Taegeuk before removing trigrams. Mirror lifetime begins on first equip,
lasts seven days, and persists across logout. Mirror and Taegeuk bind to account.

Stats and absorption use the three-grade compatibility balance implemented in
`CollectionEquipment.cpp`. The mirror
set multiplier uses the lowest enchantment across the eight trigrams.
To absorb, click an unequipped Taegeuk in the bag, then click **Absorb** with
all eight same-grade trigrams at +5 or above equipped. This stores a random
absorption row without consuming trigrams. An existing absorption cannot be
overwritten. Its bonus requires an active set whose lowest enchantment is +5.

Hanin mirrors 24054–24056 amplify the five essence slots by 100%, 110% or 120%.
Essences 24057–24059 store one rolled ability per instance. Movement speed,
character EXP and ancient EXP each permit only one essence; other options can
stack. Remove essences before removing Hanin's Mirror.

OpenKal reserves 24060 for attack Yeoiju, 24061 for defense Yeoiju and 24062 for
fortification material, with five slots per equipment family and levels 0–20.
Their benefits are PvP-only, so they grant no combat bonus under this
server's PvP policy. They can be equipped, upgraded and persisted. Their icons
reuse existing collection art because the matching reference item definitions
are unavailable. Item definitions are under `config/items/collection`.
The Gae-Yoo shop references the current mirror and Essence Box IDs. Shared
Collection ID constants live in `openkal/server/CollectionEquipment.h`.

## Essence boxes

Item **23012**, Low Class Essence Box, uses the existing `item784` icon and opens
with a right-click in the inventory or its consumable shortcut. For testing,
`/getitemid 23012 5` grants five boxes.

Edit `config/collection/essence_box.json` on the server:
- `essence_count.min` and `max` define the inclusive, uniformly sampled total.
  Defaults: 1–5; set both to the same number for a fixed total (valid range 1–216).
- `grade_percent.low`, `middle` and `high` select items 24057, 24058 and 24059.
  Defaults: 80%, 15%, 5%. Each essence rolls independently; percentages must
  total 100. Zero disables a grade. Each instance also rolls its own ability.

The server requires room for the maximum reward before drawing, including the
box's cell when consuming its last unit. This prevents inventory space from
biasing the rolls. Insufficient space, invalid configuration or a failed save
leaves the box untouched. Consuming the box and saving all rewards is one SQLite
transaction. The server enforces the item's 1500 ms cooldown. Restart the server
after edits; packaged deployments must rebuild their config archive.

## Upgrades

Drag matching material onto the unequipped target in the normal bag. The server
owns every random draw; neither probabilities nor outcomes come from the client.

- Trigram: one raw trigram of the same grade and position. Success converts a
  raw target to the wearable definition at +1, or adds one enchantment up to +15.
  Failure consumes the material and decreases enchantment by one, floored at 0.
  A +0 trigram cannot be equipped.
- Essence: another essence of the same grade. Success promotes Low to Middle or
  Middle to High and rolls a new ability. Failure preserves the target.
- Yeoiju: another of the same attack/defense family and exact level, plus one
  fortification material. Success adds one level; failure preserves the target.

Materials are consumed on an accepted attempt, including failure. Wrong,
foreign, equipped, exhausted or over-limit targets/materials are rejected
without consuming anything.

The editable server tables are `config/collection/trigram.json`, `hanin.json`
and `yeoiju.json`. `success_percent` is indexed by destination stage minus one.
Defaults are 50%, 25%, 12.5%, 6.25%, and so on. Hanin has two stages, trigram
fifteen and Yeoiju twenty. Use decimal percentages; 0 disables a stage, 100
makes it certain. Restart the server after editing. Missing, malformed,
non-finite, out-of-range or incomplete tables disable collection upgrades.
Packaged deployments must rebuild their config archive after editing sources.

## PvP isolation

Collection bonuses are calculated separately from ordinary equipment.
Player-versus-player combat removes every collection contribution except
movement speed from both participants, including primaries, their derived
stats, HP/MP, hit, evasion, defenses, resistance, critical and explosive damage.
The baseline profile remains active for 30 seconds after the last PvP contact
so attacking a nearby monster cannot restore bonus vitals during the fight.
HP/MP fractions are preserved when changing profiles; the transition cannot
revive a dead player. Positive HP stays at least one to avoid a rounding death.
Delayed and area effects have separate baseline PvP power snapshots.

Location reuses the existing persisted collection wear slot and wire equipped
flag; both client and server route those instances into separate collection
storage. State reuses the existing persisted/wire item fields: `upgrade_level` for
trigram/Yeoiju level, `random_stat_type` 100–121 plus `random_stat_value` for
abilities, and `mix_type` 1–184 for Taegeuk absorption. Generic equipment
calculations exclude collection items so these fields are not interpreted as
weapon enhancements. No database migration or packet format change is needed.

## Validation

`essence_box` checks grade boundaries, invalid configuration, full inventories,
stacked boxes, cooldowns, replayed packets and transactional persistence.
`collection_equipment` checks upgrade boundaries, dependency rules, exact
instance slots, absorption and every ability's PvP filtering. `collection_server`
exercises authoritative packets, material consumption, persistence and combat
profile changes against a local server. `inventory_hud` checks drag/drop,
double-click, hover on the Quest tab and viewport fitting at multiple scales;
its `--capture` mode includes drawer and tooltip screenshots.
The inventory and server tests also cover freed capacity, full-bag removal
rejection, exact-instance transfers, repeated acknowledgements and restoring
collection equipment alongside a full inventory.

References: [Hanin's Mirror](https://gameagit.com/kalonline/Collection),
[Yin-Yang](https://gameagit.com/kalonline/YinYang),
[Trigrams](https://gameagit.com/kalonline/Trigram).

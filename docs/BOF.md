# Beads of Fire (BOF) — Set-Armor Visual Effects

Reference for `ef_setitem_*` FX shaders used for **Beads of Fire** (BOF) armor
set bonuses. The shaders are read from the data root selected in the active profile.

## Where the shaders live

The effects are **not** standalone files. They are embedded in:

| File | Shaders |
|------|---------|
| `Effect/Fx/set_item.dat` | Main BOF set (levels 46–60, Christmas event) |
| `Effect/Fx/set_item1.dat` | Thief variants, level 65, some `realeffectBoneIndex` fields |

`.dat` files are decoded with `terrain::kDecodeTable4` (same as `SpriteFx::index_scripts`).
`skill_effect_view` lists them on the **Fx** tab after loading `--data`.

Total: **44** unique `ef_setitem_*` shaders.

## Name format — not hex, not bones

```
ef_setitem_XXYYZZ
           │││││└─ ZZ = class (00 Knight, 01 Mage, 02 Archer, 03 Thief)
           ││└└─── YY = armor tier (46, 50, 55, 60, 65; special 10 = Christmas)
           └└───── XX = set completeness (01 = half set, 02 = full set)
```

The six digits are **decimal**, zero-padded (`%02d%02d%02d`), **not** hexadecimal and
**not** skeleton bone IDs.

### Runtime naming

OpenKal derives the suffix from three project-level values: set completeness,
armor tier, and character class. Each value is written as a two-digit decimal
field and concatenated in that order.

| Field | Values |
|-------|--------|
| Set completeness | `1` → `01` half set, `2` → `02` full set |
| Armor tier | `46`, `50`, `55`, `60`, (`63`), `65` |
| Character class | `0` Knight, `1` Mage, `2` Archer, (`3` Thief in assets) |

Tier 63 has no matching shader in the inspected `set_item*.dat` inventory.
The name fields describe the set; attachment comes from stage bone fields.

## Complete shader inventory

### Standard BOF tiers (per class × half/full × level)

| Level | Half (01) | Full (02) |
|-------|-----------|-----------|
| 46 | `ef_setitem_01460N` | `ef_setitem_02460N` |
| 50 | `ef_setitem_01500N` | `ef_setitem_02500N` |
| 55 | `ef_setitem_01550N` | `ef_setitem_02550N` |
| 60 | `ef_setitem_01600N` | `ef_setitem_02600N` |
| 65 | `ef_setitem_01650N` | `ef_setitem_02650N` |

`N` = class digit (`0`–`3`).

### Special / anomalous

| Shader | Notes |
|--------|-------|
| `ef_setitem_021000`–`021003` | Christmas event (`particle "13chrismas_setitem"`, level digit `10`) |
| `ef_setitem_002650` | Likely dev/leftover entry in `set_item1.dat`; same particles as L65 full
  Knight but name does not follow `02` + `65` + `00` pattern |

## FX structure

Each shader has one `description` block with **1–3 stages**. Typical fields per stage:

```
particle            "item_effect60_2"
model               ""
sound               ""
effectBoneIndex     5          // EFFECTBONE slot (logical)
realeffectBoneIndex 5          // optional: resolved GB skeleton bone (set_item1.dat)
delay               30.0
duration            3.0
restart             1
offset              ( 0 0 0 )
rotationYawPitchRoll ( 0 0 0 )
```

`SpriteFx::parse_fx` reads particle, model, sound, offset, duration, delay,
restart, `effectBoneIndex`, `realeffectBoneIndex` and `useEffectBoneRotation`.
Parsing a field alone does not verify its rendered behavior.

## Bone attachment — EFFECTBONE slots, not name digits

Bones are chosen per stage via `effectBoneIndex` (EFFECTBONE slot index). Some newer
entries also specify `realeffectBoneIndex` (direct skeleton bone in the `.gb` rig).

### Knight / Mage / Archer (classes 0–2)

| Set type | Stages | `effectBoneIndex` | Particles |
|----------|--------|-------------------|-----------|
| Half (`01`) | 2 | `5`, `6` | `*_2` on both (e.g. `item_effect50_2`) |
| Full (`02`) | 3 | `5`, `6`, `1` | two `*_2` + one `*_1` or `*_6` body aura |

Mage/Archer use `item_staffeffect{level}_*` instead of `item_effect{level}_*` for
classes 1–2 (staff-held weapon mesh).

### Thief (class 3)

| Set type | Stages | `effectBoneIndex` | `realeffectBoneIndex` | Particles |
|----------|--------|-------------------|----------------------|-----------|
| Half | 2 | `6`, `7` | `60`, `56` | `item_effect{lvl}_2_3` |
| Full | 3 | `6`, `7`, `1` | `60`, `56`, `3` | two `*_2_3` + `*_1_3` |

Thief uses **different EFFECTBONE slots** (6/7 instead of 5/6) and class-specific
particle suffix `_3`.

### Level 65 `realeffectBoneIndex` by class

| Class | Slot 5 | Slot 6 | Slot 1 |
|-------|--------|--------|--------|
| Knight | 5 | 11 | 15 |
| Mage | 59 | 63 (full) / 11 (half) | 3 |
| Archer | 44 | 48 | 3 |
| Thief | — | 60 | 3 (via slot 7→56) |

All observed `realeffectBoneIndex` values across BOF FX:
`3, 5, 11, 15, 44, 48, 56, 59, 60, 63`.

### Interpretation (armor parts)

The slot pattern matches **per-piece armor glow** plus an optional **torso aura**:

| `effectBoneIndex` | Typical role | Classes |
|-------------------|--------------|---------|
| `5` | Upper-body / gauntlet region | Knight, Mage, Archer |
| `6` | Lower-body / pants region | Knight, Mage, Archer |
| `7` | Thief-specific lower slot | Thief only |
| `1` | Spine / torso (full-set aura) | All (full set only) |
| `-1` | Origin / feet | Christmas Mage/Thief only |

Half set = glow on **two** armor regions. Full set adds a **third** stage on slot `1`
with a different particle (`_1` or `_6` suffix).

The six name digits do **not** encode which bones are used; that is entirely in the
shader stages.

## Particle naming

| Pattern | Used by |
|---------|---------|
| `item_effect{level}_2` | Knight armor piece glow |
| `item_effect{level}_1` | Knight full-set body aura |
| `item_effect{level}_6` | Knight full-set body aura (level 46 only) |
| `item_staffeffect{level}_2` / `_1` / `_6` | Mage, Archer |
| `item_effect{level}_2_3` / `_1_3` | Thief |
| `item_effect65_*` | Level 65 (shared across classes in `set_item1.dat`) |
| `13chrismas_setitem` | Christmas event shaders |

## Runtime integration

`config/bof.json` controls supported armor parts, minimum grade, application
success, piece/set bonuses and available FX tiers. The defaults require grade 46,
three pieces for a half set and five for a full set. Loaded item definitions
supply the armor/bead records; optional catalogs remain external data.

The client derives local set completeness and FX tier from worn items carrying
the authoritative BOF marker and the shared `BofCatalog` rules. It passes those
values to `BofFx::apply_wear`; transformations detach the effect. This is not
limited to a Knight-only chat simulation.

`BofFx::follow` uses slots 1/5/6 for Knight, Mage and Archer and 1/6/7 for Thief.
A stage's `realeffectBoneIndex` takes precedence; otherwise
`EquipmentPaths::effect_slot_skeleton` resolves the class slot. Pose bone-world
coordinates drive following, with feet as the unresolved fallback.

BOF attachment overrides the authored 30-second delay with **10 seconds**;
the authored 3-second duration remains, producing a **13-second** loop.
Other effects retain their own delays.

Debug controls remain available:

- `/bof gloves`, `chest`, `pants`, `boots`, `helmet`: toggle a part.
- `/bof all`: enable all five if incomplete, otherwise clear them.
- `/bof clear` or `/bof off`: clear the debug parts.
- `/bof level 60` or `/bof 60`: select 46/50/55/60/65.
- `/bof` or `/bof status`: print current state.

Aliases include `gauntlet`, `upper`, `armor`, `lower` and `helm`. Debug thresholds
are three/five parts and the initial tier is 60.

## Verification and references

`bof_catalog` checks configured equipment/set rules with owned fixtures.
Inspect optional effects with `skill_effect_view --data DATA_ROOT`, Fx tab,
filter `ef_setitem`. Verify class-specific attachments, full timing/looping,
materials and observer presentation in addition to catalog results. A visible
particle alone does not establish correct worn-set behavior for every class.

- Runtime assets: `Effect/Fx/set_item.dat` and `set_item1.dat` in the active data root.
- Decoder: `libs/terrain/include/openkal/terrain/Decode4.h`.
- Stage parser: `client/src/world/SpriteFx.cpp`.
- Set rules: `libs/server/include/openkal/server/BofCatalog.h`.
- Local attachment/following: `client/src/player/BofFx.cpp`.

# UI variable catalog

## `powerbook`

`powerbook.json` is the fullscreen monster/drop guide. P opens it when chat and
other fullscreen views are inactive; P, Escape and `window.close` close it.
It captures gameplay and other UI input for its entire open/close frame.
The runtime reads the active optional monster, group, itemgroup and map catalogs;
it never embeds external game data in the layout. Monsters sort by ascending
level, then name and ID. Missing catalogs and map artwork have explicit messages.

| Key | Meaning |
|---|---|
| `powerbook.title` | Book heading. |
| `powerbook.selection` | Selected monster name and level. |
| `powerbook.monsters_label` | Monster column heading. |
| `powerbook.drops_label` | Drop column heading. |
| `powerbook.spawns_label` | Spawn map heading. |
| `powerbook.previous` / `powerbook.next` | Spawn navigation captions. |
| `powerbook.up` / `powerbook.down` | List scroll button captions. |
| `powerbook.blank` | Empty caption for the monster row hit targets. |
| `powerbook.empty` | Missing monster catalog message. |
| `powerbook.monster_range` | Visible row range and monster count. |
| `powerbook.drop_status` | Distinct possible item count, empty or incomplete catalog message. |
| `powerbook.spawn_status` | Area, logical map and selected spawn ordinal, or empty message. |
| `powerbook.spawn_visible` | True when the selection has a spawn rectangle. |
| `powerbook.spawn_multiple` | True when spawn navigation is useful. |
| `powerbook.legend` | Yellow highlight and conditional quest-drop explanation. |
| `powerbook.controls` | Mouse wheel and scrollbar guidance. |
| `powerbook.monster.N.name` / `powerbook.monster.N.level` | Visible monster row, N = 0..15. Long names are fitted to the row. |
| `powerbook.monster.N.visible` / `powerbook.monster.N.color` | Row visibility and selected/alternating background. |
| `powerbook.drop.N.name` / `powerbook.drop.N.quest` | Item name and optional Q badge, N = 0..59. Names appear in hover overlays. |
| `powerbook.drop.N.color` | Slot outline color, highlighted while hovered. |
| `powerbook.drop.N.visible` | Whether the drop row contains an item. |
| `powerbook.drop.N` | Icon-slot binding using the shared item catalog. |
| `powerbook.map` | Rectangular minimap canvas, centered on the selected spawn. |

Actions: `powerbook.select.N` selects visible row 0..15;
`powerbook.monsters.up`, `powerbook.monsters.down`, `powerbook.drops.up` and
`powerbook.drops.down` scroll one row. The wheel scrolls the hovered list by
three rows per tick; each scrollbar supports track clicks and thumb dragging.
The drop panel displays six columns and ten rows of inventory-framed item
slots. Drop scroll offsets count grid rows, so the last partial row remains
accessible. Hovering a populated slot shows the inventory tooltip, including
catalog descriptions, talisman effects, item stats and character requirements.
The overlay uses current character class/level and unmodified catalog values;
it does not invent rolled prefix or enchantment bonuses. Q badges and overlays
identify quest-only drops. Empty slots have no tooltip; missing item definitions
show an ID fallback. The tooltip is drawn above the fullscreen book and flips
near the viewport edges using the shared inventory placement rules.
The overlay scales with the fullscreen book, including text, border, padding
and pointer spacing. Long tooltip lines wrap at 440 layout pixels or the
available viewport width while preserving their stat colors.
Slots use 36-pixel icons inside 48-pixel inventory frames. The fullscreen frame
uses the standard Dragon nine-slice preset, with stone panel textures, bronze
section headings and existing menu emblems. Navigation and scroll buttons use
the shared idle/hover/pressed/disabled artwork; scroll buttons disable at their
list bounds. Hit tests include the inset client origin at every viewport scale.
The close control is the shared unlabeled Dragon X, anchored in the bottom-right
footer. Each column has the shared bronze gradient heading, flush with the
inside edges of a two-pixel bronze border around its entire block. Its image
source skips the gradient's baked top highlight, avoiding a doubled upper edge.
The blocks begin below the outer dragon ornament so the
first heading remains unobstructed. No on-screen keyboard hint is needed.
`powerbook.spawn.previous` and `powerbook.spawn.next` cycle every authored enabled
spawn, including other maps. Other visible spawns on that map are highlighted.
`window.close` preserves selection/scroll state for reopening.

The drop list deduplicates reachable items, includes the server's normal-combat
global drop group, and labels quest-only items. It does not display drop chances
or roll counts. Item descriptions retain their own effects and usage details.
Prefix variants and stack sizes share a single
item row. Coordinates use the existing overworld/dungeon minimap transforms;
the map view does not change world rendering or the camera.

`tests/PowerbookTest.cpp` creates its monster/item/loot catalogs and map textures
independently in a temporary directory. These fixtures contain no imported game
data. Running its executable with an output prefix also renders both map types
at 1600×900, 1280×720 and 1024×768 through the real UI renderer.

Runtime values interpolated as `${group.key}` in `openkal.ui/v1` documents.
See [`TEMPLATE.md`](TEMPLATE.md) for the document shape, format suffixes
(`:comma`, `:geons`, `:+`, `:pct`), `visible_when`, and slot context keys.

This file is the source of truth. When you add a dialog, widget bind, action,
or `${…}` key, update the matching table here (and `TEMPLATE.md` if the
widget vocabulary changes).

Live layouts still hardcode many of these strings in C++. The **Used by**
column names the *intended* surface once a file is migrated, plus today’s
JSON/C++ owner.

Types: `string`, `int`, `int64`, `float` (0..1 ratios), `bool`, `enum`.
Every numeric / enum key has a `_label` sibling unless the key is already a
display string.

`intl` is INT (not `int`) so JSON and `${}` paths stay unambiguous.

---

## Interpolation recap

| Token | Example output |
|---|---|
| `${stats.hth}` | `42` |
| `${stats.hth_label}` | `HTH` |
| `${vital.hp}/${vital.max_hp}` | `123/456` |
| `${inventory.money:comma}` | `1,234,567` |
| `${inventory.money:geons}` | `1,234,567` in the matching Geon color |
| `${transform.str:+}` | `+3` |
| `${exp.pct:pct}` | `37%` |

Missing keys render empty. Inside `icon_slot` / `icon_grid` cells, `slot.*`
is also in scope.

---

## `theme`

From the document’s `theme` block, not from the player.

| Key | Type | Example | Used by |
|---|---|---|---|
| `theme.colors.label` | string | `#E8DCC0` | all windows |
| `theme.colors.value` | string | `#FFFAEB` | all windows |
| `theme.colors.pending` | string | `#C8E090` | Character Info PU preview |
| `theme.colors.title` | string | `#F2F0C0` | window captions |
| `theme.font_pixel_size` | int | `10` | default FreeType size |

---

## `ui`

Static chrome copy. Values may later come from a locale table; keys stay
stable.

| Key | Type | Example | Used by |
|---|---|---|---|
| `ui.title` | string | `Info` | window caption (`title.text` today) |
| `ui.close` | string | `Close` | footer / skill learn |
| `ui.ok` | string | `Apply` | Character Info |
| `ui.point` | string | `Point` | Character Info PU remainder |
| `ui.honor` | string | `Honor` | Character Info footer stub |
| `ui.ancient_animal` | string | `Ancient animal` | Character Info / Transform |
| `ui.riding_title` | string | `Riding animals` | Riding window |
| `ui.message` | string | `Message` | Inventory footer |
| `ui.private_shop` | string | `Private shop` | Inventory footer |
| `ui.tab_normal` | string | `General` | Inventory |
| `ui.tab_task` | string | `Other` | Inventory |
| `ui.skill_chart` | string | `Skill chart` | Skill learn |
| `ui.active` | string | `Active` | Skill learn |
| `ui.passive` | string | `Passive` | Skill learn |
| `ui.start_mixing` | string | `Start mixing` | Mix Master |
| `ui.tab_common` | string | `Common` | Skill learn |
| `ui.tab_job1` | string | `Job 1` | Skill learn |
| `ui.tab_job2` | string | `Job 2` | Skill learn |
| `ui.tab_job3` | string | `Job 3` | Skill learn |
| `ui.tab_awaken` | string | `Awaken` | Skill learn |
| `ui.tab_action` | string | `Action` | Skill learn |
| `ui.equip_egg_hint` | string | `Equip a Shinsu egg in the sinsu slot.` | Transform empty state |

---

## `riding`

Common riding-animal registration and active state. Source: authoritative
`S2C_RIDING` snapshots plus the item catalog.

| Key | Type | Example | Used by |
|---|---|---|---|
| `riding.registered` | bool | `true` | Riding registered state |
| `riding.empty` | bool | `false` | Riding empty state |
| `riding.common` | bool | `false` | Common riding detail visibility |
| `riding.battle` | bool | `true` | Battle Riding detail visibility |
| `riding.name` | string | `White Tiger` | Riding window |
| `riding.status` | string | `Mounted` | Riding window |
| `riding.remaining` | string | `6d 23h left` | Riding window |
| `riding.grade` | string | `Rare` | Per-IID Battle Riding grade |
| `riding.rarity` | string | `Blue` | Existing Item Rarity presentation mapped from grade |
| `riding.satiety` | string | `83 / 100` | Battle Riding satiety |
| `riding.grade_line` | string | `Rare (Green)` | Framed grade value; `--` for common/empty state |
| `riding.satiety_ratio` | float | `0.83` | Server-backed satiety bar |
| `riding.equipment_hint` | string | `Drag gear in/out. Right-click to remove.` | Drop instructions or validation feedback |
| `riding.hint` | string | `Press R to mount or dismount.` | Riding window footer |
| `riding.status_label` / `riding.duration_label` / `riding.grade_label` / `riding.satiety_label` | string | `Status` | Framed detail captions |
| `riding.mount_label` | string | `Riding pet` | Registered mount icon caption |
| `riding.helmet_label` / `riding.armor_label` / `riding.saddle_label` / `riding.cloth_label` / `riding.magnet_label` | string | `Helmet` | Framed equipment slot captions |
| `riding.feed_label` / `riding.ability_label` / `riding.reroll_label` / `riding.enchant_label` / `riding.upgrade_label` | string | `Feed` | Management button captions |
| `stablemaster.open` | bool | `true` | Stablemaster service section visibility after talking to Edmund |
| `stablemaster.title` | string | `Stablemaster services` | Service section heading |
| `stablemaster.seals` | string | `Riding Seals: 12` | Current inventory Seal count |
| `stablemaster.exchange_label` | string | `Horse (2 Seals)` | Starter-mount exchange button |
| `stablemaster.promote_label` | string | `Promote (6 Seals)` | Registered Horse promotion button |
| `stablemaster.grade_label` | string | `Grade (10 Seals)` | Deterministic next-grade cost or Epic status |
| `stablemaster.writ_label` | string | `Time Writ (+7d)` | Registered timed-mount extension button |
| `stablemaster.duplicate_label` | string | `Convert (+2 Seals)` | Selected duplicate conversion button |
| `stablemaster.duplicate_selection` | string | `Drag Riding Pet` | Duplicate-selection slot state |

Icon binds: `riding.mount`, `riding.equipment.helmet`,
`riding.equipment.armor`, `riding.equipment.saddle`,
`riding.equipment.cloth`, and `riding.equipment.magnet`.
The five equipment slots accept `riding.helmet`, `riding.armor`,
`riding.saddle`, `riding.cloth`, and `riding.magnet`, respectively.
Only normal-bag equipment matching the registered Battle Riding pet's grade
can be dropped. Both helmet variants use the helmet slot. A valid drop sends
the dragged item's IID; a right-click on an occupied slot, or a drag back into the normal inventory,
requests removal.
Icons and the satiety bar update from the server snapshot, and rejected drops
stay in the bag. The exact equipped instance receives the inventory
yellow overlay; replacing or removing it updates the marker without adding
its bonuses to character wear. Empty slots retain their Dragon frame
and never resolve an icon without an equipment IID. Hovering shows the item tooltip and its enchant level.

The Stablemaster section adds icon bind and accept tag
`stablemaster.duplicate_mount`. It accepts one non-equipped Riding Pet dragged
from the normal inventory, excluding the registered mount. Actions are
`stablemaster.exchange`, `stablemaster.promote`, `stablemaster.grade`,
`stablemaster.writ`, and `stablemaster.duplicate`. Every mutation remains
server-authoritative and requires the player to be in range of Edmund.

Management actions remain `riding.feed`, `riding.ability`, `riding.reroll`,
`riding.enchant`, and `riding.upgrade`. They retain their existing material and
first-eligible-equipment selection behavior. Equipment
buttons and the old `riding.slot.*` actions have been replaced by icon slots.
The 440 x 580 client area uses the shared `dragon_dialog` frame, a Riding menu
emblem, regular/bold FreeType faces, framed equipment slots, and a cropped Dragon
EXP texture for satiety. Management and Stablemaster buttons share the Dragon
button states; the close control is anchored on the lower-right footer.
The editor supplies a populated Battle Riding preview.

---

## `player`

Identity block on Character Info and the status HUD. Source:
`NetClient::CharInfoStats` plus class portrait BMP.

| Key | Type | Example | Used by |
|---|---|---|---|
| `player.name` | string | `Adventurer` | Character Info, status HUD, NPC dialogue text |
| `player.name_label` | string | `Name` | Character Info |
| `player.job` | string | `Magician` | Character Info |
| `player.job_label` | string | `Job` | Character Info |
| `player.guild` | string | `-` | Character Info |
| `player.guild_label` | string | `Guild` | Character Info |
| `player.guild_pos` | string | `-` | Character Info |
| `player.guild_pos_label` | string | `Pos` | Character Info |
| `player.gov_pos` | string | `-` | Character Info |
| `player.gov_pos_label` | string | `G.P` | Character Info |
| `player.awaken` | string | `Awaken -` | Character Info |
| `player.awaken_label` | string | *(empty; value includes prefix today)* | Character Info |
| `player.level` | int | `1` | Character Info, status HUD, level orb |
| `player.level_label` | string | `Level` | Character Info |
| `player.ctb` | int | `0` | Character Info |
| `player.ctb_label` | string | `CTB` | Character Info |
| `player.class_icon` | string | `main-character-icon(musa).bmp` | status HUD |
| `player.specialty_index` | int | `0` | job-title lookup |
| `player.clazz` | int | `1` | Knight=0 … Thief=3 (PU cost preview) |

---

## `stats`

Primary attributes (`PropertyType` Str..Dex) and PU. The Character Info display
formats each primary as `base +equipment bonus`; the base portion includes
staged spend and the bonus sums worn item definitions, prefixes, random
armor attributes, and Collection PvE primaries. Collection includes active
trigram/Taegeuk bonuses and Hanin's Essences amplified by the equipped mirror;
fractional totals are truncated in the same way as server combat.

| Key | Type | Example | Used by |
|---|---|---|---|
| `stats.str` | string | `20 +4` | Character Info, Transform bonus row |
| `stats.str_label` | string | `STR` | Character Info, Transform |
| `stats.str_pending` | bool | `false` | Character Info staged PU |
| `stats.str_preview` | int | `21` | value after staged PU |
| `stats.hth` | string | `18 +2` | Character Info, Transform |
| `stats.hth_label` | string | `HTH` | Character Info (`Health` is a C++ fallback today; JSON uses `HTH`) |
| `stats.hth_pending` | bool | `false` | Character Info |
| `stats.hth_preview` | int | `18` | Character Info |
| `stats.intl` | string | `40 +6` | Character Info (`PropertyType::Int`) |
| `stats.intl_label` | string | `INT` | Character Info |
| `stats.intl_pending` | bool | `false` | Character Info |
| `stats.intl_preview` | int | `40` | Character Info |
| `stats.wis` | string | `22 +0` | Character Info |
| `stats.wis_label` | string | `WIS` | Character Info |
| `stats.wis_pending` | bool | `false` | Character Info |
| `stats.wis_preview` | int | `22` | Character Info |
| `stats.dex` | string | `15 +3` | Character Info (label `AGI` in live JSON) |
| `stats.dex_label` | string | `AGI` | Character Info |
| `stats.dex_pending` | bool | `false` | Character Info |
| `stats.dex_preview` | int | `15` | Character Info |
| `stats.pu_point` | int | `5` | remaining PU before staging |
| `stats.pu_point_label` | string | `Point` | Character Info |
| `stats.pu_remaining` | int | `3` | after staged cost |
| `stats.su_point` | int | `0` | `PropertyType::SuPoint` (not shown yet) |
| `stats.su_point_label` | string | `SU` | reserved |

`stats.spend` / `stats.unspend` / `stats.apply` are **actions**, not
interpolated values (see [`TEMPLATE.md`](TEMPLATE.md) buttons).

---

## `combat`

Derived combat stats. Source: `CharInfoStats`. Attack fields expose both a
range string and the raw min/max.

| Key | Type | Example | Used by |
|---|---|---|---|
| `combat.attack` | string | `12–18` | Character Info ATK |
| `combat.attack_label` | string | `ATK` | Character Info |
| `combat.min_attack` | int | `12` | preview / tooltips |
| `combat.max_attack` | int | `18` | preview / tooltips |
| `combat.mattack` | string | `40–55` | Character Info Mg.ATK |
| `combat.mattack_label` | string | `Mg.ATK` | Character Info |
| `combat.min_magic` | int | `40` | preview |
| `combat.max_magic` | int | `55` | preview |
| `combat.hit` | int | `30` | Character Info OTP |
| `combat.hit_label` | string | `OTP` | Character Info |
| `combat.evasion` | int | `12` | Character Info Eva (`CharInfoStats.dodge`) |
| `combat.evasion_label` | string | `Eva` | Character Info |
| `combat.defense` | int | `20` | Character Info |
| `combat.defense_label` | string | `DEF` | Character Info |
| `combat.absorb` | int | `5` | Character Info Absorb.(%) |
| `combat.absorb_label` | string | `Absorb.(%)` | Character Info |
| `combat.aspeed` | int | `700` | `PropertyType::ASpeed` (not shown on Info yet) |
| `combat.aspeed_label` | string | `ASPD` | reserved |
| `combat.mspeed` | int | `0` | signed move-speed percent (`P_MSPEED`) |
| `combat.mspeed_label` | string | `Move` | reserved |

Each of `attack`, `mattack`, `hit`, `evasion`, `defense`, `absorb` also has a
`_pending` bool when PU preview changes the value.

---

## `resist`

Element resists. Source: `CharInfoStats.res_*`.

| Key | Type | Example | Used by |
|---|---|---|---|
| `resist.fire` | int | `0` | Character Info |
| `resist.fire_label` | string | `Fire` | Character Info |
| `resist.ice` | int | `0` | Character Info |
| `resist.ice_label` | string | `Ice` | Character Info |
| `resist.lightning` | int | `0` | Character Info |
| `resist.lightning_label` | string | `Lightning` | Character Info |
| `resist.curse` | int | `0` | Character Info |
| `resist.curse_label` | string | `Curse` | Character Info |
| `resist.palsy` | int | `0` | Character Info (Non Elemental) |
| `resist.palsy_label` | string | `Non Elemental` | Character Info |

---

## `vital`

HP / MP. Source: `CharInfoStats` and `StatusWidgetData`. Ratios are 0..1 for
`progress` widgets.

| Key | Type | Example | Used by |
|---|---|---|---|
| `vital.hp` | int | `123` | Character Info, status HUD |
| `vital.hp_label` | string | `HP` | Character Info |
| `vital.max_hp` | int | `456` | Character Info, status HUD |
| `vital.hp_text` | string | `123/456` | convenience for `"${vital.hp_text}"` |
| `vital.hp_ratio` | float | `0.27` | status / target `progress` |
| `vital.mp` | int | `80` | Character Info, status HUD |
| `vital.mp_label` | string | `MP` | Character Info |
| `vital.max_mp` | int | `200` | Character Info, status HUD |
| `vital.mp_text` | string | `80/200` | convenience |
| `vital.mp_ratio` | float | `0.4` | status `progress` |

`vital.hp_pending` / `vital.mp_pending` are true when PU preview changes max
HP/MP.

---

## `exp`

Dragon HUD (`DragonHudData`). Player EXP is the bottom gold trough; transform
Rage is the red trough immediately above it. Ratios are independently cropped.

| Key | Type | Example | Used by |
|---|---|---|---|
| `exp.current` | int64 | `1200` | EXP HUD |
| `exp.next` | int64 | `3000` | EXP needed for this level (`total_exp`) |
| `exp.pct` | float | `0.4` | bottom EXP bar ratio |
| `exp.sinsu_pct` | float | `0.1` | Rage bar ratio (`rage_current / rage_total`) |
| `exp.rage_current` | int64 | `50` | EXP HUD / Transform |
| `exp.rage_total` | int64 | `500` | EXP HUD |

---

## `inventory`

The Dragon inventory uses a 532 x 508 client box including the cosmetic slots.
The 14 worn equipment cells and eight skin cells stay visible on the left;
General and Other have vertical
scroll viewports on the right. Requested HUD zoom is capped to fit the full frame.
All labels use FreeType. The bronze Geon panel retains its coin artwork, label,
and right-aligned, comma-formatted amount outside the scrolling area. Its
340 x 48 artwork, label and amount share the same 92-pixel horizontal shift
for the added skin columns; the coin artwork is not stretched.
Opening the inventory selects General.

- **General** (`normal`) stacks six 6-by-6 blocks vertically. The first 72 cells
  are permanent; four timed expansions each add 36 writable cells. There are no
  register tabs. All blocks omit headings and use the normal slot row
  spacing between them. Expansion state remains available through the Expand
  tooltip and locked-slot shading. Expired cells allow take-out, but reject deposits and swaps that would
  put another item into an expired cell.
- **Other** (`quest`) stacks **Quest Items** above **Task Items**, with 36 cells
  per section and a FreeType heading above each grid. `Bag::quest` stores Quest
  cells 0–35 and protected Task cells 36–71. Consumables and ordinary quest rewards
  belong in General.
- Task Items are special definitions with `class[0] == "quest"` that occur in
  positive, direct quest-script item grants and have no positive item entry in
  the configured loot groups. Ordinary weapons, consumables and money rewards
  never become Task Items. The shared `QuestDb` source classification drives
  both client and server; no item names, ID ranges or private source are used.
  Unsupported or missing quest data leaves a special item in Quest Items.
- Task Items cannot be dragged, manually used, sorted into new positions,
  destroyed, dropped, sold or deposited in account storage. The server enforces
  deletion, drop and transfer restrictions. Quest scripts still remove required
  Task Items through their authoritative item exchanges.
- Ordinary equipment and Collection store worn instances outside General.
  Equipping frees the source cell; replacement returns the previous instance to
  that cell. Unequipping requires a free unlocked General cell. Only server
  acknowledgement applies the transfer, preserving the exact IID and metadata.

General has six independent icon toggles directly above its slots:
Consumables, Weapons, Armors, Enchantment, Skins & Pets and Other items.
Selected categories combine with OR; no selection shows everything. Each
change resets General to the first row and compacts matching items in source
order. The projection also refreshes after item updates. It does not change
inventory storage, capacity, instance IDs or the Other sections. Enabled pages
share one compact projection; each expired page is filtered separately and
retains its take-out-only restrictions. Blank displayed cells resolve only to
actually empty source cells, so hidden items cannot be overwritten.

Classification uses item properties, not display names. Consumables use the
existing consume rules (including Auto-HP and inventory writs). Weapons use the
weapon class; Armors include defense gear and ornaments, except face/mask items.
Enchantment includes prefix/charming talismans, Beads of Fire, Qigong tools,
Dragon Spirit Scrolls, weapon enchantment talismans, Revision Hammers,
Polishing Stones and Collection materials. Skins & Pets includes face/mask
items, pets, limited pets, transformation animals and mounts. Unknown or
unmatched definitions belong to Other items.

The mouse wheel scrolls complete rows; arrow buttons and a draggable scrollbar
provide the same navigation. General and Other retain independent scroll
positions. Only complete visible grid rows participate in drawing and hit tests;
scrolling while dragging preserves the input slot.

All item operations keep their existing server authority and instance IDs.
Restored legacy Other overflow stays visible in General instead of being lost.
Selling and account storage continue to include ordinary consumables from General.
At login, legacy General overflow is moved to free account-storage cells with
its instance metadata intact. If both inventories are full, remaining rows stay
saved and the client explains that room and a reconnect are required.

Empty ordinary equipment cells reuse the pre-existing `slot back-*.bmp`
references through `empty.file`. These compatible-client images are optional
runtime input from the configured `UI` / `HyperText` search directories; they
are not redistributed with OpenKal. Missing files leave the Dragon slot frame
visible. Mask and Mount retain the neutral frame because the previous inventory
provided accessory/flag backgrounds, not matching images for those categories.

General block label bindings remain available to custom layouts. The compact
default omits General headings and displays the Quest Items and Task Items headings.

| Key | Type | Example | Used by |
|---|---|---|---|
| `inventory.tab` | enum | `normal` | `normal`, `quest` |
| `inventory.normal_active` | bool | `true` | General visibility |
| `inventory.quest_active` | bool | `false` | Other visibility |
| `inventory.bag_active` | bool | `true` | Sort and Destroy visibility |
| `inventory.scroll` | number | `322` | Active vertical offset in native pixels |
| `inventory.scroll_up` | bool | `true` | Earlier rows available |
| `inventory.scroll_down` | bool | `true` | Later rows available |
| `inventory.scroll_up_label` | string | `^` | Scroll-up caption |
| `inventory.scroll_down_label` | string | `v` | Scroll-down caption |
| `inventory.filter.consumables` | bool | `false` | Selected General category |
| `inventory.filter.consumables_visible` | bool | `false` | Selected indicator, General only |
| `inventory.filter.weapons` | bool | `false` | Selected General category |
| `inventory.filter.weapons_visible` | bool | `false` | Selected indicator, General only |
| `inventory.filter.armors` | bool | `false` | Selected General category |
| `inventory.filter.armors_visible` | bool | `false` | Selected indicator, General only |
| `inventory.filter.enchantment` | bool | `false` | Selected General category |
| `inventory.filter.enchantment_visible` | bool | `false` | Selected indicator, General only |
| `inventory.filter.skins_pets` | bool | `false` | Selected General category |
| `inventory.filter.skins_pets_visible` | bool | `false` | Selected indicator, General only |
| `inventory.filter.other` | bool | `false` | Selected General category |
| `inventory.filter.other_visible` | bool | `false` | Selected indicator, General only |
| `inventory.equipment_label` | string | `Equipped` | Worn equipment heading |
| `inventory.skin_label` | string | `Skins` | Cosmetic slot heading |
| `inventory.skin_locked_label` | string | `Locked` | Whole-body skin lock caption |
| `inventory.skin_locked.<part>` | bool | `false` | True for helmet, gloves, pants, boots and face while a whole-body skin is worn; weapon, shield and upper remain available |
| `inventory.general_capacity` | string | `General: 108 / 216 slots unlocked` | General heading |
| `inventory.general_hint` | string | `72 permanent slots + four timed expansions` | General hint |
| `inventory.other_capacity` | string | `Other: 72 permanent slots` | Other heading |
| `inventory.quest_label` | string | `Quest Items  8 / 36` | Quest count |
| `inventory.task_label` | string | `Task Items  12 / 36` | Task count |
| `inventory.expand_label` | string | `Expand +36` | Expansion button; `Fully expanded` at the limit |
| `inventory.sort_label` | string | `Sort` | Sort button |
| `inventory.destroy_label` | string | `Destroy` | Destroy target caption |
| `inventory.money` | int | `12345678` | Geon amount |
| `inventory.money_label` | string | `Geon` | Geon label |
| `inventory.page1_label` | string | `1-36  Permanent` | Block range and lifetime |
| `inventory.page1_locked` | bool | `false` | Block writability |
| `inventory.page1_shaded` | bool | `false` | Locked shading while General is active |
| `inventory.page2_label` | string | `37-72  Permanent` | Block range and lifetime |
| `inventory.page2_locked` | bool | `false` | Block writability |
| `inventory.page2_shaded` | bool | `false` | Locked shading while General is active |
| `inventory.page3_label` | string | `73-108  Locked` | Block range and lifetime |
| `inventory.page3_locked` | bool | `true` | Block writability |
| `inventory.page3_shaded` | bool | `true` | Locked shading while General is active |
| `inventory.page4_label` | string | `109-144  Locked` | Block range and lifetime |
| `inventory.page4_locked` | bool | `true` | Block writability |
| `inventory.page4_shaded` | bool | `true` | Locked shading while General is active |
| `inventory.page5_label` | string | `145-180  Locked` | Block range and lifetime |
| `inventory.page5_locked` | bool | `true` | Block writability |
| `inventory.page5_shaded` | bool | `true` | Locked shading while General is active |
| `inventory.page6_label` | string | `181-216  Locked` | Block range and lifetime |
| `inventory.page6_locked` | bool | `true` | Block writability |
| `inventory.page6_shaded` | bool | `true` | Locked shading while General is active |
| `inventory.wear_label.helmet` | string | `Helmet` | Worn slot caption |
| `inventory.wear_label.upper` | string | `Armor` | Worn slot caption |
| `inventory.wear_label.gauntlet` | string | `Gloves` | Worn slot caption |
| `inventory.wear_label.pants` | string | `Pants` | Worn slot caption |
| `inventory.wear_label.boots` | string | `Boots` | Worn slot caption |
| `inventory.wear_label.weapon` | string | `Weapon` | Worn slot caption |
| `inventory.wear_label.shield` | string | `Shield` | Worn slot caption |
| `inventory.wear_label.ring` | string | `Ring` | Worn slot caption |
| `inventory.wear_label.necklace` | string | `Necklace` | Worn slot caption |
| `inventory.wear_label.accessory` | string | `Accessory I` | Worn slot caption |
| `inventory.wear_label.face` | string | `Mask` | Worn slot caption |
| `inventory.wear_label.sinsu` | string | `Sin Su` | Worn slot caption |
| `inventory.wear_label.pet` | string | `Pet` | Worn slot caption |
| `inventory.wear_label.ride` | string | `Mount` | Worn slot caption |

Grid binds: `inventory.general.0` through `inventory.general.5` resolve each
36-cell General block; `inventory.quest` resolves Other cells 0–35;
`inventory.task` resolves Other cells 36–71. All are ordinary `icon_grid`
widgets. Existing `inventory.wear.*` paths retain the same suffixes as the
`inventory.wear_label.*` tokens above. Cosmetic icon slots bind to
`inventory.wear.skin.<part>`, where `<part>` is `helmet`, `upper`, `gauntlet`,
`pants`, `boots`, `weapon`, `shield` or `face`. A whole-body skin occupies `upper`.
It returns existing body skins to General and locks the remaining body skin
slots. Weapon and shield skins remain independent. Double-clicking a body-part
skin in General automatically returns a whole-body skin before equipping it;
locked slots do not accept drag/drop. All transfers are server acknowledged,
and insufficient return space preserves every item. Cosmetic storage is outside
General capacity; combat equipment and bonuses stay independent.
Destroy bind: `inventory.destroy`.

Action `inventory.sort` requests server-side consolidation of compatible stackable
items in the selected section. The server checks the catalog, instance properties,
bindings and count capacity, commits all affected rows atomically, then updates the
live inventory and acknowledges the request. Failed saves leave every stack intact.
Only after the item updates and successful acknowledgement does the client order
General across writable blocks, leaving expired cells in place. On Other it merges
and sorts Quest Items and leaves Task Items in place. Sorts are stable by name, item
index and descending count, preserving instance metadata. The action is disabled
during a captured item press/drag or an outstanding sort request. Active trades
reject consolidation. Repeating a request cannot create items.

Actions `inventory.filter.consumables`, `inventory.filter.weapons`,
`inventory.filter.armors`, `inventory.filter.enchantment`,
`inventory.filter.skins_pets` and `inventory.filter.other` toggle their category.
They are hidden on Other and disabled during a captured item press/drag.
Hovering each symbol displays its name, selected state and combination guidance
with FreeType text. Active symbols have a pressed frame and green underline.

Action `inventory.expand` requests use of the shortest-duration available
Inventory Unlock writ, activating the next 36-cell General block. It neither
grants free capacity nor consumes locally before server acknowledgement. It is
disabled at 216 slots and during a captured item press/drag. Its hover tooltip
lists all four expansion ranges, including each remaining lifetime or Locked
state. If no writ is present, the button is disabled and the tooltip explains
that an Inventory Unlock writ is needed.

Actions `inventory.scroll.up` and `inventory.scroll.down` move by one row.
Hovering General/Other for 350 ms while dragging changes the active view while
retaining the underlying storage and instance. Task cells cannot be moved, and Quest/Task cells cannot be swapped
across their category boundary. Destroy retains its existing confirmation flow;
`window.close` remains the shared footer exit.

### Collection drawer

`inventory.collection_toggle` opens the separate `inventory_collection.json`
panel using the restored frame handle. Hovering the handle for 350 ms during an
item drag also opens it. The drawer remains available beside General and Other;
its geometry scales to fit the viewport. Closed or animating cells do not accept
item drops. `collection.toggle` supplies the handle's `<` or `>` caption.
`inventory.collection_absorb` tracks the last selected General Taegeuk IID across
sorting and tab changes; the server validates that item and its eight trigrams.

`inventory.collection.0` through `.25` retain their bindings: 0 Yin-Yang Mirror,
1–8 Geon/Tae/I/Jin/Son/Gam/Gan/Gon, 9 Taegeuk, 10 Hanin's Mirror, 11–15 essences,
16–20 attack Yeoiju and 21–25 defense Yeoiju. Drag a matching General item onto a
cell to equip; double-click an equipped item to remove it. Removal requires a
free writable General cell and waits for server acknowledgement.

String tokens supplied by the inventory host:

- `collection.title`, `collection.absorb`, `collection.absorb_help`
- `collection.taegeuk`, `collection.taegeuk_help`
- `collection.hanin`, `collection.hanin_help`
- `collection.yeoiju`, `collection.attack`, `collection.defense`, `collection.pvp_notice`
| Key | Type | Example | Used by |
|---|---|---|---|
| `collection.empty.0` | string | `Mirror` | Collection cell 0; blank when occupied |
| `collection.empty.1` | string | `Mirror` | Collection cell 1; blank when occupied |
| `collection.empty.2` | string | `Mirror` | Collection cell 2; blank when occupied |
| `collection.empty.3` | string | `Mirror` | Collection cell 3; blank when occupied |
| `collection.empty.4` | string | `Mirror` | Collection cell 4; blank when occupied |
| `collection.empty.5` | string | `Mirror` | Collection cell 5; blank when occupied |
| `collection.empty.6` | string | `Mirror` | Collection cell 6; blank when occupied |
| `collection.empty.7` | string | `Mirror` | Collection cell 7; blank when occupied |
| `collection.empty.8` | string | `Mirror` | Collection cell 8; blank when occupied |
| `collection.empty.9` | string | `Mirror` | Collection cell 9; blank when occupied |
| `collection.empty.10` | string | `Mirror` | Collection cell 10; blank when occupied |
| `collection.empty.11` | string | `Mirror` | Collection cell 11; blank when occupied |
| `collection.empty.12` | string | `Mirror` | Collection cell 12; blank when occupied |
| `collection.empty.13` | string | `Mirror` | Collection cell 13; blank when occupied |
| `collection.empty.14` | string | `Mirror` | Collection cell 14; blank when occupied |
| `collection.empty.15` | string | `Mirror` | Collection cell 15; blank when occupied |
| `collection.empty.16` | string | `Mirror` | Collection cell 16; blank when occupied |
| `collection.empty.17` | string | `Mirror` | Collection cell 17; blank when occupied |
| `collection.empty.18` | string | `Mirror` | Collection cell 18; blank when occupied |
| `collection.empty.19` | string | `Mirror` | Collection cell 19; blank when occupied |
| `collection.empty.20` | string | `Mirror` | Collection cell 20; blank when occupied |
| `collection.empty.21` | string | `Mirror` | Collection cell 21; blank when occupied |
| `collection.empty.22` | string | `Mirror` | Collection cell 22; blank when occupied |
| `collection.empty.23` | string | `Mirror` | Collection cell 23; blank when occupied |
| `collection.empty.24` | string | `Mirror` | Collection cell 24; blank when occupied |
| `collection.empty.25` | string | `Mirror` | Collection cell 25; blank when occupied |

---

## `slot`

In scope only while drawing/hit-testing one `icon_slot` or `icon_grid` cell.

| Key | Type | Example | Used by |
|---|---|---|---|
| `slot.icon` | string | `item032.bmp` | resolved BMP stem |
| `slot.count` | int | `5` | stack badge |
| `slot.equipped` | bool | `false` | worn instance in bag |
| `slot.empty` | bool | `true` | no icon |
| `slot.level` | int | `3` | skill rank under icon |
| `slot.index` | int | `90` | item definition or skill index |
| `slot.iid` | int | `0` | inventory instance id |
| `slot.selected` | bool | `false` | grid highlight |

---

## `apply_item`

Character Info Active Effects grid. The old decorative `apply_item.tab` selector
was removed; all item effects share one grid.

| Key | Type | Example | Used by |
|---|---|---|---|
| `apply_item.buffs` | bind | *(icons)* | visible `icon_grid` of shopbuff BMPs |

---

## `skillbar`

The bottom-centered, non-draggable `config/ui/dragon_hud.json` replaces the
old `quickslot.json`, `skill_bar.json`, and `exp.json` panels. `status.json`
keeps the class emblem and level visible; its framed name appears only while
hovering the class icon. The hidden name area passes pointer input through.
Native size of the bottom HUD is 792 × 180;
smaller viewports fit the entire HUD with uniform scaling.

| Key | Type | Example | Used by |
|---|---|---|---|
| `hud.class_hovered` | bool | `true` | Status name and frame visibility; true only over the class icon while pointer interaction is enabled. The editor previews it as true. |
| `skillbar.active_label` | string | `ACTIVE` | Active skill caption |
| `skillbar.auto_hp_label` | string | `AUTO HP` | Automatic medicine slot caption |
| `skillbar.auto_hp_percent` | int | `50` | Selected threshold (25, 50, 75, 100), or 0 when empty or out of medicine; controls the yellow HP-orb marker |
| `skillbar.auto_hp_25`, `skillbar.auto_hp_50`, `skillbar.auto_hp_75`, `skillbar.auto_hp_100` | bool | `true` | Exactly one threshold line is visible while medicine is available |
| `skillbar.auto_hp_status` | string | `AUTO HP 50%` | FreeType label inside the HP orb |

Icon binds: `skillbar.active`, `skillbar.hotbar` (indices 0–9),
`skillbar.hotbar_shift` (indices 10–19), `skillbar.auto_hp`. The Auto-HP slot
accepts `item.auto_hp` (items with `specialty.refresh.auto_hp: true`). Drag a
medicine into it; the stack stays in the bag. Double-click its icon to cycle
75 → 50 → 25 → 100 → 75 percent. Drag it back to the inventory or outside the
HUD to clear the binding without dropping or consuming medicine. The client
requests one dose when alive and strictly below the threshold; the server
checks HP again and enforces the shared medicine cooldown. Depleted stacks
leave a dim icon and hide the orb marker until restocked. The binding and
threshold are saved per character in `ui_settings.json` whenever changed and
restored on login, including bindings whose bag stacks are depleted. Older
settings default to an empty slot at 75 percent. Unknown or ineligible item
definitions clear the slot on restore.

SkillBar owns rendering and interaction
for these bindings, including item counts, cooldowns, tooltips and cross-row
swaps. Lower row keys are 1–9,0; upper row keys are Shift+1–9,0. These replace
the former Shift+digit city-teleport shortcuts. Text-input focus suppresses
both rows. The auto-HP slot has no binding, action or drop target yet.

All twenty bindings persist per character through C2S/S2C_SHORTCUT and SQLite.
The decoder also accepts old ten-slot packets (empty upper row); the additive
SQLite migration preserves slots 0–9 and initializes slots 10–19 to -1.
New packets contain the flag byte, active uint16, and twenty uint16 bindings
(43-byte payload); old 23-byte payloads remain readable. Deploy the updated
client and server together to persist the upper row.

---

## `skill`

Skill learn window and hotbar.

| Key | Type | Example | Used by |
|---|---|---|---|
| `skill.tab` | enum | `common` | `common` \| `job1` \| `job2` \| `job3` \| `awaken` \| `action` |
| `skill.active_label` | string | `Active` | section caption (same as `ui.active`) |
| `skill.passive_label` | string | `Passive` | section caption |
| `skill.hover_name` | string | `Fire Ball` | tooltip / status line |

`skill_window.json` uses the Dragon frame, `dragon_menu/skill.bmp`, a bold
FreeType title and an anchored footer close button. Six standard buttons use
`skill.tab.common`, `skill.tab.job1`, `skill.tab.job2`, `skill.tab.job3`,
`skill.tab.awaken`, and `skill.tab.action` actions. The host disables locked
specialty pages and keeps the selected page pressed.

| Key | Type | Example | Meaning |
|---|---|---|---|
| `skill.title` | string | `Skills` | Dragon dialog caption |
| `skill.action_page` | bool | `false` | Continuous Action grid visibility |
| `skill.class_page` | bool | `true` | Active/Passive class grid visibility |

Grid binds: `skill.active`, `skill.passive`, `skill.actions`. The Action page
shows a continuous five-row grid; class pages show three active and two passive
rows. Visibility uses `skill.action_page` and `skill.class_page`. The client draws skill icons,
selection, cooldowns and FreeType rank labels over the shared slot frames;
learning and hotbar dragging retain their existing behavior. The editor supplies
the Common page's labels and frames. See `skillbar` for HUD hotbars.
Active mouse slot: `skill.active_slot` (`icon_slot.bind`).

---

## `transform`

H-key Ancient animal window and worn-egg bonuses
(`protocol::worn_transform_bonus`).

| Key | Type | Example | Used by |
|---|---|---|---|
| `transform.equipped` | bool | `true` | hide egg UI when false |
| `transform.name` | string | `Baby Wolf` | egg name / stage name |
| `transform.level` | int | `12` | egg level |
| `transform.exp` | int | `40` | EXP toward next animal level |
| `transform.rage` | int | `50` | `P_RAGE` (also `exp.rage_current`) |
| `transform.form_stage` | int | `1` | 0 = none, 1..3 baby/young/adult |
| `transform.race` | int | `0` | item definition race |
| `transform.str` | int | `3` | worn bonus (use `: +` in text) |
| `transform.hth` | int | `2` | worn bonus |
| `transform.dex` | int | `0` | worn bonus |
| `transform.intl` | int | `1` | worn bonus |
| `transform.wis` | int | `0` | worn bonus |

Egg icon bind: `transform.egg`. Stage grid: `transform.stages`. Animal skill
grid: `transform.animals`.

The Shinsu window uses a 440 x 254 client area with the shared `dragon_dialog`
frame, the Shinsu menu emblem, regular/bold FreeType faces, and an anchored
lower-right close button. Egg details and worn bonuses sit above two groups of
framed stage and skill icons. Existing stage selection, dragging and tooltips
continue to use the JSON grid positions.

---

## `mix`

Dragon Mix Master uses the shared frame, framed recipe slots, centered Dragon
buttons and the anchored footer close control. The center accepts the weapon;
the four outer slots accept weapons, armor and mixing stones. Clicking a placed
item removes it. The NPC selects elemental or imperial mode; existing recipe
validation and the charge/send flow are preserved.

| Key | Type | Example | Used by |
|---|---|---|---|
| `mix.title` | string | `Mix Master` | Dragon title |
| `mix.mode` | string | `Elemental Mixing` | Active NPC mode; alternatively `Imperial Mixing` |
| `mix.placement_hint` | string | `Place a weapon in the center and materials around it.` | Placement guidance |
| `mix.weapon_label` | string | `Weapon` | Center slot caption |
| `mix.remove_hint` | string | `Click a placed item to remove it.` | Removal guidance |
| `mix.status` | string | `Place the weapon you want to mix in the center.` | Empty, unavailable, incomplete or ready feedback |
| `mix.can_start` | bool | `false` | enables Start Mixing |
| `mix.can_start_label` | string | `Start mixing` | same as `ui.start_mixing` |

Slot binds: `mix.center`, `mix.north`, `mix.east`, `mix.south`, `mix.west`.
Actions: `mix.start`, `window.close`.

## `mix_list`

`mixing_item_list.json` is the read-only Dragon view for the NPC material lists.
`MixWindow` switches between this document and the mixing-slot document within
the existing Mixing window, retaining normal focus, Escape and dragging behavior.
The lists accept no inventory drops and never send a mixing request.

Rows come from `MixCatalog::material_rows`: upgrade recipes use the same effective
grade constraints as the matcher; creation recipes use their own material specs.
Nine rows fit each page. With the current configuration, Upgrade has a page for
ordinary stones and a second page for Grade 3, while Create Weapon has one page.
Gold comes from the configured recipe cost, including zero-cost recipes.

| Key | Type | Meaning |
|---|---|---|
| `mix_list.title` | string | Window title |
| `mix_list.upgrade_label`, `mix_list.create_label` | string | View-selector captions |
| `mix_list.upgrade_color`, `mix_list.create_color` | color | Active view emphasis |
| `mix_list.description` | string | Current Upgrade/Create description |
| `mix_list.weapon_label`, `mix_list.extra_label`, `mix_list.armor_label`, `mix_list.stones_label`, `mix_list.gold_label` | string | Table column captions |
| `mix_list.previous_label`, `mix_list.next_label` | string | Page-control captions |
| `mix_list.page` | string | One-based current page and total |
| `mix_list.empty` | bool | No recipes available |
| `mix_list.empty_label` | string | Empty-list message |
| `mix_list.note`, `mix_list.stone_note`, `mix_list.warning` | string | Mixing rules and material-consumption guidance |
| `mix_list.row.0` through `mix_list.row.8` | group | `visible`, `weapon`, `extra`, `armor`, `stones`, `gold` for each displayed row |

Actions: `mix.list.upgrade`, `mix.list.create`, `mix.list.previous`,
`mix.list.next`, `window.close`. Page controls disable at their bounds. Switching
views resets paging. All cells use standard FreeType text and fill widgets;
the UI editor supplies a populated preview.

The view displays the nine configured grade combinations for upgrading and
creating weapons, plus the Grade-3 material requirements and recipe costs.

---

## `chat`

| Key | Type | Example | Used by |
|---|---|---|---|
| `chat.mode` | string | `Normal` / `PM` | mode label |
| `chat.mode_color` | color | `#EBEBE0` | channel-colored compose label |
| `chat.tab` | string | `normal` | multiplayer `channels` tab strip |
| `chat.tab_normal` | string | `Normal` | Normal tab caption |
| `chat.tab_party` | string | `Party` | Party tab caption |
| `chat.tab_guild` | string | `Guild` | Guild tab caption |
| `chat.tab_alliance` | string | `Alliance` | Alliance tab caption |
| `chat.tab_squad` | string | `Squad` | Squad tab caption |
| `chat.tab_shout` | string | `Shout` | Shout tab caption |
| `chat.input` | string | *(compose buffer)* | `input` bind |
| `chat.lines` | list | *(message rows)* | `list` bind |
| `chat.messages` | list | *(battle and system rows)* | message feed in `chat` / `battle_feed` |
| `chat.focused` | bool | `false` | command input visibility in `battle_feed` |
| `chat.history_open` | bool | `false` | unfaded scrollback and its hint |
| `chat.history_hint` | string | `Wheel: history \| Enter: command \| Esc: close` | scrollback hint |

`chat.json` places a standard `tabs` widget (`channels`, bound to `chat.tab`)
below the compose row. Its IDs are `normal`, `party`, `guild`, `alliance`, `squad`,
and `shout`, in that order. `ChatWidget` shares the document geometry between tab
rendering and hit testing; the UI editor previews the same captions.
The JSON width is the base width. At layout time multiplayer chat expands into
half of the positive horizontal gap between its right edge and the left edge of
the centered skill/Dragon HUD. The remaining half stays clear. Saved chat X
positions and UI scaling use the same calculation. The tab strip stretches
proportionally, and the input/history use the expanded width.
The compose label uses its sending channel's RGB color, including explicit
prefixes; personal-message drafts show `PM` in blue. Backspace uses the operating
system's native key-repeat events, preserving its initial delay and repeat rate
in every text field rather than deleting once per rendered frame.
Selecting a tab changes both the visible scrollback filter and the default send
destination without taking keyboard focus. Each tab initially shows only its own
channel; Normal additionally shows blue personal messages. The options gear edits
the selected tab's channel flags. Battle message flags and the separate upper
battle/system feed stay global. Hidden channel messages remain in scrollback and
become visible when their tab/filter is selected.

The display omits routing markers (`#`, `$`, `~`, `^`, `!`) and the `@Name`
recipient prefix. Punctuation within the message body remains unchanged.
Incoming personal messages retain their sender as reply metadata. Clicking any
visible wrapped row opens a focused `@Partner ` draft; outgoing echoes target
the recipient. The click regions follow the rendered filtering,
wrapping and scroll position. Opening a PM switches to Normal and enables its
personal-message filter. It does not send a message.

Plain input follows the selected tab. Explicit `#`, `$`, `~`, `^`, `@Name`, `!`
and slash commands keep their routing. Party chat reaches current party members
across maps and sight range; Shout reaches playing clients on the server, with
`/s` and `/y` normalized to the `^` channel marker. Party chat is golden yellow, Guild turquoise,
Shout pink, personal messages blue, and Alliance dark blue-purple. Alliance and
Squad are prepared tabs: plain input retains its draft and reports that the
channel is unavailable until those membership systems exist. Squad is intended
to join multiple parties. Server replies appear in the upper system feed so
membership failures remain visible on every tab. Channel preferences are stored
per character with `channel_version: 2`; the previous matrix migrates supported
custom filters, resets obsolete active tabs to Normal, and upgrades old default
rows to focused filters. Battle preferences survive the migration.

`battle_feed.json` is the single-player presentation of `ChatWidget`. It reuses
the message filters and history; player channels remain in the multiplayer
`chat.json` presentation. Server chat announcements/replies join the event feed.
The host draws `panel_fill` as a cached soft alpha mask without frame artwork,
bottom-aligns at most five 20-pixel rows, and fades events from age 8 to 12 seconds.
Wheel input opens unfaded history; Enter opens a slash-command editor. Escape or
an outside click closes history. Passive messages pass world clicks through.
At sufficient viewport width the panel sits 24 scaled pixels from the bottom-left
edge and at least 24 pixels clear of the Dragon HUD; otherwise it sits above it.
Its position is automatic, independent of saved multiplayer chat positions.

---

## `hud`

Menu-strip **actions** (`HudMenuAction`), not interpolated stats. Bind a
`button.action` to:

`hud.menu.sinsu`, `hud.menu.riding`, `hud.menu.character`, `hud.menu.inventory`,
`hud.menu.skill`, `hud.menu.quest`, `hud.menu.party`, `hud.menu.community`,
`hud.menu.map`, `hud.menu.messages`, `hud.menu.trading_post`, `hud.menu.system`.

Live `GameHud` resolves menu actions from the `button` hit regions in
`config/ui/menu_bar.json`. Any remaining part of the strip is its drag surface.

The compact Dragon menu is 472 × 42 native pixels. Its twelve 34 × 34 buttons
have separate 24 × 24 relief icons. `file_default`, `file_over` and `file_on`
select idle, hover and open/pressed artwork. Open state comes from the actual
window, including keyboard toggles and close buttons; several markers can be
on at once. Foreground windows, modal dialogs and dragging suppress hover.
The end caps and narrow rail remain drag surfaces; positions use the existing
per-character HUD store.

For menu buttons, `text` supplies the tooltip (the visible button `label`
stays empty). `MenuBarWidget` interpolates it and renders FreeType text,
clamped to the screen, below the button or above it near the bottom edge.

| Key | Type | Value | Used by |
|---|---|---|---|
| `hud.menu.sinsu.tooltip` | string | `Shinsu (H)` | menu tooltip |
| `hud.menu.riding.tooltip` | string | `Riding (E)` | menu tooltip |
| `hud.menu.character.tooltip` | string | `Character (C)` | menu tooltip |
| `hud.menu.inventory.tooltip` | string | `Inventory (I)` | menu tooltip |
| `hud.menu.skill.tooltip` | string | `Skills (K)` | menu tooltip |
| `hud.menu.quest.tooltip` | string | `Quests (Q)` | menu tooltip |
| `hud.menu.party.tooltip` | string | `Party actions / invite selected player` | menu tooltip |
| `hud.menu.community.tooltip` | string | `Guild` | multiplayer guild window |
| `hud.menu.map.tooltip` | string | `Map (M)` | menu tooltip |
| `hud.menu.messages.tooltip` | string | `Messages (L)` | menu tooltip |
| `hud.menu.messages.unread` | integer | `3` | unread badge visibility |
| `hud.menu.messages.badge` | string | `3` / `99+` | FreeType unread badge |
| `hud.menu.trading_post.tooltip` | string | `Trading Post` | menu tooltip |
| `hud.menu.trading_post.pending` | bool | `true` | collection indicator visibility |
| `hud.menu.trading_post.badge` | string | `!` | outlined FreeType collection indicator |
| `hud.menu.system.tooltip` | string | `System (Esc)` | menu tooltip |

The Trading Post balance shows a black `!` with the same white outline as the
message count while this character has purchased items, sale proceeds or
withdrawn items awaiting collection. The server supplies the flag on world
entry and pushes committed purchases/returns to the buyer and sale proceeds to
an online seller. Opening Collection does not dismiss it. Connection and
character changes clear the old character's flag until the new snapshot arrives.

Messages toggles the Dragon inbox, matching L. The badge hides at zero and
caps its display at `99+`. Read IDs from older local settings remain supported;
the server also persists read state. Selecting a row does not mark it read:
use Read or double-click. Claimed messages count as read. The buff notification
indicates unclaimed attachments independently and uses the same bronze envelope.

Shinsu opens the existing Transform window, matching H. Party opens its existing
actions dialog; Community opens Guild on enabled multiplayer servers. Map opens the full-screen world map; `M`,
`Esc`, or its lower-right close control closes it. The overlay assembles a 5x
viewport around the player from the same `MiniMap/minimap_n_*.gtx` sections as
the minimap, so the hero and camera-direction markers use the exact same atlas
coordinates. It loads a few missing sections per frame and shows a loading
counter while doing so.

---

## `game_menu`

The modal in-game menu (`config/ui/game_menu.json`) uses the following static
TrueType labels and view-state flags:

| Key | Type | Example | Used by |
|---|---|---|---|
| `game_menu.title` | string | `Menu` | ESC menu title |
| `game_menu.cancel` | string | `Cancel` | close-menu button |
| `game_menu.options` | string | `Options` | reserved options button |
| `game_menu.select_character` | string | `Select Character` | return-to-selection button |
| `game_menu.close` | string | `Close` | client-close button |
| `game_menu.close_question` | string | `Are you sure?` | close confirmation |
| `game_menu.yes` | string | `Yes` | confirm client close |
| `game_menu.no` | string | `No` | cancel client close |
| `game_menu.main` | bool | `true` | main button visibility |
| `game_menu.confirming` | bool | `false` | confirmation visibility |

Button actions: `game_menu.cancel`, `game_menu.options`,
`game_menu.select_character`, `game_menu.close`, `game_menu.close_confirm`, and
`game_menu.close_cancel`. `game_menu.options` opens the profile-backed settings
window.

---

## `character_delete`

Character-selection deletion confirmation and recovery list
(`config/ui/character_delete.json`, `BootShell`).

| Key | Type | Notes |
|---|---|---|
| `character_delete.title` | string | Modal caption |
| `character_delete.question` | string | Confirmation question including the selected name |
| `character_delete.recovery_note` | string | Thirty-day recovery explanation |
| `character_delete.yes` | string | Confirm soft deletion |
| `character_delete.no` | string | Cancel soft deletion |
| `character_delete.columns` | string | Level/name/class/days column headings |
| `character_delete.empty` | string | Empty deleted-character list message |
| `character_delete.status` | string | Restore feedback |
| `character_delete.restore` | string | Restore button label |
| `character_delete.close` | string | Close-list button label |
| `character_delete.confirming` | bool | Confirmation-widget visibility |
| `character_delete.listing` | bool | Recovery-list visibility |
| `character_delete.empty_visible` | bool | Empty-list message visibility |

List bind: `character_delete.rows`. Actions: `character_delete.confirm`,
`character_delete.cancel`, `character_delete.select`, `character_delete.restore`,
and `character_delete.close`.

---

## `settings`

The in-game settings window (`config/ui/settings.json`, `SettingsWindow`) edits
a draft of the client values stored in the active `.profile` file. When the
startup profile comes from read-only `config.pak`, client settings are saved to
`client_settings.profile` in the save directory (`OPENKAL_SAVE_DIR` when set),
created on the first save and loaded again on startup. Display,
rendering, and audio changes apply after a successful save. World-streaming changes are marked
as restart-required because the current streamer captures its budgets when the
world session starts.

The standard `tabs` widget binds `settings.tab` to `display`, `world`, `render`,
`audio`, or `battle`. Boolean `settings.page.<id>` values control each page's
visibility. `settings.world_tab`, `settings.render_tab`, `settings.battle_heading`,
and `settings.battle_note` provide the additional tab captions and filter guidance.
`settings.battle.0` through `.9` are toggle labels in `BattleMsgKind` order
(Damage, Skill, HP, MP, Rage, EXP, Animal EXP, Item use, Item get, Geon);
`settings.battle.system` labels the untyped-message toggle. Matching
`settings.battle.<index>` and `settings.battle.system` actions toggle the draft.
Save applies and persists these filters per character in `ui_settings.json`;
Cancel discards them. Each label key has `.active` and `.inactive` Boolean
companions for the emerald and empty-socket images; both are false off the Battle
page. The long button artwork has its own native 276 x 30 proportions.
Before character selection, `settings.battle_available` is false and
`settings.battle_unavailable` is true: the Battle tab becomes a grey, non-interactive
button and clicking it leaves the current page selected.
Old saves default system messages to enabled. The editor
previews the Battle messages page. Filters apply to retained history as well as
new messages; hidden events are still retained.

| Key | Type | Example | Used by |
|---|---|---|---|
| `settings.title` | string | `Options` | window title |
| `settings.display_heading` | string | `Display` | section title |
| `settings.mode_label` / `settings.mode` | string | `Window mode` / `Borderless maximized` | display mode row |
| `settings.resolution_label` / `settings.resolution` | string | `Resolution` / `1920 x 1080` | resolution row |
| `settings.world_heading` | string | `World and effects` | section title |
| `settings.preset_label` / `settings.preset` | string | `Quality preset` / `High` | preset row |
| `settings.view_distance_label` / `settings.view_distance` | string | `View radius (tiles)` / `3` | streaming row |
| `settings.preload_margin_label` / `settings.preload_margin` | string | `Preload margin (tiles)` / `1` | streaming row |
| `settings.resident_limit_label` / `settings.resident_limit` | string | `Resident tile limit` / `81` | cache row |
| `settings.upload_budget_label` / `settings.upload_budget` | string | `Terrain upload budget` / `2.50 ms` | terrain GPU-upload row |
| `settings.effect_distance_label` / `settings.effect_distance` | string | `Effect distance` / `500` | authored particle-cull scale row |
| `settings.restart_note` | string | `World settings apply after restarting the client.` | world section note |
| `settings.status` | string | `Save failed: profile changed on disk` | profile-save feedback |
| `settings.render_heading` | string | `Rendering` | section title |
| `settings.grass_label` / `settings.grass` | string | `Terrain grass` / `On` | KCM decoration-layer toggle |
| `settings.bloom_label` / `settings.bloom` | string | `Bloom` / `Subtle` | bloom quality row |
| `settings.shadows_label` / `settings.shadows` | string | `Ground shadows` / `Actors + world` | projected actor/world-shadow coverage row |
| `settings.monsters_label` / `settings.monsters` | string | `Rendered monsters` / `25` | closest visible monster-body limit; `Max` disables the limit while nameplates remain visible |
| `settings.players_label` / `settings.players` | string | `Rendered players` / `25` | nearest non-party remote player bodies and shadows, plus all own-party members; selected non-party target has priority; local player is excluded; `Max` disables the limit |
| `settings.player_names_label` / `settings.player_names` | string | `Player nameplates` / `10` | nearest on-screen non-party name and guild labels, plus all own-party labels; `Off` hides only non-party labels, `Max` disables the name limit; chat bubbles remain visible |
| `settings.audio_heading` | string | `Audio` | section title |
| `settings.effect_volume_label` / `settings.effect_volume` | string | `Effects` / `5` | initial 0--5 effect volume |
| `settings.bgm_volume_label` / `settings.bgm_volume` | string | `Background music` / `5` | initial 0--5 BGM volume |
| `settings.bgm_enabled_label` / `settings.bgm_enabled` | string | `BGM` / `On` | BGM toggle |
| `settings.listener_follows_camera_label` / `settings.listener_follows_camera` | string | `Camera-based audio` / `Off` | use the zoom-dependent camera position as the spatial-audio listener |
| `settings.previous` / `settings.next` | string | `<` / `>` | value buttons |
| `settings.save` / `settings.cancel` | string | `Save` / `Cancel` | footer buttons |

Actions: `settings.display.previous`, `settings.display.next`,
`settings.resolution.previous`, `settings.resolution.next`,
`settings.preset.previous`, `settings.preset.next`, and matching
`settings.<field>.decrease` / `settings.<field>.increase` actions for
`view_distance`, `preload_margin`, `resident_limit`, `upload_budget`,
`effect_distance`, `effect_volume`, and `bgm_volume`. Rendering uses
`settings.grass.toggle`, `settings.bloom.previous`, `settings.bloom.next`,
`settings.shadows.previous`, `settings.shadows.next`, `settings.monsters.previous`,
`settings.monsters.next`, `settings.players.previous`, `settings.players.next`,
`settings.player_names.previous`, and `settings.player_names.next`. `settings.bgm.toggle`
toggles music and `settings.listener_follows_camera.toggle` toggles camera-based audio;
`settings.save` writes the profile and `settings.close` discards the draft.

---

## `quest`

Quest journal (`config/ui/quest_journal.json`, `QuestJournalWindow`) uses the
shared Dragon frame, quest menu emblem, and anchored footer close control.
The `journal` canvas (`kind: quest_journal`) contains wrapped selectable rows
in its left 40% and the selected quest details in the remaining space. Item
progress stays at the bottom of the detail pane and updates from the live bag.
The canvas geometry, captions, frame, and close button are editable in JSON;
quest wrapping and selection remain in C++ and use dedicated FreeType faces.

| Key | Type | Notes |
|---|---|---|
| `quest.title` | string | Window caption |
| `quest.active_label` | string | Active-quest section heading |
| `quest.details_label` | string | Selected-quest section heading |
| `quest.count` | string | Number of active quests |
| `quest.snitch_label` | string | Objective map button caption |

Canvas kind: `quest_journal`. Actions: `window.close`, `quest.snitch`.

`quest.snitch` opens the compact, draggable `quest_snitch.json` Dragon dialog.
Its standard rectangular `minimap` canvas binds to `snitch.map` and reuses the
Powerbook map renderer. The current selected quest and flag determine sources:
missing required items show matching normal drops or drops for that exact quest
and stage; met requirements and talk objectives show the stage's return NPC.
The live bag, quest updates and selection changes refresh the destination.
Missing catalog data produces an explicit empty state. Escape or the footer X
closes Snitch; closing the journal also closes its child window.

| Key | Type | Notes |
|---|---|---|
| `snitch.title` | string | Window caption |
| `snitch.target`, `snitch.target_more` | string | Font-wrapped target name, up to two lines |
| `snitch.position` | string | Map and center coordinates in server pixels |
| `snitch.legend` | string | Marker explanation |
| `snitch.page` | string | Current destination / total |
| `snitch.multiple` | bool | Show destination paging controls |
| `snitch.previous`, `snitch.next` | string | Paging button captions |
| `snitch.map` | canvas | Selected NPC point or monster spawn area |

Actions: `snitch.previous`, `snitch.next` cycle configured destinations;
`window.close` closes the compact window. The editor previews synthetic bindings
and a highlighted map area using existing schema widgets.

---

## `npc`

NPC talk (`NpcTalkWindow` + `DialogCatalog`). Its frame and default geometry are
defined in `config/ui/npc_talk.json` with Dragon chrome, a quest emblem and an
anchored footer close button. Body and response-text wrapping stay in C++.
Each response groups its text and button on a subtly darkened background with
padding and a gap between rows. Hovering either the text or button highlights
the entire row. The separate, fixed-width `Select` button is aligned
20 native pixels from the right client edge. Only the button activates a response;
wrapped text reserves the button column, and the button is centered on its row.
`DialogDef.width/height` may override the layout size. Wrapped body lines and
responses flow across bounded pages, so neither long explanations nor large
service menus are clipped. Previous/Next and the mouse wheel navigate pages;
wheel input over the conversation is consumed instead of zooming the camera.
Paginated conversations reserve the last 44 native pixels of the content area
for navigation.
Response hitboxes use native coordinates on the active page, so moving or
scaling the window does not leave stale clickable areas.

NPC dialogue captions, body lines and link labels accept `${player.name}`. It is
resolved to the active character name when the dialogue opens, before FreeType
wrapping and link layout. The shared dialogue definition remains a template.
For example, `[${player.name}] : I will help.` renders as
`[TestKn8] : I will help.` for that character. Local previews without a character
name use `Player`. Link actions and quest IDs are never interpolated.

| Key | Type | Notes |
|---|---|---|
| `npc.name` | string | Speaker |
| `npc.closable` | bool | Shows the footer close button when dismissal is allowed |
| `npc.paged` | bool | Shows navigation when content spans multiple pages |
| `npc.page` | string | One-based current page and total |
| `npc.previous_label` / `npc.next_label` | string | Navigation captions |
| `npc.body` | string | Dialog text |
| `npc.link_label` | string | Current hyperlink caption |

List binds: `npc.body`, `npc.links`. Button actions: `npc.open_dialog`,
`npc.quest_link`, `npc.save_revival:<point>`, `npc.village_revival`,
`npc.blacksmith`, `npc.mixing`, `npc.speed_medicine`, `npc.previous`, `npc.next`.
Previous and Next are disabled at their respective bounds. Unknown link types
leave the conversation open instead of silently ending it.

The project-owned blacksmith menu (dialog 243) opens the Explosive Blow service,
its instructions (244), inventory conversion instructions (990243), and trigram
instructions (990246). Costume/riding decomposition and equipment exchange
explicitly lead to an availability explanation (990244); they do not masquerade
as executable services or silently close the conversation. These texts are
OpenKal content based on the implemented inventory and upgrade behavior;
no external dialogue text is imported.
Dialog link types `mixing_up` and `mixing_new` open the actual mixing service.
`mixing_list_up` and `mixing_list_new` open the corresponding read-only material
table. Dialog 277 uses these separate list actions for its two Mixing Item List
responses; selection is based on the action type, never on the displayed text.
`npc.shop` opens the current server-authoritative NPC shop.
`npc.speed_medicine` opens the two-route quantity selector after dialog link type
`speed_medicine` is selected; the server still validates the current chemist and range.

The project-owned Executive menu (NPC definition 210, dialog 184) uses dialog
link type `guild_create` to open the creation page with live server requirements
and fees, and `guild` to open the guild browser. Both report unavailable guild
service in system chat. Guild request rejections appear in system chat as well
as the guild window, including required and owned Zamogeon on insufficient funds.

---

## `speed_medicine`

Chemist recipe selector (`config/ui/speed_medicine.json`, `SpeedMedicineWindow`).
The requirements panel lists five resources with separate right-aligned owned and
required amounts. Legacy `materials` and `fee` summary tokens remain available.

| Key | Type | Notes |
|---|---|---|
| `speed_medicine.title` | string | Window caption |
| `speed_medicine.from_blood_fish_label` | string | Medicine-from-Sea-Bream-with-Blood label |
| `speed_medicine.blood_fish_label` | string | Sea-Bream-with-Blood-from-raw-materials label |
| `speed_medicine.from_blood_fish` | uint16 | Requested medicine-from-Sea-Bream-with-Blood count |
| `speed_medicine.blood_fish` | uint16 | Requested Sea-Bream-with-Blood-from-raw-materials count |
| `speed_medicine.requirements_label` | string | Requirements column heading |
| `speed_medicine.owned_label` | string | Owned column heading |
| `speed_medicine.required_label` | string | Required column heading |
| `speed_medicine.requirements.demon_blood.label` | string | Blood requirement label |
| `speed_medicine.requirements.demon_blood.owned` | uint64 | Owned Blood amount |
| `speed_medicine.requirements.demon_blood.required` | uint64 | Required Blood amount for the selected recipes |
| `speed_medicine.requirements.sea_bream.label` | string | Sea Bream requirement label |
| `speed_medicine.requirements.sea_bream.owned` | uint64 | Owned Sea Bream amount |
| `speed_medicine.requirements.sea_bream.required` | uint64 | Required Sea Bream amount for the selected recipes |
| `speed_medicine.requirements.bamboo_keg.label` | string | Keg requirement label |
| `speed_medicine.requirements.bamboo_keg.owned` | uint64 | Owned Keg amount |
| `speed_medicine.requirements.bamboo_keg.required` | uint64 | Required Keg amount for the selected recipes |
| `speed_medicine.requirements.blood_fish.label` | string | Sea Bream with Blood requirement label |
| `speed_medicine.requirements.blood_fish.owned` | uint64 | Owned Sea Bream with Blood amount |
| `speed_medicine.requirements.blood_fish.required` | uint64 | Required Sea Bream with Blood amount for the selected recipes |
| `speed_medicine.requirements.geon.label` | string | Geon requirement label |
| `speed_medicine.requirements.geon.owned` | uint64 | Owned Geon amount |
| `speed_medicine.requirements.geon.required` | uint64 | Required Geon amount for the selected recipes |
| `speed_medicine.materials` | string | Required and owned raw-material summary |
| `speed_medicine.fee` | string | Required Sea Bream with Blood and Geon summary |
| `speed_medicine.hint` | string | Production time and cancellation hint |
| `speed_medicine.make_label` | string | Make button label |

Actions: `speed_medicine.from_blood_fish.decrease`,
`speed_medicine.from_blood_fish.increase`, `speed_medicine.blood_fish.decrease`,
`speed_medicine.blood_fish.increase`, `speed_medicine.make`.

---

## `blacksmith`

Explosive Blow (`config/ui/blacksmith.json`, `BlacksmithWindow`) uses the shared
Dragon frame, skill emblem, FreeType caption and anchored footer close control.
Its standard icon slot resolves the exact selected weapon instance and protection
marker. Only the framed weapon slot accepts inventory drops; clicking it removes
the selection. Long weapon names are shortened to fit the document's text width.
The title supports dragging, and all controls use document hit testing at the
active UI scale. Hover, pressed and disabled states use the shared Dragon art.

| Key | Type | Notes |
|---|---|---|
| `blacksmith.title` | string | Dialog caption |
| `blacksmith.weapon_name` | string | Selected weapon name or empty-slot label |
| `blacksmith.weapon_hint` | string | Weapon grade/removal hint or eligibility hint |
| `blacksmith.placement_hint` | string | Inventory drag instruction |
| `blacksmith.level_label`, `blacksmith.chance_label`, `blacksmith.stones_label`, `blacksmith.fee_label` | string | Detail row captions |
| `blacksmith.level`, `blacksmith.cap` | string | Current Explosive Blow level and maximum, or `-` |
| `blacksmith.chance` | string | Current success percentage, or `-` |
| `blacksmith.stones_owned`, `blacksmith.stones_required` | string | Bag total and stones per Revise attempt |
| `blacksmith.geon_owned`, `blacksmith.fee` | string | Bag currency and next Explosive Blow fee |
| `blacksmith.status`, `blacksmith.warning` | string | Readiness feedback and failure warning |
| `blacksmith.revise_label`, `blacksmith.upgrade_label` | string | Action captions |
| `blacksmith.can_revise` | bool | Eligible weapon, enough stones and chance below 100% |
| `blacksmith.can_eb` | bool | Eligible weapon, below cap and enough Geon |

Slot bind: `blacksmith.weapon`; accept tag: `item.weapon`. Button actions:
`blacksmith.revise`, `blacksmith.explosive_blow`, `window.close`.
Availability is refreshed from the live bag on both drawing and clicking, so
removed or equipped weapons cannot trigger stale actions. The existing charge
overlay, requests, upgrade rules and resource consumption remain the action path.
The editor supplies synthetic preview values for every binding; no schema type
or new action is introduced.

---

## `shop`

NPC merchant (`config/ui/shop.json`, `ShopWindow`). The left pane uses the
Dragon frame and slot artwork with a compacted list of normal, unequipped stacks
whose definition has `sell > 0`. Empty and unsaleable slots are omitted from
both drawing and hit testing. Register tabs select 36-entry pages and clamp to
the last populated page as items disappear. CTRL toggles additional marks;
selling multiple marked entries sells their full stacks. The right grid uses
5 columns and 7 rows; arrows page through server offers with `buy > 0`.
Both panes use FreeType text, Dragon buttons and a shared opaque background.
Both grids use 32-pixel icons inside 42-pixel frames on a 46-pixel pitch in both
axes. Their panels use 12-pixel insets; the left pane reserves its final row for
register tabs. Currency, selection details and the quantity overlay use the same
dark fill and 1-pixel bronze borders as NPC response rows.

| Key | Type | Notes |
|---|---|---|
| `shop.title` | string | Window caption |
| `shop.inventory_label` | string | Left-pane caption |
| `shop.offers_label` | string | Right-pane caption |
| `shop.money` | string | Current player money line |
| `shop.money_label` | string | Currency field label (`Geon`) |
| `shop.selected_name` | string | Selected item name |
| `shop.selected_price` | string | Selected unit buy/sell price |
| `shop.inventory_register` | int | Zero-based left-pane inventory register |
| `shop.inventory_count` | int | Number of saleable stacks across inventory pages |
| `shop.offers_page` | string | Current and total merchant pages, e.g. `1/2` |
| `shop.selection_hint` | string | Hint for CTRL multi-selection |
| `shop.quantity_open` | bool | Shows the modal quantity layer |
| `shop.buy_quantity_open` | bool | Shows the additive preset buttons for purchases |
| `shop.quantity_prompt` | string | Buy/sell prompt caption |
| `shop.quantity` | int | Pending amount |

Slot bind: `shop.currency` (Geon item icon with item color-key transparency).
Grid binds: `shop.inventory`, `shop.offers`. Tab bind: `shop.inventory_register`.
Actions: `shop.offers.previous`, `shop.offers.next`, `shop.buy`,
`shop.sell`, `shop.quantity.decrease`, `shop.quantity.increase`,
`shop.quantity.add10`, `shop.quantity.add100`, `shop.quantity.add1000`,
`shop.quantity.max`, `shop.quantity.confirm`, `shop.quantity.cancel`.

Non-stackable items (including `stackable = false` and wearable items) are bought
or sold one at a time without a quantity prompt. Stackable items use the prompt.

Purchase presets replace the untouched initial quantity of one, then add their
amount on each click. Purchases, including typed quantities and Max, are capped
at 10,000 items per transaction; the server enforces the same limit. Selling
retains the selected stack limit.

---

## `storage`

Shared account inventory (`config/ui/storage.json`, `StorageWindow`). The left
pane shows a compact view of the character's unequipped General and Other
consumable stacks, including items with no NPC sell value. The right pane is account-owned storage
with six permanently enabled registers (216 slots). A single stack opens a
quantity prompt for a specific amount or the complete stack. Deposits cost 60
Geon per transfer. CTRL-click toggles multiple stacks on either side, matching
the shop selection model; the matching action button queues every marked stack
as a complete-stack transfer.

| Key | Type | Notes |
|---|---|---|
| `storage.title` | string | Window caption |
| `storage.inventory_label` | string | Character-inventory pane caption |
| `storage.storage_label` | string | Account-storage pane caption |
| `storage.inventory_tab` | string | Selected id of the fixed character-inventory header tab |
| `storage.storage_tab` | string | Selected id of the fixed account-storage header tab |
| `storage.inventory_page` | int | Zero-based character-inventory register |
| `storage.page` | int | Zero-based account-storage register |
| `storage.money` | int64 | Current character money |
| `storage.fee` | string | Deposit-fee hint |
| `storage.count_text` | string | Used and total storage slots, e.g. `12/216` |
| `storage.selected_name` | string | Selected item name |
| `storage.quantity_open` | bool | Whether the modal transfer-quantity layer is visible |
| `storage.quantity_prompt` | string | Direction-specific quantity prompt |
| `storage.quantity` | uint32 | Editable amount selected for the transfer |

Grid binds: `storage.inventory`, `storage.items`. Header tab binds:
`storage.inventory_tab`, `storage.storage_tab`. Register tab binds:
`storage.inventory_page`, `storage.page`. Actions: `storage.deposit`,
`storage.withdraw`, `storage.quantity.decrease`, `storage.quantity.increase`,
`storage.quantity.all`, `storage.quantity.confirm`, `storage.quantity.cancel`.
NPC dialog link type `storage` requests the current account-storage snapshot
from the selected storage keeper.

---

## How to extend

1. Add a row to the matching group (or a new `## \`group\`` section).
2. Prefer `_label` siblings over baking copy into JSON.
3. Prefer `bind` paths for icons over inventing parallel `${item_icon_12}` keys.
4. Name actions `group.verb` (`stats.apply`, `window.close`, `window.noop`).
5. Never reuse `int` as a JSON key; use `intl` for Intelligence.


## `creation`

Character creation (`config/ui/character_create.json`, `BootShell`,
`CharacterCreationDraft`) uses a class picker followed by a single-character
preview with a framed editing panel. All panel text is FreeType.

- `creation.class`: selected class name, used as the panel title.
- `creation.name`: input text with the active caret; input bind of the same name.
  Player names require 2–14 ASCII letters or digits. The client ignores spaces
  and special characters, and the lobby applies the same validation to requests.
- `creation.face`, `creation.hair`: selected natural appearance mesh indices.
  Choices are discovered from the active class's `Model/Clothes` assets.
- `creation.str`, `creation.hth`, `creation.intl`, `creation.wis`, `creation.dex`:
  class bases plus the five allocated starting points.
- `creation.points`: remaining point count with its label.
- `creation.status`: validation or persistence feedback.
- `creation.name_label`, `creation.appearance_label`, `creation.face_label`,
  `creation.hair_label`, `creation.stats_label`, `creation.str_label`,
  `creation.hth_label`, `creation.intl_label`, `creation.wis_label`,
  `creation.dex_label`: field and section captions.
- `creation.previous`, `creation.next`, `creation.minus`, `creation.plus`,
  `creation.back`, `creation.create`: button labels.

Actions: `creation.face_previous`, `creation.face_next`,
`creation.hair_previous`, `creation.hair_next`, `creation.stat_plus_0` through
`creation.stat_plus_4`, `creation.stat_minus_0` through `creation.stat_minus_4`,
`creation.submit`, and `creation.back`. Stat indices use STR, HTH, INT, WIS, DEX
order. Refunds cannot go below the class base. Submit requires a valid name and
all five points spent. Back returns to the class picker; Back there returns to
character selection. Face and hair persist with the character and survive
clothing changes and server saves.

## Minimap

The movable `config/ui/minimap.json` uses native Dragon artwork with a circular
`canvas` (`kind: minimap`, `shape: circle`). Both zoom controls keep the existing
`minimap.zoom_in` / `minimap.zoom_out` actions and 1 / 4 / 8 zoom steps.

| Path | Type | Example | Meaning |
|---|---|---|---|
| `minimap.area_name` | string | `Village of Narootuh` | Resolved area name; settlement labels require the current area tile’s Village flag. Known dungeon map IDs display `D1`–`D6`; fallbacks are `Naraeha` outdoors and `Dungeon` indoors. |
| `minimap.coordinates` | string | `8040, 8085` | Absolute world tile X/Y from client X/Z and the profile origin, using server pixels divided by 32. Height is excluded. |
| `minimap.north` | string | `N` | Fixed north caption; the camera-facing marker rotates independently. |
| `minimap.zoom_in_label` | string | `+` | FreeType zoom-in button caption. |
| `minimap.zoom_out_label` | string | `−` | FreeType zoom-out button caption. |

`MinimapWidgetData::set_area_name` is the integration point for future named
regions. Dungeon texture stems are not treated as player-facing area names.
No region boundaries or hard-coded Narootuh assignment are introduced here.

## Dragon dialogs

`chrome.preset: dragon_dialog` provides the shared frame and opaque interior.
Its `chrome.edges: repeat` keeps rail texture density constant as the frame grows.
Character Info uses `dragon_menu/character.bmp` as its `dialog_icon`; there is no
upper-right close action. A `window.close` button is anchored to the lower-right.

| Key | Type | Example | Used by |
|---|---|---|---|
| `character.attributes` | string | `Attributes` | Character Info section heading |
| `character.combat` | string | `Combat` | Character Info section heading |
| `character.resistances` | string | `Resistances` | Character Info section heading |
| `character.active_effects` | string | `Active Effects` | Character Info section heading |

## `startup` and `selection`

`startup.json` supplies the entrance overlay with `startup.title`, `startup.hint`
and `startup.single_player`, `startup.options`, and `startup.close`. Their matching
actions start Single Player, open the shared Options dialog, and quit. Enter also
starts Single Player. Escape cancels an open options draft or quits the entrance.
Saving options writes the active profile; display settings apply immediately and
world, graphics, and audio settings are used by subsequent game initialization.

`character_select.json` supplies the character overview; `character_navigation.json`
supplies the unframed bottom-center count and previous / enter / next controls:

- `selection.title`, `selection.attributes`: panel and attribute headings.
- `selection.name`, `selection.class`, `selection.level`: selected character identity.
- `selection.count`: one-based selected position and occupied slot count.
- `selection.str`, `selection.hth`, `selection.intl`, `selection.wis`,
  `selection.dex`, with matching `_label` keys: attribute values and captions.
- `selection.previous`, `selection.next`, `selection.enter`: navigation labels.

Actions `selection.previous`, `selection.next`, and `selection.select` navigate
or enter the world. Left/Right arrow presses use the same neighbour order; holding
a key does not repeat. Enter enters the world. The host disables unavailable
neighbours and entry while empty, transitioning, or covered by the deletion modal. Empty attributes show
`-`; the empty selection count is `0 / 0`.

All boot, creation, deletion, game-menu and settings buttons use the shared
Dragon idle, hover, pressed and disabled art. `UiViewHost::set_pointer_hit`
provides default pointer feedback without replacing document textures.

## `mail`

`config/ui/mail.json` uses the shared Dragon frame, close control, standard
buttons, fills, text and an attachment slot. There are no custom canvas widgets.
The editor previews a populated inbox using the same variables. The eight-row
inbox has paging and wheel navigation; the nine-line body wraps and scrolls
independently. Selection survives refresh and sorting by subject, date or status.
A single click selects; Read or double-click displays the message. Refresh is
limited to once every three seconds, matching the compatible client. Receiving an
attachment and deleting mail require confirmation. Messages without attachments can be read and deleted directly. For attached
items, Delete is disabled until the attachment has been received; the server enforces ownership and this restriction.
Received attachments keep their item image and quantity, with the image rendered
in grayscale according to the server's claimed state. Available attachments stay
in color.
Each inbox row shows a bronze paperclip when both its item ID and quantity are
nonzero, including after the attachment has been received. Read state does not
affect the paperclip. Rows without an attachment and unused rows hide it; the
editor preview includes both kinds of message. The subject reserves space for
the indicator at the right edge of its row.
Deletion retains the reward source key so reconnecting cannot recreate rewards.

| Key | Type | Meaning |
|---|---|---|
| `mail.title` | string | Caption or presentation value |
| `mail.inbox_label` | string | Caption or presentation value |
| `mail.details_label` | string | Caption or presentation value |
| `mail.summary` | string | Caption or presentation value |
| `mail.date_label` | string | Caption or presentation value |
| `mail.subject_label` | string | Caption or presentation value |
| `mail.status_label` | string | Caption or presentation value |
| `mail.read_label` | string | Caption or presentation value |
| `mail.refresh_label` | string | Caption or presentation value |
| `mail.claim_label` | string | Caption or presentation value |
| `mail.delete_label` | string | Caption or presentation value |
| `mail.previous_label` | string | Caption or presentation value |
| `mail.next_label` | string | Caption or presentation value |
| `mail.body_up_label` | string | Caption or presentation value |
| `mail.body_down_label` | string | Caption or presentation value |
| `mail.confirm_label` | string | Caption or presentation value |
| `mail.cancel_label` | string | Caption or presentation value |
| `mail.empty_label` | string | Caption or presentation value |
| `mail.empty` | bool | Empty-inbox visibility |
| `mail.viewing` | bool | Detail visibility |
| `mail.not_viewing` | bool | Selection-hint visibility |
| `mail.select_hint` | string | Caption or presentation value |
| `mail.status` | string | Caption or presentation value |
| `mail.page` | string | Caption or presentation value |
| `mail.confirming` | bool | Confirmation visibility |
| `mail.confirmation` | string | Caption or presentation value |
| `mail.subject` | string | Caption or presentation value |
| `mail.sender` | string | Caption or presentation value |
| `mail.date` | string | Caption or presentation value |
| `mail.attachment_name` | string | Caption or presentation value |
| `mail.attachment_count` | string | Caption or presentation value |
| `mail.attachment_status` | string | Caption or presentation value |
| `mail.delete_hint` | string | Caption or presentation value |
| `mail.body_page` | string | Caption or presentation value |
| `mail.row.0` through `mail.row.7` | group | `visible`, `selected`, `has_attachment`, `subject`, `date`, `status`, `color` for each visible row; `has_attachment` is true only for a populated row with a nonzero item ID and quantity |
| `mail.body.0` through `mail.body.8` | string | Wrapped message lines at the current body scroll position |

Slot bind: `mail.attachment`. Actions: `mail.select.0` through `mail.select.7`,
`mail.read`, `mail.refresh`, `mail.sort.subject`, `mail.sort.date`,
`mail.sort.status`, `mail.previous`, `mail.next`, `mail.body_up`,
`mail.body_down`, `mail.claim`, `mail.delete`, `mail.confirm`, `mail.cancel`,
and `window.close`.

The system inbox lists durable server rewards and supports reading, sorting,
claiming attachments and confirmed deletion. It does not expose player letters,
parcels, cash-on-delivery, replies, returns or payment controls. Durable reward
mail does not expire automatically.

Protected equipment instances expose a runtime protection marker to item icons
in Inventory and account storage. The marker adds a silent, looping white-blue
diagonal light sweep every two seconds without a new JSON binding or widget type. Inventory and storage tooltips
show `Enchantment protected: 1 attempt`. See `docs/PolishingStone.md` for the
single-attempt consumption rules.

## `talent`

Fullscreen `config/ui/talent_tree.json` (`TalentTreeWindow`) uses the standard
schema and FreeType text. The document's 1600×900 coordinates scale uniformly to
fit the current viewport; opaque shading fills any remaining margins.

- `talent.title`, `talent.points`: caption and available/spent/pending point totals.
- `talent.physical_label`, `talent.magical_label`, `talent.defensive_label`,
  `talent.qol_label`, `talent.upgrades_label`: path captions.
- `talent.class_name`, `talent.future_label`, `talent.future_hint`: identity and
  the unavailable Class Updates branch.
- `talent.apply`, `talent.exit`, `talent.beacon`: button labels, including
  server-wait and Beacon cooldown/activity feedback.
- `talent.status`: general Apply/connection feedback, independent of node details.
- `talent.overlay_visible`: true only while the pointer is over a node.
- `talent.detail_title`, `talent.detail_cost`, `talent.detail`,
  `talent.detail_requirement`, `talent.detail_extra`: hovered node name, cost and
  remaining budget, individual positive/negative effects, predecessor and current
  unlock state. The host draws these in an opaque overlay beside the node with
  wrapped FreeType text. Bonuses are green and penalties red; the panel switches
  sides and clamps to the viewport at the screen edges. Moving away hides it.
  No node descriptions remain in the footer.
- `talent.node.0` through `talent.node.36`: individual node icon-slot bindings.
  Their `.color`, `.line` and `.cost` siblings provide border color, connector
  color, and ordinal/cost labels. Committed icons render in color; all other
  icons render in grayscale. A gold border indicates pending choices, green
  indicates committed choices, bronze indicates affordable next nodes, and gray
  indicates unavailable nodes.

Icon-slot bindings: `talent.class` (the identity HUD's class emblem),
`talent.future` (grayscale reserved-branch seal), and `talent.node.N`.
Actions: `talent.apply`, `talent.beacon`, `window.close`. Clicking an icon stages
that node; its predecessor and cost are validated before a draft changes.
Exit/Escape discards the draft; Apply waits for authoritative server confirmation.
Shift+K and the Skills dialog's `skill.talent_tree` action open the tree.

The Skills dialog adds `skill.talent_tree_label` and `ui.tab_talents` with actions
`skill.talent_tree` and `skill.tab.talents`. The Talents page uses the existing
active/passive grids for the equippable Beacon self buff and five cumulative
branch passives. Beacon can be activated from its skill-bar binding or through
`talent.beacon`, and appears in the existing buff / Skill Info column while active.
See `docs/TalentTree.md` for progression, timing and persistence semantics.

### Multiplayer startup and account registration

`startup.multiplayer` supplies the primary entrance button label. Its action
`startup.multiplayer` selects the remote startup path; `startup.single_player`
continues to use the local SQLite path.

`play_mode.json` is the return menu after leaving character selection or a lost
online connection. It uses `mode.title`, `mode.single`, `mode.multi`, `mode.quit`
and `mode.status_line0` through `mode.status_line2`. Its actions are `mode.single`,
`mode.multi` and `mode.quit`.

`multiplayer_login.json` connects to the endpoint in `server/client.json` when opened;
server address and port are not editable bindings. It uses standard input/text/button
widgets. The editable
bindings are `online.username`, `online.email`,
`online.password` and `online.confirmation`. Password bindings are masked by
default; `online.toggle_passwords` reveals both transient presentation strings
until hidden again or the form/request ends. The transient form owns the actual
password and clears it after a request or when leaving the form. Each field has a matching
`online.<field>_label` token. Other labels are `online.title`, `online.connect`,
`online.login`, `online.register`, `online.back` and `online.controls`. Status
text is split across `online.status_line0` through `online.status_line2`.
`online.password_feedback` reports the current byte length and server password
policy result; `online.confirmation_feedback` reports whether the passwords match.
Their colors are `online.password_color` and `online.confirmation_color`.
`online.toggle_passwords` supplies the show/hide button label. Each editable field
has an `online.<field>_background` color, highlighting select-all. Ctrl+A selects
the focused value, Ctrl+C copies it, Ctrl+X cuts it and Ctrl+V inserts clipboard
text. Oversized pastes and control characters are rejected without changing the
value. Sign-in requires a valid password; registration also requires confirmation.

Visibility flags are `online.connect_visible`, `online.account_fields`,
`online.registering`, `online.registration_allowed`, `online.login_visible` and
`online.create_visible`. `online.registration_allowed` is true only after the
server advertises registration and while the registration form is closed.
The host disables actions during asynchronous requests. Supported actions are
`online.connect`, `online.login`, `online.register`,
`online.submit_registration`, `online.toggle_passwords` and `online.back`.

The editor supplies synthetic preview values for these tokens. These layouts
introduce no new schema types. Account details, database connection strings and
TLS private keys must never become packaged UI configuration.


## `party`

`party.json` supplies a 150-pixel-wide persistent roster with a thin bronze
22-pixel title region and the same three-pixel bitmap border as `chat.json`.
The host extends its `panel_fill` and `frame` widgets to the actual list height.
`party_member.json` adds one 28-pixel names/HP row per member; its HP bar uses
`dragon_hud/party_hp_back.bmp` and a cropped `party_hp_fill.bmp` gradient.
`party_invitation.json` provides the independent accept/decline prompt with the
shared Dragon dialog frame. `party_actions.json` provides membership actions;
the leader's removable members use repeated `party_action_member.json` rows.
The list's preferred position is saved separately from these layouts as
`characters.<id>.panels.party` in `ui_settings.json`, with logical `x` and `y`
coordinates. It is restored per character even before joining a party.

| Key | Type | Example | Used by |
|---|---|---|---|
| `party.title` | string | `Party` | Bronze list heading |
| `party.invitation_title` | string | `Party invitation` | Dragon prompt title |
| `party.inviter` | string | `Companion` | Inviter name, fitted to the content width |
| `party.invitation` | string | `would like you to join their party.` | Invitation body |
| `party.is_leader` | bool | `true` | Leader-only invitation control |
| `party.role` | string | `Party leader` | Management role caption |
| `party.count` | string | `3 / 12 members` | Current membership count |
| `party.leave` / `party.remove` | string | `Leave party` | Membership action labels |
| `party.invite_label` | string | `Invite: Companion` | Selected player or disabled invite hint |
| `party.accept` / `party.decline` | string | `Accept` | Invitation prompt buttons |
| `party.member.name` | string | `* Companion` | Member row; asterisk marks leader |
| `party.member.hp_ratio` | float | `0.85` | Server-backed gradient HP bar |
| `settings.misc_heading` | string | `Misc` | Options tab and heading |
| `settings.page.misc` | bool | `true` | Misc page visibility |
| `settings.ignore_party_label` | string | `Ignore party invitations` | Misc preference caption |
| `settings.ignore_party` | string | `On` | Saved-profile draft value |

Actions are `party.accept`, `party.decline`, `party.invite`, `party.leave`,
`party.kick`, `window.close` and `settings.misc.ignore_party`. `party.kick` applies
to the specific repeated row, never the leader's own row. Both the UI and server
check leader permissions. Invite is disabled for missing/existing targets and a
full party. Membership snapshots update the rows and permissions immediately.

Outside a party, `hud.menu.party` explicitly invites the selected world player.
During membership it opens the actions dialog instead. Its tooltip reads
`Party actions / invite selected player`. Selecting a player alone has no party
overlay. The roster stays visible throughout membership. Escape closes only the
management dialog, and leaving removes the roster after the server snapshot.
Invitations appear independently of the roster and never add list rows.
`settings.tab` includes `misc`, with the existing Save/Cancel lifecycle.

See [Party behavior](../Party.md) for reward rules and behavioral provenance.

## Guild (`guild.json`)

The window follows the global UI scale and reduces it only as needed to fit
the screen. Its `dialog_icon` reuses `dragon_menu/community.bmp` from the menu
bar and renders above the Dragon frame.
Opening or reopening centers the complete frame on screen. It remains centered
when the screen or UI scale changes until the player drags it; a dragged window
keeps its chosen position for the current opening.

One universal Dragon window uses standard text, input, fill, image, tabs and button
widgets. The host is `GuildWindow`; no schema/widget extension is required.
All new text is FreeType. Requirements, prices, grade and permission state come
from the server's versioned Guild view, never from client gameplay constants.

Bindings:

- `guild.title`, `guild.own`, `guild.summary`, `guild.body`, `guild.status`:
  title, own membership/rank, inspected guild summary, invitation/announcement,
  and operation result.
- `guild.status_more`: second line of the operation result. Both status lines
  are measured with the body font and centered within a reserved footer area;
  overflowing results end with an ellipsis. Other single-line guild labels and
  input previews also fit their available width without changing stored text.
- `guild.requirements`, `guild.fees`, `guild.pages`: effective founding policy,
  application/admission fees and cooldown, and page position.
- `guild.pagination`: shows Previous, the centered page indicator and Next
  outside Announcement. Long announcements scroll within the text area.
- `guild.input`: focused UTF-8 text editor shared by name, search and text edits.
  Search/Create uses a centered name field and centered requirements and fees.
  Guild-name entry accepts at most 14 Unicode scalars without whitespace;
  server policy may impose a smaller alphabet-specific bound. Invitation and
  announcement prose retain spaces and paragraph editing.
  Keyboard capture precedes global shortcuts, including Powerbook and talents;
  character events, spaces and Backspace remain available to the editor.
- `guild.management_input`: shows the left-aligned text editor and rank summary
  on Invitation. Announcement uses the separate multiline area below.
- `guild.browsing`: shows Details on both Ranking and Search/Create pages;
  choosing a guild row enables it.
- `guild.tab.N` and `guild.page.N`: tab labels and page visibility for indices
  0–9 (Ranking, Search/Create, Members, Applications, My requests, Invitation,
  Announcement, Membership, reserved, Details). Details opens from a list row;
  category 8 is no longer offered. Rank administration uses `guild_ranks.json`.
- `guild.tab`: selected navigation category ID, `0`–`8`, bound to the standard
  tabs widget. `guild.tab.N.active` supplies the matching background and gold
  underline visibility. Details retains the originating Ranking or Search/Create
  category; clicking that category returns to its list. Clicking the current
  page's tab preserves the text draft, row selection and pagination.
- `guild.member` / `guild.non_member`: mutually exclusive navigation layouts.
  Members omit Search/Create and spread the remaining seven tabs across the
  strip. `guild.tab.N.member_active` / `.non_member_active` select the matching
  background and underline. Search and founding actions are disabled during
  membership; joining on Search/Create switches to Members and refreshes it.
- `guild.table.visible`, `.members`, `.simple`, `.empty`, `.empty_label`:
  table frame, six-column roster, three-column browser/application view, and
  explicit empty state. `guild.column.N` supplies the current heading text.
  Members and Membership show Name, Rank, Level, Daily points, Status and Last
  seen (UTC). Ranking/Search show Guild, Grade and Points. Applications show
  Applicant, Level and Expires (UTC); My requests show Guild and Expires (UTC).
- `guild.row.N.cell.C`: font-measured cell text, with ellipsis at the column
  boundary. `.table_visible`, `.members_visible`, `.simple_visible` and
  `.selected` control full-row hitboxes, cell groups and the selection fill.
  `.body_visible` displays wrapped Invitation/Details prose instead of table cells.
  `guild.body_visible` reserves the lower body caption for Invitation/Details;
  founding requirements and fees appear only on Search/Create.
- `guild.row.N.visible`, `.text`, `.color`: ten paged row bindings, indices 0–9.
  Member rows show rank/title, name, level, today's points, online state, and
  last connection in UTC. Online is white and offline is gray. Invitation,
  and detail pages reuse these rows for wrapped full text with
  Previous/Next paging.
- `guild.formation`, `guild.has_formation`: founding prompt and visibility.
- `guild.membership`, `.membership_leader`, `.membership_hint`: membership
  controls and explanation on category 7. Daily reward, leadership Transfer and
  Leave guild are shown only here. Daily reward is placed above the separate
  membership change controls; Leave guild shares Refresh's footer baseline.
  `guild.daily` is enabled only when the authenticated server view's
  `own.daily_reward_available` is true and no claim is pending. The server uses
  the same policy check for the view and claim, including the live wallet limit,
  guild grade/penalty, membership duration and the current server reward day.
- `guild.announcement_text`, `.announcement_scroll`, `.announcement_hint`:
  published text or the full editing draft, wrapped-line scroll offset and
  mode guidance. The announcement area uses `input` with `multiline: true`.
  `.announcement_can_edit` shows Edit announcement for officers with the
  announcement permission; `.announcement_editing` shows Save/Cancel while
  drafting. Readers never acquire keyboard focus by clicking the text.
- `guild.<action>_label`: button copy for refresh, search, create, details, apply,
  withdraw, accept, temporary, reject, leave, expel, change_rank, edit_ranks,
  set_invitation, set_announcement, edit_announcement, cancel_announcement, transfer, daily,
  previous, next, formation_accept and formation_decline.

Actions use the same `guild.<action>` names. Selecting a `guild.tab` item changes
the page; the host also accepts the existing `guild.tab.N` actions.
`guild.select.N` selects a visible row. `window.close` releases focus. Management controls consult the
current server rank mask. Expel, leave and transfer require a second click.
The window joins the existing Escape close order. A founding request opens it
for a participant who still needs to consent. The menu retains eleven buttons.

`guild.edit_announcement` begins an explicit draft; Enter inserts an LF, arrows
and pointer clicks position the UTF-8 cursor, and Backspace/Delete edit in place.
Paging and wheel scrolling retain paragraphs and blank lines. Background
refreshes preserve the draft. `guild.set_announcement` publishes only after a
matching successful server reply; send/server failures keep the draft available.
`guild.cancel_announcement` discards it. Permission or membership loss ends
editing. The protocol and server allow LF only in Announcement text; names,
rank titles and other commands retain their control-character validation.

### Shared rank dialog (`guild_ranks.json`)

Members offers `guild.change_rank` for an eligible selected member and
`guild.edit_ranks` for officers allowed to rename ranks or edit permissions.
Both open the same modal Dragon layout. The main window remains visible and
blocked, without a dark rectangular overlay; Escape/Close dismisses the child
first. Text uses FreeType. Both modes share the same graphical permission
indicators; only Edit mode offers clickable checkbox hitboxes.

- `guild_ranks.open`, `.assign`, `.edit`: modal state and the two modes.
- `.title`, `.context`, `.hint`, `.selected`, `.availability`: mode caption,
  fixed target member or administration context, guidance, selected title and
  the full reason a rank cannot be assigned.
- `.row.R.title`, `.selected`, `.color`, `.availability`, `.permission.B`:
  five rank rows (`R` 1–5), selection fill, availability, and Yes/empty permission
  cells (`B` 0–4). `.rank_heading`, `.availability_heading`, `.heading.B` label
  the rank/permission matrix. Names and masks come from the guild view. The
  server's `guild.rank_options` reports full-guild rank occupancy, limits and
  unlimited membership independently of roster pagination.
- `.input`, `.name_label`, `.permissions_label`, `.option.B`: draft title,
  section captions and checked permission labels. `.option.B.label`, `.checked`
  and `.color` supply the editor's checkbox caption, overlay and enabled tint.
  `.save_title`,
  `.save_permissions`, `.confirm`, `.cancel`: action captions.
- `.status`, `.status_more`: font-measured request feedback and pending state.

`guild_ranks.select.R` selects a rank without sending a mutation.
`guild_ranks.confirm` submits Rank for the member ID captured when opening the
dialog. Current, protected and full ranks cannot be confirmed. Leadership
continues to use Transfer. `guild_ranks.option.B` toggles a permission draft;
`guild_ranks.save_title` and `.save_permissions` submit the selected rank's
title or mask separately. Only the leader may edit permissions for ranks 2–5;
rename rights control title editing. The server validates all changes.
Rank-title entry and saving require 1–14 Unicode scalars without whitespace.
The default `OfficialMember` title follows this rule. Persisted names and titles
remain readable until explicitly edited.

`guild_ranks.cancel` closes without submitting a draft. Requests already sent
remain server operations. Controls disable during a pending mutation, re-enable
on failure, and assignment closes only on the matching successful response.
Stable member IDs and response request IDs prevent reordered rows or unrelated
replies from changing the selected operation. Send failures restore controls;
membership loss dismisses the dialog. Background refreshes preserve drafts.

## Player options (`player_options.json`, `player_option_row.json`)

O opens a modal context for the selected online player. Available party/guild
action rows come from the server and refresh while open; unavailable actions
have no row. A separate Send PM button becomes available when the authenticated
context supplies the player's name, including when no group actions are offered.

- `player_options.title`, `.target`: caption and selected player's name.
- `player_options.empty`, `.empty_label`: empty-action state and explanation.
- `player_options.status`, `.status_more`: two font-measured feedback lines.
- `player_options.action_label`: caption for the current repeated action row.
- `player_options.pm_available`, `.pm_label`: local Send PM availability and caption.

`player_options.send_pm` closes the context and opens a focused `@Partner `
chat draft using the authenticated target name. This is a local compose action;
only submitting the chat sends a message. It does not add a protocol action.
`player_options.choose` submits the group action represented by that row.
`window.close`, Escape, O, or clicking outside closes the dialog. The current
rows are Invite to party and Request to join the selected member's guild. The
latter carries the displayed application price and creates a normal persistent
application. Selection and request IDs prevent late replies from changing a
newly opened context. Future action kinds reuse the same row layout and host.


## `crafting`

`consumable_crafting.json` is one shared Dragon recipe window hosted by
`ConsumableCraftingWindow`. NPC dialogue links `craft_medicine`,
`craft_magic_potion` and `craft_auto_medicine` select the family. Each family
has up to four independent recipe rows; unconfigured families show an empty
state and no craft controls. All labels use FreeType.

- `crafting.title`: selected family and window caption.
- `crafting.make_label`, `crafting.close_label`: action labels.
- `crafting.hint`: crafting and ingredient guidance.
- `crafting.empty`, `crafting.empty_label`: future-family empty state.
- `crafting.row.N.visible`: recipe-row visibility, with N from 0 through 3.
- `crafting.row.N.title`: output quantity and name.
- `crafting.row.N.materials`: ingredient quantities and names.
- `crafting.row.N.owned`: current inventory counts for those ingredients.
- `crafting.row.N.color`: green when ingredients suffice, muted otherwise.

Actions `crafting.make.0`, `.1`, `.2` and `.3` request one craft of their displayed
recipe; `window.close` and Escape dismiss the window. Controls consult the
current bag on both draw and click. The window stays open after crafting so
server inventory updates refresh availability. The server independently checks
the active alchemist, distance, map, player state and ingredients. Ingredients
and outputs persist in one transaction; a full bag or failed save consumes nothing.
The UI editor uses the same standard widgets and synthetic preview bindings.

## `skin_shop`

`config/ui/skin_shop.json` is the Skin Artisan ticket exchange. All labels use
FreeType. Values below are strings supplied by `SkinShopWindow`; the editor gets
synthetic equivalents from `ui_fill_dummy_preview`.

| Key | Purpose |
|---|---|
| `skin_shop.title` | Window caption |
| `skin_shop.weapons`, `skin_shop.full`, `skin_shop.parts` | Category captions |
| `skin_shop.category` | Current category name |
| `skin_shop.selected` | Selected skin name or selection hint |
| `skin_shop.tickets`, `skin_shop.cost` | Current bag ticket total and selected server price |
| `skin_shop.rotate_left`, `skin_shop.rotate_right`, `skin_shop.rotate_hint` | Mirror rotation controls |
| `skin_shop.redeem`, `skin_shop.close`, `skin_shop.hint` | Footer actions and free-preview hint |

Canvas bindings: `skin_shop.mirror` displays the detached character outfit;
`skin_shop.catalog` displays locally generated thumbnails, grouped cards and the
scrollbar. Their document rectangles determine rendering and hit testing. No new
widget or canvas kind is required. The editor can position and resize both
existing canvas widgets without loading optional game models.

Actions: `skin_shop.weapons`, `skin_shop.full`, `skin_shop.parts` change category;
`skin_shop.left` and `skin_shop.right` rotate the mirror; `skin_shop.redeem` submits
one selected skin to the server and is disabled without enough tickets;
`skin_shop.close` closes the window. Selection alone never spends tickets.
NPC dialogue link types `skin_weapons`, `skin_full`, and `skin_parts` request the
same authoritative shop catalog and select the initial family. These links do
not depend on their displayed text. The conversation is OpenKal content.


## `trade`, `trade_invitation` and `trade_quantity`

These modal universal layouts use standard text, button, fill and icon-grid
widgets, `dragon_dialog` chrome and FreeType. Inventory scrolling uses the
existing `viewport` / `scroll_bind` feature. The editor renders the same grid
frames, clips complete rows and supplies synthetic token values.

- `trade.title`, `trade.status`, `trade.invitation`: partner caption, server status
  and invitation prompt.
- `trade.inventory_label`, `trade.other_label`, `trade.own_label`: panel headings.
- `trade.money`, `trade.partner_money`: editable own Geons and read-only partner Geons.
- `trade.own_status`, `trade.other_status`: Accepted / Not accepted indicators.
- `trade.inventory_scroll`: local inventory offset in native pixels.
- `trade.scroll`: visible inventory row position.
- `trade.quantity_title`, `trade.quantity_prompt`, `trade.quantity`: quantity dialog
  caption, valid range and editable amount.

Grid bindings: `trade.inventory` retains actual inventory cell indices;
`trade.other` and `trade.own` each contain two rows of six offer slots. The host
resolves item icons, counts, bindings and disabled inventory cells. Hover uses
standard item tooltips. Amounts already offered disappear from the local inventory
view until withdrawn or the trade closes; no inventory rows change until commit.

Actions: `trade.accept` accepts an invitation; `trade.confirm` is the trade
Accept button. `trade.reject` cancels the entire trade. `trade.money` focuses the
Geon input; `trade.read_money` is read-only. `trade.scroll_up` and
`trade.scroll_down` move inventory one row, as does the mouse wheel.
`trade.quantity_accept`, `trade.quantity_cancel` confirm/cancel the quantity
selector; `trade.quantity` is its number field. Dismissing the quantity selector
keeps the trade open. Escape outside that selector rejects the trade.

Double-click an inventory item or drag it into a free own-offer cell to add it.
Stacks open a quantity selector; single items move immediately. Double-click an
own-offer stack to change its total amount (zero withdraws it), right-click to
withdraw it, or drag it back to inventory. Own-offer cells can be rearranged.
Items and Geon edits publish automatically. Accept stays disabled while a command
awaits its own server acknowledgement. A new offer revision invalidates both
acceptances. Reject remains available while waiting; only authoritative close
releases the traffic lock.


## Trading Post

Documents: `trading_post`, `trading_post_quantity`, `trading_post_option`.
The window uses FreeType text and the Dragon dialog chrome. The option document
is a repeated standard button strip; the editor can edit all three documents.

- `hud.menu.trading_post` opens the market; `${hud.menu.trading_post.tooltip}`
  is the menu tooltip. The menu open flag reflects the window's visibility.
- `trading_post.row0` through `trading_post.row5` bind ten cells each to a page
  of offers, eligible local sale items, owned offers or collection receipts.
  Each cell supports the existing count badge and selection rendering.
- `${trading_post.range0}` through `${trading_post.range5}` give the minimum and
  maximum unit price of the corresponding row. Sale inventory has no heading.
- `${trading_post.title}`, `${trading_post.tab_buy}`, `${trading_post.tab_sell}`,
  `${trading_post.tab_own}`, `${trading_post.tab_collection}` label the window
  and tabs; actions `trading_post.tab_buy`, `.tab_sell`, `.tab_own`, and
  `.tab_collection` switch views.
- `${trading_post.class_label}`, `.type_label`, `.prefix_label`, `.grade_label`,
  `.order_label` label filters; `${trading_post.class}`, `.type`, `.prefix`,
  `.grade`, `.order` show selected values. Actions `trading_post.filter_class`,
  `.filter_type`, `.filter_prefix`, `.filter_grade`, `.filter_order` open the
  selector. Prefix is disabled outside equipment categories. Type/class changes
  reset grade, and leaving equipment resets prefix. Grades follow the catalog.
- `${trading_post.option}` labels a repeated dropdown choice; action
  `trading_post.option` identifies its standard button. The host dispatches the
  repeated row and supports wheel scrolling through long option lists.
- `trading_post.sell` controls visibility of the unit-price editor beside the
  selected item. `${trading_post.price_label}` labels it; the standard `input`
  binds to `trading_post.price`. `${trading_post.price}` shows the numeric draft
  and caret while focused, grouped Geons when blurred, or an empty-field prompt.
  `${trading_post.price_color}` supplies the text color. A new sale selection
  clears and focuses the price; listing requires an explicitly entered positive
  price. The older `trading_post.price` button action remains accepted by the host.
  `trading_post.inspect` shows the ordinary selected-stack total outside Sell.
- `${trading_post.selected}` and `${trading_post.details}` describe the selection.
  `${trading_post.selected_color}` and `${trading_post.details_color}` use the
  Geon tier when the label represents proceeds or a stack total; ordinary item
  descriptions retain the regular text color. `${trading_post.price_color}`
  applies the same tiers to the editable unit price, with a muted empty prompt.
  `${trading_post.act}` labels action `trading_post.act`, which buys, lists or
  withdraws depending on the view. In Collection opened by a Trading Post Agent,
  the main action is Collect All, enabled whenever deliveries are available
  without requiring a selected slot. It collects across all pages and refreshes
  page one; receipts blocked by inventory or wallet capacity remain available.
  `${trading_post.collect_selected}` labels the separate Collection-only action
  `trading_post.collect_selected`, which requires a selected receipt.
  Both pickups require NPC context; menu-only Collection directs the player to
  the agent and keeps pickup disabled. Legacy action `trading_post.collect_all`
  and its `${trading_post.collect_all}` label remain available to custom layouts.
- Actions `trading_post.refresh`, `.prev`, `.next` refresh or change page.
  `${trading_post.page}` displays the one-based current/total page count (for
  example `1/2`), using the matching server result count or eligible sale inventory.
  Empty results display `1/1`; missing totals (legacy servers or refresh errors)
  display the current page followed by `/?`.
  Arrow eligibility comes
  from the authoritative `more` flag or eligible sale-inventory length.
- `${trading_post.status1}` and `${trading_post.status2}` show results and pending
  state. Mutations and view/filter changes are disabled during a pending command.
- `${trading_post.quantity_title}`, `.quantity_prompt`, `.quantity_unit_price`,
  `.quantity_total` label the quantity modal, including the unit price and total
  for the requested quantity; `${trading_post.quantity}` and bind
  `trading_post.quantity` supply its numeric draft and caret. Actions
  `trading_post.quantity.decrease`, `.increase`, `.add10`, `.add100`, `.add1000`,
  `.max`, `.cancel`, `.confirm` reuse the shop's quantity-control behavior.
- Action `window.close` closes the market. Escape closes a dropdown or quantity
  modal first, then the market. The open market captures input and fits to the
  viewport. Dialog link type `trading_post_collection` opens Collection with the
  runtime NPC ID. Pickup commands revalidate the collector, map and proximity on
  the server; closing the window clears the NPC context.

`trading_post.buy`, `.sell`, `.own` and `.collection` also drive the active tab
underline. `trading_post.filters` shows the filters on Buy/Sell. `trading_post.own` and
`trading_post.collection` show the view-specific help text supplied by
`${trading_post.own_help}`, `${trading_post.collection_help}` and
`${trading_post.collection_help2}`. `${trading_post.buy_color}`, `.sell_color`,
`.own_color` and `.collection_color` mark the active tab. Long dropdowns scroll
with the mouse wheel; their host keeps the list within the viewport.

Currency fields use `:geons` in the inventory, NPC shop, storage, player trade,
blacksmith, Speed Medicine service and trading post. Each amount is white below
100,000, green below 1,000,000, yellow below 10,000,000, orange below 100,000,000,
blue below 1,000,000,000 and purple from 1,000,000,000 onward. Price ranges,
unit prices and stack totals receive separate colors for their own amounts.

## `clock`

`clock.json` is the compact draggable server/session clock. It uses the existing
chat border textures at the same three-pixel thickness and FreeType text. `clock_event_options.json` is its
upward-opening, viewport-clamped event selector, with six choices per page.
Small catalogs shrink the selector to their actual row count and hide paging.
The default position is the lower right, above the bottom HUD. The integrated
time header moves the panel; position uses the existing per-character HUD settings.
The full widget is 198 x 118 native pixels, with a decorative clock emblem,
prominent server time and no separate title strip.

| Key | Meaning |
|---|---|
| `clock.title` | Optional caption for custom layouts; omitted by the compact default. |
| `clock.server_label` / `clock.online_label` | Server-time and account-session labels. |
| `clock.server_time` | Server-local `HH:MM:SS`; `--:--:--` before synchronization. |
| `clock.online_time` | Current authenticated session `HH:MM:SS`, with hours above 23. |
| `clock.selected_event` | Selected event name, or No event, with dropdown indicator. |
| `clock.countdown` | Selected event countdown `DD:HH:MM:SS`; `00:00:00:00` without a selection. |
| `clock.option.0` through `clock.option.5` | Current page's choice captions. |
| `clock.option.0.visible` through `clock.option.5.visible` | Whether each choice exists. |
| `clock.paging` | True when more than one choice page exists. |
| `clock.previous` / `clock.next` / `clock.page` | Selector paging captions and current/total page. |
| `settings.clock_label` | Misc page checkbox label. |
| `settings.clock_checked` | X for the enabled clock checkbox, empty otherwise. |

Actions: `clock.events` opens the selector; `clock.choose.0` through
`clock.choose.5` select a row (including No event); `clock.previous` and
`clock.next` change its page. `window.close` disables the widget and immediately
saves `client.misc.clock_enabled=0` in the active profile. The Misc checkbox
action `settings.misc.clock` edits that same preference in the settings draft;
Save applies it and Cancel discards it. Quality presets preserve this preference.
The default is enabled. Disabling the widget leaves synchronization running.

The optional server-owned `clock_events` list in `config/server_events.json` is
empty initially. Each future notice has a unique `id` (up to 32 bytes), `name`
(up to 48 bytes), `weekdays` (unique numbers 0=Sunday through 6=Saturday),
`hour` (0–23) and `minute` (0–59). Up to sixteen notices are supported. Example:

```json
{"id": "castle_war", "name": "Castle War", "weekdays": [6], "hour": 20, "minute": 0}
```

These are countdown notices; adding one does not activate gameplay. Restart the
server after changing the list. Times use the server process's local timezone
and calendar, including DST; a time inside a spring-forward gap is normalized
by the platform calendar. The server sends the next two actual occurrences, so
the client can advance to the following occurrence between hourly snapshots.
Selection is local to the current game session and survives ordinary snapshots
by stable event ID; an absent event shows No event and zero countdown.

`S2C::ClockSync` is an appended OpenKal extension. Its little-endian payload is
Unix seconds (`u64`), UTC offset seconds (`i32`), current account online seconds
(`u64`), event count (`u8`), then each event's NUL-terminated ID/name and next two
Unix timestamps (`u64`, `u64`). It is sent after entering the world and every
3,600,000 monotonic milliseconds per connection. Online duration starts when
account authentication completes (world entry for embedded single player),
including time spent selecting a character; it is not lifetime playtime.
The client advances all values with `steady_clock`, independent of PC wall-time
changes. Hourly snapshots correct drift and the server UTC offset. Disconnect
clears the snapshot and displays placeholders until the new connection syncs.

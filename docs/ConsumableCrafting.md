# Consumable crafting

The Medicine Alchemist offers three family entries: Medicine, Magic Potions and
Auto Medicine. All three families use the same recipe window, with three rows
for Medicine and Magic Potions and four rows for Auto Medicine. Recipe panels
start below the upper-left frame ornament so their backgrounds remain rectangular.

Current recipes, each producing exactly one item without a Geon fee or delay:

- Two Small Medicine (23000) plus one Aloe Vera (23027) produce Medium Medicine (23001).
- Two Medium Medicine plus one Aloe Vera produce Large Medicine (23002).
- Two Large Medicine plus one Aloe Vera produce Huge Medicine (23003).

- Two Small Magic Potions (23004) plus one Blue Lotus Leaf (23028) produce Medium Magic Potion (23005).
- Two Medium Magic Potions plus one Blue Lotus Leaf produce Large Magic Potion (23006).
- Two Large Magic Potions plus one Blue Lotus Leaf produce Huge Magic Potion (23007).
- One normal Medicine plus one Ginseng Root (23029) produces one Auto-HP Medicine
  of the same size: Small 23000 → 23008, Medium 23001 → 23009,
  Large 23002 → 23010 and Huge 23003 → 23011.

`ConsumableCrafting` contains the shared family and recipe descriptors used by
both the server and client. To add a future recipe, give it a unique flag under
OpenKal quest index 9600 and add its ingredients/output to the selected family.
Flags 1–3 identify Medicine upgrades, 4–6 identify Magic Potion upgrades,
and 7–10 identify the four Auto-HP conversions. The same UI and server exchange
path handle every family.

NPC definition 60001 uses dialog 990300. Its `placement.beside_dialog: 192` finds
the lowest-index absolute Speed-up Medicine chemist at runtime. The alchemist
stands at the explicitly configured server coordinates
`[257454, 258802, 16080]`, inheriting only the anchor's map, facing and appearance.
An explicit `xy` triple takes precedence over the relative placement offset.
Both NPC loaders use `load_npc_definitions`, so local
and network spawns agree. Missing optional anchors omit the companion instead
of spawning at the origin. Relative placement never rewrites source catalogs.

Crafting requires a successful conversation with this alchemist and validates
range/map, a living player and no teleport, fishing or cooking in progress.
All ingredients must exist before output insertion. Split input stacks and
existing output stacks are supported. Account/character binding on consumed
materials carries to the output. A full inventory is evaluated after consuming
inputs, allowing a freed input slot to hold the result. Failed insertion restores
the in-memory bag; failed persistence rolls back both ingredients and output.
SQLite and PostgreSQL use one optimistic transaction over the affected rows.

The recipes, dialogue, relative-placement fields, layout, implementation and synthetic
tests were created for OpenKal under its GPL-3.0-or-later license. No external
NPC configuration, dialogue or asset data is redistributed by this feature.

Validation: `consumable_crafting_server` exercises actual server requests,
all ten recipes, split stacks, binding, invalid requests, full inventory and
injected persistence failure. `consumable_crafting_ui` checks family dispatch,
stock availability, all four Auto Medicine rows, relative placement and scaling. Its optional first argument
is a capture filename prefix for rendered previews.

# Gathering plants

Aloe Vera, Blue Lotus and Ginseng are server-authoritative gathering nodes. Click a plant to approach it
and gather it, using the same targeting and range checks as a Wooden Box. The
player crouches once over two seconds and returns to standing. Four ingredients are
inserted directly into General inventory with the shipped settings. The plant disappears for every player;
concurrent requests cannot collect it twice. Moving cancels the action. A full
inventory, failed persistence, death, or loss of range preserves the plant.

Aloe Vera (23027), Blue Lotus Leaf (23028) and Ginseng Root (23029) are stackable
crafting ingredients. At the Medicine Alchemist, Aloe Vera upgrades Medicine,
lotus leaves upgrade Magic Potions, and ginseng roots convert normal Medicine
into Auto-HP Medicine of the same size. See [Consumable crafting](ConsumableCrafting.md).

## Server settings

Embedded servers and standalone Unix servers load `config/server/plants.json`.
A dedicated multiplayer server can override these defaults in its private
`server.json` (see `server.example.json`):

```json
"plants": {
  "types": {
    "aloe_vera": {
      "entity_index": 65000,
      "item_index": 23027,
      "items_per_pick": 4,
      "enabled": true,
      "max_plants": 256,
      "plants_per_area": 8,
      "respawn_seconds": 600
    }
  }
}
```

The shipped `blue_lotus` (entity 65001, item 23028) and `ginseng` (entity 65002,
item 23029) entries use the same values as `aloe_vera`: four ingredients per
pick, 256 plants per type, eight per area and 600 seconds of regrowth.

Restart the server after changing these settings. With the default packaged
configuration, rebuild `openkal_config_archive` after editing the source JSON
and restart the game in single-player; editing the source alone does not update
the running `config.pak`. A profile with an explicit loose `configuration_root`
reads that directory instead.

All settings belong to a named entry under `types`. Add further plant types next
to `aloe_vera`; each needs a unique `entity_index`, its own Item-race entity/model
definition, and a stackable ingredient `item_index`. The same type identities and
entity resources must be shipped to the client. Dedicated server overrides
replace the complete type list. An empty `types` object disables all populations.
The previous flat configuration must be moved into a type entry.

For each type, `items_per_pick` is the fixed number of ingredients awarded by a
successful harvest (1–1000, default 1). The complete stack must fit; failed
inventory insertion preserves the plant and awards nothing. `enabled: false`,
`max_plants: 0`, or `plants_per_area: 0` disables random spawning of that type. `max_plants` caps
its maintained population across the world, including slots waiting to regrow;
`plants_per_area` limits that type in each 8192 by 8192 server-pixel area. These limits are
independent of the ordinary monster population cap. Allowed ranges are 0–10000
for each type's world cap, 0–128 per area, and 1–86400 seconds for regrowth.

Areas are populated lazily as their authoritative area maps load around players.
Initial placement and regrowth share a budget of eight placement jobs per server
tick, each with at most 128 candidate positions. Large configured populations
therefore fill gradually while the server continues accepting clients and
processing packets.
Their catalog entries must explicitly identify them as outdoor maps. Missing
maps and dungeon geometry are never guessed. Random positions require a walkable
footprint outside Village, Portal, and Object tiles; Blocked and
NotMoveable subcell masks are both checked. Plants are separated by at least 256
server pixels. Areas with too little eligible ground may contain fewer plants.
Safety alone does not exclude outdoor hunting ground: the surrounding wilderness
can carry this flag as well as a city. Village is the city exclusion.
After harvesting, a slot regrows at a new random eligible point in its initial
area after ten minutes by default. Populations and timers restart with the server.
`respawn_seconds` controls regrowth after harvesting; the initial population is
placed when an eligible area first loads.

## Owned resources

Entity definition 65000 is reserved for OpenKal's Aloe Vera node. Its appearance
uses the owned model at `RuntimeData/models/plants/aloe.gb` with scale 0.185,
roughly 40% of ordinary player height. No legacy treasure-box timings are changed.
The inventory icon depicts a single detached leaf; provenance is recorded in
`RuntimeData/models/plants/README.md`. `tools/generate_aloe.py` regenerates the
world model and texture while preserving the separate icon. All assets are included in `config.pak`;
no proprietary plant assets are needed.

Blue Lotus (65001) uses `blue_lotus.gb` and Ginseng (65002) uses `ginseng.gb`,
both at the same 0.185 scale. The lotus has broad leaves and blue petals;
ginseng has a root crown and five-leaf clusters. The inventory icons depict
one detached lotus leaf and one ginseng root. Regenerate the world meshes and
textures with `python3 tools/generate_crafting_plants.py`; icons remain separate.

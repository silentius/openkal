# Polishing Stone

Definition **23017** is a stackable Polishing Stone. Its image key `shop015`
and description match legacy item **481**.
Drag one stone onto a single unequipped weapon, armor piece or shield. The server consumes one stone
and sets one protection charge on that item instance. Existing protection
cannot be stacked or overwritten, and rejected applications consume nothing.

The next accepted enchantment consumes the charge on either success or failure.
A protected failure preserves endurance, maximum endurance and enchantment
values, and cannot destroy the item or award destruction compensation. Failed
attempts still consume their normal talisman or gold cost. Invalid targets,
capped enchantments and insufficient funds retain protection. Repairing an item
or adding Stone of Revision success-rate points does not use the charge.

Protection covers prefix talismans, charming talismans, Bead of Fire, Qigong
perforation and Explosive Blow. Successful Qigong ability and Dragon Fusing
applications also consume the next-attempt charge. Collection upgrades and
Mix Master recipes are separate operations.

The marker persists in SQLite schema migration 25 and travels in INSERTITEM
info bit 4. Existing items default to unprotected; packet size is unchanged.
Inventory, worn-item icons, drag ghosts and account storage display the marker;
item tooltips show `Enchantment protected: 1 attempt` through TrueType text.

## Protection effect

The effect is `UiView::draw_enchant_protection`, a project-owned procedural UI
light band. It has no script, particle stages, texture or sound dependency.
A full white-blue stripe crosses the icon interior from top left to bottom
right, repeating every two seconds, driven by monotonic time. The stripe runs
perpendicular to its travel direction and spans the full clipped diagonal at
mid-sweep. A long blue gradient trails behind the bright leading line toward
the upper left, fading quadratically to full transparency over 0.55 normalized
icon widths. Sixteen non-overlapping alpha steps form the tail; three additional
alpha-blended layers provide the local halo and bright core.
The band enters and leaves beyond the corners, avoiding a visible reset flash.
The effect scales with the item icon and stops as soon as the marker clears.
It does not modify world particles, player meshes, map rendering or audio.

`PolishingStoneTest` covers deterministic success, damaged failure, destruction,
second attempts and invalid attempts. `PolishingStoneServerTest` uses independently
written synthetic item definitions, an isolated SQLite database and a Unix
socket to check consumption, rejection, migration, relogin and destruction after
protection is spent. The inventory HUD test checks drag requests for weapon and
armor instance IDs. Its `--capture-polishing <output.ppm>` mode verifies actual
GL motion direction, full diagonal coverage, the fading rear gradient, clipping,
an exact two-second loop
and a static rendered preview at a fixed phase.

## Provenance

Behavior and test fixtures are OpenKal work under GPL-3.0-or-later,
integrated with the existing enchantment rules. The OpenKal icon is documented in
`RuntimeData/templates/PolishingStone.md` but is no longer selected by this item.

The catalog fields come from the optional `openkal_data` catalog,
`config/items/general/charm/0481_polishing_stone.json` (2026-10-22). The legacy
image remains a reference to user-supplied runtime assets; no image is copied
into OpenKal and no redistribution license for that external image is asserted.

## Related enchantment items

The current catalog assigns **23018** to
Perforation Shot (formerly **3057**, image `shop143`), **23019** to Dragon
Spirit Scroll (formerly **2102**, image `shop134`) and **23020** to Qigong
Talisman (formerly **3056**, image `shop142`). Their selected definitions come
from the corresponding `general/gslot`, `general/etc` and `general/gigong`
entries in the optional `openkal_data` catalog. The image keys and
item properties are retained. External artwork remains optional user-supplied
runtime data. Client and server use the shared `Qigong` and `DragonFusing`
index functions for the new IDs; legacy IDs no longer activate those operations.
Drop-group item references in the optional catalog use the new IDs, including
**481** to **23017** for Polishing Stone. The five affected quest rewards use
the same mapping.

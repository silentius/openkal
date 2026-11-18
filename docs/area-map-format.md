# OpenKal area map format

OpenKal area maps use the `.akarea` extension. A cell file is named
`n_XXX_YYY.akarea` and covers the same 256 by 256 world-tile extent as the
terrain patch at those coordinates.

Versions 2 and 3 have this little-endian layout:

1. `AKAR` magic, `uint16` format version, `uint16` header size.
2. `int32` cell X and Y.
3. `uint16` tiles per axis and Blocked subcells per tile axis.
4. `uint32` tile-attribute count, blocked-mask count, attribute payload bytes,
   blocked payload bytes, and FNV-1a payload checksum.
5. RLE tile attributes, followed by RLE collision-layer masks.

Each RLE attribute run is `uint32 count, uint32 value`. A version 2 collision
run is `uint32 count, uint16 blocked, uint16 not_moveable`. Version 3 uses the
same fields with a `uint64` NotMoveable mask. Blocked uses four by four bits
per tile (eight world pixels per side). Version 3 NotMoveable uses eight by
eight bits per tile (four world pixels per side). Both use row-major bit order.

OpenKal can edit version 2 movement masks at four-pixel precision. Each existing
eight-pixel subcell covers four movement cells with the same initial value.
Saving uses version 3 only when the finer cells differ within an eight-pixel
subcell. The client, server, and editor must support version 3 to load these files.

`area_import` accepts a legacy KSM file and writes an `.akarea` cell. The
legacy four by four blocked subcells are copied without resampling. All 16 low
bits are collision subcells, including bit 15 at (3, 3); none is a whole-tile
NotMoveable flag. For decoded legacy maps, `--player-collision` copies the same
subcells to the OpenKal NotMoveable layer so the mask also restricts player
movement. Otherwise the decoded NotMoveable layer starts empty. Zone flags
start at bit 16 (Portal). Existing `.akarea`
masks remain authoritative and are not rewritten on load, preserving editor
changes. Version 1 `.akarea` files are intentionally unsupported and must
be regenerated from their KSM sources.

Area masks constrain server navigation and spawning. Local player movement
uses terrain height and CLS collision, as in the compatible client. Navigation
masks must not veto entry to portals, bridge decks or downhill ledges; those
places can intentionally overlap blocked cells. Geometric walls, unreachable
upward steps and empty indoor space still block players.

The independent NotMoveable layer blocks player movement on terrain and indoor
floors. Because this mask has no height, a reachable outdoor deck above the
terrain overrides it: bridges remain traversable even when the riverbed or
gorge below them is marked NotMoveable. Terrain beside a bridge stays blocked,
and an unreachable roof does not override the mask. Collision walls still stop
movement on a bridge.

`area_import --smooth` performs one conservative contour pass after import. It
only removes supported protrusions and convex corner cells from components of
at least nine subcells. It never adds blocked space, preserves thin lines and
small components, and leaves the outer cell border untouched so independently
imported neighboring maps cannot develop seams.

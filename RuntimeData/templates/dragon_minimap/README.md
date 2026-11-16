# Dragon minimap assets

The live assembly is `config/ui/minimap.json` (`openkal.ui/v1`). It uses a
260-by-272 logical-pixel panel with a circular map aperture. The canvas is
188 pixels across, slightly overlapping the bronze inner rim to avoid gaps.
The visible map opening is approximately 184 pixels across.

`bmp/` contains five native 32-bit BI_RGB textures with straight alpha:

- `frame.bmp`: 241 by 258, transparent exterior and map aperture.
- `area_support.bmp`: 79 by 27, a solid bronze relief filling the gap below the nameplate.
- `area_plate.bmp`: 146 by 30, blank dark nameplate.
- `coordinates_plate.bmp`: 110 by 28, blank coordinate plaque.
- `zoom_button.bmp`: 26 by 26, blank round button reused for both zoom actions.

Keep all BGRA bytes when exporting. The build copies these textures unchanged
to `RuntimeData/UI/dragon_minimap`; `config.pak` and installed loose assets include
the same bytes. Labels, north, coordinates and zoom symbols are FreeType text,
not part of the bitmaps. No map terrain is baked into the frame.

Artwork was generated with the built-in image generation tool from the approved
Dragon minimap concept. Prompt: create a transparent sprite sheet matching the
aged bronze eastern dragon, small jade ornament and dark stone of the approved
concept; isolate a truly circular empty ring, blank nameplate, blank coordinate
plaque and blank round button; omit terrain, text, symbols and external shadows.
The sprites were mechanically extracted and resized for runtime export.

The separate nameplate infill was generated with the built-in image tool.
Prompt: a broad, solid, shallow bronze wedge with a straight upper edge and a
swept lower edge, filled with carved dragon-scale and flame relief; match the
existing frame, transparent background, no holes, text, gems, glow or shadow.
It renders behind the nameplate and ring, filling their intervening area and
ending before the nameplate's raised right corner. Zoom buttons overlap the
lower-right ornament arc and align with its upper and lower ends.

The widget uses the existing per-character HUD position save/load flow. Its map,
rim and plaques can be dragged; zoom controls consume clicks without dragging.
Empty frame corners do not block the world. After settings restore, the HUD is
clamped to the current framebuffer, including the new larger minimap dimensions.

Area names are resolved from the player's world-tile position before the minimap is
drawn. Settlement names require the `Village` flag on the current area tile;
nearby safety zones and missing area data do not count as being in town.
Dungeon labels use the current server map ID (`D1` through `D6`, including alternate
floor IDs), independently of reused minimap stems. Unknown indoor maps use
`Dungeon` and cannot inherit an outdoor label. Outdoor names follow the
[official area map](https://gameagit.com/kalonline/Map); unnamed terrain uses `Naraeha`.
Coordinates are absolute world tiles, converted from client X/Z with the active
profile origin. They can therefore be shared independently of the camera or
indoor minimap texture offsets.

Validation: `minimap`, `minimap_hud`, `ui_document`, and `runtime_icons` tests.
The optional GL check also captures native/scaled outdoor and dungeon layouts:

```sh
build-ninja/tests/openkal_test_minimap_hud --capture /tmp/minimap.ppm /path/to/client/data
```

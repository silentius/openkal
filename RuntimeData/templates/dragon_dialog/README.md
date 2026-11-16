# Shared Dragon dialog templates

Character Info uses these templates through the shared `dragon_dialog` UI preset.
Other dialogs can adopt the same preset, menu emblem, and button artwork.

## Design contract

- Opaque charcoal stone interior and title band, plain bronze rails, small jade insets.
- Straight rails have no glyphs or repeated ornaments. Their bronze profile matches
  the title rails and joins through a compact bronze scroll at the upper-right corner.
  This corner has its own asymmetric shape and contains no gemstone.
- The upper-left dragon surrounds an empty dialog medallion. Overlay the existing
  menu icon (`dragon_menu/character.bmp` for Character Info); do not bake one
  dialog's identity into the common frame.
- The title texture continues behind the dragon's open mouth. Never cut this
  region out when assembling the frame.
- No upper-right close control. Overlay the separate `close_*` medallion at the
  bottom-right of every migrated dialog and bind it to `window.close`.
- All titles, labels and plus/minus glyphs remain FreeType text.
- The shared footer draws a right-aligned `OpenKal` inscription, inset 30 pixels
  from the client right edge and 22 pixels below its bottom. Like the HUD brand,
  it uses bold 12-pixel FreeType text in `#211C13` over a `#C0A064` highlight
  offset by one pixel. The renderer and UI editor add it automatically.
- The footer inlay reuses the textured bronze gradient from the HUD inscription
  panel. Its unlettered center (14, 22, 18, 76) is transposed and repeated with
  alternating mirror direction at native horizontal density inside the existing rails; no duplicate bitmap is stored.
- Text buttons and square icon buttons share the same bronze/stone family:
  normal, brighter hover, recessed pressed with jade inset, and muted disabled.

## Files

`source/frame-quiet.png` and `source/buttons.png` are the active generated RGBA
master sheets. Only the upper-right slice comes from
`source/frame-corner-ornament.png`; all other slices remain byte-identical to
the quiet frame revision.
They are retained for deterministic exports and are excluded from runtime packages.
`bmp/` contains 27 native 32-bit, uncompressed BGRA assets:

- Eight frame corners/rails and one opaque interior texture.
- Four text-button backgrounds at 144 x 48 native pixels.
- Four square icon-button backgrounds at 28 x 28 pixels.
- Four round close buttons at 36 x 36 pixels.
- Four long button backgrounds at 276 x 30 pixels, scaled uniformly from
  `source/long_buttons.png` to preserve their bronze border proportions.
- Two round 20 x 20 indicators: an emerald for active and an empty socket for inactive.

`slices.json` records crop rectangles and native sizes. Corners are deliberately
asymmetric: keep the dragon at native size and stretch/tile the straight rails.
These are individual assets, not a flattened screenshot of a dialog.

Regenerate the BMPs with:

```sh
python3 tools/export_dragon_dialog_templates.py
python3 tools/export_collection_handle.py
cmake --build build-ninja --target openkal_runtime_icons
```

The exporter only crops, normalizes sprite dimensions, and writes straight BGRA.
Near-opaque generated surfaces (alpha >= 240) become fully opaque, so the world
cannot faintly show through solid stone; antialiased silhouette edges remain.
No source art is overwritten. The build copies BMPs unchanged to
`RuntimeData/UI/dragon_dialog` and the packager includes that same namespace.
Do not run these sheets through the 32-pixel item-icon converter.

## Runtime integration

`config/ui/character_info.json` is the first migrated layout. The shared renderer
and UI editor use a repeating nine-slice frame with 40/84/40/56 client padding.
`title` and `dialog_icon` are overlays above the chrome. The footer close button
uses `x_from_right: 16`, `y_from_bottom: 0`, and a 56 x 56 rectangle, so it follows
independent width/height edits. Character Info's content requires 420 x 560.
Hover, pressed, and disabled textures are wired through `UiViewHost`, including
per-attribute availability for plus/minus controls.
Stat allocation and buff tooltips remain functional; the inactive numbered
active-effect selectors have been removed. Reusable tab artwork is still a
separate future addition when a dialog needs actual tabbed content.

Use the universal `openkal.ui/v1` schema and its catalog/editor together.
Other dialogs retain their existing presets until individually migrated.

## Artwork provenance

`source/mail_attachment.svg` is OpenKal vector artwork under
GPL-3.0-or-later, drawn independently without external reference assets. Its
bronze paperclip is exported to `bmp/mail_attachment.bmp` at 16 x 16 with
straight BGRA alpha. Regenerate it with
`python3 tools/export_mail_attachment_icon.py` (Pillow and ImageMagick required
for this authoring step). Runtime packaging copies the BMP without conversion.

The inventory drawer uses a dedicated 28 x 112 gold-and-jade clasp, centered
on its left rail. `source/collection_handle.png` contains idle, hover and pressed
states generated with the built-in image tool. The complete prompt is retained
in `source/collection_handle.prompt.txt`; the border-matching color edit is in
`source/collection_handle-color.prompt.txt`. Its separate exporter crops each state
and scales uniformly, preserving transparency and the generated proportions.
The direction chevron is rendered with FreeType at runtime.

Generated with the built-in image-generation tool from the project-created Dragon
Character Info concept, after the mouth backing correction. Generation briefs:

- Frame: the approved frame emptied of all text, internal sections and buttons;
  an empty opaque medallion, opaque stone behind the dragon mouth, no upper-right
  X, and transparent exterior. Thin bronze rails and jade accents retained.
- Quiet frame revision: built-in image editing removed the patterned rails and
  upper-right spiral, matching both sides to the thin title rail. The exact
  generation prompt is retained in `source/frame-quiet.prompt.txt`.
- Upper-right revision: built-in image editing added an independent bronze
  leaf/cloud scroll inspired by the lower-left corner, without a gemstone. Only
  its 64 x 96 corner slice is exported. The full prompt is retained in
  `source/frame-corner-ornament.prompt.txt`.
- Long controls: `source/long_buttons.png` was generated with the built-in image
  tool using the existing buttons and quiet frame as references. It contains four
  elongated blank buttons plus separate emerald and empty-socket indicators.
  The exact prompt is retained in `source/long_buttons.prompt.txt`.
- Controls: a four-column, three-row RGBA sheet. Columns are normal, hover,
  pressed and disabled. Rows are empty text buttons, empty square icon buttons,
  and round exit medallions with the close glyph. No baked text-button labels.

The source sheets remain artwork masters. Live assembly and behavior are covered
by `character_info_hud` and `ui_document`; the former also supports `--capture`.


### Inventory Geon field

`source/inventory_money.svg` is OpenKal vector artwork under
GPL-3.0-or-later, drawn independently without external assets. It contains an
opaque dark field, a fine bronze bevel and three small gold coin stacks. It has
no baked text: `inventory.money_label` and `inventory.money:comma` remain
FreeType labels in `inventory.json`. Export the 340 x 48 straight-BGRA bitmap with
`python3 tools/export_inventory_money.py`; runtime packaging copies
`bmp/inventory_money.bmp` into `UI/dragon_dialog` unchanged.
The same exporter extracts the source vector's coin group into the transparent
32 x 32 `bmp/geons.bmp` currency icon used for trading-post sale proceeds.
Both assets use the same OpenKal artwork and GPL-3.0-or-later license.

### Inventory filter symbols

`source/inventory_filter_*.svg` contains six OpenKal vector glyphs:
a potion bottle, sword, armor, enchantment wand, paw and miscellaneous box.
They were drawn independently for this interface under GPL-3.0-or-later,
without external artwork or font glyphs. Export their 24 x 24 straight-BGRA
bitmaps with `python3 tools/export_inventory_filters.py`. Existing icon-button
frames supply normal, hover and selected states; category labels remain
FreeType hover text in the client.

### Compact HUD chrome

`bmp/compact_*.bmp` are procedural five-pixel bronze bevels for the
`dragon_compact` nine-slice preset. They use no external source artwork.
Regenerate these eight slices with `python3 tools/generate_compact_chrome.py`.
The generator and output are GPL-3.0-or-later. The clock combines them with
the existing owned stone interior and close button. The same generator draws
`bmp/clock_emblem.bmp`, an small bronze clock face with transparent
corners. It is decorative; the authoritative time is displayed as FreeType text.

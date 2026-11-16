# Image provenance

This record identifies all **170 project image files** reviewed on
**2026-11-04**. It connects the files to their retained generation outputs,
editable sources, processing steps and Git introductions. Runtime distribution
and processing are described in [RuntimeData/README.md](../RuntimeData/README.md).

- [Per-file manifest](provenance/Images.json): repository paths, SHA-256,
  dimensions, source families, transformations, verification results and first
  Git introductions, following renames.
- [Generation prompt records](provenance/ImageGenerationPrompts.md): 82 recorded
  prompts, including intermediate concepts and edits. The manifest links the
  selected outputs to their prompt records.

The records cover source masters, runtime template bitmaps, SVGs, the three
plant textures and cursor strips. Ignored build
products, local generation caches, discarded concepts, images present only in
Git history and optional user-supplied game data are outside this inventory.
The inventory describes the current working tree, excluding pending deletions.

## Evidence and remaining limits

The investigation compared retained generation outputs, project authoring
sources and Git history. The manifest identifies 30 artwork families.

Every tracked image has one manifest entry with one of these evidence kinds:

- **27 exact generated outputs:** complete-file SHA-256 matches to retained
  image-generation outputs.
- **101 reproduced image derivatives:** the recorded cropping, resizing,
  background correction or export steps reproduce the current bytes or pixels.
- **27 reproduced project artworks:** procedural textures, cursors, drawn
  cosmetic icons and vector exports reproduced from their project sources or
  recovered authoring commands.
- **11 retained vectors:** editable SVG sources with authoring records and
  Git history.
- **2 retained project sources:** the Trading Post PNG and SVG. Their current
  files and adjacent authoring documentation are retained; no historical
  generation lookup was performed for this inventory update.
- **2 documented derivatives:** `party_heading.bmp` and `party_hp_fill.bmp`
  have retained SVG sources, documentation and a shared introduction commit.
  Rasterizing the SVG gradients with ImageMagick on the review host did not
  reproduce every current pixel, so equality is not claimed.

Reproductions ran in a temporary directory. Pixel comparisons include the alpha
bytes of 32-bit BI_RGB BMPs; a decoder that discards BMP alpha was not used as
the sole evidence for those matches. Complete-file matches and pixel matches
are distinguished in the manifest.

The 2026-11-04 current-tree update adds nine procedural compact-frame/clock
bitmaps, the Geon coin derivative and the four Trading Post source/export files.
The twelve new bitmap entries reproduce byte for byte using the retained
generators and local source files. All 170 current hashes match. This update
used no Git history, private chats or external sources and changed no image bytes.

The recorded Git dates are commit author dates. The prompt records contain the
tool's `revisedPrompt` field. The records do not identify the image-generation
model.

## Authorship, distribution and references

All 170 repository images were created for OpenKal using AI. The per-file
records describe raster generation, vector and procedural authoring, and
the processing steps used to produce derived exports. OpenKal distributes the
repository images under
[GPL-3.0-or-later](../LICENSE).

Generated origin does not mean that no reference was supplied. The recovered
history explicitly records these inputs:

- The inventory-unlock request supplied `item756.bmp`; its prompt calls it a
  strict style reference for a newly generated tab-and-key token.
- The Medicine/Magic Potion request supplied bottle and item symbols
  named `item556.bmp`, `item618.bmp`, `item619.bmp`, `item620.bmp`, `item095.BMP`
  and `item364.bmp`. The chat also inspected `item363.bmp` and used OpenKal's
  generated Riding Cache as a style reference. Red bottles edit the generated
  blue bottles. Their generated origin is verified by reproduction of all eight
  current templates.
- The EXP-event buff request supplied `buff389.bmp`. The final icon edits the
  preceding newly generated buff symbol.
- The first Dragon HUD request linked a
  [Diablo IV screenshot](https://images.pushsquare.com/3fd9e11cae2f6/diablo-4-ps5-preview.large.jpg)
  for layout inspiration. The recovered initial prompt describes a custom
  Korean/East Asian fantasy HUD with bronze dragons and a twenty-slot grid.
  Later production sheets use the generated OpenKal concepts.
- Other recorded image edits use earlier OpenKal outputs or procedural
  artwork. The manifest names these dependencies rather than treating each
  export as a separate independent generation.

The historical external reference images are not copied into these records.
Their licenses or permissions were not established by this reconstruction;
no license for them is inferred from the project license or from a generation
record. The retained prompts and reference descriptions make those inputs
explicit without reproducing the external image files.

## Logo, inventory tokens and EXP icons

`RuntimeData/OpenKal.png` matches the generated master. Its prompt requests
a rounded black-and-white Yin-Yang with O/K letters and transparency. The
repository uses the master at its original size.

`invUnlock.png`, `accBound.png`, the four riding templates, `expEventCoin.png`
and `expEvent.png` each have their own source records. The open riding cache
edits the closed cache. The account-bound artwork uses a purple overlay.

The donation coin was initially saved as `expEvent.png`, then renamed to
`expEventCoin.png` when the separate buff symbol was introduced. The coin's
recorded background flattening and flood fill reproduce its current pixels.
The current buff symbol reproduces the final generated edit with the recorded
512-by-512 Lanczos export. The manifest records the rename and the origin of
each image.

## Medicine and Auto-HP Medicine

The eight `medicine{Small,Medium,Large,Huge}.png` and
`magicPotion{Small,Medium,Large,Huge}.png` templates derive from eight outputs
generated on 2026-10-07. The recorded nearest-neighbor reduction to 512 by 512
and black-background flattening reproduce every current template's pixels.

A different eight-bottle sheet was generated during the 2026-10-16 release
discussion. It does not reproduce the current templates and is not recorded
as their source. This distinction prevents a later proposal from replacing
the actual origin of the retained files.

The four `medicineAutoHp*.png` files derive from outputs generated on 2026-10-18.
Normal Medicine was edited to add bronze dragon mountings, followed by background
correction and a readability pass. The final outputs plus the recorded 2%
near-black normalization reproduce all four current images. The detailed briefs
remain in [AutoHpMedicine.md](../RuntimeData/templates/AutoHpMedicine.md).

## Dragon HUD, minimap and menu

The twenty action-HUD bitmaps derive from generated production sheets and
edits. The recovered source rectangles, Lanczos scaling, right-wing mirroring
and straight-BGRA export reproduce all twenty files byte for byte.

Four of the five minimap bitmaps are crops of the production sheet.
`area_support.bmp` comes from the final broad bronze infill generation. All
five exports reproduce byte for byte.

The fifteen base menu sprites derive from generated sheet cells. Alpha-bound
trimming and native-size exports reproduce their pixels including alpha. BMP
resolution metadata differs from the temporary reconstruction, so these are
recorded as pixel matches. Collection and messages artwork has separate
records below. See the existing
[HUD](../RuntimeData/templates/dragon_hud/README.md),
[minimap](../RuntimeData/templates/dragon_minimap/README.md) and
[menu](../RuntimeData/templates/dragon_menu/README.md) instructions.

## Dialog frames and controls

The retained `frame-quiet.png`, `frame-corner-ornament.png` and `buttons.png`
masters match their generation outputs. The quiet-frame and corner edits come
from separate outputs. Only the upper-right bitmap slice uses the corner
revision.

`long_buttons.png` matches its generation output, which used the earlier
controls and frame as references. It supplies the four long-button states and
two toggle indicators. Running `tools/export_dragon_dialog_templates.py` in a temporary
checkout reproduces all 27 frame/control bitmaps byte for byte. Source-to-export
paths are recorded individually in the manifest. Existing prompts and slicing
instructions remain in the [dialog README](../RuntimeData/templates/dragon_dialog/README.md).

## Collection, messages and project-created vector controls

The Collection emblem and three-state drawer handle match their selected
generation outputs, including the final handle color correction.
`tools/export_collection_icon.py` and `tools/export_collection_handle.py`
reproduce their five bitmap exports byte for byte.

The bronze messages envelope was generated using existing quest and community
icons as references. Its retained master is unchanged. The recorded trim, resize
and BGRA export reproduce `messages.bmp` byte for byte.

The paperclip, inventory Geon field and six inventory-filter glyphs were drawn
as SVGs. The retained vectors and their three exporter scripts reproduce all
eight associated bitmap exports byte for byte.

The three party vectors carry the contributors' GPL notice. The back plate
reproduces byte for byte. The two gradient bitmap limitations are recorded above.
See [Party.md](Party.md).

## Talents, ingredients and plant textures

The talent atlas matches its 2026-10-19 generation output. The twelve exported
icons reproduce byte for byte using `tools/export_talent_icons.py`.
The [atlas and prompt](../RuntimeData/templates/talents/README.md)
remain in the repository.

The detached Aloe Vera leaf was generated on 2026-10-28 using a project-created
procedural whole-plant icon as reference. Blue Lotus Leaf and Ginseng Root were
generated on 2026-10-29. Their briefs specify no image references. All three
retained PNGs match their generated outputs.

The world-plant DDS textures are separate procedural artwork. The generators
`tools/generate_aloe.py` and `tools/generate_crafting_plants.py` reproduce all
three textures byte for byte. They are not exports of the generated inventory
icons. Their source descriptions and icon prompts remain in the
[plant README](../RuntimeData/models/plants/README.md).

## Enchantment artwork

`polishingStone.png` matches the output generated on 2026-10-19. The current item
definition selects a different icon from optional user data. That external image
is outside this inventory.

`revisionHammer.png` is the final hammer-and-chisel edit from 2026-10-24. The base
Talisman of Enchantment and its five numeral variants were generated on the same
date. Every retained PNG matches its selected output. The numeral family uses
the enlarged final edits. The [hammer](../RuntimeData/templates/RevisionHammer.md)
and [talisman](../RuntimeData/templates/TalismanOfEnchantment.md) briefs describe
the artwork.

## Cosmetics and cursors

`skinBody.png` and `skinWeapon.png` are Pillow drawings made on 2026-10-29. The
drawing uses a rounded frame with a polygonal tunic or weapon, rendered at 128
by 128 and reduced to 32 by 32. Both current PNGs reproduce byte for byte.

`skinTicket.png` matches its 2026-10-30 generation output. See the [individual
provenance note](../RuntimeData/templates/skinTicket.provenance.md) for its brief.

The three cursor strips are project vector and procedural designs from
2026-10-14. Running `tools/generate_cursors.py` reproduces all three strips
byte for byte, including the hand animation.

## Maintaining the record

For a new image, record its prompt, reference inputs, master SHA-256 and export
steps. Link generation outputs to their public prompt records. For vector and
procedural artwork, retain the editable source or generator. Add a manifest
entry for every distributed source image and derivative. Keep private authoring
metadata in private records.

Recompute SHA-256 and update the associated evidence after intentional image
edits. Record generation dates separately from Git introduction dates and
mark unavailable evidence explicitly.

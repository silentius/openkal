# Image provenance

This record identifies all **170 project image files** present in the OpenKal
working tree reviewed on **2026-11-04**. It connects the files to their authoring
chats, retained generation outputs, editable sources, processing steps and Git
introductions. Runtime distribution and processing are described in
[RuntimeData/README.md](../RuntimeData/README.md).

- [Per-file manifest](provenance/Images.json): exact repository paths, SHA-256,
  dimensions, source families, chat IDs, generation-event IDs, transformations,
  verification results and first Git introductions, following renames.
- [Generation prompt records](provenance/ImageGenerationPrompts.md): 82 recovered
  tool-recorded prompts from the relevant image-generation chats, including
  intermediate concepts and edits. Selected outputs are identified in the manifest.

The records cover source masters, runtime template bitmaps, SVGs, the three
plant textures and cursor strips. Ignored build
products, local generation caches, discarded concepts, images present only in
Git history and optional user-supplied game data are outside this inventory.
The inventory describes the current working tree, excluding pending deletions.

## Evidence and remaining limits

The investigation used the app's current and archived chat listings, historical
chat retrieval, local chat metadata for older chats beyond the listing limit,
retained generated outputs, project authoring sources and Git history. The
manifest retains 28 source chats and now identifies 30 artwork families. Chat
titles are preserved verbatim as identifiers; unrelated messages and personal
filesystem paths are not reproduced.

Every tracked image has one manifest entry with one of these evidence kinds:

- **27 exact generated outputs:** complete-file SHA-256 matches to retained
  image-generation results from the identified chats.
- **101 reproduced image derivatives:** the recorded cropping, resizing,
  background correction or export steps reproduce the current bytes or pixels.
- **27 reproduced project artworks:** procedural textures, cursors, drawn
  cosmetic icons and vector exports reproduced from their project sources or
  recovered authoring commands.
- **11 retained vectors:** editable SVG sources with authoring-chat
  context and Git history, rather than image-generation outputs.
- **2 retained project sources:** the Trading Post PNG and SVG. Their current
  files and adjacent authoring documentation are retained; no historical
  generation lookup was performed for this inventory update.
- **2 documented derivatives:** `party_heading.bmp` and `party_hp_fill.bmp`
  have retained SVG sources, their authoring chat, documentation and a shared
  introduction commit. Rasterizing the SVG gradients with ImageMagick on the
  review host did not reproduce every current pixel, so equality is not claimed.

Reproductions ran in a temporary directory. Pixel comparisons include the alpha
bytes of 32-bit BI_RGB BMPs; a decoder that discards BMP alpha was not used as
the sole evidence for those matches. Complete-file matches and pixel matches
are distinguished in the manifest.

The 2026-11-04 current-tree update adds nine procedural compact-frame/clock
bitmaps, the Geon coin derivative and the four Trading Post source/export files.
The twelve new bitmap entries reproduce byte for byte using the retained
generators and local source files. All 170 current hashes match. This update
used no Git history, private chats or external sources and changed no image bytes.

The recorded Git dates are commit author dates, not image-creation dates.
Prompt records contain the tool's `revisedPrompt` field and the enclosing turn's
start time; these are not hidden model instructions or precise generation
timestamps. The actual image-generation model is not exposed by these records.
No assistant name is used to infer an image model for an individual file.

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

`RuntimeData/OpenKal.png` is the unchanged generated master from
**“Erstelle weiches 3D-Yin-Yang”**, chat
`01a0a54c-d821-78b3-8b54-ab8e26fe15eb`. Its prompt requests the rounded
black-and-white Yin-Yang with O/K letters and transparency. The chat also made
a smaller export; the repository PNG matches the generated master itself.

`invUnlock.png`, `accBound.png`, the four riding templates, `expEventCoin.png`
and `expEvent.png` each have their own source records. The riding files come
from **“Generiere Battle-Riding-Bilder”**, chat
`01a1012a-970b-72f2-9079-6fe657cbd35a`; the open cache edits the closed cache.
The account-bound purple overlay comes from **“Create purple bound-item overlay”**.

The donation coin was initially saved as `expEvent.png`, then renamed to
`expEventCoin.png` when the separate buff symbol was introduced. The coin's
recorded background flattening and flood fill reproduce its current pixels.
The current buff symbol reproduces the final generated edit with the recorded
512-by-512 Lanczos export. This rename is preserved in the provenance instead
of assigning the buff chat to both files.

## Medicine and Auto-HP Medicine

The eight `medicine{Small,Medium,Large,Huge}.png` and
`magicPotion{Small,Medium,Large,Huge}.png` templates derive from the eight
outputs in **“Erstelle blaue und rote Tränke”**, chat
`01a116e2-26b6-7d71-9c98-7e706f04536a`, on 2026-10-07. The recorded
nearest-neighbor reduction to 512 by 512 and black-background flattening
reproduce every current template's pixels.

A different eight-bottle sheet was generated during the 2026-10-16 release
discussion. It does not reproduce the current templates and is not recorded
as their source. This distinction prevents a later proposal from replacing
the actual origin of the retained files.

The four `medicineAutoHp*.png` files come from
**“Add dragon designs to medicine potes”**, chat
`01a14e08-0e08-7561-bc71-33b8f487f1d1`, on 2026-10-18. Normal Medicine
was edited to add bronze dragon mountings, followed by background correction
and a readability pass. The final outputs plus the recorded 2% near-black
normalization reproduce all four current images. The detailed briefs remain in
[AutoHpMedicine.md](../RuntimeData/templates/AutoHpMedicine.md).

## Dragon HUD, minimap and menu

The twenty action-HUD bitmaps derive from the production sheets and
edits in **“Generiere KalOnline UI-Beispiel”**, chat
`01a1175a-6f75-78c0-a53d-ff0429033330`. The recovered source rectangles,
Lanczos scaling, right-wing mirroring and straight-BGRA export reproduce all
twenty files byte for byte. The generation outputs remain available
in the app cache; the discarded repository source sheets are not restored here.

The five minimap bitmaps come from **“Runde Minimap planen”**, chat
`01a1251f-d049-7801-91a2-646028a23ad6`. Four are crops of the production
sheet. `area_support.bmp` comes from the final broad bronze infill generation,
rather than the superseded narrow tail-bracket design. All five exports reproduce
byte for byte.

The fifteen base menu sprites come from **“Menüleisten-UI im Dragon-Stil”**,
chat `01a12550-0004-74c0-a4e3-052b53565f22`. Recovered sheet cells,
alpha-bound trimming and native-size exports reproduce their pixels including
alpha. BMP resolution metadata differs from the temporary reconstruction, so
these are recorded as pixel matches. Collection and messages artwork has the
separate records below. See the existing
[HUD](../RuntimeData/templates/dragon_hud/README.md),
[minimap](../RuntimeData/templates/dragon_minimap/README.md) and
[menu](../RuntimeData/templates/dragon_menu/README.md) instructions.

## Dialog frames and controls

The retained `frame-quiet.png`, `frame-corner-ornament.png` and `buttons.png`
masters are unchanged generation results from
**“Chatbox im Singleplayer verbessern”**, chat
`01a1256d-2bdf-7c61-8025-47c79539ae57`. The quiet-frame and corner edits
are separate events; only the upper-right bitmap slice uses the corner revision.

`long_buttons.png` is an unchanged output from
**“Neue Designs für restliche UIs”**, chat
`01a12af2-01e2-7b92-ab62-b53ad3710afd`, using the earlier controls and
frame as references. It supplies the four long-button states and two toggle
indicators. Running `tools/export_dragon_dialog_templates.py` in a temporary
checkout reproduces all 27 frame/control bitmaps byte for byte. Source-to-export
paths are recorded individually in the manifest. Existing prompts and slicing
instructions remain in the [dialog README](../RuntimeData/templates/dragon_dialog/README.md).

## Collection, messages and project-created vector controls

The Collection emblem and three-state drawer handle were generated in
**“Add Taegeuk Hanin and Yeoju”**, chat
`01a13f17-4d77-73d0-8df4-f54bb7af92e9`. The retained PNGs match their
selected generation outputs, including the final handle color correction.
`tools/export_collection_icon.py` and `tools/export_collection_handle.py`
reproduce their five bitmap exports byte for byte.

The bronze messages envelope was generated in
**“Fix TransformWindow and add messages”**, chat
`01a12bc6-180c-7d83-b934-2e0aabaf5167`, using existing quest/community
icons as references. Its retained master is unchanged; the recorded trim,
resize and BGRA export reproduce `messages.bmp` byte for byte.

The paperclip, inventory Geon field and six inventory-filter glyphs are initial
SVG drawings from **“Add attachment icon to messages”** and
**“Inventar auf altes Design zurück”**. They are not raster image-generation
outputs. The retained vectors and their three exporter scripts reproduce all
eight associated bitmap exports byte for byte.

The three party vectors and three associated bitmaps come from
**“Partyfenster und Einladungen ergänze”**, chat
`01a16257-84f5-70c3-b301-8f9162ae7310`. The SVGs carry the contributors'
GPL notice. The back plate reproduces byte for byte; the two gradient bitmap
limitations are recorded above. See [Party.md](Party.md).

## Talents, ingredients and plant textures

The talent atlas is the unchanged generation from
**“Add trade tree progression”**, chat
`01a15427-3651-77d1-9c8a-2057b3bcd405`, on 2026-10-19. The twelve
exported icons reproduce byte for byte using `tools/export_talent_icons.py`.
The [atlas and prompt](../RuntimeData/templates/talents/README.md)
remain in the repository.

The detached Aloe Vera leaf comes from **“Kleine Aloe Vera modellieren”**,
chat `01a18407-8fa8-7f81-8f58-570b5239e4f0`, on 2026-10-28. Its
reference was a project-created procedural whole-plant icon. Blue Lotus Leaf
and Ginseng Root come from **“Ideen für Potion-Zutaten finden”**, chat
`01a1878e-e607-7632-8a6f-e26f8da82526`, on 2026-10-29, with no image
references specified in their generation briefs. All three retained PNGs match
their generated outputs.

The world-plant DDS textures are separate procedural artwork. The generators
`tools/generate_aloe.py` and `tools/generate_crafting_plants.py` reproduce all
three textures byte for byte. They are not exports of the generated inventory
icons. Their source descriptions and icon prompts remain in the
[plant README](../RuntimeData/models/plants/README.md).

## Enchantment artwork

`polishingStone.png` is the unchanged generated output from
**“Add polishing stone protection”**, chat
`01a152ce-afd3-7f23-a730-f89da13f0f6c`, on 2026-10-19. It is retained
project artwork; the current item definition selects a different icon from
optional user data. That external image is outside this inventory.

`revisionHammer.png` is the final hammer-and-chisel edit from
**“Add shop-bought revision stone item”**, chat
`01a16e95-697b-7991-9f83-5713059b8679`, on 2026-10-24. The base
Talisman of Enchantment and five numeral variants come from
**“Add Talisman of Enchantment”**, chat
`01a16ec9-baa7-72d1-8129-a0752bd4304c`, on the same date. Every retained
PNG matches its selected output; the numeral family uses the enlarged final
edits rather than the earlier smaller inscriptions. Existing
[hammer](../RuntimeData/templates/RevisionHammer.md) and
[talisman](../RuntimeData/templates/TalismanOfEnchantment.md) briefs remain in place.

## Cosmetics and cursors

`skinBody.png` and `skinWeapon.png` are Pillow drawings from
**“Kostümsystem prüfen und planen”**, chat
`01a1881b-e724-76e1-aa8e-5751361f960a`. The drawing command was recovered
from the 2026-10-29 chat log: a rounded frame with a polygonal tunic or weapon,
rendered at 128 by 128 and reduced to 32 by 32. Both current PNGs reproduce
byte for byte; neither is a rendered proprietary model or an Imagegen output.

`skinTicket.png` is the unchanged generated output from
**“Erkläre Skin Crafting und Enhancing”**, chat
`01a18d70-4a55-7cd1-bcc5-abc8a5e60c80`, on 2026-10-30. Its initial
[individual provenance note](../RuntimeData/templates/skinTicket.provenance.md)
and prompt are retained.

The three cursor strips originate in **“Add custom animated cursors”**, chat
`01a13ba4-6a84-7930-a7a1-b6c8cb1faef1`, on 2026-10-14. They are
project vector/procedural designs generated by `tools/generate_cursors.py`.
All three strips reproduce byte for byte, including the hand animation.

## Maintaining the record

For a new image, retain the selected generation's chat and event IDs, its
tool-recorded prompt, any reference inputs, the master SHA-256 and the export
steps. For vector/procedural artwork, retain its editable source or generator.
Add a manifest entry for each distributed source image and derivative.

Recompute SHA-256 after intentional image edits and update the associated
evidence; an old hash describes the old bytes. Do not turn Git introduction
dates into generation dates or replace an unknown image model with the chat's
assistant model. Keep unavailable evidence explicitly marked.

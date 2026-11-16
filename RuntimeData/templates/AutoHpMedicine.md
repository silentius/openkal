# Auto-HP Medicine artwork

Four additional icon templates for Small, Medium, Large and Huge Medicine:

- `medicineAutoHpSmall.png`
- `medicineAutoHpMedium.png`
- `medicineAutoHpLarge.png`
- `medicineAutoHpHuge.png`

Each PNG is an opaque authoring asset with a pure black (`#000000`)
background matching the existing Medicine templates. The existing runtime
icon build discovers these templates and exports 32-by-32, black-keyed BMPs
under `RuntimeData/HyperText` in the build directory. Future item image keys
are the filenames without extensions. Item definitions 23008–23011 use these
icons for Small, Medium, Large and Huge Auto Medicine. Their healing values
are 100, 300, 500 and 800 HP respectively, copied from the existing medicine
definitions with the corresponding display names. `specialty.refresh.auto_hp`
restricts them to the Auto-HP slot; see `docs/ui/VARIABLES.md` for interaction.

## Provenance

Created for OpenKal on 2026-10-18 with the built-in OpenAI image-generation
tool. Inputs are the corresponding `medicineSmall.png`,
`medicineMedium.png`, `medicineLarge.png` and `medicineHuge.png` templates
already in this repository.
The design adds antique bronze dragon mountings while retaining recognizable
medicine and the four bottle shapes. No external asset pack was used.
The new artwork is distributed under the repository's GPL-3.0-or-later license;
see the root `LICENSE`.

## Generation prompts

The following shared prompt was used once per source image, with the variant
sentence below appended. Transparent background was enabled for every call.

The final background correction used the built-in image tool with transparency
disabled and the corresponding generated icon as its edit target:

The resulting RGB PNGs have near-black colors normalized to exact black
(ImageMagick, 2% color tolerance) for the runtime color key.

"Use case: precise-object-edit. This existing OpenKal inventory icon is the edit target. Make exactly one technical change: replace all transparent background with perfectly uniform opaque pure black RGB(0,0,0), hex #000000, for BMP color-key compatibility. Preserve the existing foreground bottle and bronze dragon pixel for pixel as closely as possible: identical silhouette, placement, size, red medicine, cork, bronze, details, highlights, colors, square framing. Do not redesign or repaint the icon, do not add anything. No gradient, lighting, backdrop texture, ground plane, drop shadow, text or border. Keep image dimensions if possible. The output must be fully opaque with pure black surrounding the icon."

Use case: precise-object-edit. Asset type: OpenKal fantasy RPG inventory icon, a new Auto-HP Medicine variant. Input image 1 is the edit target from the existing OpenKal repository. Preserve its distinctive bottle silhouette, proportions, red medicine, glass highlights, upright front view and cork stopper. Add a premium antique bronze dragon-shaped mounting attached to the bottle: a sculpted dragon head by the shoulder, a curved scaled body framing one side, a tail supporting the base and a bronze neck collar. The dragon is metal hardware, not a living animal. Keep most of the central red liquid and glass visible. Warm bronze edge highlights, restrained patina and dark carved recesses. Match the existing crisp stylized pixel-art inventory rendering; readable bold shapes at 32x32, no microscopic filigree. Single centered bottle, square canvas, entire object visible with a small transparent margin. Genuinely transparent background and transparent space around the silhouette; no black rectangle, ground, shadow plane, UI frame, text, label, watermark, aura or extra items. Keep all four variants stylistically coherent.

- Small: The Small version has a slender long neck and a small teardrop body. Preserve that long-necked narrow silhouette.
- Medium: The Medium version has a medium neck and a round pear-shaped belly. Preserve that silhouette.
- Large: The Large version has a short neck, broad rounded shoulders, round body and a small glass foot. Preserve that silhouette.
- Huge: The Huge version has a very wide squat round body, almost no neck, broad opening and a wide cork. Preserve that squat wide silhouette.

## Readability revision (2026-10-18)

Manual inspection found the Medium mounting difficult to distinguish at
32-by-32 pixels. All four icons now use broader claw tips, brighter bronze
bands and stronger separation around the dragon head. Bottle shapes and red
medicine remain the identifying base. The revision uses the built-in image
tool with opaque black backgrounds; the existing 2% near-black normalization
is retained. Review the actual BMP export at native size, not only the source.

### Medium edit prompt

Use case: precise-object-edit. Edit this existing Medium Auto-HP Medicine game icon with a restrained readability pass, not a redesign. Preserve the bottle's medium pear shape, upright placement, red liquid, cork, glass highlights, antique bronze dragon mounting and black background. The actual delivered game icon is only 32x32 pixels: fine scales currently average into an indistinct brown patch. Strengthen just THREE existing metal details into bold recognizable shapes: (1) make the existing dragon head on the upper-left shoulder more clearly silhouetted, a modestly projecting angular snout facing left with one clear black mouth notch and TWO thick horns; separate head from the glass/collar with a deep dark contour; (2) make the neck collar a broad bright warm-bronze band with a dark lower edge; (3) make the existing lower-left mounting grip into three broad, clearly separated bronze claw tips extending a little over the red glass, and give the curved right body and base a continuous strong bronze highlight. These are bronze fittings, not a living dragon. Keep at least 70 percent of the red glass area unobscured. At native 32px, key bright bands/claw tips and dark gaps need to survive as roughly 1-2 pixels, the dragon head around 7x7 pixels. Reduce tiny scale texture and engraving in favor of large light and dark planes. Retain existing style, bronze material, composition, object size and margins. No wings, no extra dragon, no new emblem or gem, no lettering, no glow, no aura, no UI border, no extra ornamentation. Background must be perfectly uniform opaque pure black #000000 for BMP color-key compatibility, including holes between fittings. Square single icon only, no comparison sheet. Aim for visually distinctive metal fittings when reduced to 32x32 with Lanczos.

### Remaining variant prompts

#### Small

Use case: precise-object-edit. Input 1 is the Small Auto-HP Medicine icon to edit. Input 2 is ONLY the readability and material reference, a revised Medium icon; do NOT copy its bottle shape. Preserve input 1's slender long-necked teardrop silhouette, red medicine, cork, glass, object placement, margins and existing dragon mounting location. This is a targeted small-icon readability correction, not a redesign. Match input 2's bolder light/dark separation: enlarge the EXISTING dragon head's readable snout/horns modestly and give it dark separation from the glass, brighten and broaden the bronze neck collar, make three broad separated bronze claw tips on the lower mounting grip project slightly over the red glass, and add a strong continuous bronze edge highlight along the metal body and base. Keep most of the red medicine unobscured. Simplify tiny scales into larger bronze planes rather than adding texture. Actual final game display is 32x32: the bright claw tips and dark separating notches must each survive as about 1-2 pixels, the head about 7x7 pixels. Preserve bronze brown midtones with bright warm edges. Preserve the Small silhouette and proportions. No new emblem, gem, wings, text, glow, aura, frame or ground. Fully opaque perfectly uniform pure black #000000 background for BMP color key, including fitting gaps. One square icon only.

#### Large

Use case: precise-object-edit. Input 1 is the Large Auto-HP Medicine icon to edit. Input 2 is ONLY the readability and material reference, a revised Medium icon; do NOT copy its bottle shape. Preserve input 1's short-necked rounded bottle with a small foot silhouette, red medicine, cork, glass, object placement, margins and existing dragon mounting location. This is a targeted small-icon readability correction, not a redesign. Match input 2's bolder light/dark separation: enlarge the EXISTING dragon head's readable snout/horns modestly and give it dark separation from the glass, brighten and broaden the bronze neck collar, make three broad separated bronze claw tips on the lower mounting grip project slightly over the red glass, and add a strong continuous bronze edge highlight along the metal body and base. Keep most of the red medicine unobscured. Simplify tiny scales into larger bronze planes rather than adding texture. Actual final game display is 32x32: the bright claw tips and dark separating notches must each survive as about 1-2 pixels, the head about 7x7 pixels. Preserve bronze brown midtones with bright warm edges. Preserve the Large silhouette and proportions. No new emblem, gem, wings, text, glow, aura, frame or ground. Fully opaque perfectly uniform pure black #000000 background for BMP color key, including fitting gaps. One square icon only.

#### Huge

Use case: precise-object-edit. Input 1 is the Huge Auto-HP Medicine icon to edit. Input 2 is ONLY the readability and material reference, a revised Medium icon; do NOT copy its bottle shape. Preserve input 1's very wide squat bottle with a broad cork and almost no neck silhouette, red medicine, cork, glass, object placement, margins and existing dragon mounting location. This is a targeted small-icon readability correction, not a redesign. Match input 2's bolder light/dark separation: enlarge the EXISTING dragon head's readable snout/horns modestly and give it dark separation from the glass, brighten and broaden the bronze neck collar, make three broad separated bronze claw tips on the lower mounting grip project slightly over the red glass, and add a strong continuous bronze edge highlight along the metal body and base. Keep most of the red medicine unobscured. Simplify tiny scales into larger bronze planes rather than adding texture. Actual final game display is 32x32: the bright claw tips and dark separating notches must each survive as about 1-2 pixels, the head about 7x7 pixels. Preserve bronze brown midtones with bright warm edges. Preserve the Huge silhouette and proportions. No new emblem, gem, wings, text, glow, aura, frame or ground. Fully opaque perfectly uniform pure black #000000 background for BMP color key, including fitting gaps. One square icon only.

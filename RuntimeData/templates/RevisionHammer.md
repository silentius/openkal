# Revision Hammer

Definition 23021 is a stackable tool sold by the normal merchants Goo-Deuk and
Gae-Yoo. Buy price: 750; sell price: 400. Drag it onto a single unequipped weapon,
armor piece (including shields), ring or trinket. A successful conversion consumes
one hammer and destroys the selected equipment. Rings and trinkets always give
exactly one Stone of Revision, including ungraded accessories. Weapons and armor
give 4 + X and 2 + X stones respectively, where X is a uniform inclusive roll
from 0 to grade / 10. For example, G50 and G53 weapons give 4–9 stones, while
G50 and G53 armor gives 2–7. Ungraded weapons and armor are rejected. Enchantment protection does
not prevent intentional conversion. Invalid applications consume nothing.

The client uses the existing enchantment drag/charge interaction. The server
validates the two instance IDs, reserves room for the maximum reward, consumes
the hammer and uses the existing equipment-destruction and reward helpers.
The local viewer applies the same rules and stages inventory changes before
inserting the reward. No new protocol message is required.

## Icon provenance

`revisionHammer.png` is project artwork created for OpenKal with the built-in
Imagegen tool on 2026-10-24, using the documented hammer-and-chisel design.
The initial hammer and its edited tool-pair image were generated without external
reference artwork. No proprietary image is copied or redistributed. The source
asset is provided with OpenKal under GPL-3.0-or-later. CMake converts it into the
transparent 32 by 32 runtime icon `HyperText/revisionHammer.bmp` in the build tree
and includes it in `config.pak`.

Initial generation prompt:

> Create one original game inventory item icon for OpenKal: a Revision Hammer, a stout steel smith's hammer with a rectangular worn iron head, bronze collar, dark walnut wood handle, small restrained blue glint on head. Korean fantasy MMORPG item art, hand-painted realistic metal and wood, simple bold silhouette readable at 32 by 32 pixels, diagonal handle from lower left to upper right, centered and filling the square with small margin. Hammer alone, no anvil, no hat, no stones, no frame, no lettering, no logo, no ground, no cast shadow outside object. Transparent background. Production asset original artwork. Save generated output as an image file.

Final edit prompt (the generated hammer was the only reference image):

> Edit the generated Revision Hammer game inventory icon. Keep its worn iron head, bronze collars, walnut handle and blue glint. Add a distinct steel chisel crossing behind the hammer to form a clear X tool-pair silhouette, with a short bronze grip and a broad sharp flat tip visible toward lower right, striking cap visible upper left. Make the hammer slightly smaller to allow both tools entirely inside the square with a small transparent margin. Original Korean fantasy MMORPG inventory art, strong readable shapes at 32x32 pixels. Transparent background, no anvil, no stones, no text, no frame, no ground shadow. Preserve real alpha transparency.

## Verification

`RevisionHammerServerTest` uses project-authored synthetic item definitions,
a Unix socket and an isolated SQLite database to verify accepted targets,
material consumption, saved rewards, rejected requests, replay protection and
relogin. `InventoryHudTest` verifies drag requests for every accepted equipment
family and rejects worn/stacked targets. `RuntimeIconsTest` checks the packaged
transparent icon; `test_shop_catalog` checks the merchant assortment and prices.

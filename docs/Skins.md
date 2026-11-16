# Cosmetic skins

Cosmetics occupy independent slots immediately to the left of the corresponding
equipment slot. Helmet, upper armor, gloves, pants, boots, weapon, shield and face
are supported. Existing equipment determines combat values, weapon animations,
hand conflicts and skill behavior. Skins contribute no stats, including when a
definition or instance contains modifiers. They cannot receive ordinary weapon
or armor upgrades. Crafting, costume enhancement and collection registration
are outside this first version.

Weapon skins require an equipped weapon of the same subclass. The server rejects
incompatible skin equips and actual weapon type changes while a conflicting skin
is worn; remove the skin before changing type. Compatible weapon swaps remain
available. Removing the actual weapon is allowed, with no skin model displayed
until a matching weapon is equipped again. Legacy incompatible appearances fall
back to the real weapon locally and for remote players.

`skin.weapon_type` accepts `sword`, `sword2h`, `wand`, `bow` or `dagger`. Without
this field, Magician/Archer/Thief skins use their unambiguous `wand`/`bow`/`dagger`
class default. All 101 current Knight weapon models have an explicit category.
Forty-one use reviewed references: twelve from community sword/giant-sword labels, two
from explicit material hand-count labels, and 27 from reviewed identical reference
geometry. The template catalog preserves source/model guards and expected shape
fingerprints. OpenKal categories cover the other 60:
23 one-hand and 37 two-hand skins, after comparing grips and sampled attack
poses. [weapon-compatibility.json](../config/skins/weapon-compatibility.json)
records each choice, its reason and reviewed attachment-shape fingerprint.
Their item descriptions explicitly say that the reference type is unverified.
The audit checks the active model and type, and fresh generation preserves the
guarded choices. Variant parity, dimensions and descriptive names do not assign
types automatically. Unknown future Knight models still fail closed.
These are OpenKal compatibility assignments, not verified reference item flags;
the extra 60 do not change the reference evidence or combat rules.

## Whole-body and individual skins

A whole-body skin goes in the chest skin slot. Equipping it automatically returns
existing body skins to General, then locks the helmet, gloves, pants, boots and
face skin slots. Real equipment, weapon skins and shield skins remain independent.
Double-clicking an individual body skin in General returns the whole-body skin
before equipping that part. Locked slots reject drag/drop. Double-clicking an
equipped skin removes that exact instance.

Every transfer waits for the server. The server reserves writable General cells
for all displaced items after freeing the incoming item's cell, then saves the
incoming and returned instances in one transaction. Insufficient space or a
failed save leaves the existing state intact. Skin instances use persistence
slot 6 and dedicated equipment cells, retaining binding and all instance fields
across reconnects. Existing equipment and collection cells keep their indices.

## Appearance and optional assets

Definitions use `"class": ["skin", "body"]` for whole-body costumes or a part
name (`helmet`, `upper`, `gauntlet`, `pants`, `boots`, `weapon`, `shield`, `face`).
The existing `action` pair identifies the model group and variant; `limit.class`
and `limit.level` apply normally. Skins must have `wear: 1` and are not stackable.

Body skins resolve `Model/Clothes/C<job>_<group>_c<variant>.gb`. Individual
clothing skins use the existing part letters. A partial costume can set
`"skin": {"costume_mesh": true}` to use the `c` mesh family while retaining its
individual slot and keeping the other clothing visible. The group 6, 12 and 13 head
meshes use this override: IDs 25005, 25011, 25012, 25150, 25156, 25157, 25266,
25272, 25273, 25386, 25392 and 25393 occupy the helmet skin slot.
Weapon skins resolve
`Item/Weapon/P<species>_S<group>_<variant>.gb`; shield skins use separator `B`.
Resolution is case insensitive and never borrows another class's skin. A missing
skin mesh falls back to the underlying equipment. Whole-body meshes replace the
four clothing parts and the normal hair. They retain the character face unless
`"skin": {"replace_face": true}` selects the costume's complete face. The observed
families 7, 8, 29 and 30 use this flag. The Thief-only 189 appearance retains
the normal face beneath its gold headwear.
`"skin": {"keep_hair": true}` is an explicit exception for custom whole-body
meshes without built-in hair/headwear. Both flags apply only while the actual
whole-body model resolves; ordinary equipment and missing-model fallbacks retain
their normal face and hair. Removing the skin restores the underlying appearance.

Character selection includes worn equipment and skins up to the protocol's
255-entry limit. Shields remain visible, re-entry preserves skin/inventory state,
and removing skins restores the underlying equipment. Whole-body and individual
replacement preserve their slot locks in both directions.

The seven legacy appearance entries keep their wire format. A nonempty skin
snapshot appends version 1, eight unsigned 16-bit skin indices and the actual
shield index. A shield also enables this extension when no skins are worn. Parsing rejects
incomplete or unknown extensions and clears cosmetics for legacy snapshots.
Login, newly visible players and live equipment changes carry the cosmetic
state; character selection uses the existing variable-length equipped list.

Two-sided blended weapon and offhand meshes stream their triangles in camera
back-to-front order. This prevents their rear surfaces from overwriting the
front texture, as reproduced on the red sphere of 25188. Local, remote and
character-selection actors use their current view matrix. This per-mesh ordering
does not resolve arbitrary intersecting transparent surfaces.

## Test catalog

IDs **25000–29999** are reserved here for OpenKal cosmetic definitions.
The current catalog contains **784 active items within 25000–25807**: 113 whole-body models,
446 weapon models and 225 individual-part skins. Allocation is separate from
the consumable and Collection ranges. The next available cosmetic ID is 25808.
Withdrawn IDs remain reserved and do not change the IDs of later items.

Body families 31, 32, 33, 34, 79 and 89 are excluded for every class. Families
31/32 are ordinary armor; 79/89 combine ordinary armor textures; 33/34 remain
withheld pending classification. Their reserved ranges are 25031–25036,
25176–25181, 25292–25297 and 25412–25417.

Every class has complete individual-part model 01-01 and 14-01 sets, grade
appearances through G55 and the documented Darkness selection. G56/G59 weapon
mappings and unavailable Darkness variants are listed in
[EquipmentSkinCatalog.md](EquipmentSkinCatalog.md). Grade labels describe the
appearance; skins retain level 1 and no combat stats. Model-group numbers are
not ordered by grade. Static front/back checks do not establish complete motion
or mixed-outfit behavior.

Boot samples use group 14, variant 1; face samples use normal face group 0,
variant 2. Knight shield sample 25042 uses 06-01, distinct from the starter Small
Iron Shield. These corrected samples retain their allocated IDs.

See [SkinItems.md](SkinItems.md) for every ID and technical model name. For example:

```text
/getitemid 25003 0 1
```

This grants the Knight body skin for model group 4, variant 1. Items are available
through the existing authorized debug command and the Skin Artisan ticket exchange.
No drop sources are configured. All models still require compatible, locally supplied
assets. A matching filename is not proof of a visually correct costume: verify
textures, body coverage, hair, animation, offhand attachment and class appearance
in game. Curated names are resolved for 74 existing IDs;
eight former Game Master display labels use Stormforged, Starfall and
Moonveil names, with Sovereign variants. Names beginning with Returned, Artificial
or GM (including Imperial GM variants) are excluded from the candidate lists.
Skins have no time-limited variants, so candidates with duration annotations are
also excluded. These naming rules resolve seventeen additional names, which are
applied to the item definitions. The remaining thirteen names are now explicitly
selected and synchronized, including the two existing Violet Wing descriptive
labels. No pending name records remain. Another 707 appearances now use
OpenKal descriptive English names; descriptions retain their model keys
and explicitly identify their name origin. The three missing-texture appearances
keep technical names until they can be inspected. Naming does not classify or
unlock weapons. Reference item IDs and stats are not adopted. See [SkinCatalogAudit.md](SkinCatalogAudit.md) for the automated
checks, name provenance and remaining review states.

`tools/create_skin_items.py DATA_ROOT NEW_OUTPUT_DIRECTORY` creates an initial
test catalog from mesh filenames and the curated name mapping. It deliberately refuses to overwrite a
curated directory. Initial generation enumerates candidate body files, weapon
groups 21–23 and 40 or above, plus one sample for each available individual part.
Excluded body families consume their previous allocation positions but produce
no active definitions. The explicit `supplemental_items` in `config/skins/catalog.json`
append the twenty complementary class parts and the later equipment appearances
without changing earlier allocations or applying the initial sample overrides.
Explicit class-compatible weapon types keep supplemental Knight swords and
greatswords in their proper shop blocks. Not every weapon mesh in these ranges is necessarily an official costume;
model annotations and separate naming provenance make appearance candidates explicit. Further catalogs must
preserve allocated and retired IDs instead of renumbering existing items.

## Validation and provenance

`skin_server` exercises actual packets, multi-item replacement, a full inventory,
class restrictions, stat isolation, duplicate requests, reconnect restoration and
transaction rollback using project-owned synthetic fixtures. `inventory_hud` checks
cosmetic storage, whole-body replacement, head-only costumes retaining body
clothing and independent skin slots, missing-model fallback and appearance
packet parsing. `multiplayer_presentation` checks cosmetic updates for existing
remote players. Render the inventory and its whole-body locks with:

```text
openkal_test_inventory_hud --capture-skins OUTPUT_PREFIX
```

All 784 active model paths have also been decoded against optional local assets
using `openkal_test_inventory_hud --verify-skin-assets DATA_ROOT`. This checks
model availability and mesh decoding, not the visual quality of every costume.

Implementation, synthetic fixtures, technical item names and the two skin icon
templates were created for OpenKal under GPL-3.0-or-later. Item model coordinates
come from observing optional local filenames. The appearance catalog does not
redistribute the external model/texture files or complete reference item tables.
Current display labels and technical model associations are retained in
`config/skins/names.json`; their wording is separate from weapon classification.
External name-reference and source-line annotations were removed without changing
the displayed names or curated mappings. See [SkinCatalogAudit.md](SkinCatalogAudit.md).
The public costume
guides are background for future work:
[Crafting](https://gameagit.com/kalonline/CostumeCrafting),
[Enchant](https://gameagit.com/kalonline/CostumeEnchant),
[Combine](https://gameagit.com/kalonline/CostumeEnchant2), and
[Collection](https://gameagit.com/kalonline/Costume_Collection).

## Skin Artisan ticket exchange

The standalone Skin Artisan (definition **60002**, dialogue **990310**) is
configured in `config/npcs/60002_skin_artisan.json`. It stores position, facing
and appearance directly and requires no external NPC-template inheritance.
The supplied NPC 1252 definition from `openkal_data` is the configuration source;
its origin/hash are retained in `_source`. The supplied JSON has no initial
author/license declaration. Models and textures remain optional external assets.
The included definition is numeric compatibility configuration plus the project's
service identifier and label; its `_source` records transfer, not an inferred
license for the external input. See the
[source-release content review](OpenSourceReleaseChecklist.md).

**Skin Ticket 23030** is a normal, stackable, freely tradeable item. It has no
fixed NPC buy or resale price. Full-body skins cost **4 tickets**; individual
parts, weapons and shields cost **2**. Redemption grants one ordinary skin item
into General, never a permanent collection unlock or automatic equip. There are
no combat bonuses, enhancement, random outcomes, gold costs, or configured drop
sources. Game developers decide ticket acquisition. Normal instance binding rules
still apply if a developer deliberately grants bound tickets.

`config/server/shops/60002_skin_artisan.json` extends `openkal.shop/v1` with
`skinCatalog: true`, `currencyItem`, `fullBodyPrice`, and `itemPrice`. The server
derives its offers from loaded cosmetic definitions and filters by the current
player's class. The client cannot submit a different class, substitute a currency,
choose the price, or redeem bulk quantities. Each purchase requires an active
shop session, the correct map, NPC proximity, a living and available character,
enough bag tickets, and space for the result. Ticket removal and output insertion
are persisted in the existing atomic inventory exchange. A failed save or lack
of room restores the whole bag. Consumed ticket stacks can free result space.

The optional `excludeItems` array removes selected item IDs from the derived skin
offers and from redemption. It preserves the item definitions and existing owned
skins. The Skin Artisan excludes the arrow attachment 25303, the missing-texture
bow 25313, and the duplicate Frost Feather Bow 25364; Frost Feather Bow 25355
remains offered. These exclusions do not imply that other similarly named skins
are interchangeable.

The existing item-list wire payload remains unchanged for gold shops. Item shops
append a nonzero unsigned 16-bit currency item index after their offers. The
purchase packet is unchanged; currency and price are resolved exclusively by the
server. The ticket service does not accept ordinary gold sell requests.

The NPC conversation opens Weapons, Full skins, or Skin pieces. Each view always
shows only the player's class. Knight weapons have separate one-hand and two-hand
blocks, with shields in the one-hand block; other classes have one weapon block.
Full-body skins use one block. Pieces use Head, Chest, Hands, Legs, Feet and Face
blocks. The blocks are stacked in one scrollable catalog, not nested category
menus. Mouse wheel input over the window never zooms the world camera.

The left mirror renders a detached `Wear` snapshot with the player's real face,
hair and outfit, then applies only the selected skin. Conflicting whole-body and
partial skins are resolved in the copy. A compatible ordinary display weapon is
used when the current combat weapon does not fit the selected appearance; the
real weapon, inventory and stats are never changed by previewing. Rotation
buttons inspect the front and back. Thumbnail images are generated locally from
the optional models: complete outfits, cropped body parts, and isolated weapons
or shields. Visible thumbnails are filled one per frame and cached for the
session. Generated model images are never redistributed with the source.

`skin_shop.json` uses universal `openkal.ui/v1` canvas widgets, Dragon nine-slice
chrome, standard buttons, and FreeType text. Canvas bounds are shared by rendering
and pointer hit testing and remain editable in the UI editor. The ticket artwork
also appears inside the upper-left frame medallion.

`skin_shop_server` covers class and offer validation, exact ticket prices,
unauthorized requests, rejected bulk purchases, insufficient tickets, split
stacks, full bags, freed input cells, database rollback and tradeable output.
`skin_shop_ui` checks categories, shield placement, detached preview equipment,
selection versus redemption and standalone NPC placement/appearance without an
external NPC definition. To capture the actual renderer using your locally supplied models:

```bash
build/tests/openkal_test_skin_shop_ui --preview /path/to/data /tmp/skin-shop.png
build/tests/openkal_test_skin_shop_ui --preview /path/to/data /tmp/skin-weapons.png weapons
build/tests/openkal_test_skin_shop_ui --preview /path/to/data /tmp/skin-pieces.png parts
```

These optional captures use synthetic ordinary display equipment and project-owned
item definitions; they do not establish that every skin's assets or animations
have been verified. Existing catalog review limitations remain in effect.

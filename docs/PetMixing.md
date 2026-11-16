# Pet mixing design

Crossbreed and Monster Pet Creation are design work. The existing playable
[pet system](Pets.md) provides lifetime, healing, pickup and presentation; it does
not implement these mixing/option systems.

## Scope

The available sources use “pet mixing” for two different systems:

1. **Crossbreed** combines two existing pets into a hybrid or rare pet.
2. **Monster Pet Creation** combines monster souls, a talisman or jewel, and
   gold into a monster-pet item.

These systems must not be implemented as ordinary weapon or armor mixing.

## 1. Crossbreed system

The public guide places Crossbreed at the Manager of Mystical Creatures in the
Pub of the Giant Bird. The UI has a pet list, a Main Pet slot, a Sub Pet slot,
and result/alternative-result previews.

Source: <https://gameagit.com/kalonline/Crossbreed>

### Input rules

- The Main Pet is the essential input and determines the hybrid family and the
  main ability.
- The Sub Pet provides the fixed additional buff.
- Main and Sub must be compatible pet categories.
- Zamogeon Pets and Kal Cash Pets cannot normally be crossbred.
- Field Pets and PvP Pets cannot be crossbred.
- Kal Cash Pets and Kal Point Pets are explicitly described as compatible.
- Pets with the same name cannot be combined.
- A pet that has already been used more than once cannot be crossbred again.
- The source pets are deleted after the hybrid pet is awarded.
- Re-equipping a pet suppresses its buff display for five minutes.

The exact persistence representation of “already used” is not given. A
per-instance usage counter or a permanent `crossbreed_used` flag should be
stored server-side; it must not be inferred from the item index alone.

### Result behavior

The order of the two inputs matters. The following Zamogeon hybrid results are
listed by the guide:

| Main Pet | Sub Pet | Result | Fixed buff |
| --- | --- | --- | --- |
| Blue Dragon | White Tiger | White Dragon | Explosive Blow +1% |
| Blue Dragon | Black Tortoise | Black Dragon | Defense +2% |
| Blue Dragon | Red Bird | Red Dragon | Evasion +1% |
| White Tiger | Blue Dragon | Blue Tiger | Attack +1% / +2% |
| White Tiger | Black Tortoise | Black Tiger | Defense +2% |
| White Tiger | Red Bird | Red Tiger | Evasion +1% |
| Black Tortoise | Blue Dragon | Blue Tortoise | Attack +1% / +2% |
| Black Tortoise | White Tiger | White Tortoise | Explosive Blow +1% |
| Black Tortoise | Red Bird | Red Tortoise | Evasion +1% |
| Red Bird | Blue Dragon | Blue Bird | Attack +1% / +2% |
| Red Bird | White Tiger | White Bird | Explosive Blow +1% |
| Red Bird | Black Tortoise | Black Bird | Defense +2% |

The guide gives separate Field/PvP values for the main and additional
abilities. The fixed buff is not affected by Blessing of Asadal.

### Base pet abilities

The guide lists these base values. They should become data, not hard-coded
conditionals:

- Blue Dragon: main Attack; additional Defense.
- White Tiger: main Explosive Blow; additional Defense or Attack depending on
  the pet variant.
- Black Tortoise: main Defense; additional Attack or Evasion.
- Red Bird: main Evasion; additional Attack or Defense.

Field/PvP Zamogeon examples:

- Blue Dragon: Attack +4% / +6%, Defense +3% / +4%.
- White Tiger: Explosive Blow +2% / +4%, Defense +3% / +4%.
- Black Tortoise: Defense +5% / +6%, Attack +2% / +3%.
- Red Bird: Evasion +2% / +4%, Attack +2% / +3%.

Cash-pet examples have stronger values and an additional ability slot. The
guide lists, among others, Blue Dragon with Attack +8% / +12%, Defense +3% /
+4%, and Explosive Blow +2% / +3%.

### Cash-pet random stat

When Cash Pets are crossbred, the result may receive an extra random stat. The
range depends on the Main Pet:

- Blue Dragon: Attack power -1% to +2%.
- White Tiger: Explosive Blow rate -1% to +2%.
- Black Tortoise: Defense -1% to +2%.
- Red Bird: Evasion -1% to +1%.

The guide also describes a low-probability Rare Pet result for Cash Pets.
Rare Pets have stronger abilities, guaranteed buff activation, and longer
runtime/cooldown behavior. Exact probabilities are not published on the page.

## 2. Monster Pet Creation

Monster Pet Creation is separate from Crossbreed. It is opened from the game
settings menu and produces a selected monster-pet item when the required
materials are supplied.

Source: <https://gameagit.com/kalonline/MonsterPet>

The guide distinguishes Normal and Premium Monster Pets:

- Normal: monster soul + Honor Pet Talisman + Zamogeon.
- Premium: monster soul + Jewel + Zamogeon.

The inspected recipe data represents two material stacks and a separate
100,000-Zamogeon cost. Material 7716 is the Honor Pet Talisman; 3360 is Jewel.

### Known recipes

The optional data catalog uses `config/pets/monster_recipes.json` with these
known recipes; the catalog is not distributed in this source checkout:

| Result item | Material 1 | Material 2 | Category |
| ---: | ---: | ---: | --- |
| 7700 | 7708 x500 | 7716 x5 | Normal |
| 7701 | 7709 x500 | 7716 x5 | Normal |
| 7702 | 7710 x600 | 7716 x5 | Normal |
| 7703 | 7711 x600 | 7716 x5 | Normal |
| 7704 | 7712 x700 | 3360 x49 | Premium |
| 7705 | 7713 x300 | 3360 x49 | Premium |
| 7706 | 7714 x300 | 3360 x49 | Premium |
| 7707 | 7715 x700 | 3360 x49 | Premium |
| 7719 | 7723 x500 | 7716 x5 | Normal |
| 7720 | 7724 x500 | 7716 x5 | Normal |
| 7721 | 7725 x600 | 3360 x49 | Premium |
| 7722 | 7726 x600 | 3360 x49 | Premium |

Every listed recipe costs 100,000 gold. The local recipe data contains no
failure probability, so the basic creation operation should be treated as
deterministic unless an explicit gameplay rule adds a failure chance.

The twelve result items use visual keys 81 through 92 and are classified as
permanent monster pets in the inspected catalog (`durationSeconds: null`).

### Monster Pet options

The public guide adds behavior not represented in the basic local recipe file:

- Normal Monster Pets receive one random buff.
- Premium Monster Pets receive one random ability and one random buff.
- Random abilities include Strength, Wisdom, Intellect, Health, Agility, Hit,
  Evasion, Defense, Resistance, Absorption, HP, SP, Speed, minimum/maximum
  physical attack, Critical, Critical Damage, Explosive Blow Damage, and
  minimum/maximum magic attack.
- Most random ability ranges are 1%–2%; Speed is listed as 1%–20%.
- Random buffs include Attack Power, Defense, Evasion, and Explosive Blow.
- Buff levels run from 1 to 10. Higher levels improve activation chance.
- Explosive Blow shows a 180-second duration and 300-second cooldown in the
  guide.

The page also describes “change option” creation: an existing Monster Pet can
be used to reroll or change options, with a Premium Pet Talisman for Premium
Pets. Exact item indices, costs, replacement rules, and random distributions
are not fully represented in the current OpenKal configuration and need an
authoritative source before implementation.

The guide states that a Monster Pet can be equipped at the same time as
another pet. This differs from the current OpenKal pet system,
which enforces one equipped pet. A compatible implementation should
use separate normal-pet and monster-pet equipment slots or equivalent server
state.

## Integration and instance state

Pet mixing uses a separate validated action from weapon/armor `MixCatalog` and
`MixingItem` requests. Existing equipment modes carry weapon, extra-weapon,
armor and mixing-stone IIDs, not ordered pet inputs or option state.

The existing `PetDef` covers item index, duration, healing and pickup. Crossbreed
needs separate recipes and per-instance state for category (Zamogeon/Cash/Point),
Field/PvP mode, prior usage, main/additional abilities, fixed buff and any random
options. Monster Pets also need random ability values and buff levels. Persist
re-equip suppression if it is an authoritative cooldown rather than display-only.
A static item index cannot identify prior Crossbreed use.

## Server transactions

### Crossbreed transaction

1. Validate the player session and the two owned item instance IDs.
2. Verify that the items are distinct, valid pets, and eligible for the same
   Crossbreed rule.
3. Verify category restrictions, same-name restriction, and usage state.
4. Resolve the ordered `(main, sub)` recipe on the server.
5. Roll the ordinary result, alternative result, random Cash stat, and Rare
   Pet result using a server-side RNG.
6. Atomically consume both source item instances and create the result item
   with all rolled options.
7. Persist the result and send the authoritative inventory/equipment update.
8. Do not trust the client’s previewed result or displayed probability.

### Monster Pet Creation transaction

1. Validate the selected result recipe from `monster_recipes.json`.
2. Verify each material stack and the 100,000-gold balance.
3. Atomically remove the material counts and gold.
4. Create the selected monster-pet item with its option state, if option
   generation is enabled.
5. Persist and synchronize the resulting item.

Both operations should be idempotent with respect to request processing and
must not consume materials before all validation has passed.

## UI and protocol

Crossbreed needs a dedicated UI with:

- Main Pet slot.
- Sub Pet slot.
- Compatible-pet list filtered by the Main Pet.
- Authoritative result preview after the server resolves the recipe.
- Result alternatives where supported.
- Separate error states for incompatible category, duplicate name, used pet,
  missing material, and insufficient money.

Monster Pet Creation needs:

- A result list.
- A recipe/material display.
- Material counts and gold cost.
- A creation action.
- Later, an option-change mode with explicit reroll costs.

The existing weapon-mixing packet should not be overloaded with these fields.
Prefer a dedicated protocol request/response pair or a clearly versioned pet
action message. Define and test an explicit OpenKal payload contract before enabling remote use.

## Open questions

The following are not resolved by the available sources:

- Exact Crossbreed result item indices and item metadata.
- Exact ordinary result, alternative-result, and Rare Pet probabilities.
- Complete Cash Pet and Kal Point Pet recipe matrix.
- Whether “used more than one time” means one prior use or a larger usage cap.
- Exact option reroll costs and success behavior for Monster Pets.
- Whether Monster Pet random options are generated at creation, at first equip,
  or by a later action.
- Persistence format for pet options and Crossbreed history.
- The OpenKal request/response contract for both systems.
- Whether the five-minute buff suppression is a client display issue or an
  authoritative gameplay cooldown.

## References

- [Pet guide](https://gameagit.com/kalonline/Pet).
- [Crossbreed guide](https://gameagit.com/kalonline/Crossbreed).
- [Monster Pet guide](https://gameagit.com/kalonline/MonsterPet).
- [Current OpenKal pets](Pets.md).

Recipe and visual observations come from optional compatible configuration data.
Keep imported catalogs separate from OpenKal source and retain their provenance.
Public behavior descriptions do not provide a complete probability table or
implementation contract.

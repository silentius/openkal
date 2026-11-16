# Damage reduction

Physical mitigation softens high Defense and Absorb without changing the ordinary
subtraction band. A positive revised attack reaches at least 1 damage inside
`final_damage`; later immunity effects can still prevent HP loss. Archer perfect
evasion has a separate high-value cap. These are implemented rules with balance
still subject to playtesting.

The implementation is in `libs/server/src/Combat.cpp`; `tests/test_combat.cpp`
covers formulas, integer bounds and dispatch. Raw equipment/monster stats and
character-sheet values are unchanged.

## Physical pipeline

1. Roll attack. When either combatant is not a player, add/subtract the existing
   level-gap `revise_damage` value (table saturated at 99). Pure PvP skips it.
2. If revised attack is nonpositive, return 0.
3. Apply `mitigate_defense`, then `mitigate_absorb`.
4. Armor Breaker adds `damage / 20` only when post-absorb damage exceeds 100.
5. The +10% attack state adds `damage / 10` after Armor Breaker.
6. Floor the result at 1, then clamp to nonnegative signed 32-bit storage.

`final_defense` selects melee or ranged defense from attack type. PvP also adds
`target.level / kDenoDef[type][class]`, using melee divisors 4/4/5 and ranged
1/5/3 for Knight/Mage/Archer. `final_absorb` includes the existing PvP bonus of 15.
These helpers still return raw totals; mitigation converts them to damage.

All divisions below truncate. Mitigation and both bonuses remain signed 64-bit
until the final storage clamp. No floating-point rounding is used.

## Defense

Let `A` be revised attack and `D = max(0, final_defense)`:

```text
A <= 0:         0
5*D <= 3*A:     A - D
otherwise:     floor(4*A*A / (5*(5*D - A)))
```

The curve joins subtraction at Defense equal to 60% of attack, matching its
value and slope. At the join, 40% of attack remains. Equal attack/defense leaves
20% before absorb and truncation. This is not a fixed percentage reduction per
Defense point; it depends on incoming attack.

For attack 100, post-defense values are:

- D 0/25/50/60 → 100/75/50/40.
- D 61/70/80/90 → 39/32/26/22.
- D 100/200/500/1000/5000 → 20/8/3/1/0.

The last zero becomes 1 only at the final floor, after absorb and modifiers.
Attack 20 against Defense 100 likewise truncates to zero before the final floor.

To avoid overflow, evaluate the branch as `A >= (5*D + 2) / 3`, widening D first.
In the tail A is below `5*INT32_MAX/3`, so its square fits unsigned 64-bit.
For unsigned `square = A*A` and `denominator = 5*(5*D - A)`, evaluate exactly:

```text
4*(square / denominator) + 4*(square % denominator) / denominator
```

The bounded remainder makes the product safe. Returning 1 as an overflow
fallback would violate monotonicity and is not part of the formula.

## Absorb

Let `M` be post-defense damage and `R = max(0, final_absorb)`. Absorb is not
clamped at 100:

```text
R <= 40:       M - floor(M*R / 100)
R > 40:        floor(36*M / (R + 20))
```

The high-value tail joins the 40-point band at 60% remaining damage with the
same continuous slope. Integer truncation makes the `<= 40` branch authoritative;
using the tail at exactly 40 can differ by one.

Overflow-safe evaluation preserves the remainder until the final division:

```text
R <= 40: M - (M/100)*R - (M%100)*R/100
R > 40:  36*(M/(R+20)) + 36*(M%(R+20))/(R+20)
```

Widen `R + 20` before adding. Helpers return signed 64-bit without a storage
clamp. For 100 post-defense damage:

- R 0/15/40 → 100/85/60.
- R 41/50/60 → 59/51/45.
- R 80/100/200/1000 → 36/30/16/3.

Attack 100, Defense 100 and Absorb 15 therefore becomes 20, then 17.
Raw Absorb 100 means 70% effective absorb reduction, not immunity. In PvP a
character's raw 100 becomes total 115, about 73.33% before integer rounding.
The current sheet displays raw values; a future tooltip should explain the
rating and PvP bonus rather than label it as exact effective reduction.

## Perfect evasion

Archer perfect evasion runs before block and ordinary `check_hit`. The base raw
percent is `skill_level + 7` (skill index 8); advanced uses its skill level
(index 20). `perfect_evasion_roll(base, advanced)` clamps negatives to zero:

- When both are at most 40, preserve the two independent legacy rolls exactly.
- Otherwise convert each raw value P: retain P through 40; above 40 use
  `100 - 3600 / (P + 20)`, widening before addition.
- Combine as `p1 + p2 - p1*p2/100`, cap at 75, then roll once. A miss requires
  a positive combined percent and `irand(1,100) <= combined`.

There is no additional per-roll cap. Checked vectors:

- 17/10 and 40/33 retain legacy two-roll dispatch.
- 41/34 converts to 41/34 and combines to 62.
- 100/93 converts to 70/69, combines to 91 and caps at 75.
- 262/255 converts to 88/87 and caps the combined result at 75.

Ordinary Eva remains independent: `add_hit + level_hit_revision + hit - dodge`
is clamped to [-41,41]. `hit_rate_pct` starts at 50, rises by two every two
steps to 90 at index 40 and reaches 95 from index 41. At -41 the ordinary
hit chance is 5%; additional dodge cannot reduce it further.

## Preserved behavior

For Defense at most 60% of revised attack and total Absorb at most 40, damage
is bit-identical to the previous subtraction path. Regression anchors include:

- Attack 20, Defense 5, Absorb 0 → 15.
- Level-20 Knight melee PvP, attack 100 and base Defense 5 → total Defense 10,
  total Absorb 15 and damage 77.
- A same-level monster attacking that player → Defense 5, Absorb 0 and damage 95.
- Armor Breaker leaves attack 100 at 100 and increases attack 120 to 126
  when no other mitigation applies.

Magic does not use these Defense/Absorb/hit formulas. Block, Knight Perfect
Defense, Mage Blessing, full elemental resistance and later godmode retain their
existing immunity behavior. The 1-HP floor guarantees neither a successful hit
nor HP loss through every later effect.

## Balance and verification

The initial joins (60% Defense, 40 Absorb) and 75% perfect-evasion cap are fixed
playtest settings, not proven balance constants. At 75% perfect evasion and 5%
ordinary hit chance, only 1.25% of attempts connect before block. A 1-HP hit can
still be erased by small amounts of healing. Attack 100 / Defense 100 / Absorb
100 deals 6 on a connected hit, or 0.075 HP per attempt under those avoidance
assumptions. These are formula examples, not measured character builds.

Compare representative builds inside the preserved band, around both joins,
and at high combined avoidance/mitigation. Record revised attack, total Defense
and Absorb, both evasion contributions, hit/dodge/block, HP, healing and attack
interval. Include PvP's +15 Absorb and test temporary immunities separately.
Measure connection rate, damage per second and survival time before changing
constants, healing, gear or the damage floor.

`combat` tests cover boundary samples, negative/zero inputs, monotonicity,
exact preservation sweeps, large integer inputs, bonus ordering/saturation and
seeded one-roll/two-roll evasion. Skill and Collection tests cover dependent
combat paths. Use the [README headless build](../README.md#build-and-test-without-external-data)
to run the relevant tests:

```sh
ctest --test-dir build-null \
  -R '^(combat|skill_buff|player_skill|player_regen|collection_equipment)$' \
  --output-on-failure
```

The curve shape follows public diminishing-return descriptions for
[League armor](https://wiki.leagueoflegends.com/en-us/Armor),
[Dota armor](https://liquipedia.net/dota2/Armor),
[Warframe armor](https://wiki.warframe.com/w/Armor) and
[Diablo armor](https://www.diablowiki.net/Armor). Their constants are not used:
OpenKal Defense shares attack units, and its curve preserves the existing band.
Implementation and fixtures are independently authored from OpenKal requirements.

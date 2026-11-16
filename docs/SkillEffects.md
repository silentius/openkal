# Skill effect assets and presentation

OpenKal implements its rendering paths and has refined them through iterative
implementation, visual testing and repeated adjustments. This reference describes
what the current client dispatches.

The class/skill IDs below are stable lookup keys. Effect labels identify the
client presentation; runtime JSON can supply different display names. A skill's
preparation, fire, projectile impact and persistent state are separate phases.
A listed mapping does not by itself verify a supplied asset pack or the rendered
appearance of every stage.

## Asset lookup and lifecycle

- `Effect/Fx` and `Effect/Sprite` names are **entries**, not necessarily individual
  files. `SpriteFx::index_scripts` reads `.txt` containers and decoded `.dat`
  containers from those directories. Plain-text entries take precedence. The
  physical container filename and its particle/model/sound stages depend on the
  separately supplied data pack; this source-only reference does not invent them.
- GB clip stems resolve under `Skill/` first and `Effect/` second, with the client
  asset resolver handling supported path variants. GB materials select their
  own textures, and animation keyframes can add particles, models and sounds.
- FX entries supply stage offsets, attachment bones, model delays/durations and
  sound names. The Knight/Mage dispatcher schedules referenced GB stages; the
  Archer dispatcher handles its sprite/particle dispatches and delayed impacts.
- `Caster` uses the caster's effect-bone/tip position; `CasterOrigin` uses its
  feet. `Target` uses the impact/spine position; `TargetOrigin` uses target feet.
  Entry-specific stage bones/offsets can further adjust these points.
- A cast-loop entry remains attached until the cast phase is cleared. A one-shot
  entry uses its stages' own lifetimes. Perfect Defense and Arrow Explosion have
  separate state-driven lifetimes described below.
- JSON `sound[phase]` adds one-shot audio. `_L` cast loops are managed by the
  session's cast-audio lifetime. JSON `effect[0]` starting with `Skill_` loads a
  preparation GB. These per-definition values are runtime inputs, not a fixed
  file catalog bundled here.

## Per-skill phase mappings

Each entry lists preparation and fire effects. Primary and secondary labels are
retained below even when equal; the presenter avoids dispatching equal labels
twice. Follow a label to its exact FX/particle entries, GB files, textures,
attachments and known limitations in the effect reference.

### Monster (lookup job -2)

#### Monster skill 29 — Lightning

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [Lightning](#lightning).

#### Monster skill 30 — Spark

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [Spark](#spark).

#### Monster skill 31 — FireBolt

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [FireBolt](#firebolt).

#### Monster skill 32 — LightningImpact

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [LightningImpact](#lightningimpact).

#### Monster skill 33 — Fireball

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [Fireball](#fireball).

#### Monster skill 34 — Explosion

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [Explosion](#explosion).

### Knight (lookup job 0)

#### Knight skill 4 — Taunt

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [Taunt](#taunt).

#### Knight skill 8 — Taunt

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [Taunt](#taunt).

#### Knight skill 10 — WildAccuracy

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [WildAccuracy](#wildaccuracy).

#### Knight skill 16 — UpperSmash

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [UpperSmash](#uppersmash).

#### Knight skill 17 — InfernalAttack

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [InfernalAttack](#infernalattack).

#### Knight skill 18 — RainforceDefence

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [RainforceDefence](#rainforcedefence).

#### Knight skill 19 — ChantOfProtect

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [ChantOfProtect](#chantofprotect).

#### Knight skill 21 — DashPrep

- Preparation: [DashPrep](#dashprep).
- Fire: no table row; optional JSON/clip presentation may still apply.

#### Knight skill 22 — ShieldAttack

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [ShieldAttack](#shieldattack).

#### Knight skill 23 — ChantOfHealing

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [ChantOfHealing](#chantofhealing).

#### Knight skill 24 — Berserk

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [Berserk](#berserk).

#### Knight skill 25 — FullSwing

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [FullSwing](#fullswing).

#### Knight skill 26 — Sacrifice

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [Sacrifice](#sacrifice).

#### Knight skill 27 — ChantOfLightning

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [ChantOfLightning](#chantoflightning).

#### Knight skill 28 — ChantOfIce

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [ChantOfIce](#chantofice).

#### Knight skill 29 — ChantOfFlame

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [ChantOfFlame](#chantofflame).

#### Knight skill 30 — ChangeToVitality

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [ChangeToVitality](#changetovitality).

#### Knight skill 31 — ChantOfAccuracy

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [ChantOfAccuracy](#chantofaccuracy).

#### Knight skill 32 — ChantOfBattle

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [ChantOfBattle](#chantofbattle).

#### Knight skill 33 — ChangeToVitality

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [ChangeToVitality](#changetovitality).

#### Knight skill 34 — ChangeToVitalityPrep, ShieldOfResilience

- Preparation: [ChangeToVitalityPrep](#changetovitalityprep).
- Fire: [ShieldOfResilience](#shieldofresilience).

#### Knight skill 35 — ReincarnatePrep, Reincarnate

- Preparation: [ReincarnatePrep](#reincarnateprep).
- Fire: [Reincarnate](#reincarnate).

#### Knight skill 38 — SpinSlash

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [SpinSlash](#spinslash).

#### Knight skill 40 — PerfectDefense

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [PerfectDefense](#perfectdefense).

#### Knight skill 42 — ProvocationOfBlow

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [ProvocationOfBlow](#provocationofblow).

### Mage (lookup job 1)

#### Mage skill 1 — BeheadingCharm

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [BeheadingCharm](#beheadingcharm).

#### Mage skill 2 — FireBolt

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [FireBolt](#firebolt).

#### Mage skill 3 — IceBolt

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [IceBolt](#icebolt).

#### Mage skill 4 — Lightning

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [Lightning](#lightning).

#### Mage skill 8 — Healing

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [Healing](#healing).

#### Mage skill 9 — LightningImpactPrep, LightningImpact

- Preparation: [LightningImpactPrep](#lightningimpactprep).
- Fire: [LightningImpact](#lightningimpact).

#### Mage skill 10 — HealingPrep, Healing

- Preparation: [HealingPrep](#healingprep).
- Fire: [Healing](#healing).

#### Mage skill 15 — Meditation

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [Meditation](#meditation).

#### Mage skill 16 — ResurrectionPrep, Resurrection

- Preparation: [ResurrectionPrep](#resurrectionprep).
- Fire: [Resurrection](#resurrection).

#### Mage skill 17 — StoneshieldPrep, Stoneshield

- Preparation: [StoneshieldPrep](#stoneshieldprep).
- Fire: [Stoneshield](#stoneshield).

#### Mage skill 18 — StoneshieldPrep, Stoneshield

- Preparation: [StoneshieldPrep](#stoneshieldprep).
- Fire: [Stoneshield](#stoneshield).

#### Mage skill 19 — StoneshieldPrep, Stoneshield

- Preparation: [StoneshieldPrep](#stoneshieldprep).
- Fire: [Stoneshield](#stoneshield).

#### Mage skill 20 — StoneshieldPrep, Stoneshield

- Preparation: [StoneshieldPrep](#stoneshieldprep).
- Fire: [Stoneshield](#stoneshield).

#### Mage skill 21 — StoneshieldPrep, Stoneshield

- Preparation: [StoneshieldPrep](#stoneshieldprep).
- Fire: [Stoneshield](#stoneshield).

#### Mage skill 22 — HealingPrep, Healing

- Preparation: [HealingPrep](#healingprep).
- Fire: [Healing](#healing).

#### Mage skill 23 — Spark

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [Spark](#spark).

#### Mage skill 24 — IceNova

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [IceNova](#icenova).

#### Mage skill 25 — Fireball

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [Fireball](#fireball) + [Fireball](#fireball).

#### Mage skill 26 — Haste

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [Haste](#haste).

#### Mage skill 27 — FrostBlast

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [FrostBlast](#frostblast).

#### Mage skill 28 — LightRemedy

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [LightRemedy](#lightremedy).

#### Mage skill 29 — LightReflection

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [LightReflection](#lightreflection).

#### Mage skill 31 — CallLightningPrep, CallLightning

- Preparation: [CallLightningPrep](#calllightningprep).
- Fire: [CallLightning](#calllightning).

#### Mage skill 32 — IceLancePrep, IceLance

- Preparation: [IceLancePrep](#icelanceprep).
- Fire: [IceLance](#icelance).

#### Mage skill 33 — ExplosionPrep, Explosion

- Preparation: [ExplosionPrep](#explosionprep).
- Fire: [Explosion](#explosion).

#### Mage skill 34 — ResurrectionPrep, Resurrection

- Preparation: [ResurrectionPrep](#resurrectionprep).
- Fire: [Resurrection](#resurrection).

#### Mage skill 35 — HealingPrep, Healing

- Preparation: [HealingPrep](#healingprep).
- Fire: [Healing](#healing).

#### Mage skill 36 — MesmerizationPrep, Mesmerization

- Preparation: [MesmerizationPrep](#mesmerizationprep).
- Fire: [Mesmerization](#mesmerization).

#### Mage skill 37 — SnowFieldPrep, SnowField

- Preparation: [SnowFieldPrep](#snowfieldprep).
- Fire: [SnowField](#snowfield).

#### Mage skill 38 — EnchantWeaponPrep, EnchantWeapon

- Preparation: [EnchantWeaponPrep](#enchantweaponprep).
- Fire: [EnchantWeapon](#enchantweapon).

#### Mage skill 39 — AuraOfShieldPrep, AuraOfShield

- Preparation: [AuraOfShieldPrep](#auraofshieldprep).
- Fire: [AuraOfShield](#auraofshield).

#### Mage skill 40 — SpiritBlastPrep, SpiritBlast

- Preparation: [SpiritBlastPrep](#spiritblastprep).
- Fire: [SpiritBlast](#spiritblast).

#### Mage skill 41 — ChainLightningPrep, Lightning

- Preparation: [ChainLightningPrep](#chainlightningprep).
- Fire: [Lightning](#lightning) + [Lightning](#lightning).

#### Mage skill 42 — CallLightningPrep, Thunder

- Preparation: [CallLightningPrep](#calllightningprep).
- Fire: [Thunder](#thunder).

#### Mage skill 43 — ThunderStormPrep, ThunderStorm

- Preparation: [ThunderStormPrep](#thunderstormprep).
- Fire: [ThunderStorm](#thunderstorm).
- Field presentation is driven by the spawned magic-monster clip rather than the fire row; see the range/channel exceptions below.

#### Mage skill 44 — IceRequeimPrep, IceRequeim

- Preparation: [IceRequeimPrep](#icerequeimprep).
- Fire: [IceRequeim](#icerequeim).

#### Mage skill 45 — IceStormPrep

- Preparation: [IceStormPrep](#icestormprep).
- Fire: no table row; optional JSON/clip presentation may still apply.
- Field presentation is driven by the spawned magic-monster clip rather than the fire row; see the range/channel exceptions below.

#### Mage skill 46 — ExplosionPrep, Explosion

- Preparation: [ExplosionPrep](#explosionprep).
- Fire: [Explosion](#explosion).

#### Mage skill 47 — ExplosiveBurstPrep, ExplosiveBurst

- Preparation: [ExplosiveBurstPrep](#explosiveburstprep).
- Fire: [ExplosiveBurst](#explosiveburst) + [ExplosiveBurst](#explosiveburst).

#### Mage skill 48 — FireRainPrep

- Preparation: [FireRainPrep](#firerainprep).
- Fire: no table row; optional JSON/clip presentation may still apply.
- Field presentation is driven by the spawned magic-monster clip rather than the fire row; see the range/channel exceptions below.

#### Mage skill 49 — AuraOfShieldPrep, BlessedStrength

- Preparation: [AuraOfShieldPrep](#auraofshieldprep).
- Fire: [BlessedStrength](#blessedstrength).

#### Mage skill 50 — AuraOfShieldPrep, BlessedHealth

- Preparation: [AuraOfShieldPrep](#auraofshieldprep).
- Fire: [BlessedHealth](#blessedhealth).

#### Mage skill 51 — AuraOfShieldPrep, BlessedDexterity

- Preparation: [AuraOfShieldPrep](#auraofshieldprep).
- Fire: [BlessedDexterity](#blesseddexterity).

#### Mage skill 52 — AuraOfShieldPrep, BlessedIntelligence

- Preparation: [AuraOfShieldPrep](#auraofshieldprep).
- Fire: [BlessedIntelligence](#blessedintelligence).

#### Mage skill 53 — AuraOfShieldPrep, AngelicDexterity

- Preparation: [AuraOfShieldPrep](#auraofshieldprep).
- Fire: [AngelicDexterity](#angelicdexterity).

#### Mage skill 54 — PurgePrep, Purge

- Preparation: [PurgePrep](#purgeprep).
- Fire: [Purge](#purge).

#### Mage skill 55 — Healing

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [Healing](#healing).

#### Mage skill 56 — ReinforceSight

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [ReinforceSight](#reinforcesight).

#### Mage skill 57 — Healing

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [Healing](#healing) + [Healing](#healing).

#### Mage skill 58 — Healing

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [Healing](#healing) + [Healing](#healing).

#### Mage skill 59 — Healing

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [Healing](#healing).

#### Mage skill 61 — AmnesiaPrep, Amnesia

- Preparation: [AmnesiaPrep](#amnesiaprep).
- Fire: [Amnesia](#amnesia).

#### Mage skill 63 — StoneshieldPrep, Stoneshield

- Preparation: [StoneshieldPrep](#stoneshieldprep).
- Fire: [Stoneshield](#stoneshield).

### Archer (lookup job 2)

#### Archer skill 4 — NegativeArrow

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [NegativeArrow](#negativearrow).

#### Archer skill 5 — ImmolationArrow

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [ImmolationArrow](#immolationarrow).

#### Archer skill 6 — FireArrow

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [FireArrow](#firearrow).

#### Archer skill 7 — CriticalChance

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [CriticalChance](#criticalchance).

#### Archer skill 9 — StunningArrow

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [StunningArrow](#stunningarrow).

#### Archer skill 10 — WildStrength

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [WildStrength](#wildstrength).

#### Archer skill 13 — PoisonArrow

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [PoisonArrow](#poisonarrow).

#### Archer skill 14 — ExplosionArrow

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [ExplosionArrow](#explosionarrow).

#### Archer skill 16 — ChargedArrowPrep, ChargedArrow

- Preparation: [ChargedArrowPrep](#chargedarrowprep).
- Fire: [ChargedArrow](#chargedarrow).

#### Archer skill 18 — VitalityDrain

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [VitalityDrain](#vitalitydrain).

#### Archer skill 19 — InflectSufferingPrep, InflectSuffering

- Preparation: [InflectSufferingPrep](#inflectsufferingprep).
- Fire: [InflectSuffering](#inflectsuffering).

#### Archer skill 22 — MeshOfSpeed

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [MeshOfSpeed](#meshofspeed).

#### Archer skill 23 — DarknessVisionPrep, DarknessVision

- Preparation: [DarknessVisionPrep](#darknessvisionprep).
- Fire: [DarknessVision](#darknessvision).

#### Archer skill 24 — ResistLightning

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [ResistLightning](#resistlightning).

#### Archer skill 25 — ResistIce

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [ResistIce](#resistice).

#### Archer skill 26 — ResistFire

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [ResistFire](#resistfire).

#### Archer skill 27 — SilentShotPrep, SilentShot

- Preparation: [SilentShotPrep](#silentshotprep).
- Fire: [SilentShot](#silentshot).

#### Archer skill 28 — ResistCurse

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [ResistCurse](#resistcurse).

#### Archer skill 29 — ResistSpirit

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [ResistSpirit](#resistspirit).

#### Archer skill 31 — EmergencyEscape

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [EmergencyEscape](#emergencyescape).

#### Archer skill 33 — CloudSight

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [CloudSight](#cloudsight).

#### Archer skill 34 — ManaBurnPrep, ManaBurn

- Preparation: [ManaBurnPrep](#manaburnprep).
- Fire: [ManaBurn](#manaburn).

#### Archer skill 35 — MeshOfChance

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [MeshOfChance](#meshofchance).

#### Archer skill 36 — MeshOfForce

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [MeshOfForce](#meshofforce).

#### Archer skill 37 — MeshOfHealth

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [MeshOfHealth](#meshofhealth).

#### Archer skill 38 — MeshOfAgility

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [MeshOfAgility](#meshofagility).

#### Archer skill 39 — ConstrictMind

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [ConstrictMind](#constrictmind).

#### Archer skill 40 — ConstrictWisdom

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [ConstrictWisdom](#constrictwisdom).

#### Archer skill 41 — VanishWall

- Preparation: [VanishWall](#vanishwall).
- Fire: [VanishWall](#vanishwall).

#### Archer skill 42 — PoisonCloudPrep

- Preparation: [PoisonCloudPrep](#poisoncloudprep).
- Fire: no table row; optional JSON/clip presentation may still apply.

#### Archer skill 43 — VanishConfidence

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [VanishConfidence](#vanishconfidence).

#### Archer skill 44 — DestroySightPrep, DestroySight

- Preparation: [DestroySightPrep](#destroysightprep).
- Fire: [DestroySight](#destroysight).

#### Archer skill 45 — BenefitAblationPrep, BenefitAblation

- Preparation: [BenefitAblationPrep](#benefitablationprep).
- Fire: [BenefitAblation](#benefitablation).

#### Archer skill 46 — EmergencyEscape

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [EmergencyEscape](#emergencyescape).

#### Archer skill 49 — ExploderArrow

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [ExploderArrow](#exploderarrow).

### Transformed caster (lookup job 255)

#### Transformed caster skill 101 — LightningRed

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [LightningRed](#lightningred).

#### Transformed caster skill 102 — GunchiIceTempest

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [GunchiIceTempest](#gunchiicetempest).

#### Transformed caster skill 104 — YungsangFatalBlow

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [YungsangFatalBlow](#yungsangfatalblow).

#### Transformed caster skill 105 — YungsangDemolition

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [YungsangDemolition](#yungsangdemolition).

#### Transformed caster skill 107 — TugukGroundSplinter

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [TugukGroundSplinter](#tugukgroundsplinter).

#### Transformed caster skill 108 — TugukShadowFang

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [TugukShadowFang](#tugukshadowfang).

#### Transformed caster skill 109 — TugukDiamondClaw

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [TugukDiamondClaw](#tugukdiamondclaw).

#### Transformed caster skill 110 — Gale1

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [Gale1](#gale1).

#### Transformed caster skill 111 — Gale2

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [Gale2](#gale2).

#### Transformed caster skill 112 — Gale3

- Preparation: no table row; optional JSON/clip presentation may still apply.
- Fire: [Gale3](#gale3).

### Range, channel and non-table paths

- **Archer 21 — Mysterious Arrow:** three [MysticArrow](#mysticarrow)
  projectiles are dispatched directly by `SkillPresenter::present` at fire.
- **Archer 47 — Arrow Rain:** targeting reuses FX entry `effect_ef092`.
  Channel animation `Tm_7_a_15` supplies `davi_ef132` / `davi_ef132_01` clip cues;
  their model/sprite stages are resolved from the runtime clip and containers.
- **Archer 42 — Poison Cloud:** targeting shows the procedural ground ring;
  `PoisonCloudPrep` supplies `effect_ef067`. The live field's clip supplies its
  subsequent cues. No fire-phase row is present in the table.
- **Mage 43, 45 and 48 — persistent storms:** targeting uses FX entries
  `effect_ef078`, `effect_ef086` and `effect_ef092`, respectively. Their spawned
  magic-monster idle clips carry `EFFECT_FX` cues for the live field. The
  presenter deliberately skips fire dispatch for persistent Mage areas.
- **Mage 47 — Explosive Burst:** preparation uses `effect_ef090`; fire uses
  `effect_ef091` at the selected ground point. It has no looping targeting FX.
- **Knight 40 — Perfect Defense:** activation uses `Tw_7_H_40_Floor`;
  `KnightFxDispatcher::set_perfect_defense` keeps `davi_ef120` on the owner for
  the active buff, including script sounds and referenced model stages.
- **Archer 49 — Arrow Explosion:** see [ExploderArrow](#exploderarrow) for the
  three-second fuse and target-bone burst.
- **Thief (job 3):** this lookup table has no rows. Runtime JSON and character
  animation cues remain separate; no undocumented fixed effect file is claimed.
- **Beheading and later skills:** callers can dispatch clip/procedural effects
  outside this table. An unmapped ID is not proof that a skill lacks animation,
  sound or effects; inspect its session/clip path and selected JSON definition.

## Effect identifiers, assets and endpoints

`FX` means a named entry under `Effect/Fx`; `particle` means a named entry under
`Effect/Sprite`. Texture names without a directory use the effect-texture loader.
These entries cover every `SkillEt` value, including identifiers whose dedicated
visual path is currently absent.

### None

- No additional effect is dispatched for this entry.

### FireBolt

Used by: Monster/31 fire, Mage/2 fire.

- Flies Skill/Skill_R_2_1.gb at 500 units/s; Sprite firebolt_projectile trail, firebolt_explosion and three firebolt_smallfire impact emitters. Sounds efs010 at launch and efs009 at impact.

### IceBolt

Used by: Mage/3 fire.

- Flies Skill/Skill_R_3_1.gb at 500 units/s; Sprite icebolt_projectile trail and icebolt_explosion impact. Sounds efs015 at launch and efs017 at impact.

### Lightning

Used by: Monster/29 fire, Mage/4 fire, Mage/41 fire.

- Procedural yellow ribbon using Lightning.GTX, lig01.GTX, star4.GTX and LightningSpark.GTX; caster mesh Skill/Skill_R_4_0.gb; sound efs021.

### Healing

Used by: Mage/8 fire, Mage/10 fire, Mage/22 fire, Mage/35 fire, Mage/55 fire, Mage/57 fire, Mage/58 fire, Mage/59 fire.

- Effect/Sprite entry `healing`: TargetOrigin, one-shot.
- Also draws a procedural target-following ring with Effect/healing.dds and plays efs022 at the target origin.

### HealingPrep

Used by: Mage/10 prep, Mage/22 prep, Mage/35 prep.

- Acknowledged without a dedicated emitter; character animation and optional JSON preparation models supply the visual.

### LightningImpact

Used by: Monster/32 fire, Mage/9 fire.

- Uses the yellow lightning ribbon, textures and caster mesh described for Lightning; sound efs021.

### LightningImpactPrep

Used by: Mage/9 prep.

- Uses the cast-model fallback Skill/Skill_M_3_0.gb at the caster tip, plus sound efs010; JSON may name an additional preparation model.

### Spark

Used by: Monster/30 fire, Mage/23 fire.

- Spawns two yellow Lightning ribbons with the same texture/caster-mesh family and sound efs021.

### IceNova

Used by: Mage/24 fire.

- Procedural expanding ring using Effect/whiteres.dds at the target origin; sound efs017.

### Fireball

Used by: Monster/33 fire, Mage/25 fire.

- Flies Skill/Skill_R_2_1.gb at 500 units/s; Sprite firebolt_projectile trail, firebolt_explosion and three firebolt_smallfire impact emitters. Sounds efs010 at launch and efs009 at impact.

### FrostBlast

Used by: Mage/27 fire.

- Flies Skill/Skill_R_3_1.gb at 500 units/s; Sprite icebolt_projectile trail and icebolt_explosion impact. Sounds efs015 at launch and efs017 at impact.

### Taunt

Used by: Knight/4 fire, Knight/8 fire.

- Effect/Sprite entry `taunt`: Target, one-shot.

### UpperSmash

Used by: Knight/16 fire.

- Effect/Fx entry `effect_ef019`: Caster, one-shot.

### InfernalAttack

Used by: Knight/17 fire.

- Effect/Fx entry `effect_ef021`: Caster, one-shot.
- Effect/Fx entry `effect_strike01`: Target, one-shot.

### ShieldAttack

Used by: Knight/22 fire.

- Effect/Fx entry `effect_ef026`: Caster, one-shot.

### FullSwing

Used by: Knight/25 fire.

- Effect/Fx entry `effect_ef039`: Caster, one-shot.

### Berserk

Used by: Knight/24 fire.

- Effect/Fx entry `effect_ef023`: Caster, one-shot.

### DashPrep

Used by: Knight/21 prep.

- Effect/Fx entry `effect_ef020`: Caster, retained until cast end.

### ChargedArrow

Used by: Archer/16 fire.

- Effect/Fx entry `effect_ef041`: Caster, one-shot.
- Visible projectile uses Item/Weapon/M_arrow.gb and the Effect/ghost_grey.dds trail; the client builds a fallback shaft when the mesh is absent. At impact the zero-extra projectile plays efc007 and resolves effect_attack02 (or effect_attack02_c for a critical result) through Skill/ then Effect/ GB lookup.

### ChargedArrowPrep

Used by: Archer/16 prep.

- Effect/Fx entry `effect_ef040`: Caster, retained until cast end.
- Effect/Fx entry `effect_ef040a`: Caster, retained until cast end.
- Effect/Fx entry `effect_ef040b`: Caster, one-shot.

### MysticArrow

- Visible projectile uses Item/Weapon/M_arrow.gb and the Effect/ghost_grey.dds trail; the client builds a fallback shaft when the mesh is absent. Mysterious Arrow (Archer 21) launches three projectiles with distinct parabolic phases. At impact the zero-extra projectile plays efc007 and resolves effect_attack02 (or effect_attack02_c for a critical result) through Skill/ then Effect/ GB lookup.

### FireArrow

Used by: Archer/6 fire.

- Visible projectile uses Item/Weapon/M_arrow.gb and the Effect/ghost_grey.dds trail; the client builds a fallback shaft when the mesh is absent. At impact the zero-extra projectile plays efc007 and resolves effect_attack02 (or effect_attack02_c for a critical result) through Skill/ then Effect/ GB lookup.

### PoisonArrow

Used by: Archer/13 fire.

- Visible projectile uses Item/Weapon/M_arrow.gb and the Effect/ghost_grey.dds trail; the client builds a fallback shaft when the mesh is absent. At impact the zero-extra projectile plays efc007 and resolves effect_attack02 (or effect_attack02_c for a critical result) through Skill/ then Effect/ GB lookup.

### ExplosionArrow

Used by: Archer/14 fire.

- Effect/Sprite entry `explosion`: Target, one-shot.
- Visible projectile uses Item/Weapon/M_arrow.gb and the Effect/ghost_grey.dds trail; the client builds a fallback shaft when the mesh is absent. Target dispatch is delayed by distance / 500 units/s. Plays efs009 and efc007 at impact.

### ExploderArrow

Used by: Archer/49 fire.

- Effect/Fx entry `davi_ef131_04`: Target, retained for the three-second fuse.
- Plants FX davi_ef131_04 on the target; after three seconds detaches the fuse and spawns Sprite davi_ef131_05 on target bone 1, using sounds read from the FX entry of the same name. Visible projectile uses Item/Weapon/M_arrow.gb and the Effect/ghost_grey.dds trail; the client builds a fallback shaft when the mesh is absent. At impact the zero-extra projectile plays efc007 and resolves effect_attack02 (or effect_attack02_c for a critical result) through Skill/ then Effect/ GB lookup.

### StunningArrow

Used by: Archer/9 fire.

- Visible projectile uses Item/Weapon/M_arrow.gb and the Effect/ghost_grey.dds trail; the client builds a fallback shaft when the mesh is absent. At impact the zero-extra projectile plays efc007 and resolves effect_attack02 (or effect_attack02_c for a critical result) through Skill/ then Effect/ GB lookup.

### NegativeArrow

Used by: Archer/4 fire.

- Visible projectile uses Item/Weapon/M_arrow.gb and the Effect/ghost_grey.dds trail; the client builds a fallback shaft when the mesh is absent. At impact the zero-extra projectile plays efc007 and resolves effect_attack02 (or effect_attack02_c for a critical result) through Skill/ then Effect/ GB lookup.

### BeheadingCharm

Used by: Mage/1 fire.

- Plays Effect/effect_behead.gb at the target for 1.2 seconds.

### Stoneshield

Used by: Mage/17 fire, Mage/18 fire, Mage/19 fire, Mage/20 fire, Mage/21 fire, Mage/63 fire.

- Acknowledged without a dedicated fire-phase mesh or emitter; the preparation supplies the visual.

### StoneshieldPrep

Used by: Mage/17 prep, Mage/18 prep, Mage/19 prep, Mage/20 prep, Mage/21 prep, Mage/63 prep.

- Procedural orbiting glow around the caster spine using the lig01.GTX effect texture.

### Haste

Used by: Mage/26 fire.

- Plays the haste GB clip once at the target's feet. The client resolves `Skill/haste.gb` first and then `Effect/haste.gb`.
- The clip uses the alpha-blended `haste` texture. Speed Up plays without skill sound, including with catalogs that list sounds for it.

### Meditation

Used by: Mage/15 fire.

- Two procedural orbiting glow polylines around the caster spine, using Effect/GenericGlow.dds for ten seconds.

### Resurrection

Used by: Mage/16 fire, Mage/34 fire.

- No dedicated visual dispatch in SkillPresenter; currently reaches the generic efs010 sound fallback. Optional JSON preparation models and character-clip cues are separate.

### ResurrectionPrep

Used by: Mage/16 prep, Mage/34 prep.

- No dedicated visual dispatch in SkillPresenter; currently reaches the generic efs010 sound fallback. Optional JSON preparation models and character-clip cues are separate.

### WildAccuracy

Used by: Knight/10 fire.

- Effect/Sprite entry `resurrection_p`: Target, one-shot.

### RainforceDefence

Used by: Knight/18 fire.

- Effect/Fx entry `effect_ef024`: TargetOrigin, one-shot.

### ChantOfProtect

Used by: Knight/19 fire.

- Effect/Fx entry `effect_ef025`: CasterOrigin, one-shot.

### PerfectDefense

Used by: Knight/40 fire.

- Effect/Fx entry `Tw_7_H_40_Floor`: CasterOrigin, one-shot.
- Activation is separate from the buff-duration FX entry davi_ef120, which follows the owner until deactivation and starts/stops its script sounds and model stages with the buff.

### ProvocationOfBlow

Used by: Knight/42 fire.

- Effect/Fx entry `Tw_7_A_44`: CasterOrigin, one-shot.

### SpinSlash

Used by: Knight/38 fire.

- Effect/Fx entry `Tw_7_A_38`: CasterOrigin, one-shot.

### ChantOfHealing

Used by: Knight/23 fire.

- Effect/Fx entry `effect_ef033`: Target, one-shot.

### Sacrifice

Used by: Knight/26 fire.

- Effect/Fx entry `effect_ef027`: Caster, one-shot.
- Effect/Fx entry `effect_ef027a`: Target, one-shot.

### ChantOfLightning

Used by: Knight/27 fire.

- Effect/Fx entry `effect_ef028`: CasterOrigin, one-shot.

### ChantOfIce

Used by: Knight/28 fire.

- Effect/Fx entry `effect_ef029`: CasterOrigin, one-shot.

### ChantOfFlame

Used by: Knight/29 fire.

- Effect/Fx entry `effect_ef030`: CasterOrigin, one-shot.

### ChangeToVitality

Used by: Knight/30 fire, Knight/33 fire.

- Effect/Fx entry `effect_ef035`: Caster, one-shot.

### ChangeToVitalityPrep

Used by: Knight/34 prep.

- Effect/Fx entry `effect_ef034`: Caster, retained until cast end.

### ChantOfAccuracy

Used by: Knight/31 fire.

- Effect/Fx entry `effect_ef031`: CasterOrigin, one-shot.

### ChantOfBattle

Used by: Knight/32 fire.

- Effect/Fx entry `effect_ef032`: CasterOrigin, one-shot.

### ShieldOfResilience

Used by: Knight/34 fire.

- Effect/Fx entry `effect_ef036`: Caster, one-shot.

### Reincarnate

Used by: Knight/35 fire.

- Effect/Fx entry `effect_ef038`: Caster, one-shot.

### ReincarnatePrep

Used by: Knight/35 prep.

- Effect/Fx entry `effect_ef037`: Caster, retained until cast end.

### CallLightning

Used by: Mage/31 fire.

- Effect/Fx entry `effect_ef075`: Target, one-shot.
- Effect/Fx entry `effect_ef075a`: Target, one-shot.
- Effect/Fx entry `effect_ef075b`: Target, one-shot.
- Effect/Fx entry `effect_ef075c`: Target, one-shot.

### CallLightningPrep

Used by: Mage/31 prep, Mage/42 prep.

- Effect/Fx entry `effect_ef074`: Caster, retained until cast end.
- Effect/Fx entry `effect_ef074_1`: Caster, retained until cast end.

### IceLance

Used by: Mage/32 fire.

- Effect/Fx entry `effect_ef081a_ice1`: TargetOrigin, one-shot.
- Effect/Fx entry `effect_ef081a_ice2`: TargetOrigin, one-shot.
- Effect/Fx entry `effect_ef081a_ice3`: TargetOrigin, one-shot.
- Effect/Fx entry `effect_ef081a_ice4`: TargetOrigin, one-shot.

### IceLancePrep

Used by: Mage/32 prep.

- Effect/Fx entry `effect_ef080`: Caster, retained until cast end.
- Effect/Fx entry `effect_ef080_1`: Caster, retained until cast end.

### Explosion

Used by: Monster/34 fire, Mage/33 fire, Mage/46 fire.

- Effect/Fx entry `effect_ef089a`: TargetOrigin, one-shot.
- Effect/Fx entry `effect_ef089b`: TargetOrigin, one-shot.
- Effect/Fx entry `effect_ef089c`: TargetOrigin, one-shot.

### ExplosionPrep

Used by: Mage/33 prep, Mage/46 prep.

- Effect/Fx entry `effect_ef088`: Caster, retained until cast end.
- Effect/Fx entry `effect_ef088_1`: Caster, retained until cast end.

### Mesmerization

Used by: Mage/36 fire.

- Effect/Fx entry `effect_ef114`: Target, one-shot.

### MesmerizationPrep

Used by: Mage/36 prep.

- Effect/Fx entry `effect_ef113`: Caster, retained until cast end.

### SnowField

Used by: Mage/37 fire.

- Effect/Fx entry `effect_ef083`: Target, one-shot.

### SnowFieldPrep

Used by: Mage/37 prep.

- Effect/Fx entry `effect_ef082`: Caster, retained until cast end.

### EnchantWeapon

Used by: Mage/38 fire.

- Effect/Fx entry `effect_ef095`: Target, one-shot.
- Effect/Fx entry `effect_ef095a`: Target, one-shot.

### EnchantWeaponPrep

Used by: Mage/38 prep.

- Effect/Fx entry `effect_ef094`: Caster, retained until cast end.

### AuraOfShield

Used by: Mage/39 fire.

- Effect/Fx entry `effect_ef099`: Target, one-shot.
- Effect/Fx entry `effect_ef099a`: Target, one-shot.

### AuraOfShieldPrep

Used by: Mage/39 prep, Mage/49 prep, Mage/50 prep, Mage/51 prep, Mage/52 prep, Mage/53 prep.

- Effect/Fx entry `effect_ef098`: Caster, retained until cast end.

### SpiritBlast

Used by: Mage/40 fire.

- Procedural ribbon using Lightning02.GTX, Sprite Player_EF_code_EF106 at the caster and lightning_impact at the target; sound efs021.

### SpiritBlastPrep

Used by: Mage/40 prep.

- Effect/Fx entry `effect_ef115`: Caster, retained until cast end.

### ChainLightningPrep

Used by: Mage/41 prep.

- Effect/Fx entry `effect_ef077`: Caster, retained until cast end.

### Thunder

Used by: Mage/42 fire.

- Effect/Fx entry `effect_ef077a`: Target, one-shot.

### ThunderStormPrep

Used by: Mage/43 prep.

- Effect/Fx entry `effect_ef078`: Caster, retained until cast end.

### ThunderStorm

Used by: Mage/43 fire.

- Effect/Fx entry `effect_ef079`: Target, one-shot.

### IceRequeim

Used by: Mage/44 fire.

- Effect/Fx entry `effect_ef085`: Caster, one-shot.
- Effect/Fx entry `effect_ef085a`: Target, one-shot.

### IceRequeimPrep

Used by: Mage/44 prep.

- Effect/Fx entry `effect_ef084`: Caster, retained until cast end.

### IceStormPrep

Used by: Mage/45 prep.

- Effect/Fx entry `effect_ef086`: Caster, retained until cast end.

### ExplosiveBurst

Used by: Mage/47 fire.

- Effect/Fx entry `effect_ef091`: TargetOrigin, one-shot.

### ExplosiveBurstPrep

Used by: Mage/47 prep.

- Effect/Fx entry `effect_ef090`: Caster, retained until cast end.

### FireRainPrep

Used by: Mage/48 prep.

- Effect/Fx entry `effect_ef092`: Caster, retained until cast end.

### BlessedStrength

Used by: Mage/49 fire.

- Effect/Fx entry `effect_ef100`: Target, one-shot.
- Effect/Fx entry `effect_ef100b`: Target, one-shot.

### BlessedHealth

Used by: Mage/50 fire.

- Effect/Fx entry `effect_ef101`: Target, one-shot.
- Effect/Fx entry `effect_ef101a`: Target, one-shot.

### BlessedDexterity

Used by: Mage/51 fire.

- Effect/Fx entry `effect_ef102`: Target, one-shot.
- Effect/Fx entry `effect_ef102a`: Target, one-shot.

### BlessedIntelligence

Used by: Mage/52 fire.

- Effect/Fx entry `effect_ef103`: Target, one-shot.
- Effect/Fx entry `effect_ef103b`: Target, one-shot.

### AngelicDexterity

Used by: Mage/53 fire.

- Effect/Fx entry `effect_ef104`: Target, one-shot.
- Effect/Fx entry `effect_ef104a`: Target, one-shot.

### Purge

Used by: Mage/54 fire.

- Effect/Fx entry `effect_ef112`: Target, one-shot.
- Effect/Fx entry `effect_ef112a`: Target, one-shot.

### PurgePrep

Used by: Mage/54 prep.

- Effect/Fx entry `effect_ef111`: Caster, retained until cast end.

### ReinforceSight

Used by: Mage/56 fire.

- No dedicated visual dispatch in SkillPresenter; currently reaches the generic efs010 sound fallback. Optional JSON preparation models and character-clip cues are separate.

### Amnesia

Used by: Mage/61 fire.

- Effect/Fx entry `effect_ef118`: Target, one-shot.
- Effect/Fx entry `effect_ef118a`: Target, one-shot.

### AmnesiaPrep

Used by: Mage/61 prep.

- Effect/Fx entry `effect_ef117`: Caster, retained until cast end.

### LightRemedy

Used by: Mage/28 fire.

- Effect/Sprite entry `healing`: TargetOrigin, one-shot.
- Also draws a procedural target-following ring with Effect/healing.dds and plays efs022 at the target origin.

### LightReflection

Used by: Mage/29 fire.

- Effect/Sprite entry `healing`: TargetOrigin, one-shot.
- Also draws a procedural target-following ring with Effect/healing.dds and plays efs022 at the target origin.

### ImmolationArrow

Used by: Archer/5 fire.

- Visible projectile uses Item/Weapon/M_arrow.gb and the Effect/ghost_grey.dds trail; the client builds a fallback shaft when the mesh is absent. At impact the zero-extra projectile plays efc007 and resolves effect_attack02 (or effect_attack02_c for a critical result) through Skill/ then Effect/ GB lookup.

### CriticalChance

Used by: Archer/7 fire.

- Effect/Sprite entry `resurrection_p`: Target, one-shot.

### WildStrength

Used by: Archer/10 fire.

- Effect/Sprite entry `resurrection_p`: Target, one-shot.

### VitalityDrain

Used by: Archer/18 fire.

- Effect/Fx entry `effect_ef051`: Caster, one-shot.
- Effect/Fx entry `effect_ef051a`: Target, one-shot.
- Adds an invisible return-flight controller from target to caster at 200 units/s; target FX is deferred until the outward travel time and triggers sound efs014.

### InflectSuffering

Used by: Archer/19 fire.

- Effect/Fx entry `effect_ef053a`: Caster, one-shot.
- Effect/Fx entry `effect_ef053b`: Target, one-shot.

### InflectSufferingPrep

Used by: Archer/19 prep.

- Effect/Fx entry `effect_ef052`: Caster, retained until cast end.

### MeshOfSpeed

Used by: Archer/22 fire.

- Effect/Fx entry `effect_ef055`: Caster, one-shot.
- Effect/Fx entry `effect_ef055a`: Target, one-shot.

### DarknessVision

Used by: Archer/23 fire.

- Effect/Fx entry `effect_ef059`: Caster, one-shot.
- Effect/Fx entry `effect_ef059a`: Target, one-shot.

### DarknessVisionPrep

Used by: Archer/23 prep.

- Effect/Fx entry `effect_ef058`: Caster, retained until cast end.

### ResistLightning

Used by: Archer/24 fire.

- Effect/Fx entry `effect_ef042`: Caster, one-shot.

### ResistIce

Used by: Archer/25 fire.

- Effect/Fx entry `effect_ef043`: Caster, one-shot.

### ResistFire

Used by: Archer/26 fire.

- Effect/Fx entry `effect_ef044`: Caster, one-shot.

### SilentShot

Used by: Archer/27 fire.

- Effect/Fx entry `effect_ef046a`: Target, one-shot.
- Visible projectile uses Item/Weapon/M_arrow.gb and the Effect/ghost_grey.dds trail; the client builds a fallback shaft when the mesh is absent. Target dispatch is delayed by distance / 500 units/s. Uses a dark non-additive trail and suppresses the generic impact clip.

### SilentShotPrep

Used by: Archer/27 prep.

- Effect/Fx entry `effect_ef045`: Caster, retained until cast end.

### ResistCurse

Used by: Archer/28 fire.

- Effect/Fx entry `effect_ef047`: Caster, one-shot.

### ResistSpirit

Used by: Archer/29 fire.

- Effect/Fx entry `effect_ef048`: Caster, one-shot.

### EmergencyEscape

Used by: Archer/31 fire, Archer/46 fire.

- Effect/Fx entry `effect_ef050`: Caster, one-shot.

### CloudSight

Used by: Archer/33 fire.

- Effect/Fx entry `effect_ef054`: Caster, one-shot.
- Effect/Fx entry `effect_ef054a`: Target, one-shot.

### ManaBurn

Used by: Archer/34 fire.

- Uses the red ribbon, flare and impact entries described for LightningRed; sound efs021.

### ManaBurnPrep

Used by: Archer/34 prep.

- Effect/Fx entry `effect_ef056`: Caster, retained until cast end.

### MeshOfChance

Used by: Archer/35 fire.

- Effect/Fx entry `effect_ef060`: Caster, one-shot.
- Effect/Fx entry `effect_ef060a`: Target, one-shot.

### MeshOfForce

Used by: Archer/36 fire.

- Effect/Fx entry `effect_ef061`: Caster, one-shot.
- Effect/Fx entry `effect_ef061a`: Target, one-shot.

### MeshOfHealth

Used by: Archer/37 fire.

- Effect/Fx entry `effect_ef062`: Caster, one-shot.
- Effect/Fx entry `effect_ef062a`: Target, one-shot.

### MeshOfAgility

Used by: Archer/38 fire.

- Effect/Fx entry `effect_ef063`: Caster, one-shot.
- Effect/Fx entry `effect_ef063a`: Target, one-shot.

### ConstrictMind

Used by: Archer/39 fire.

- Effect/Fx entry `effect_ef064`: Caster, one-shot.
- Effect/Fx entry `effect_ef064a`: Target, one-shot.

### ConstrictWisdom

Used by: Archer/40 fire.

- Effect/Fx entry `effect_ef065`: Caster, one-shot.
- Effect/Fx entry `effect_ef065a`: Target, one-shot.

### VanishWall

Used by: Archer/41 prep, Archer/41 fire.

- Effect/Fx entry `effect_ef066`: Caster, one-shot.
- Effect/Fx entry `effect_ef066a`: Target, one-shot.

### VanishConfidence

Used by: Archer/43 fire.

- Effect/Fx entry `effect_ef069`: Caster, one-shot.
- Effect/Fx entry `effect_ef069a`: Target, one-shot.

### DestroySight

Used by: Archer/44 fire.

- Effect/Fx entry `effect_ef071`: Caster, one-shot.
- Effect/Fx entry `effect_ef071a`: Target, one-shot.

### DestroySightPrep

Used by: Archer/44 prep.

- Effect/Fx entry `effect_ef070`: Caster, retained until cast end.

### BenefitAblation

Used by: Archer/45 fire.

- Effect/Fx entry `effect_ef073`: Caster, one-shot.
- Effect/Fx entry `effect_ef073a`: Target, one-shot.

### BenefitAblationPrep

Used by: Archer/45 prep.

- Effect/Fx entry `effect_ef072`: Caster, retained until cast end.

### PoisonCloudPrep

Used by: Archer/42 prep.

- Effect/Fx entry `effect_ef067`: Caster, retained until cast end.

### LightningRed

Used by: Transformed caster/101 fire.

- Procedural red ribbon using LightningRed.GTX, Sprite lightningRed_flare at the caster and lightningRed_impact at the target; sound efs021.

### GunchiIceTempest

Used by: Transformed caster/102 fire.

- No dedicated visual dispatch in SkillPresenter; currently reaches the generic efs010 sound fallback. Optional JSON preparation models and character-clip cues are separate.

### YungsangFatalBlow

Used by: Transformed caster/104 fire.

- No dedicated visual dispatch in SkillPresenter; currently reaches the generic efs010 sound fallback. Optional JSON preparation models and character-clip cues are separate.

### YungsangDemolition

Used by: Transformed caster/105 fire.

- No dedicated visual dispatch in SkillPresenter; currently reaches the generic efs010 sound fallback. Optional JSON preparation models and character-clip cues are separate.

### TugukGroundSplinter

Used by: Transformed caster/107 fire.

- No dedicated visual dispatch in SkillPresenter; currently reaches the generic efs010 sound fallback. Optional JSON preparation models and character-clip cues are separate.

### TugukShadowFang

Used by: Transformed caster/108 fire.

- No dedicated visual dispatch in SkillPresenter; currently reaches the generic efs010 sound fallback. Optional JSON preparation models and character-clip cues are separate.

### TugukDiamondClaw

Used by: Transformed caster/109 fire.

- No dedicated visual dispatch in SkillPresenter; currently reaches the generic efs010 sound fallback. Optional JSON preparation models and character-clip cues are separate.

### Gale1

Used by: Transformed caster/110 fire.

- No dedicated visual dispatch in SkillPresenter; currently reaches the generic efs010 sound fallback. Optional JSON preparation models and character-clip cues are separate.

### Gale2

Used by: Transformed caster/111 fire.

- No dedicated visual dispatch in SkillPresenter; currently reaches the generic efs010 sound fallback. Optional JSON preparation models and character-clip cues are separate.

### Gale3

Used by: Transformed caster/112 fire.

- No dedicated visual dispatch in SkillPresenter; currently reaches the generic efs010 sound fallback. Optional JSON preparation models and character-clip cues are separate.

## Maintaining and verifying this reference

The executable mapping is in `SkillEffectTable.cpp`; placement and procedural
selection are in `SkillPresenter.cpp`, `KnightFxDispatcher.cpp`,
`ArcherFxDispatcher.cpp` and `CombatFx.cpp`. Asset lookup is in `SpriteFx.cpp`.
Update both this reference and enum documentation when a mapping changes.

The owned `skill_ability_tooltip` and presentation regressions exercise mappings,
phase routing, projectile/state lifetimes and synthetic script behavior. This
editorial update changes no dispatch value, algorithm or rendered behavior.
A visual verification of a specific data pack still requires resolving its
actual container entries and stages, then checking texture, blend, timing,
attachment, motion and sound with `skill_effect_view`. No external asset or
private source was opened to write this reference.

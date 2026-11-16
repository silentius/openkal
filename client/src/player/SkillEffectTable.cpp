/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/SkillEffectTable.h"

#include "openkal/protocol/ArcherSkill.h"
#include "openkal/protocol/MageSkill.h"

#include <cstddef>
#include <iterator>

namespace openkal::client {
    namespace {

        // Sorted by (job << 16) + (phase << 8) + skill for binary lookup.
        constexpr SkillEffectEntry kEffects[] = {
            // compatibility behavior uses effect job 3 in the compatible client.
            // OpenKal reserves job 3 for Thief, so Monster uses its catalog key -2.
            {-2, 1, 29, SkillEt::Lightning, SkillEt::None},
            {-2, 1, 30, SkillEt::Spark, SkillEt::None},
            {-2, 1, 31, SkillEt::FireBolt, SkillEt::None},
            {-2, 1, 32, SkillEt::LightningImpact, SkillEt::None},
            {-2, 1, 33, SkillEt::Fireball, SkillEt::None},
            {-2, 1, 34, SkillEt::Explosion, SkillEt::None},

            {0, 0, 21, SkillEt::DashPrep, SkillEt::None},
            {0, 0, 34, SkillEt::ChangeToVitalityPrep, SkillEt::None},
            {0, 0, 35, SkillEt::ReincarnatePrep, SkillEt::None},
            {0, 1, 4, SkillEt::Taunt, SkillEt::None},
            {0, 1, 8, SkillEt::Taunt, SkillEt::None},
            {0, 1, 10, SkillEt::WildAccuracy, SkillEt::None},
            {0, 1, 16, SkillEt::UpperSmash, SkillEt::None},
            {0, 1, 17, SkillEt::InfernalAttack, SkillEt::None},
            {0, 1, 18, SkillEt::RainforceDefence, SkillEt::None},
            {0, 1, 19, SkillEt::ChantOfProtect, SkillEt::None},
            {0, 1, 22, SkillEt::ShieldAttack, SkillEt::None},
            {0, 1, 23, SkillEt::ChantOfHealing, SkillEt::None},
            {0, 1, 24, SkillEt::Berserk, SkillEt::None},
            {0, 1, 25, SkillEt::FullSwing, SkillEt::None},
            {0, 1, 26, SkillEt::Sacrifice, SkillEt::None},
            {0, 1, 27, SkillEt::ChantOfLightning, SkillEt::None},
            {0, 1, 28, SkillEt::ChantOfIce, SkillEt::None},
            {0, 1, 29, SkillEt::ChantOfFlame, SkillEt::None},
            {0, 1, 30, SkillEt::ChangeToVitality, SkillEt::None},
            {0, 1, 31, SkillEt::ChantOfAccuracy, SkillEt::None},
            {0, 1, 32, SkillEt::ChantOfBattle, SkillEt::None},
            {0, 1, 33, SkillEt::ChangeToVitality, SkillEt::None},
            {0, 1, 34, SkillEt::ShieldOfResilience, SkillEt::None},
            {0, 1, 35, SkillEt::Reincarnate, SkillEt::None},
            {0, 1, 38, SkillEt::SpinSlash, SkillEt::None},
            {0, 1, 40, SkillEt::PerfectDefense, SkillEt::None},
            {0, 1, 42, SkillEt::ProvocationOfBlow, SkillEt::None},

            {1, 0, 9, SkillEt::LightningImpactPrep, SkillEt::None},
            {1, 0, 10, SkillEt::HealingPrep, SkillEt::None},
            {1, 0, 16, SkillEt::ResurrectionPrep, SkillEt::None},
            {1, 0, 17, SkillEt::StoneshieldPrep, SkillEt::None},
            {1, 0, 18, SkillEt::StoneshieldPrep, SkillEt::None},
            {1, 0, 19, SkillEt::StoneshieldPrep, SkillEt::None},
            {1, 0, 20, SkillEt::StoneshieldPrep, SkillEt::None},
            {1, 0, 21, SkillEt::StoneshieldPrep, SkillEt::None},
            {1, 0, 22, SkillEt::HealingPrep, SkillEt::None},
            {1, 0, 31, SkillEt::CallLightningPrep, SkillEt::None},
            {1, 0, 32, SkillEt::IceLancePrep, SkillEt::None},
            {1, 0, 33, SkillEt::ExplosionPrep, SkillEt::None},
            {1, 0, 34, SkillEt::ResurrectionPrep, SkillEt::None},
            {1, 0, 35, SkillEt::HealingPrep, SkillEt::None},
            {1, 0, 36, SkillEt::MesmerizationPrep, SkillEt::None},
            {1, 0, 37, SkillEt::SnowFieldPrep, SkillEt::None},
            {1, 0, 38, SkillEt::EnchantWeaponPrep, SkillEt::None},
            {1, 0, 39, SkillEt::AuraOfShieldPrep, SkillEt::None},
            {1, 0, 40, SkillEt::SpiritBlastPrep, SkillEt::None},
            {1, 0, 41, SkillEt::ChainLightningPrep, SkillEt::None},
            {1, 0, 42, SkillEt::CallLightningPrep, SkillEt::None},
            {1, 0, 43, SkillEt::ThunderStormPrep, SkillEt::None},
            {1, 0, 44, SkillEt::IceRequeimPrep, SkillEt::None},
            {1, 0, 45, SkillEt::IceStormPrep, SkillEt::None},
            {1, 0, 46, SkillEt::ExplosionPrep, SkillEt::None},
            {1, 0, 47, SkillEt::ExplosiveBurstPrep, SkillEt::None},
            {1, 0, 48, SkillEt::FireRainPrep, SkillEt::None},
            {1, 0, 49, SkillEt::AuraOfShieldPrep, SkillEt::None},
            {1, 0, 50, SkillEt::AuraOfShieldPrep, SkillEt::None},
            {1, 0, 51, SkillEt::AuraOfShieldPrep, SkillEt::None},
            {1, 0, 52, SkillEt::AuraOfShieldPrep, SkillEt::None},
            {1, 0, 53, SkillEt::AuraOfShieldPrep, SkillEt::None},
            {1, 0, 54, SkillEt::PurgePrep, SkillEt::None},
            {1, 0, 61, SkillEt::AmnesiaPrep, SkillEt::None},
            {1, 0, 63, SkillEt::StoneshieldPrep, SkillEt::None},

            {1, 1, 1, SkillEt::BeheadingCharm, SkillEt::None},
            {1, 1, 2, SkillEt::FireBolt, SkillEt::None},
            {1, 1, 3, SkillEt::IceBolt, SkillEt::None},
            {1, 1, 4, SkillEt::Lightning, SkillEt::None},
            {1, 1, 8, SkillEt::Healing, SkillEt::None},
            {1, 1, 9, SkillEt::LightningImpact, SkillEt::None},
            {1, 1, 10, SkillEt::Healing, SkillEt::None},
            {1, 1, 15, SkillEt::Meditation, SkillEt::None},
            {1, 1, 16, SkillEt::Resurrection, SkillEt::None},
            {1, 1, 17, SkillEt::Stoneshield, SkillEt::None},
            {1, 1, 18, SkillEt::Stoneshield, SkillEt::None},
            {1, 1, 19, SkillEt::Stoneshield, SkillEt::None},
            {1, 1, 20, SkillEt::Stoneshield, SkillEt::None},
            {1, 1, 21, SkillEt::Stoneshield, SkillEt::None},
            {1, 1, 22, SkillEt::Healing, SkillEt::None},
            {1, 1, 23, SkillEt::Spark, SkillEt::None},
            {1, 1, 24, SkillEt::IceNova, SkillEt::None},
            {1, 1, 25, SkillEt::Fireball, SkillEt::Fireball},
            {1, 1, 26, SkillEt::Haste, SkillEt::None},
            {1, 1, 27, SkillEt::FrostBlast, SkillEt::None},
            {1, 1, 28, SkillEt::None, SkillEt::LightRemedy},
            {1, 1, 29, SkillEt::LightReflection, SkillEt::None},
            {1, 1, 31, SkillEt::CallLightning, SkillEt::None},
            {1, 1, 32, SkillEt::IceLance, SkillEt::None},
            {1, 1, 33, SkillEt::Explosion, SkillEt::None},
            {1, 1, 34, SkillEt::Resurrection, SkillEt::None},
            {1, 1, 35, SkillEt::Healing, SkillEt::None},
            {1, 1, 36, SkillEt::Mesmerization, SkillEt::None},
            {1, 1, 37, SkillEt::SnowField, SkillEt::None},
            {1, 1, 38, SkillEt::EnchantWeapon, SkillEt::None},
            {1, 1, 39, SkillEt::AuraOfShield, SkillEt::None},
            {1, 1, 40, SkillEt::SpiritBlast, SkillEt::None},
            {1, 1, 41, SkillEt::Lightning, SkillEt::Lightning},
            {1, 1, 42, SkillEt::Thunder, SkillEt::None},
            {1, 1, 43, SkillEt::ThunderStorm, SkillEt::None},
            {1, 1, 44, SkillEt::IceRequeim, SkillEt::None},
            {1, 1, 46, SkillEt::Explosion, SkillEt::None},
            {1, 1, 47, SkillEt::ExplosiveBurst, SkillEt::ExplosiveBurst},
            {1, 1, 49, SkillEt::BlessedStrength, SkillEt::None},
            {1, 1, 50, SkillEt::BlessedHealth, SkillEt::None},
            {1, 1, 51, SkillEt::BlessedDexterity, SkillEt::None},
            {1, 1, 52, SkillEt::BlessedIntelligence, SkillEt::None},
            {1, 1, 53, SkillEt::AngelicDexterity, SkillEt::None},
            {1, 1, 54, SkillEt::Purge, SkillEt::None},
            {1, 1, 55, SkillEt::Healing, SkillEt::None},
            {1, 1, 56, SkillEt::ReinforceSight, SkillEt::None},
            {1, 1, 57, SkillEt::Healing, SkillEt::Healing},
            {1, 1, 58, SkillEt::Healing, SkillEt::Healing},
            {1, 1, 59, SkillEt::Healing, SkillEt::None},
            {1, 1, 61, SkillEt::Amnesia, SkillEt::None},
            {1, 1, 63, SkillEt::Stoneshield, SkillEt::None},

            {2, 0, 16, SkillEt::ChargedArrowPrep, SkillEt::None},
            {2, 0, 19, SkillEt::InflectSufferingPrep, SkillEt::None},
            {2, 0, 23, SkillEt::DarknessVisionPrep, SkillEt::None},
            {2, 0, 27, SkillEt::SilentShotPrep, SkillEt::None},
            {2, 0, 34, SkillEt::ManaBurnPrep, SkillEt::None},
            {2, 0, 41, SkillEt::VanishWall, SkillEt::None},
            {2, 0, 42, SkillEt::PoisonCloudPrep, SkillEt::None},
            {2, 0, 44, SkillEt::DestroySightPrep, SkillEt::None},
            {2, 0, 45, SkillEt::BenefitAblationPrep, SkillEt::None},

            {2, 1, 4, SkillEt::NegativeArrow, SkillEt::None},
            {2, 1, 5, SkillEt::ImmolationArrow, SkillEt::None},
            {2, 1, 6, SkillEt::FireArrow, SkillEt::None},
            {2, 1, 7, SkillEt::CriticalChance, SkillEt::None},
            {2, 1, 9, SkillEt::StunningArrow, SkillEt::None},
            {2, 1, 10, SkillEt::WildStrength, SkillEt::None},
            {2, 1, 13, SkillEt::PoisonArrow, SkillEt::None},
            {2, 1, 14, SkillEt::ExplosionArrow, SkillEt::None},
            {2, 1, 16, SkillEt::ChargedArrow, SkillEt::None},
            {2, 1, 18, SkillEt::VitalityDrain, SkillEt::None},
            {2, 1, 19, SkillEt::InflectSuffering, SkillEt::None},
            {2, 1, 22, SkillEt::MeshOfSpeed, SkillEt::None},
            {2, 1, 23, SkillEt::DarknessVision, SkillEt::None},
            {2, 1, 24, SkillEt::ResistLightning, SkillEt::None},
            {2, 1, 25, SkillEt::ResistIce, SkillEt::None},
            {2, 1, 26, SkillEt::ResistFire, SkillEt::None},
            {2, 1, 27, SkillEt::SilentShot, SkillEt::None},
            {2, 1, 28, SkillEt::ResistCurse, SkillEt::None},
            {2, 1, 29, SkillEt::ResistSpirit, SkillEt::None},
            {2, 1, 31, SkillEt::EmergencyEscape, SkillEt::None},
            {2, 1, 33, SkillEt::CloudSight, SkillEt::None},
            {2, 1, 34, SkillEt::ManaBurn, SkillEt::None},
            {2, 1, 35, SkillEt::MeshOfChance, SkillEt::None},
            {2, 1, 36, SkillEt::MeshOfForce, SkillEt::None},
            {2, 1, 37, SkillEt::MeshOfHealth, SkillEt::None},
            {2, 1, 38, SkillEt::MeshOfAgility, SkillEt::None},
            {2, 1, 39, SkillEt::ConstrictMind, SkillEt::None},
            {2, 1, 40, SkillEt::ConstrictWisdom, SkillEt::None},
            {2, 1, 41, SkillEt::VanishWall, SkillEt::None},
            {2, 1, 43, SkillEt::VanishConfidence, SkillEt::None},
            {2, 1, 44, SkillEt::DestroySight, SkillEt::None},
            {2, 1, 45, SkillEt::BenefitAblation, SkillEt::None},
            {2, 1, 46, SkillEt::EmergencyEscape, SkillEt::None},
            {2, 1, 49, SkillEt::ExploderArrow, SkillEt::None},

            {255, 1, 101, SkillEt::LightningRed, SkillEt::None},
            {255, 1, 102, SkillEt::GunchiIceTempest, SkillEt::None},
            {255, 1, 104, SkillEt::YungsangFatalBlow, SkillEt::None},
            {255, 1, 105, SkillEt::YungsangDemolition, SkillEt::None},
            {255, 1, 107, SkillEt::TugukGroundSplinter, SkillEt::None},
            {255, 1, 108, SkillEt::TugukShadowFang, SkillEt::None},
            {255, 1, 109, SkillEt::TugukDiamondClaw, SkillEt::None},
            {255, 1, 110, SkillEt::Gale1, SkillEt::None},
            {255, 1, 111, SkillEt::Gale2, SkillEt::None},
            {255, 1, 112, SkillEt::Gale3, SkillEt::None},
        };

        // Compatibility behavior dispatch, grouped by SkillEt.
        constexpr SkillFxDispatch kKnightFx[] = {
            {SkillEt::Taunt, "taunt", SkillFxAssetKind::Particle, SkillFxAttach::Target, false},
            {SkillEt::UpperSmash,
             "effect_ef019",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             false},
            {SkillEt::InfernalAttack,
             "effect_ef021",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             false},
            {SkillEt::InfernalAttack,
             "effect_strike01",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::ShieldAttack,
             "effect_ef026",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             false},
            {SkillEt::FullSwing,
             "effect_ef039",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             false},
            {SkillEt::Berserk, "effect_ef023", SkillFxAssetKind::Fx, SkillFxAttach::Caster, false},
            {SkillEt::DashPrep, "effect_ef020", SkillFxAssetKind::Fx, SkillFxAttach::Caster, true},
            {SkillEt::WildAccuracy,
             "resurrection_p",
             SkillFxAssetKind::Particle,
             SkillFxAttach::Target,
             false},
            {SkillEt::RainforceDefence,
             "effect_ef024",
             SkillFxAssetKind::Fx,
             SkillFxAttach::TargetOrigin,
             false},
            {SkillEt::ChantOfProtect,
             "effect_ef025",
             SkillFxAssetKind::Fx,
             SkillFxAttach::CasterOrigin,
             false},
            {SkillEt::PerfectDefense,
             "Tw_7_H_40_Floor",
             SkillFxAssetKind::Fx,
             SkillFxAttach::CasterOrigin,
             false},
            {SkillEt::ProvocationOfBlow,
             "Tw_7_A_44",
             SkillFxAssetKind::Fx,
             SkillFxAttach::CasterOrigin,
             false},
            {SkillEt::SpinSlash,
             "Tw_7_A_38",
             SkillFxAssetKind::Fx,
             SkillFxAttach::CasterOrigin,
             false},
            {SkillEt::ChantOfHealing,
             "effect_ef033",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::Sacrifice,
             "effect_ef027",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             false},
            {SkillEt::Sacrifice,
             "effect_ef027a",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::ChantOfLightning,
             "effect_ef028",
             SkillFxAssetKind::Fx,
             SkillFxAttach::CasterOrigin,
             false},
            {SkillEt::ChantOfIce,
             "effect_ef029",
             SkillFxAssetKind::Fx,
             SkillFxAttach::CasterOrigin,
             false},
            {SkillEt::ChantOfFlame,
             "effect_ef030",
             SkillFxAssetKind::Fx,
             SkillFxAttach::CasterOrigin,
             false},
            {SkillEt::ChangeToVitality,
             "effect_ef035",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             false},
            {SkillEt::ChangeToVitalityPrep,
             "effect_ef034",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             true},
            {SkillEt::ChantOfAccuracy,
             "effect_ef031",
             SkillFxAssetKind::Fx,
             SkillFxAttach::CasterOrigin,
             false},
            {SkillEt::ChantOfBattle,
             "effect_ef032",
             SkillFxAssetKind::Fx,
             SkillFxAttach::CasterOrigin,
             false},
            {SkillEt::ShieldOfResilience,
             "effect_ef036",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             false},
            {SkillEt::Reincarnate,
             "effect_ef038",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             false},
            {SkillEt::ReincarnatePrep,
             "effect_ef037",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             true},
        };

        // Archer script/particle dispatches, grouped by presentation effect.
        constexpr SkillFxDispatch kArcherFx[] = {
            {SkillEt::ChargedArrow,
             "effect_ef041",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             false},
            {SkillEt::ChargedArrowPrep,
             "effect_ef040",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             true},
            {SkillEt::ChargedArrowPrep,
             "effect_ef040a",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             true},
            {SkillEt::ChargedArrowPrep,
             "effect_ef040b",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             false},
            {SkillEt::ExplosionArrow,
             "explosion",
             SkillFxAssetKind::Particle,
             SkillFxAttach::Target,
             false},
            {SkillEt::ExploderArrow,
             "davi_ef131_04",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             true},
            {SkillEt::CriticalChance,
             "resurrection_p",
             SkillFxAssetKind::Particle,
             SkillFxAttach::Target,
             false},
            {SkillEt::WildStrength,
             "resurrection_p",
             SkillFxAssetKind::Particle,
             SkillFxAttach::Target,
             false},
            {SkillEt::VitalityDrain,
             "effect_ef051",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             false},
            {SkillEt::VitalityDrain,
             "effect_ef051a",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::InflectSuffering,
             "effect_ef053a",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             false},
            {SkillEt::InflectSuffering,
             "effect_ef053b",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::InflectSufferingPrep,
             "effect_ef052",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             true},
            {SkillEt::MeshOfSpeed,
             "effect_ef055",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             false},
            {SkillEt::MeshOfSpeed,
             "effect_ef055a",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::DarknessVision,
             "effect_ef059",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             false},
            {SkillEt::DarknessVision,
             "effect_ef059a",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::DarknessVisionPrep,
             "effect_ef058",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             true},
            {SkillEt::ResistLightning,
             "effect_ef042",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             false},
            {SkillEt::ResistIce,
             "effect_ef043",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             false},
            {SkillEt::ResistFire,
             "effect_ef044",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             false},
            {SkillEt::SilentShot,
             "effect_ef046a",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::SilentShotPrep,
             "effect_ef045",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             true},
            {SkillEt::ResistCurse,
             "effect_ef047",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             false},
            {SkillEt::ResistSpirit,
             "effect_ef048",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             false},
            {SkillEt::EmergencyEscape,
             "effect_ef050",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             false},
            {SkillEt::CloudSight,
             "effect_ef054",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             false},
            {SkillEt::CloudSight,
             "effect_ef054a",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::ManaBurnPrep,
             "effect_ef056",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             true},
            {SkillEt::MeshOfChance,
             "effect_ef060",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             false},
            {SkillEt::MeshOfChance,
             "effect_ef060a",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::MeshOfForce,
             "effect_ef061",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             false},
            {SkillEt::MeshOfForce,
             "effect_ef061a",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::MeshOfHealth,
             "effect_ef062",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             false},
            {SkillEt::MeshOfHealth,
             "effect_ef062a",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::MeshOfAgility,
             "effect_ef063",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             false},
            {SkillEt::MeshOfAgility,
             "effect_ef063a",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::ConstrictMind,
             "effect_ef064",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             false},
            {SkillEt::ConstrictMind,
             "effect_ef064a",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::ConstrictWisdom,
             "effect_ef065",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             false},
            {SkillEt::ConstrictWisdom,
             "effect_ef065a",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::VanishWall,
             "effect_ef066",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             false},
            {SkillEt::VanishWall,
             "effect_ef066a",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::PoisonCloudPrep,
             "effect_ef067",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             true},
            {SkillEt::VanishConfidence,
             "effect_ef069",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             false},
            {SkillEt::VanishConfidence,
             "effect_ef069a",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::DestroySight,
             "effect_ef071",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             false},
            {SkillEt::DestroySight,
             "effect_ef071a",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::DestroySightPrep,
             "effect_ef070",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             true},
            {SkillEt::BenefitAblation,
             "effect_ef073",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             false},
            {SkillEt::BenefitAblation,
             "effect_ef073a",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::BenefitAblationPrep,
             "effect_ef072",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             true},
        };

        // Compatibility behavior names. Prep stays on the caster
        // (charge). Fire oneshots sit on the skill recipient — the mage
        // themselves, or the selected player — not on a separate caster slot.
        constexpr SkillFxDispatch kMageFx[] = {
            {SkillEt::MesmerizationPrep,
             "effect_ef113",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             true},
            {SkillEt::Mesmerization,
             "effect_ef114",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::SpiritBlastPrep,
             "effect_ef115",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             true},
            {SkillEt::EnchantWeaponPrep,
             "effect_ef094",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             true},
            {SkillEt::EnchantWeapon,
             "effect_ef095",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::EnchantWeapon,
             "effect_ef095a",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::AuraOfShieldPrep,
             "effect_ef098",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             true},
            {SkillEt::AuraOfShield,
             "effect_ef099",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::AuraOfShield,
             "effect_ef099a",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::BlessedStrength,
             "effect_ef100",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::BlessedStrength,
             "effect_ef100b",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::BlessedHealth,
             "effect_ef101",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::BlessedHealth,
             "effect_ef101a",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::BlessedDexterity,
             "effect_ef102",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::BlessedDexterity,
             "effect_ef102a",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::BlessedIntelligence,
             "effect_ef103",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::BlessedIntelligence,
             "effect_ef103b",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::AngelicDexterity,
             "effect_ef104",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::AngelicDexterity,
             "effect_ef104a",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::PurgePrep, "effect_ef111", SkillFxAssetKind::Fx, SkillFxAttach::Caster, true},
            {SkillEt::Purge, "effect_ef112", SkillFxAssetKind::Fx, SkillFxAttach::Target, false},
            {SkillEt::Purge, "effect_ef112a", SkillFxAssetKind::Fx, SkillFxAttach::Target, false},
            {SkillEt::AmnesiaPrep,
             "effect_ef117",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             true},
            {SkillEt::Amnesia, "effect_ef118", SkillFxAssetKind::Fx, SkillFxAttach::Target, false},
            {SkillEt::Amnesia, "effect_ef118a", SkillFxAssetKind::Fx, SkillFxAttach::Target, false},
            {SkillEt::CallLightningPrep,
             "effect_ef074",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             true},
            {SkillEt::CallLightningPrep,
             "effect_ef074_1",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             true},
            {SkillEt::CallLightning,
             "effect_ef075",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::CallLightning,
             "effect_ef075a",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::CallLightning,
             "effect_ef075b",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::CallLightning,
             "effect_ef075c",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::IceLancePrep,
             "effect_ef080",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             true},
            {SkillEt::IceLancePrep,
             "effect_ef080_1",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             true},
            {SkillEt::IceLance,
             "effect_ef081a_ice1",
             SkillFxAssetKind::Fx,
             SkillFxAttach::TargetOrigin,
             false},
            {SkillEt::IceLance,
             "effect_ef081a_ice2",
             SkillFxAssetKind::Fx,
             SkillFxAttach::TargetOrigin,
             false},
            {SkillEt::IceLance,
             "effect_ef081a_ice3",
             SkillFxAssetKind::Fx,
             SkillFxAttach::TargetOrigin,
             false},
            {SkillEt::IceLance,
             "effect_ef081a_ice4",
             SkillFxAssetKind::Fx,
             SkillFxAttach::TargetOrigin,
             false},
            {SkillEt::SnowFieldPrep,
             "effect_ef082",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             true},
            {SkillEt::SnowField,
             "effect_ef083",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::ChainLightningPrep,
             "effect_ef077",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             true},
            {SkillEt::Thunder, "effect_ef077a", SkillFxAssetKind::Fx, SkillFxAttach::Target, false},
            {SkillEt::IceRequeimPrep,
             "effect_ef084",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             true},
            {SkillEt::IceRequeim,
             "effect_ef085",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             false},
            {SkillEt::IceRequeim,
             "effect_ef085a",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::ExplosionPrep,
             "effect_ef088",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             true},
            {SkillEt::ExplosionPrep,
             "effect_ef088_1",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             true},
            {SkillEt::Explosion,
             "effect_ef089a",
             SkillFxAssetKind::Fx,
             SkillFxAttach::TargetOrigin,
             false},
            {SkillEt::Explosion,
             "effect_ef089b",
             SkillFxAssetKind::Fx,
             SkillFxAttach::TargetOrigin,
             false},
            {SkillEt::Explosion,
             "effect_ef089c",
             SkillFxAssetKind::Fx,
             SkillFxAttach::TargetOrigin,
             false},
            {SkillEt::ExplosiveBurstPrep,
             "effect_ef090",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             true},
            {SkillEt::ExplosiveBurst,
             "effect_ef091",
             SkillFxAssetKind::Fx,
             SkillFxAttach::TargetOrigin,
             false},
            {SkillEt::ThunderStormPrep,
             "effect_ef078",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             true},
            {SkillEt::IceStormPrep,
             "effect_ef086",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             true},
            {SkillEt::FireRainPrep,
             "effect_ef092",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Caster,
             true},
            {SkillEt::ThunderStorm,
             "effect_ef079",
             SkillFxAssetKind::Fx,
             SkillFxAttach::Target,
             false},
            {SkillEt::Healing,
             "healing",
             SkillFxAssetKind::Particle,
             SkillFxAttach::TargetOrigin,
             false},
            {SkillEt::LightRemedy,
             "healing",
             SkillFxAssetKind::Particle,
             SkillFxAttach::TargetOrigin,
             false},
            {SkillEt::LightReflection,
             "healing",
             SkillFxAssetKind::Particle,
             SkillFxAttach::TargetOrigin,
             false},
        };

        constexpr int effect_key(int job, int phase, int skill) {
            return (job << 16) + (phase << 8) + skill;
        }

    }  // namespace

    const SkillEffectEntry* find_skill_effect(int job, int phase, int skill) {
        const int findkey = effect_key(job, phase, skill);
        int start = 0;
        int end = static_cast<int>(sizeof(kEffects) / sizeof(kEffects[0])) - 1;
        while (start <= end) {
            const int mid = (start + end) >> 1;
            const SkillEffectEntry& se = kEffects[static_cast<std::size_t>(mid)];
            const int key = effect_key(se.job, se.phase, se.skill);
            if (findkey == key) {
                return &se;
            }
            if (findkey > key) {
                start = mid + 1;
            } else {
                end = mid - 1;
            }
        }
        return nullptr;
    }

    SkillFxDispatchList::SkillFxDispatchList() = default;

    SkillFxDispatchList::SkillFxDispatchList(const SkillFxDispatch* data, std::size_t size)
        : m_data(data), m_size(size) {}

    bool SkillFxDispatchList::empty() const {
        return m_size == 0;
    }

    std::size_t SkillFxDispatchList::size() const {
        return m_size;
    }

    const SkillFxDispatch& SkillFxDispatchList::operator[](std::size_t index) const {
        return m_data[index];
    }

    const SkillFxDispatch* SkillFxDispatchList::begin() const {
        return m_data;
    }

    const SkillFxDispatch* SkillFxDispatchList::end() const {
        return m_size == 0 ? m_data : m_data + m_size;
    }

    SkillFxDispatchList knight_fx_dispatches(SkillEt et) {
        std::size_t first = 0;
        while (first < std::size(kKnightFx) && kKnightFx[first].effect != et) {
            ++first;
        }
        if (first == std::size(kKnightFx)) {
            return {};
        }

        std::size_t last = first + 1;
        while (last < std::size(kKnightFx) && kKnightFx[last].effect == et) {
            ++last;
        }
        return SkillFxDispatchList(kKnightFx + first, last - first);
    }

    SkillFxDispatchList archer_fx_dispatches(SkillEt et) {
        std::size_t first = 0;
        while (first < std::size(kArcherFx) && kArcherFx[first].effect != et) {
            ++first;
        }
        if (first == std::size(kArcherFx)) {
            return {};
        }

        std::size_t last = first + 1;
        while (last < std::size(kArcherFx) && kArcherFx[last].effect == et) {
            ++last;
        }
        return SkillFxDispatchList(kArcherFx + first, last - first);
    }

    SkillFxDispatchList mage_fx_dispatches(SkillEt et) {
        std::size_t first = 0;
        while (first < std::size(kMageFx) && kMageFx[first].effect != et) {
            ++first;
        }
        if (first == std::size(kMageFx)) {
            return {};
        }

        std::size_t last = first + 1;
        while (last < std::size(kMageFx) && kMageFx[last].effect == et) {
            ++last;
        }
        return SkillFxDispatchList(kMageFx + first, last - first);
    }

    std::string_view mage_range_preview_fx(int job, int skill_index) {
        if (!protocol::mage_uses_range_skill_selector(skill_index) ||
            protocol::mage_persistent_duration_ms(skill_index) <= 0) {
            return {};
        }
        const SkillEffectEntry* se = find_skill_effect(job, 0, skill_index);
        if (!se || se->effect == SkillEt::None) {
            return {};
        }
        const SkillFxDispatchList list = mage_fx_dispatches(se->effect);
        if (list.empty()) {
            return {};
        }
        return list[0].asset;
    }

    std::string_view range_skill_preview_fx(int job, int skill_index) {
        if (job == 2 &&
            protocol::archer_special_kind(skill_index) == protocol::ArcherSpecialKind::ArrowRain) {
            // Has no Archer ET for 47; Fire Storm targeting is the
            // looping ground seal. Channel rain uses Tm_7_a_15 `davi_ef132*` cues.
            return "effect_ef092";
        }
        return mage_range_preview_fx(job, skill_index);
    }

    const char* skill_et_name(SkillEt et) {
        switch (et) {
        case SkillEt::None:
            return "None";
        case SkillEt::FireBolt:
            return "FireBolt";
        case SkillEt::IceBolt:
            return "IceBolt";
        case SkillEt::Lightning:
            return "Lightning";
        case SkillEt::Healing:
            return "Healing";
        case SkillEt::HealingPrep:
            return "HealingPrep";
        case SkillEt::Taunt:
            return "Taunt";
        case SkillEt::ProvocationOfBlow:
            return "ProvocationOfBlow";
        case SkillEt::SpinSlash:
            return "SpinSlash";
        case SkillEt::ChantOfProtect:
            return "ChantOfProtect";
        case SkillEt::FireArrow:
            return "FireArrow";
        case SkillEt::ExploderArrow:
            return "ExploderArrow";
        default:
            return "SkillEt";
        }
    }

    float fire_delay_damage_hp_ratio(int cur_hp, int max_hp, int saved_damage, float elapsed_s) {
        if (max_hp <= 0) {
            return 0.0f;
        }
        const float maxf = static_cast<float>(max_hp);
        const float hp = static_cast<float>(cur_hp < 0 ? 0 : cur_hp);
        if (saved_damage <= 0 || elapsed_s >= 1.0f) {
            const float r = hp / maxf;
            if (r <= 0.0f) {
                return 0.0f;
            }
            return r >= 1.0f ? 1.0f : r;
        }
        const float t = elapsed_s < 0.0f ? 0.0f : elapsed_s;
        const float remaining = static_cast<float>(saved_damage) * (1.0f - t);
        const float r = (hp + remaining) / maxf;
        if (r <= 0.0f) {
            return 0.0f;
        }
        return r >= 1.0f ? 1.0f : r;
    }

}  // namespace openkal::client

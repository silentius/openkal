/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/SkillBuff.h"

#include "openkal/protocol/ArcherSkill.h"
#include "openkal/protocol/ThiefSkill.h"

#include <algorithm>

namespace openkal::server {

    namespace {

        [[nodiscard]] constexpr bool is_knight_chant(SkillBuffId id) {
            switch (id) {
            case SkillBuffId::KnightDefenseChant:
            case SkillBuffId::KnightHealingChant:
            case SkillBuffId::KnightLightningChant:
            case SkillBuffId::KnightIceChant:
            case SkillBuffId::KnightFlameChant:
            case SkillBuffId::KnightAccuracyChant:
            case SkillBuffId::KnightBattleChant:
                return true;
            default:
                return false;
            }
        }

        [[nodiscard]] std::int32_t active_value(const std::vector<SkillBuffEntry>& entries,
                                                SkillBuffId id,
                                                bool secondary = false) {
            const auto it =
                std::find_if(entries.begin(), entries.end(), [id](const SkillBuffEntry& entry) {
                    return entry.id == id;
                });
            if (it == entries.end()) {
                return 0;
            }
            return secondary ? it->value2 : it->value1;
        }

        [[nodiscard]] constexpr bool is_internal_status(SkillBuffId id) {
            switch (id) {
            case SkillBuffId::ArcherStun:
            case SkillBuffId::ArcherSpiritDamage:
            case SkillBuffId::ArcherCurseCritical:
            case SkillBuffId::ArcherCurseStrength:
            case SkillBuffId::ArcherCurseHealth:
            case SkillBuffId::ArcherCurseAgility:
            case SkillBuffId::ArcherCurseIntelligence:
            case SkillBuffId::ArcherCurseWisdom:
            case SkillBuffId::ArcherCurseDefense:
            case SkillBuffId::ArcherMeshSpeed:
            case SkillBuffId::ArcherDarknessVision:
            case SkillBuffId::ArcherCloudSight:
            case SkillBuffId::ArcherManaBurn:
            case SkillBuffId::ArcherVanishConfidence:
            case SkillBuffId::ArcherDestroySight:
            case SkillBuffId::ArcherMagicStop:
            case SkillBuffId::ArcherSilenceShot:
            case SkillBuffId::ThiefDanceOfKilling:
            case SkillBuffId::ThiefCounterAttack:
            case SkillBuffId::ThiefDualShadow:
            case SkillBuffId::ThiefWindCry:
            case SkillBuffId::ThiefWrath:
            case SkillBuffId::ThiefFlameDance:
            case SkillBuffId::ThiefAnkleAmputate:
            case SkillBuffId::ThiefArmorBreaker:
            case SkillBuffId::MageIcicleSlow:
            case SkillBuffId::MageMagicIntensification:
            case SkillBuffId::MageBlessing:
                return true;
            default:
                return false;
            }
        }

        [[nodiscard]] constexpr bool is_ablation_protected(SkillBuffId id) {
            switch (id) {
            case SkillBuffId::KnightPerfectDefense:
            case SkillBuffId::ThiefHiding:
                return true;
            default:
                return false;
            }
        }

    }  // namespace

    std::uint64_t SkillBuffSet::bstate_for(SkillBuffId id) {
        using protocol::kCbsSkillAnimalRage;
        using protocol::kCbsSkillArcherChanceCapture;
        using protocol::kCbsSkillArcherMuscleSolidation;
        using protocol::kCbsSkillArcherSharpness;
        using protocol::kCbsSkillAuraDefense;
        using protocol::kCbsSkillBlessAgility;
        using protocol::kCbsSkillBlessCritical;
        using protocol::kCbsSkillBlessHealth;
        using protocol::kCbsSkillBlessIntelligence;
        using protocol::kCbsSkillBlessStrength;
        using protocol::kCbsSkillEscape;
        using protocol::kCbsSkillHaste;
        using protocol::kCbsSkillHiding;
        using protocol::kCbsSkillKnightAccuracy;
        using protocol::kCbsSkillKnightBattle;
        using protocol::kCbsSkillKnightBerserk;
        using protocol::kCbsSkillKnightConcentration;
        using protocol::kCbsSkillKnightDefense;
        using protocol::kCbsSkillKnightElasticShield;
        using protocol::kCbsSkillKnightFlame;
        using protocol::kCbsSkillKnightForceDefense;
        using protocol::kCbsSkillKnightHealing;
        using protocol::kCbsSkillKnightIce;
        using protocol::kCbsSkillKnightLightning;
        using protocol::kCbsSkillKnightPerfectDefense;
        using protocol::kCbsSkillKnightProtect;
        using protocol::kCbsSkillMeditation;
        using protocol::kCbsSkillRefiningWeapon;
        using protocol::kCbsSkillResistCurse;
        using protocol::kCbsSkillResistFire;
        using protocol::kCbsSkillResistIce;
        using protocol::kCbsSkillResistLightning;
        using protocol::kCbsSkillResistSpirit;
        using protocol::kCbsSkillStone;
        using protocol::kCbsSkillThiefDanceOfKilling;
        switch (id) {
        case SkillBuffId::StoneShield:
            return kCbsSkillStone;
        case SkillBuffId::Haste:
            return kCbsSkillHaste;
        case SkillBuffId::Meditation:
            return kCbsSkillMeditation;
        case SkillBuffId::RefiningWeapon:
            return kCbsSkillRefiningWeapon;
        case SkillBuffId::AuraDefense:
            return kCbsSkillAuraDefense;
        case SkillBuffId::BlessStrength:
            return kCbsSkillBlessStrength;
        case SkillBuffId::BlessHealth:
            return kCbsSkillBlessHealth;
        case SkillBuffId::BlessAgility:
            return kCbsSkillBlessAgility;
        case SkillBuffId::BlessIntelligence:
            return kCbsSkillBlessIntelligence;
        case SkillBuffId::BlessCritical:
            return kCbsSkillBlessCritical;
        case SkillBuffId::KnightDefenseChant:
            return kCbsSkillKnightDefense;
        case SkillBuffId::KnightForceDefense:
            return kCbsSkillKnightForceDefense;
        case SkillBuffId::KnightHealingChant:
            return kCbsSkillKnightHealing;
        case SkillBuffId::KnightLightningChant:
            return kCbsSkillKnightLightning;
        case SkillBuffId::KnightIceChant:
            return kCbsSkillKnightIce;
        case SkillBuffId::KnightFlameChant:
            return kCbsSkillKnightFlame;
        case SkillBuffId::KnightAccuracyChant:
            return kCbsSkillKnightAccuracy;
        case SkillBuffId::KnightBattleChant:
            return kCbsSkillKnightBattle;
        case SkillBuffId::KnightBerserk:
            return kCbsSkillKnightBerserk;
        case SkillBuffId::KnightConcentration:
            return kCbsSkillKnightConcentration;
        case SkillBuffId::KnightProtect:
            return kCbsSkillKnightProtect;
        case SkillBuffId::KnightElasticShield:
            return kCbsSkillKnightElasticShield;
        case SkillBuffId::KnightPerfectDefense:
            return kCbsSkillKnightPerfectDefense;
        case SkillBuffId::ArcherFireResist:
            return kCbsSkillResistFire;
        case SkillBuffId::ArcherIceResist:
            return kCbsSkillResistIce;
        case SkillBuffId::ArcherLightningResist:
            return kCbsSkillResistLightning;
        case SkillBuffId::ArcherEscape:
            return kCbsSkillEscape;
        case SkillBuffId::ArcherSharpness:
            return kCbsSkillArcherSharpness;
        case SkillBuffId::YungsangRage:
            return kCbsSkillAnimalRage;
        case SkillBuffId::ArcherChanceCapture:
            return kCbsSkillArcherChanceCapture;
        case SkillBuffId::ArcherMuscleSolidation:
            return kCbsSkillArcherMuscleSolidation;
        case SkillBuffId::ArcherCurseResist:
            return kCbsSkillResistCurse;
        case SkillBuffId::ArcherSpiritResist:
            return kCbsSkillResistSpirit;
        case SkillBuffId::ArcherStun:
        case SkillBuffId::ArcherSpiritDamage:
        case SkillBuffId::ArcherCurseCritical:
        case SkillBuffId::ArcherCurseStrength:
        case SkillBuffId::ArcherCurseHealth:
        case SkillBuffId::ArcherCurseAgility:
        case SkillBuffId::ArcherCurseIntelligence:
        case SkillBuffId::ArcherCurseWisdom:
        case SkillBuffId::ArcherCurseDefense:
        case SkillBuffId::ArcherMeshSpeed:
        case SkillBuffId::ArcherDarknessVision:
        case SkillBuffId::ArcherCloudSight:
        case SkillBuffId::ArcherManaBurn:
        case SkillBuffId::ArcherVanishConfidence:
        case SkillBuffId::ArcherDestroySight:
        case SkillBuffId::ArcherMagicStop:
        case SkillBuffId::ArcherSilenceShot:
            return 0;
        case SkillBuffId::ThiefHiding:
            return kCbsSkillHiding;
        case SkillBuffId::ThiefDanceOfKilling:
            return kCbsSkillThiefDanceOfKilling;
        case SkillBuffId::ThiefCounterAttack:
        case SkillBuffId::ThiefDualShadow:
        case SkillBuffId::ThiefWindCry:
        case SkillBuffId::ThiefWrath:
        case SkillBuffId::ThiefFlameDance:
        case SkillBuffId::ThiefAnkleAmputate:
        case SkillBuffId::ThiefArmorBreaker:
        case SkillBuffId::MageIcicleSlow:
        case SkillBuffId::MageMagicIntensification:
        case SkillBuffId::MageBlessing:
            return 0;
        }
        return 0;
    }

    std::optional<SkillBuffId> SkillBuffSet::buff_id_for_skill(int clazz, int skill_index) {
        if (skill_index == 106) {
            return SkillBuffId::YungsangRage;
        }
        if (skill_index == 110) {
            return SkillBuffId::ThiefHiding;
        }
        if (clazz == 1) {
            if (skill_index == 64) {
                return SkillBuffId::MageMagicIntensification;
            }
            if (skill_index == 69) {
                return SkillBuffId::MageBlessing;
            }
            if (skill_index >= 17 && skill_index <= 21) {
                return SkillBuffId::StoneShield;
            }
            if (skill_index == 63) {
                return SkillBuffId::StoneShield;
            }
            if (skill_index == 26) {
                return SkillBuffId::Haste;
            }
            if (skill_index == 15) {
                return SkillBuffId::Meditation;
            }
            if (skill_index == 38) {
                return SkillBuffId::RefiningWeapon;
            }
            if (skill_index == 39) {
                return SkillBuffId::AuraDefense;
            }
            if (skill_index == 49) {
                return SkillBuffId::BlessStrength;
            }
            if (skill_index == 50) {
                return SkillBuffId::BlessHealth;
            }
            if (skill_index == 51) {
                return SkillBuffId::BlessAgility;
            }
            if (skill_index == 52) {
                return SkillBuffId::BlessIntelligence;
            }
            if (skill_index == 53) {
                return SkillBuffId::BlessCritical;
            }
        } else if (clazz == 0) {
            switch (skill_index) {
            case 10:
                return SkillBuffId::KnightConcentration;
            case 18:
                return SkillBuffId::KnightForceDefense;
            case 19:
                return SkillBuffId::KnightDefenseChant;
            case 23:
                return SkillBuffId::KnightHealingChant;
            case 24:
                return SkillBuffId::KnightBerserk;
            case 27:
                return SkillBuffId::KnightLightningChant;
            case 28:
                return SkillBuffId::KnightIceChant;
            case 29:
                return SkillBuffId::KnightFlameChant;
            case 31:
                return SkillBuffId::KnightAccuracyChant;
            case 32:
                return SkillBuffId::KnightBattleChant;
            case 14:
                return SkillBuffId::KnightProtect;
            case 34:
                return SkillBuffId::KnightElasticShield;
            case 40:
                return SkillBuffId::KnightPerfectDefense;
            default:
                break;
            }
        } else if (clazz == 2) {
            switch (skill_index) {
            case 7:
                return SkillBuffId::ArcherChanceCapture;
            case 10:
                return SkillBuffId::ArcherMuscleSolidation;
            case 24:
                return SkillBuffId::ArcherLightningResist;
            case 25:
                return SkillBuffId::ArcherIceResist;
            case 26:
                return SkillBuffId::ArcherFireResist;
            case 28:
                return SkillBuffId::ArcherCurseResist;
            case 29:
                return SkillBuffId::ArcherSpiritResist;
            case 31:
                return SkillBuffId::ArcherEscape;
            case 46:
                return SkillBuffId::ArcherSharpness;
            default:
                break;
            }
        } else if (clazz == 3) {
            switch (skill_index) {
            case 6:
                return SkillBuffId::ThiefHiding;
            case 12:
                return SkillBuffId::ThiefDualShadow;
            case 13:
                return SkillBuffId::ThiefDanceOfKilling;
            case 20:
                return SkillBuffId::ThiefCounterAttack;
            case 29:
                return SkillBuffId::ThiefWindCry;
            case 31:
                return SkillBuffId::ThiefWrath;
            case 35:
                return SkillBuffId::ThiefFlameDance;
            default:
                break;
            }
        }
        return std::nullopt;
    }

    std::optional<SkillBuffId> SkillBuffSet::archer_status_id_for_skill(int skill_index) {
        switch (skill_index) {
        case 9:
            return SkillBuffId::ArcherStun;
        case 15:
            return SkillBuffId::ArcherMagicStop;
        case 21:
            return SkillBuffId::ArcherSpiritDamage;
        case 22:
            return SkillBuffId::ArcherMeshSpeed;
        case 23:
            return SkillBuffId::ArcherDarknessVision;
        case 27:
            return SkillBuffId::ArcherSilenceShot;
        case 33:
            return SkillBuffId::ArcherCloudSight;
        case 34:
            return SkillBuffId::ArcherManaBurn;
        case 35:
            return SkillBuffId::ArcherCurseCritical;
        case 36:
            return SkillBuffId::ArcherCurseStrength;
        case 37:
            return SkillBuffId::ArcherCurseHealth;
        case 38:
            return SkillBuffId::ArcherCurseAgility;
        case 39:
            return SkillBuffId::ArcherCurseIntelligence;
        case 40:
            return SkillBuffId::ArcherCurseWisdom;
        case 41:
            return SkillBuffId::ArcherCurseDefense;
        case 43:
            return SkillBuffId::ArcherVanishConfidence;
        case 44:
            return SkillBuffId::ArcherDestroySight;
        default:
            return std::nullopt;
        }
    }

    SkillBuffCategory SkillBuffSet::category_for(SkillBuffId id) {
        switch (id) {
        case SkillBuffId::KnightDefenseChant:
        case SkillBuffId::KnightHealingChant:
        case SkillBuffId::KnightLightningChant:
        case SkillBuffId::KnightIceChant:
        case SkillBuffId::KnightFlameChant:
        case SkillBuffId::KnightAccuracyChant:
        case SkillBuffId::KnightBattleChant:
            return SkillBuffCategory::KnightChant;
        case SkillBuffId::ArcherFireResist:
        case SkillBuffId::ArcherIceResist:
        case SkillBuffId::ArcherLightningResist:
        case SkillBuffId::ArcherCurseResist:
        case SkillBuffId::ArcherSpiritResist:
            return SkillBuffCategory::Resistance;
        case SkillBuffId::ArcherStun:
        case SkillBuffId::ArcherMeshSpeed:
        case SkillBuffId::ArcherDarknessVision:
        case SkillBuffId::ArcherCloudSight:
        case SkillBuffId::ArcherDestroySight:
        case SkillBuffId::ArcherMagicStop:
        case SkillBuffId::ArcherSilenceShot:
        case SkillBuffId::ThiefAnkleAmputate:
        case SkillBuffId::MageIcicleSlow:
            return SkillBuffCategory::Control;
        case SkillBuffId::ArcherSpiritDamage:
        case SkillBuffId::ArcherManaBurn:
            return SkillBuffCategory::DamageOverTime;
        case SkillBuffId::ArcherCurseCritical:
        case SkillBuffId::ArcherCurseStrength:
        case SkillBuffId::ArcherCurseHealth:
        case SkillBuffId::ArcherCurseAgility:
        case SkillBuffId::ArcherCurseIntelligence:
        case SkillBuffId::ArcherCurseWisdom:
        case SkillBuffId::ArcherCurseDefense:
        case SkillBuffId::ArcherVanishConfidence:
        case SkillBuffId::ThiefArmorBreaker:
            return SkillBuffCategory::Curse;
        case SkillBuffId::ThiefHiding:
            return SkillBuffCategory::Concealment;
        default:
            return SkillBuffCategory::Beneficial;
        }
    }

    int SkillBuffSet::default_duration_sec(SkillBuffId id, int skill_level) {
        const int lv = std::max(1, skill_level);
        switch (id) {
        case SkillBuffId::StoneShield:
            return 900;
        case SkillBuffId::KnightConcentration:
            // compatibility behavior → CreateBuff(BUFF_HITUP, 900, …).
            return 900;
        case SkillBuffId::KnightForceDefense:
            return 1200;
        case SkillBuffId::KnightDefenseChant:
        case SkillBuffId::KnightHealingChant:
        case SkillBuffId::KnightLightningChant:
        case SkillBuffId::KnightIceChant:
        case SkillBuffId::KnightFlameChant:
        case SkillBuffId::KnightAccuracyChant:
        case SkillBuffId::KnightBattleChant:
            return 0;
        case SkillBuffId::Haste:
            return 600 * lv;
        case SkillBuffId::Meditation:
            return 720 + 10 * lv;
        case SkillBuffId::RefiningWeapon:
        case SkillBuffId::BlessCritical:
            return 900;
        case SkillBuffId::AuraDefense:
            return 600;
        case SkillBuffId::BlessStrength:
        case SkillBuffId::BlessHealth:
        case SkillBuffId::BlessAgility:
        case SkillBuffId::BlessIntelligence:
            return 1200;
        case SkillBuffId::KnightBerserk:
            return 60;
        case SkillBuffId::KnightProtect:
            return 300 * lv + 300;
        case SkillBuffId::KnightElasticShield:
            return 15;
        case SkillBuffId::KnightPerfectDefense:
            return 35 + 5 * lv;
        case SkillBuffId::ArcherChanceCapture:
        case SkillBuffId::ArcherMuscleSolidation:
        case SkillBuffId::ArcherFireResist:
        case SkillBuffId::ArcherIceResist:
        case SkillBuffId::ArcherLightningResist:
        case SkillBuffId::ArcherCurseResist:
        case SkillBuffId::ArcherSpiritResist:
            return 900;
        case SkillBuffId::ArcherEscape:
            return protocol::kArcherEscapeDurationSec;
        case SkillBuffId::ArcherSharpness:
            return protocol::kArcherSharpnessDurationSec;
        case SkillBuffId::YungsangRage:
            return 15;
        case SkillBuffId::ArcherStun:
            return lv / 2 + 1;
        case SkillBuffId::ArcherSpiritDamage:
            return 5;
        case SkillBuffId::ArcherMeshSpeed:
            return 15;
        case SkillBuffId::ArcherDarknessVision:
            return 8 * lv;
        case SkillBuffId::ArcherCloudSight:
            return 15;
        case SkillBuffId::ArcherManaBurn:
            return 10;
        case SkillBuffId::ArcherMagicStop:
            return 8;
        case SkillBuffId::ArcherSilenceShot:
            return 10;
        case SkillBuffId::ArcherVanishConfidence:
            return 15;
        case SkillBuffId::ArcherDestroySight:
            return 10;
        case SkillBuffId::ArcherCurseCritical:
        case SkillBuffId::ArcherCurseStrength:
        case SkillBuffId::ArcherCurseHealth:
        case SkillBuffId::ArcherCurseAgility:
        case SkillBuffId::ArcherCurseIntelligence:
        case SkillBuffId::ArcherCurseWisdom:
            return 20 * lv + 50;
        case SkillBuffId::ArcherCurseDefense:
            return 10 * lv + 30;
        case SkillBuffId::ThiefHiding:
            return 5 * lv + 15;
        case SkillBuffId::ThiefDanceOfKilling:
            return 15 * lv;
        case SkillBuffId::ThiefDualShadow:
            return 15;
        case SkillBuffId::ThiefCounterAttack:
            return 30;
        case SkillBuffId::ThiefWindCry:
        case SkillBuffId::ThiefWrath:
            return 180;
        case SkillBuffId::ThiefFlameDance:
            return 240;
        case SkillBuffId::ThiefAnkleAmputate:
            return 3 * lv;
        case SkillBuffId::ThiefArmorBreaker:
            return 9;
        case SkillBuffId::MageIcicleSlow:
            return 5;
        case SkillBuffId::MageMagicIntensification:
            return 25 + 5 * lv;
        case SkillBuffId::MageBlessing:
            return 3 + 3 * lv;
        default:
            return 180;
        }
    }

    void SkillBuffSet::default_values(SkillBuffId id,
                                      int skill_index,
                                      int skill_level,
                                      std::int32_t& out_v1,
                                      std::int32_t& out_v2) {
        const int lv = std::max(1, skill_level);
        out_v1 = 0;
        out_v2 = 0;
        switch (id) {
        case SkillBuffId::StoneShield: {
            // Defensive Incantation tiers: absorb / defense from JSON value1/value2.
            static constexpr std::int32_t kAbsorb[] = {1, 1, 2, 2, 2, 3};
            static constexpr std::int32_t kDef[] = {3, 5, 7, 10, 15, 21};
            int tier = 0;
            if (skill_index >= 17 && skill_index <= 21) {
                tier = skill_index - 17;
            } else if (skill_index == 63) {
                tier = 5;
            }
            tier = std::clamp(tier, 0, 5);
            out_v1 = kAbsorb[static_cast<std::size_t>(tier)];
            out_v2 = kDef[static_cast<std::size_t>(tier)];
            break;
        }
        case SkillBuffId::Haste:
            out_v1 = 15 * lv;
            break;
        case SkillBuffId::Meditation:
            out_v1 = lv;
            break;
        case SkillBuffId::RefiningWeapon:
            out_v1 = 8 * lv + 16;
            break;
        case SkillBuffId::AuraDefense:
            out_v1 = 30 * lv + 5;
            out_v2 = 20 * lv + 5;
            break;
        case SkillBuffId::BlessStrength:
            out_v1 = 6 * lv + 2;
            break;
        case SkillBuffId::BlessHealth:
        case SkillBuffId::BlessAgility:
            out_v1 = 8 * lv + 3;
            break;
        case SkillBuffId::BlessIntelligence:
            out_v1 = 6 * lv + 2;
            break;
        case SkillBuffId::BlessCritical:
            out_v1 = 3 * lv + 2;
            break;
        case SkillBuffId::KnightForceDefense:
            out_v1 = 7 * lv + 14;
            out_v2 = 3 * lv + 14;
            break;
        case SkillBuffId::KnightDefenseChant:
            out_v1 = 7 * lv + 9;
            out_v2 = 3 * lv + 9;
            break;
        case SkillBuffId::KnightHealingChant:
            out_v1 = 8 * lv + 4;
            break;
        case SkillBuffId::KnightLightningChant:
        case SkillBuffId::KnightIceChant:
        case SkillBuffId::KnightFlameChant:
            out_v1 = 3 * lv + 2;
            break;
        case SkillBuffId::KnightAccuracyChant:
            out_v1 = 5 * lv + 5;
            break;
        case SkillBuffId::KnightBattleChant:
            out_v1 = 25 * lv + 25;
            break;
        case SkillBuffId::KnightBerserk:
            out_v1 = 6 * lv + 5;
            out_v2 = -(2 * lv + 3);
            break;
        case SkillBuffId::KnightConcentration:
            // compatibility behavior: CreateBuff(BUFF_HITUP, 900, 3) — fixed +3 hit.
            out_v1 = 3;
            break;
        case SkillBuffId::KnightProtect:
            out_v1 = -(10 * lv + 40);
            break;
        case SkillBuffId::KnightElasticShield:
            out_v1 = 350;
            out_v2 = 300;
            break;
        case SkillBuffId::KnightPerfectDefense:
            out_v1 = 90;
            out_v2 = 30;
            break;
        case SkillBuffId::ArcherFireResist:
        case SkillBuffId::ArcherIceResist:
        case SkillBuffId::ArcherLightningResist:
        case SkillBuffId::ArcherCurseResist:
        case SkillBuffId::ArcherSpiritResist:
            out_v1 = 5 * lv + 10;
            break;
        case SkillBuffId::ArcherEscape:
            out_v1 = protocol::kArcherEscapeMoveSpeedPercent;
            break;
        case SkillBuffId::ArcherSharpness:
            out_v1 = protocol::kArcherSharpnessMoveSpeedPercent;
            break;
        case SkillBuffId::YungsangRage:
            out_v1 = 0;
            break;
        case SkillBuffId::ArcherChanceCapture:
            out_v1 = 2 * lv + 3;
            out_v2 = lv;
            break;
        case SkillBuffId::ArcherMuscleSolidation:
            out_v1 = 5;
            break;
        case SkillBuffId::ArcherStun:
            out_v1 = lv / 2 + 1;
            break;
        case SkillBuffId::ArcherMeshSpeed:
            out_v1 = -30 * lv;
            break;
        case SkillBuffId::ArcherDarknessVision:
            out_v1 = -50;
            break;
        case SkillBuffId::ArcherManaBurn:
            out_v1 = (250 * lv + 100) / 10;
            break;
        case SkillBuffId::ArcherSilenceShot:
            out_v1 = 70 * lv - 20;
            break;
        case SkillBuffId::ArcherVanishConfidence:
            out_v1 = -50;
            break;
        case SkillBuffId::ArcherSpiritDamage:
        case SkillBuffId::ArcherCloudSight:
        case SkillBuffId::ArcherMagicStop:
        case SkillBuffId::ArcherDestroySight:
        case SkillBuffId::ArcherCurseCritical:
            // These magnitudes depend on the rolled attack or target's current stats.
            break;
        case SkillBuffId::ArcherCurseStrength:
            out_v1 = -24 * lv;
            break;
        case SkillBuffId::ArcherCurseHealth:
            out_v1 = -12 * lv;
            break;
        case SkillBuffId::ArcherCurseAgility:
            out_v1 = -20 * lv;
            break;
        case SkillBuffId::ArcherCurseIntelligence:
            out_v1 = -24 * lv;
            break;
        case SkillBuffId::ArcherCurseWisdom:
            out_v1 = -16 * lv;
            break;
        case SkillBuffId::ArcherCurseDefense:
            out_v1 = -(8 * lv + 4);
            out_v2 = -(6 * lv + 2);
            break;
        case SkillBuffId::ThiefHiding:
            out_v1 = protocol::kThiefHidingMoveSpeedPercent;
            break;
        case SkillBuffId::ThiefDanceOfKilling:
            // The rank drives the replacement hit while this timed mode is active.
            out_v1 = lv;
            break;
        case SkillBuffId::ThiefDualShadow:
            out_v1 = 2 * lv;
            out_v2 = 3 * lv;
            break;
        case SkillBuffId::ThiefWindCry:
            out_v1 = 5 * lv;
            break;
        case SkillBuffId::ThiefWrath:
            out_v1 = 5 * lv;
            break;
        case SkillBuffId::ThiefFlameDance:
            out_v1 = 10 * lv;
            break;
        case SkillBuffId::ThiefAnkleAmputate:
            out_v1 = -50;
            break;
        case SkillBuffId::ThiefArmorBreaker:
            out_v1 = 5;
            break;
        case SkillBuffId::MageIcicleSlow:
            out_v1 = -100;
            break;
        case SkillBuffId::MageMagicIntensification:
            out_v1 = lv;
            break;
        case SkillBuffId::MageBlessing:
            out_v1 = 100;
            break;
        }
    }

    std::uint64_t SkillBuffSet::bstate() const {
        std::uint64_t bits = 0;
        for (const auto& e : m_entries) {
            bits |= e.bstate;
        }
        return bits;
    }

    bool SkillBuffSet::has(std::uint64_t bit) const {
        return (bstate() & bit) != 0;
    }

    std::int32_t SkillBuffSet::remain_of(const SkillBuffEntry& entry, std::int64_t now_unix) {
        if (entry.expire_unix == 0) {
            return 0;
        }
        if (entry.expire_unix <= now_unix) {
            return 0;
        }
        return static_cast<std::int32_t>(entry.expire_unix - now_unix);
    }

    bool SkillBuffSet::apply(SkillBuffId id,
                             int clazz,
                             int skill_index,
                             int duration_sec,
                             std::int32_t value1,
                             std::int32_t value2,
                             std::int64_t now_unix,
                             std::int32_t& out_remain,
                             std::uint64_t& out_bstate,
                             int chant_slots) {
        const std::uint64_t bit = bstate_for(id);
        if ((bit == 0 && !is_internal_status(id)) || duration_sec < 0) {
            return false;
        }
        out_bstate = bit;
        out_remain = duration_sec;
        if (is_knight_chant(id)) {
            const int slots = std::max(1, chant_slots);
            m_entries.erase(
                std::remove_if(m_entries.begin(),
                               m_entries.end(),
                               [id](const SkillBuffEntry& entry) { return entry.id == id; }),
                m_entries.end());
            const auto other_chant = [](const SkillBuffEntry& entry) {
                return is_knight_chant(entry.id);
            };
            int others =
                static_cast<int>(std::count_if(m_entries.begin(), m_entries.end(), other_chant));
            while (others >= slots) {
                const auto it = std::find_if(m_entries.begin(), m_entries.end(), other_chant);
                if (it == m_entries.end()) {
                    break;
                }
                m_entries.erase(it);
                --others;
            }
        }
        for (auto& e : m_entries) {
            if (e.id == id) {
                e.clazz = clazz;
                e.skill_index = skill_index;
                e.value1 = value1;
                e.value2 = value2;
                e.expire_unix = duration_sec == 0 ? 0 : now_unix + duration_sec;
                e.bstate = bit;
                return true;
            }
        }
        SkillBuffEntry e;
        e.id = id;
        e.bstate = bit;
        e.skill_index = skill_index;
        e.clazz = clazz;
        e.value1 = value1;
        e.value2 = value2;
        e.expire_unix = duration_sec == 0 ? 0 : now_unix + duration_sec;
        m_entries.push_back(e);
        return true;
    }

    bool SkillBuffSet::apply_item_haste(int duration_sec,
                                        std::int32_t move_speed_percent,
                                        std::int64_t now_unix,
                                        std::int32_t& out_remain,
                                        std::uint64_t& out_bstate) {
        if (duration_sec <= 0 || move_speed_percent <= 0 || has(protocol::kCbsSkillHaste)) {
            out_remain = 0;
            out_bstate = 0;
            return false;
        }
        return apply(SkillBuffId::Haste,
                     1,
                     0,
                     duration_sec,
                     move_speed_percent,
                     0,
                     now_unix,
                     out_remain,
                     out_bstate);
    }

    std::uint64_t SkillBuffSet::tick(std::int64_t now_unix) {
        std::uint64_t cleared = 0;
        m_entries.erase(std::remove_if(m_entries.begin(),
                                       m_entries.end(),
                                       [now_unix, &cleared](const SkillBuffEntry& entry) {
                                           const bool expired = entry.expire_unix != 0 &&
                                                                entry.expire_unix <= now_unix;
                                           if (expired) {
                                               cleared |= entry.bstate;
                                           }
                                           return expired;
                                       }),
                        m_entries.end());
        return cleared;
    }

    std::uint64_t SkillBuffSet::remove(SkillBuffId id) {
        std::uint64_t cleared = 0;
        m_entries.erase(std::remove_if(m_entries.begin(),
                                       m_entries.end(),
                                       [id, &cleared](const SkillBuffEntry& entry) {
                                           if (entry.id == id) {
                                               cleared |= entry.bstate;
                                               return true;
                                           }
                                           return false;
                                       }),
                        m_entries.end());
        return cleared;
    }

    const SkillBuffEntry* SkillBuffSet::find(SkillBuffId id) const {
        const auto it = std::find_if(m_entries.begin(),
                                     m_entries.end(),
                                     [id](const SkillBuffEntry& entry) { return entry.id == id; });
        return it == m_entries.end() ? nullptr : &*it;
    }

    std::uint64_t SkillBuffSet::remove_category(SkillBuffCategory category) {
        std::uint64_t cleared = 0;
        m_entries.erase(std::remove_if(m_entries.begin(),
                                       m_entries.end(),
                                       [category, &cleared](const SkillBuffEntry& entry) {
                                           if (category == SkillBuffCategory::Beneficial &&
                                               is_ablation_protected(entry.id)) {
                                               return false;
                                           }
                                           if (entry.expire_unix == 0 &&
                                               category == SkillBuffCategory::Beneficial) {
                                               return false;
                                           }
                                           if (category_for(entry.id) == category) {
                                               cleared |= entry.bstate;
                                               return true;
                                           }
                                           return false;
                                       }),
                        m_entries.end());
        return cleared;
    }

    std::uint64_t SkillBuffSet::remove_skill(int clazz, int skill_index) {
        std::uint64_t cleared = 0;
        m_entries.erase(std::remove_if(m_entries.begin(),
                                       m_entries.end(),
                                       [clazz, skill_index, &cleared](const SkillBuffEntry& entry) {
                                           if (entry.clazz == clazz &&
                                               entry.skill_index == skill_index) {
                                               cleared |= entry.bstate;
                                               return true;
                                           }
                                           return false;
                                       }),
                        m_entries.end());
        return cleared;
    }

    std::uint64_t SkillBuffSet::clear() {
        std::uint64_t cleared = 0;
        for (const SkillBuffEntry& entry : m_entries) {
            cleared |= entry.bstate;
        }
        m_entries.clear();
        return cleared;
    }

    std::int32_t SkillBuffSet::value(SkillBuffId id, bool secondary) const {
        return active_value(m_entries, id, secondary);
    }

    std::int32_t SkillBuffSet::stone_absorb() const {
        std::int32_t sum = 0;
        for (const auto& e : m_entries) {
            if (e.id == SkillBuffId::StoneShield) {
                sum += e.value1;
            }
        }
        return sum;
    }

    std::int32_t SkillBuffSet::stone_defense() const {
        std::int32_t sum = 0;
        for (const auto& e : m_entries) {
            if (e.id == SkillBuffId::StoneShield) {
                sum += e.value2;
            }
        }
        return sum;
    }

    std::int32_t SkillBuffSet::haste_mspeed_percent() const {
        for (const auto& e : m_entries) {
            if (e.id == SkillBuffId::Haste) {
                return e.value1;
            }
        }
        return 0;
    }

    std::int32_t SkillBuffSet::move_speed_percent() const {
        std::int32_t percent = haste_mspeed_percent();
        if (const SkillBuffEntry* escape = find(SkillBuffId::ArcherEscape)) {
            percent += escape->value1;
        }
        if (const SkillBuffEntry* sharpness = find(SkillBuffId::ArcherSharpness)) {
            percent += sharpness->value1;
        }
        if (const SkillBuffEntry* hiding = find(SkillBuffId::ThiefHiding);
            hiding && hiding->value1 < 0) {
            percent += hiding->value1;
        }
        if (const SkillBuffEntry* ankle = find(SkillBuffId::ThiefAnkleAmputate)) {
            percent += ankle->value1;
        }
        percent += active_value(m_entries, SkillBuffId::MageIcicleSlow);
        return percent;
    }

    std::int32_t SkillBuffSet::meditation_level() const {
        for (const auto& e : m_entries) {
            if (e.id == SkillBuffId::Meditation) {
                return e.value1;
            }
        }
        return 0;
    }

    std::int32_t SkillBuffSet::refining_weapon_points() const {
        for (const auto& e : m_entries) {
            if (e.id == SkillBuffId::RefiningWeapon) {
                return e.value1;
            }
        }
        return 0;
    }

    std::int32_t SkillBuffSet::aura_defense_points() const {
        for (const auto& e : m_entries) {
            if (e.id == SkillBuffId::AuraDefense) {
                return e.value1;
            }
        }
        return 0;
    }

    std::int32_t SkillBuffSet::aura_range_defense_points() const {
        for (const auto& e : m_entries) {
            if (e.id == SkillBuffId::AuraDefense) {
                return e.value2;
            }
        }
        return 0;
    }

    std::int32_t SkillBuffSet::blessing_strength_points() const {
        for (const auto& e : m_entries) {
            if (e.id == SkillBuffId::BlessStrength) {
                return e.value1;
            }
        }
        return 0;
    }

    std::int32_t SkillBuffSet::blessing_health_points() const {
        for (const auto& e : m_entries) {
            if (e.id == SkillBuffId::BlessHealth) {
                return e.value1;
            }
        }
        return 0;
    }

    std::int32_t SkillBuffSet::blessing_agility_points() const {
        for (const auto& e : m_entries) {
            if (e.id == SkillBuffId::BlessAgility) {
                return e.value1;
            }
        }
        return 0;
    }

    std::int32_t SkillBuffSet::blessing_intelligence_points() const {
        for (const auto& e : m_entries) {
            if (e.id == SkillBuffId::BlessIntelligence) {
                return e.value1;
            }
        }
        return 0;
    }

    std::int32_t SkillBuffSet::blessing_critical_points() const {
        for (const auto& e : m_entries) {
            if (e.id == SkillBuffId::BlessCritical) {
                return e.value1;
            }
        }
        return 0;
    }

    std::int32_t SkillBuffSet::concentration_hit() const {
        for (const auto& e : m_entries) {
            if (e.id == SkillBuffId::KnightConcentration) {
                return e.value1;
            }
        }
        return 0;
    }

    std::int32_t SkillBuffSet::force_defense_percent() const {
        return active_value(m_entries, SkillBuffId::KnightForceDefense);
    }

    std::int32_t SkillBuffSet::force_range_defense_percent() const {
        return active_value(m_entries, SkillBuffId::KnightForceDefense, true);
    }

    std::int32_t SkillBuffSet::defense_chant_percent() const {
        return active_value(m_entries, SkillBuffId::KnightDefenseChant);
    }

    std::int32_t SkillBuffSet::defense_chant_range_percent() const {
        return active_value(m_entries, SkillBuffId::KnightDefenseChant, true);
    }

    std::int32_t SkillBuffSet::recovery_chant_points() const {
        return active_value(m_entries, SkillBuffId::KnightHealingChant);
    }

    std::int32_t SkillBuffSet::lightning_chant_resistance() const {
        return active_value(m_entries, SkillBuffId::KnightLightningChant);
    }

    std::int32_t SkillBuffSet::ice_chant_resistance() const {
        return active_value(m_entries, SkillBuffId::KnightIceChant);
    }

    std::int32_t SkillBuffSet::flame_chant_resistance() const {
        return active_value(m_entries, SkillBuffId::KnightFlameChant);
    }

    std::int32_t SkillBuffSet::accuracy_chant_points() const {
        return active_value(m_entries, SkillBuffId::KnightAccuracyChant);
    }

    bool SkillBuffSet::has_yungsang_rage() const {
        return find(SkillBuffId::YungsangRage) != nullptr;
    }

    std::int32_t SkillBuffSet::battle_chant_attack_points() const {
        return active_value(m_entries, SkillBuffId::KnightBattleChant);
    }

    std::int32_t SkillBuffSet::berserk_attack_percent() const {
        return active_value(m_entries, SkillBuffId::KnightBerserk);
    }

    std::int32_t SkillBuffSet::berserk_defense_percent() const {
        return active_value(m_entries, SkillBuffId::KnightBerserk, true);
    }

    std::int32_t SkillBuffSet::protect_hostility_percent() const {
        return active_value(m_entries, SkillBuffId::KnightProtect);
    }

    std::int32_t SkillBuffSet::elastic_shield_defense_points() const {
        return active_value(m_entries, SkillBuffId::KnightElasticShield);
    }

    std::int32_t SkillBuffSet::elastic_shield_range_defense_points() const {
        return active_value(m_entries, SkillBuffId::KnightElasticShield, true);
    }

    std::int32_t SkillBuffSet::perfect_defense_self_percent() const {
        if (find(SkillBuffId::MageBlessing)) {
            return 100;
        }
        return active_value(m_entries, SkillBuffId::KnightPerfectDefense);
    }

    std::int32_t SkillBuffSet::perfect_defense_party_percent() const {
        return active_value(m_entries, SkillBuffId::KnightPerfectDefense, true);
    }

    std::int32_t SkillBuffSet::archer_chance_capture_fatal_rate() const {
        return active_value(m_entries, SkillBuffId::ArcherChanceCapture);
    }

    std::int32_t SkillBuffSet::archer_chance_capture_hit() const {
        return active_value(m_entries, SkillBuffId::ArcherChanceCapture, true);
    }

    std::int32_t SkillBuffSet::archer_muscle_min_attack() const {
        return active_value(m_entries, SkillBuffId::ArcherMuscleSolidation);
    }

    std::int32_t SkillBuffSet::archer_lightning_resistance() const {
        return active_value(m_entries, SkillBuffId::ArcherLightningResist);
    }

    std::int32_t SkillBuffSet::archer_ice_resistance() const {
        return active_value(m_entries, SkillBuffId::ArcherIceResist);
    }

    std::int32_t SkillBuffSet::archer_fire_resistance() const {
        return active_value(m_entries, SkillBuffId::ArcherFireResist);
    }

    std::int32_t SkillBuffSet::archer_curse_resistance() const {
        return active_value(m_entries, SkillBuffId::ArcherCurseResist);
    }

    std::int32_t SkillBuffSet::archer_spirit_resistance() const {
        return active_value(m_entries, SkillBuffId::ArcherSpiritResist);
    }

    std::int32_t SkillBuffSet::archer_stun_seconds() const {
        return active_value(m_entries, SkillBuffId::ArcherStun);
    }

    std::int32_t SkillBuffSet::archer_spirit_damage_per_tick() const {
        return active_value(m_entries, SkillBuffId::ArcherSpiritDamage);
    }

    std::int32_t SkillBuffSet::archer_attack_speed_percent() const {
        return active_value(m_entries, SkillBuffId::ArcherMeshSpeed);
    }

    std::int32_t SkillBuffSet::archer_move_speed_percent() const {
        return active_value(m_entries, SkillBuffId::ArcherDarknessVision) +
               active_value(m_entries, SkillBuffId::ThiefAnkleAmputate);
    }

    std::int32_t SkillBuffSet::archer_range_delta() const {
        return active_value(m_entries, SkillBuffId::ArcherCloudSight);
    }

    std::int32_t SkillBuffSet::archer_hit_percent() const {
        return active_value(m_entries, SkillBuffId::ArcherVanishConfidence);
    }

    bool SkillBuffSet::archer_magic_stopped() const {
        return find(SkillBuffId::ArcherMagicStop) != nullptr;
    }

    bool SkillBuffSet::archer_silence_shot() const {
        return find(SkillBuffId::ArcherSilenceShot) != nullptr;
    }

    bool SkillBuffSet::archer_destroy_sight() const {
        return find(SkillBuffId::ArcherDestroySight) != nullptr;
    }

    std::int32_t SkillBuffSet::archer_mana_burn_per_tick() const {
        return active_value(m_entries, SkillBuffId::ArcherManaBurn);
    }

    std::int32_t SkillBuffSet::archer_silence_shot_mp_drain() const {
        return active_value(m_entries, SkillBuffId::ArcherSilenceShot);
    }

    bool
    SkillBuffSet::values(SkillBuffId id, std::int32_t& out_value1, std::int32_t& out_value2) const {
        const auto it = std::find_if(m_entries.begin(),
                                     m_entries.end(),
                                     [id](const SkillBuffEntry& entry) { return entry.id == id; });
        if (it == m_entries.end()) {
            out_value1 = 0;
            out_value2 = 0;
            return false;
        }
        out_value1 = it->value1;
        out_value2 = it->value2;
        return true;
    }

}  // namespace openkal::server

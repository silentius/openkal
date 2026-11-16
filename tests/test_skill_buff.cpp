/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/ArcherSkill.h"
#include "openkal/protocol/BuffState.h"
#include "openkal/server/PayBuff.h"
#include "openkal/server/SkillBuff.h"

#include <cstdlib>
#include <iostream>

namespace {

    void expect(bool cond, const char* msg) {
        if (!cond) {
            std::cerr << "FAIL: " << msg << '\n';
            std::exit(1);
        }
    }

}  // namespace

int main() {
    using openkal::protocol::BuffType;
    using openkal::server::SkillBuffCategory;
    using openkal::server::SkillBuffId;
    using openkal::server::SkillBuffSet;

    {
        SkillBuffSet buffs;
        std::int32_t remain = 0;
        std::uint64_t bstate = 0;
        expect(buffs.apply(SkillBuffId::MageIcicleSlow, 1, 66, 5, -100, 0, 1000, remain, bstate),
               "Icicle applies its independent movement status");
        expect(buffs.move_speed_percent() == -100, "Icicle reduces movement for five seconds");
        expect(buffs.apply(SkillBuffId::MageBlessing, 1, 69, 6, 100, 0, 1000, remain, bstate),
               "Blessing applies temporary protection");
        expect(buffs.perfect_defense_self_percent() == 100, "Blessing blocks incoming damage");
        expect(SkillBuffSet::default_duration_sec(SkillBuffId::MageMagicIntensification, 2) == 35 &&
                   SkillBuffSet::buff_id_for_skill(1, 64) == SkillBuffId::MageMagicIntensification,
               "Magic Intensification resolves its hook state and active duration");
    }

    expect(static_cast<int>(BuffType::Haste) == 12, "Haste item buff id");
    expect(openkal::protocol::bstate_to_buff_icon(openkal::protocol::kCbsSkillHaste) ==
               "buff021.bmp",
           "Haste uses the level-three Speed Up skill-info icon");
    expect(static_cast<int>(BuffType::InventoryUnlock) == 100, "inventory unlock buff id");
    expect(static_cast<int>(BuffType::StoneExp) == 101, "Stone of EXP buff id");
    expect(static_cast<int>(BuffType::StoneWealth) == 102, "Stone of Wealth buff id");
    expect(static_cast<int>(BuffType::StoneAnimalExp) == 103,
           "Stone of Ancient Animal EXP buff id");
    expect(static_cast<int>(BuffType::StoneFishery) == 104, "Stone of Fishery buff id");
    expect(openkal::server::pay_buff_bstate(static_cast<int>(BuffType::StoneExp)) ==
               openkal::protocol::kCbsStoneExp,
           "Stone of EXP maps to its CBS bit");
    {
        openkal::server::PayBuffSet stone_exp;
        std::int32_t remain = 0;
        std::uint64_t bstate = 0;
        expect(stone_exp.apply(
                   static_cast<int>(BuffType::StoneExp), 43200, 1000, remain, bstate, 2411),
               "apply Stone of EXP");
        expect(remain == 43200, "Stone of EXP remain");
        expect(bstate == openkal::protocol::kCbsStoneExp, "Stone of EXP CBS bit");
        expect(stone_exp.entries().front().source_item_index == 2411,
               "Stone of EXP retains its source item");
        const auto saved = stone_exp.snapshot(1, 1000);
        expect(saved.size() == 1 && saved.front().source_item_index == 2411,
               "Stone of EXP persists its source item");
    }

    expect(SkillBuffSet::buff_id_for_skill(1, 17) == SkillBuffId::StoneShield,
           "mage stone shield id");
    expect(SkillBuffSet::buff_id_for_skill(1, 26) == SkillBuffId::Haste, "mage haste id");
    expect(SkillBuffSet::buff_id_for_skill(1, 38) == SkillBuffId::RefiningWeapon,
           "mage refining weapon id");
    expect(SkillBuffSet::buff_id_for_skill(1, 39) == SkillBuffId::AuraDefense,
           "mage Aura Defense id");
    expect(SkillBuffSet::buff_id_for_skill(1, 49) == SkillBuffId::BlessStrength,
           "mage Strength blessing id");
    expect(SkillBuffSet::buff_id_for_skill(1, 53) == SkillBuffId::BlessCritical,
           "mage critical blessing id");
    expect(SkillBuffSet::buff_id_for_skill(0, 10) == SkillBuffId::KnightConcentration,
           "knight concentration");
    expect(SkillBuffSet::buff_id_for_skill(0, 18) == SkillBuffId::KnightForceDefense,
           "knight Force of Defense");
    expect(SkillBuffSet::buff_id_for_skill(0, 19) == SkillBuffId::KnightDefenseChant,
           "knight Call of Defense");
    expect(SkillBuffSet::buff_id_for_skill(0, 24) == SkillBuffId::KnightBerserk, "knight berserk");
    expect(SkillBuffSet::buff_id_for_skill(3, 6) == SkillBuffId::ThiefHiding, "thief hiding");
    expect(!SkillBuffSet::buff_id_for_skill(3, 5).has_value(),
           "critical damage passive is not thief hiding");
    expect(SkillBuffSet::buff_id_for_skill(3, 13) == SkillBuffId::ThiefDanceOfKilling,
           "Dance of Killing mapping");
    expect(SkillBuffSet::buff_id_for_skill(3, 20) == SkillBuffId::ThiefCounterAttack,
           "Counter Attack mapping");
    expect(SkillBuffSet::buff_id_for_skill(2, 7) == SkillBuffId::ArcherChanceCapture,
           "Archer Chance Capture mapping");
    expect(SkillBuffSet::buff_id_for_skill(2, 10) == SkillBuffId::ArcherMuscleSolidation,
           "Archer Muscle Solidation mapping");
    expect(SkillBuffSet::buff_id_for_skill(2, 24) == SkillBuffId::ArcherLightningResist,
           "Archer lightning resistance mapping");
    expect(SkillBuffSet::buff_id_for_skill(2, 25) == SkillBuffId::ArcherIceResist,
           "Archer ice resistance mapping");
    expect(SkillBuffSet::buff_id_for_skill(2, 26) == SkillBuffId::ArcherFireResist,
           "Archer fire resistance mapping");
    expect(SkillBuffSet::buff_id_for_skill(2, 28) == SkillBuffId::ArcherCurseResist,
           "Archer curse resistance mapping");
    expect(SkillBuffSet::buff_id_for_skill(2, 29) == SkillBuffId::ArcherSpiritResist,
           "Archer spirit resistance mapping");
    expect(SkillBuffSet::buff_id_for_skill(2, 31) == SkillBuffId::ArcherEscape,
           "Archer Emergency Escape mapping");
    expect(SkillBuffSet::buff_id_for_skill(2, 46) == SkillBuffId::ArcherSharpness,
           "Archer Sharpness mapping");
    expect(openkal::protocol::skill_to_bstate(2, 31) == openkal::protocol::kCbsSkillEscape,
           "Emergency Escape skill_to_bstate");
    expect(openkal::protocol::skill_to_bstate(2, 46) == openkal::protocol::kCbsSkillArcherSharpness,
           "Sharpness skill_to_bstate");
    expect(openkal::protocol::skill_to_bstate(3, 13) ==
               openkal::protocol::kCbsSkillThiefDanceOfKilling,
           "Dance of Killing skill_to_bstate");
    expect(SkillBuffSet::buff_id_for_skill(0, 106) == SkillBuffId::YungsangRage,
           "Rage maps for a knight caster");
    expect(SkillBuffSet::buff_id_for_skill(10, 106) == SkillBuffId::YungsangRage,
           "Rage maps for Yungsang class");
    expect(openkal::protocol::skill_to_bstate(0, 106) == openkal::protocol::kCbsSkillAnimalRage,
           "Rage uses its own CBS bit");
    expect(SkillBuffSet::buff_id_for_skill(0, 110) == SkillBuffId::ThiefHiding,
           "Gale hiding maps for a knight caster");
    expect(SkillBuffSet::buff_id_for_skill(18, 110) == SkillBuffId::ThiefHiding,
           "Gale hiding maps for Gale class");
    expect(openkal::protocol::skill_to_bstate(18, 110) == openkal::protocol::kCbsSkillHiding,
           "Gale High Class Hiding uses CMS_HIDE CBS bit");
    {
        SkillBuffSet hiding;
        std::int32_t remain = 0;
        std::uint64_t bstate = 0;
        expect(hiding.apply(SkillBuffId::ThiefHiding, 18, 110, 20, 1, 0, 1000, remain, bstate),
               "apply Gale hiding");
        expect(remain == 20, "Gale hiding remain");
        expect(bstate == openkal::protocol::kCbsSkillHiding, "Gale hiding bstate");
        expect(hiding.has(openkal::protocol::kCbsSkillHiding), "Gale hiding bit is active");
    }
    expect(openkal::protocol::bstate_to_buff_icon(openkal::protocol::kCbsSkillAnimalRage) ==
               "active046.bmp",
           "Rage HUD icon");
    expect(SkillBuffSet::category_for(SkillBuffId::YungsangRage) == SkillBuffCategory::Beneficial,
           "Rage is not a knight Call");
    expect(SkillBuffSet::default_duration_sec(SkillBuffId::YungsangRage, 1) == 15,
           "Rage lasts 15 seconds");
    {
        SkillBuffSet stacked;
        std::int32_t remain = 0;
        std::uint64_t bstate = 0;
        expect(stacked.apply(
                   SkillBuffId::KnightAccuracyChant, 0, 31, 0, 13, 0, 1000, remain, bstate, 1),
               "apply Call of On Target Point");
        expect(stacked.apply(SkillBuffId::YungsangRage, 10, 106, 15, 0, 0, 1000, remain, bstate),
               "apply Rage beside OTP");
        expect(stacked.accuracy_chant_points() == 13, "Rage does not replace OTP");
        expect(stacked.has_yungsang_rage(), "Rage stays active with OTP");
        expect(stacked.entries().size() == 2, "OTP and Rage coexist");
    }
    expect(!SkillBuffSet::buff_id_for_skill(2, 14).has_value(),
           "Archer Explosion is not a resistance buff");
    expect(!SkillBuffSet::buff_id_for_skill(2, 15).has_value(),
           "Archer Silence is not a resistance buff");
    expect(!SkillBuffSet::buff_id_for_skill(2, 16).has_value(),
           "Archer Focus Shot is not a resistance buff");
    expect(!SkillBuffSet::buff_id_for_skill(2, 20).has_value(),
           "Archer advanced evasion is not Emergency Escape");
    expect(openkal::protocol::skill_to_bstate(0, 18) ==
               openkal::protocol::kCbsSkillKnightForceDefense,
           "Force of Defense skill_to_bstate");
    expect(openkal::protocol::skill_to_bstate(0, 19) == openkal::protocol::kCbsSkillKnightDefense,
           "Call of Defense skill_to_bstate");
    expect(openkal::protocol::skill_to_bstate(0, 18) != openkal::protocol::skill_to_bstate(0, 19),
           "Force of Defense and Call of Defense use distinct CBS bits");
    expect(openkal::protocol::bstate_to_buff_icon(openkal::protocol::kCbsSkillKnightForceDefense) ==
               "active060.bmp",
           "Force of Defense fallback icon");
    expect(openkal::protocol::bstate_to_buff_icon(openkal::protocol::kCbsSkillKnightDefense) ==
               "active061.bmp",
           "Call of Defense fallback icon");
    expect(openkal::protocol::skill_to_bstate(0, 27) == openkal::protocol::kCbsSkillKnightLightning,
           "lightning chant skill_to_bstate");
    expect(openkal::protocol::skill_to_bstate(0, 28) == openkal::protocol::kCbsSkillKnightIce,
           "ice chant skill_to_bstate");
    expect(openkal::protocol::skill_to_bstate(0, 29) == openkal::protocol::kCbsSkillKnightFlame,
           "flame chant skill_to_bstate");
    expect(openkal::protocol::bstate_to_buff_icon(openkal::protocol::kCbsSkillKnightLightning) ==
               "active096.bmp",
           "lightning chant fallback icon");
    expect(openkal::protocol::bstate_to_buff_icon(openkal::protocol::kCbsSkillKnightIce) ==
               "active097.bmp",
           "ice chant fallback icon");
    expect(openkal::protocol::bstate_to_buff_icon(openkal::protocol::kCbsSkillKnightFlame) ==
               "active098.bmp",
           "flame chant fallback icon");
    for (int clazz = 0; clazz < 4; ++clazz) {
        for (int index = 0; index < 80; ++index) {
            const auto id = SkillBuffSet::buff_id_for_skill(clazz, index);
            const std::uint64_t mapped = openkal::protocol::skill_to_bstate(clazz, index);
            if (!id) {
                expect(mapped == 0, "unmapped skill has no bstate");
            } else {
                expect(mapped == SkillBuffSet::bstate_for(*id),
                       "skill_to_bstate matches server bstate_for");
            }
        }
    }

    SkillBuffSet set;
    std::int32_t remain = 0;
    std::uint64_t bstate = 0;
    expect(set.apply(SkillBuffId::StoneShield, 1, 17, 900, 1, 3, 1000, remain, bstate),
           "apply stone");
    expect(remain == 900, "stone remain");
    expect(bstate == openkal::protocol::kCbsSkillStone, "stone bstate");
    expect(set.stone_absorb() == 1, "absorb");
    expect(set.stone_defense() == 3, "defense");
    expect(!openkal::protocol::bstate_to_buff_icon(bstate).empty(), "stone icon");
    {
        static constexpr std::int32_t kAbsorb[] = {1, 1, 2, 2, 2, 3};
        static constexpr std::int32_t kDefense[] = {3, 5, 7, 10, 15, 21};
        for (int tier = 0; tier < 6; ++tier) {
            const int skill_index = tier < 5 ? 17 + tier : 63;
            std::int32_t absorb = 0;
            std::int32_t defense = 0;
            SkillBuffSet::default_values(SkillBuffId::StoneShield, skill_index, 1, absorb, defense);
            expect(absorb == kAbsorb[tier], "stone absorb tier");
            expect(defense == kDefense[tier], "stone defense tier");
        }
    }
    expect(SkillBuffSet::default_duration_sec(SkillBuffId::Meditation, 1) == 730,
           "meditation rank 1 duration");
    expect(SkillBuffSet::default_duration_sec(SkillBuffId::Meditation, 5) == 770,
           "meditation rank 5 duration");
    expect(SkillBuffSet::default_duration_sec(SkillBuffId::ThiefDanceOfKilling, 1) == 15,
           "Dance of Killing rank 1 duration");
    expect(SkillBuffSet::default_duration_sec(SkillBuffId::ThiefDanceOfKilling, 2) == 30,
           "Dance of Killing rank 2 duration");
    {
        std::int32_t dance_level = 0;
        std::int32_t unused = 0;
        SkillBuffSet::default_values(SkillBuffId::ThiefDanceOfKilling, 13, 2, dance_level, unused);
        expect(dance_level == 2, "Dance of Killing stores its replacement-hit rank");
        expect(unused == 0, "Dance of Killing has no evasion bonus");
        SkillBuffSet dance;
        expect(dance.apply(SkillBuffId::ThiefDanceOfKilling,
                           3,
                           13,
                           30,
                           dance_level,
                           unused,
                           1000,
                           remain,
                           bstate),
               "apply Dance of Killing");
        expect(bstate == openkal::protocol::kCbsSkillThiefDanceOfKilling,
               "Dance of Killing exposes its active mode to the client");
        expect(dance.value(SkillBuffId::ThiefDanceOfKilling) == 2,
               "active Dance of Killing preserves rank");
    }
    {
        std::int32_t meditation = 0;
        std::int32_t unused = 0;
        SkillBuffSet::default_values(SkillBuffId::Meditation, 15, 4, meditation, unused);
        expect(meditation == 4, "meditation value is skill rank");
        SkillBuffSet meditation_set;
        expect(meditation_set.apply(
                   SkillBuffId::Meditation, 1, 15, 760, meditation, 0, 1000, remain, bstate),
               "apply meditation");
        expect(meditation_set.meditation_level() == 4, "active meditation rank");
        expect(meditation_set.remove(SkillBuffId::Meditation) ==
                   openkal::protocol::kCbsSkillMeditation,
               "remove meditation returns cleared state");
        expect(meditation_set.meditation_level() == 0, "removed meditation is inactive");
    }

    expect(set.apply(SkillBuffId::Haste, 1, 26, 600, 15, 0, 1000, remain, bstate), "apply haste");
    expect(set.haste_mspeed_percent() == 15, "haste pct");
    expect(set.move_speed_percent() == 15, "haste contributes to combined move speed");
    {
        SkillBuffSet medicine;
        expect(medicine.apply_item_haste(1800, 65, 1000, remain, bstate),
               "Speed-up Medicine applies Haste");
        expect(remain == 1800 && bstate == openkal::protocol::kCbsSkillHaste,
               "Speed-up Medicine exposes its duration and Haste state");
        expect(medicine.haste_mspeed_percent() == 65,
               "Speed-up Medicine uses its authored movement bonus");
        expect(!medicine.apply_item_haste(1800, 65, 1001, remain, bstate),
               "active Haste prevents consuming another Speed-up Medicine");
    }
    expect(set.apply(SkillBuffId::RefiningWeapon, 1, 38, 900, 24, 0, 1000, remain, bstate),
           "apply refining weapon");
    expect(set.refining_weapon_points() == 24, "refining weapon points");
    expect(bstate == openkal::protocol::kCbsSkillRefiningWeapon, "refining weapon bstate");
    expect(openkal::protocol::bstate_to_buff_icon(bstate) == "active065.bmp",
           "refining weapon icon");
    {
        SkillBuffSet mage_stats;
        struct BuffCase {
            SkillBuffId id;
            int skill_index;
            int skill_level;
            int value1;
            int value2;
            int duration;
        };
        const BuffCase cases[] = {
            {SkillBuffId::AuraDefense, 39, 2, 65, 45, 600},
            {SkillBuffId::BlessStrength, 49, 2, 14, 0, 1200},
            {SkillBuffId::BlessHealth, 50, 2, 19, 0, 1200},
            {SkillBuffId::BlessAgility, 51, 2, 19, 0, 1200},
            {SkillBuffId::BlessIntelligence, 52, 2, 14, 0, 1200},
            {SkillBuffId::BlessCritical, 53, 2, 8, 0, 900},
        };
        for (const BuffCase& buff : cases) {
            std::int32_t value1 = 0;
            std::int32_t value2 = 0;
            SkillBuffSet::default_values(
                buff.id, buff.skill_index, buff.skill_level, value1, value2);
            expect(value1 == buff.value1 && value2 == buff.value2, "Mage stat buff values");
            expect(SkillBuffSet::default_duration_sec(buff.id, buff.skill_level) == buff.duration,
                   "Mage stat buff duration");
            expect(mage_stats.apply(buff.id,
                                    1,
                                    buff.skill_index,
                                    buff.duration,
                                    value1,
                                    value2,
                                    1000,
                                    remain,
                                    bstate),
                   "apply Mage stat buff");
        }
        expect(mage_stats.aura_defense_points() == 65, "Aura Defense close points");
        expect(mage_stats.aura_range_defense_points() == 45, "Aura Defense ranged points");
        expect(mage_stats.blessing_strength_points() == 14, "Blessing of Strength points");
        expect(mage_stats.blessing_health_points() == 19, "Blessing of Health points");
        expect(mage_stats.blessing_agility_points() == 19, "Blessing of Agility points");
        expect(mage_stats.blessing_intelligence_points() == 14, "Blessing of Intelligence points");
        expect(mage_stats.blessing_critical_points() == 8, "Increase of Critical Hit points");
    }
    {
        struct MappingCase {
            int skill_index;
            SkillBuffId id;
        };
        const MappingCase mappings[] = {
            {14, SkillBuffId::KnightProtect},
            {18, SkillBuffId::KnightForceDefense},
            {19, SkillBuffId::KnightDefenseChant},
            {23, SkillBuffId::KnightHealingChant},
            {24, SkillBuffId::KnightBerserk},
            {27, SkillBuffId::KnightLightningChant},
            {28, SkillBuffId::KnightIceChant},
            {29, SkillBuffId::KnightFlameChant},
            {31, SkillBuffId::KnightAccuracyChant},
            {32, SkillBuffId::KnightBattleChant},
            {34, SkillBuffId::KnightElasticShield},
            {40, SkillBuffId::KnightPerfectDefense},
        };
        for (const MappingCase& mapping : mappings) {
            expect(SkillBuffSet::buff_id_for_skill(0, mapping.skill_index) == mapping.id,
                   "Knight skill mapping");
        }

        struct ValueCase {
            SkillBuffId id;
            int skill_index;
            int skill_level;
            std::int32_t value1;
            std::int32_t value2;
            int duration;
        };
        const ValueCase values[] = {
            {SkillBuffId::KnightProtect, 14, 2, -60, 0, 900},
            {SkillBuffId::KnightForceDefense, 18, 2, 28, 20, 1200},
            {SkillBuffId::KnightDefenseChant, 19, 2, 23, 15, 0},
            {SkillBuffId::KnightHealingChant, 23, 2, 20, 0, 0},
            {SkillBuffId::KnightBerserk, 24, 2, 17, -7, 60},
            {SkillBuffId::KnightLightningChant, 27, 2, 8, 0, 0},
            {SkillBuffId::KnightIceChant, 28, 2, 8, 0, 0},
            {SkillBuffId::KnightFlameChant, 29, 2, 8, 0, 0},
            {SkillBuffId::KnightAccuracyChant, 31, 2, 15, 0, 0},
            {SkillBuffId::KnightBattleChant, 32, 2, 75, 0, 0},
            {SkillBuffId::KnightElasticShield, 34, 1, 350, 300, 15},
            {SkillBuffId::KnightPerfectDefense, 40, 1, 90, 30, 40},
        };
        for (const ValueCase& value : values) {
            std::int32_t value1 = 0;
            std::int32_t value2 = 0;
            SkillBuffSet::default_values(
                value.id, value.skill_index, value.skill_level, value1, value2);
            expect(value1 == value.value1 && value2 == value.value2, "Knight buff values");
            expect(SkillBuffSet::default_duration_sec(value.id, value.skill_level) ==
                       value.duration,
                   "Knight buff duration");
        }
        expect(SkillBuffSet::default_duration_sec(SkillBuffId::KnightPerfectDefense, 6) == 65,
               "Perfect Defense rank 6 lasts 65 seconds");

        struct GetterCase {
            SkillBuffId id;
            int skill_index;
            int duration;
            std::int32_t value1;
            std::int32_t value2;
            std::int32_t (SkillBuffSet::*getter)() const;
            std::int32_t expected;
        };
        const GetterCase getters[] = {
            {SkillBuffId::KnightForceDefense,
             18,
             1200,
             28,
             20,
             &SkillBuffSet::force_defense_percent,
             28},
            {SkillBuffId::KnightDefenseChant,
             19,
             0,
             23,
             15,
             &SkillBuffSet::defense_chant_percent,
             23},
            {SkillBuffId::KnightHealingChant,
             23,
             0,
             20,
             0,
             &SkillBuffSet::recovery_chant_points,
             20},
            {SkillBuffId::KnightLightningChant,
             27,
             0,
             8,
             0,
             &SkillBuffSet::lightning_chant_resistance,
             8},
            {SkillBuffId::KnightIceChant, 28, 0, 8, 0, &SkillBuffSet::ice_chant_resistance, 8},
            {SkillBuffId::KnightFlameChant, 29, 0, 8, 0, &SkillBuffSet::flame_chant_resistance, 8},
            {SkillBuffId::KnightAccuracyChant,
             31,
             0,
             15,
             0,
             &SkillBuffSet::accuracy_chant_points,
             15},
            {SkillBuffId::KnightBattleChant,
             32,
             0,
             75,
             0,
             &SkillBuffSet::battle_chant_attack_points,
             75},
            {SkillBuffId::KnightBerserk, 24, 60, 17, -7, &SkillBuffSet::berserk_attack_percent, 17},
            {SkillBuffId::KnightProtect,
             14,
             900,
             -60,
             0,
             &SkillBuffSet::protect_hostility_percent,
             -60},
            {SkillBuffId::KnightElasticShield,
             34,
             15,
             350,
             300,
             &SkillBuffSet::elastic_shield_defense_points,
             350},
            {SkillBuffId::KnightPerfectDefense,
             40,
             30,
             90,
             30,
             &SkillBuffSet::perfect_defense_self_percent,
             90},
        };
        for (const GetterCase& getter : getters) {
            SkillBuffSet knight;
            expect(knight.apply(getter.id,
                                0,
                                getter.skill_index,
                                getter.duration,
                                getter.value1,
                                getter.value2,
                                1000,
                                remain,
                                bstate),
                   "apply Knight getter buff");
            expect((knight.*getter.getter)() == getter.expected, "Knight primary getter");
        }

        SkillBuffSet knight;
        expect(knight.apply(
                   SkillBuffId::KnightForceDefense, 0, 18, 1200, 28, 20, 1000, remain, bstate),
               "apply Force of Defense");
        expect(bstate == openkal::protocol::kCbsSkillKnightForceDefense, "Force of Defense bstate");
        expect(knight.force_range_defense_percent() == 20, "Force ranged-defense getter");
        expect(
            knight.apply(SkillBuffId::KnightDefenseChant, 0, 19, 0, 23, 15, 1000, remain, bstate),
            "apply Call of Defense");
        expect(knight.defense_chant_range_percent() == 15, "Call Defense ranged getter");
        expect(knight.entries().size() == 2, "Force Defense coexists with chant");
        expect(knight.apply(SkillBuffId::KnightHealingChant, 0, 23, 0, 20, 0, 1000, remain, bstate),
               "replace chant");
        expect(knight.defense_chant_percent() == 0, "old chant removed");
        expect(knight.recovery_chant_points() == 20, "replacement chant active");
        expect(knight.entries().size() == 2, "only one chant remains");
        expect(knight.tick(100000) == openkal::protocol::kCbsSkillKnightForceDefense,
               "timed Force Defense expires");
        expect(knight.recovery_chant_points() == 20, "continuous chant does not expire");
        expect(knight.entries().size() == 1, "only chant remains after Force Defense");
        expect(knight.entries()[0].expire_unix == 0, "chant expire_unix stays 0");
        expect(SkillBuffSet::remain_of(knight.entries()[0], 100000) == 0,
               "permanent remain_of is 0");
        expect(remain == 0, "chant apply remain is 0");

        expect(openkal::server::kKnightCallHarmonySkillIndex == 45, "Call Harmony skill index");
        expect(openkal::server::knight_chant_slots(0) == 1, "no Harmony: one Call");
        expect(openkal::server::knight_chant_slots(1) == 2, "Harmony rank 1: two Calls");
        expect(openkal::server::knight_chant_slots(2) == openkal::server::kKnightChantKindCount,
               "Harmony rank 2: all Calls");

        SkillBuffSet stacked;
        expect(stacked.apply(
                   SkillBuffId::KnightDefenseChant, 0, 19, 0, 23, 15, 1000, remain, bstate, 2),
               "stack Call of Defense");
        expect(stacked.apply(
                   SkillBuffId::KnightHealingChant, 0, 23, 0, 20, 0, 1000, remain, bstate, 2),
               "stack Call of Recovery");
        expect(stacked.defense_chant_percent() == 23, "two slots keep Defense");
        expect(stacked.recovery_chant_points() == 20, "two slots keep Recovery");
        expect(stacked.entries().size() == 2, "two Call slots filled");
        expect(stacked.apply(
                   SkillBuffId::KnightLightningChant, 0, 27, 0, 10, 0, 1000, remain, bstate, 2),
               "third Call at two slots");
        expect(stacked.defense_chant_percent() == 0, "oldest Call dropped at cap");
        expect(stacked.recovery_chant_points() == 20, "newer Call kept at cap");
        expect(stacked.lightning_chant_resistance() == 10, "newest Call applied");
        expect(stacked.entries().size() == 2, "cap stays at two Calls");

        SkillBuffSet all_calls;
        const struct {
            SkillBuffId id;
            int skill_index;
            std::int32_t value1;
        } calls[] = {
            {SkillBuffId::KnightDefenseChant, 19, 23},
            {SkillBuffId::KnightHealingChant, 23, 20},
            {SkillBuffId::KnightLightningChant, 27, 10},
            {SkillBuffId::KnightIceChant, 28, 11},
            {SkillBuffId::KnightFlameChant, 29, 12},
            {SkillBuffId::KnightAccuracyChant, 31, 13},
            {SkillBuffId::KnightBattleChant, 32, 14},
        };
        for (const auto& call : calls) {
            expect(all_calls.apply(call.id,
                                   0,
                                   call.skill_index,
                                   0,
                                   call.value1,
                                   0,
                                   1000,
                                   remain,
                                   bstate,
                                   openkal::server::kKnightChantKindCount),
                   "apply stacked Call");
        }
        expect(all_calls.entries().size() == 7, "rank 2 keeps every Call");
        expect(all_calls.defense_chant_percent() == 23, "all Calls keep Defense");
        expect(all_calls.recovery_chant_points() == 20, "all Calls keep Recovery");
        expect(all_calls.lightning_chant_resistance() == 10, "all Calls keep Lightning");
        expect(all_calls.ice_chant_resistance() == 11, "all Calls keep Ice");
        expect(all_calls.flame_chant_resistance() == 12, "all Calls keep Fire");
        expect(all_calls.accuracy_chant_points() == 13, "all Calls keep On Target");
        expect(all_calls.battle_chant_attack_points() == 14, "all Calls keep Physical Attack");

        SkillBuffSet secondary;
        expect(secondary.apply(SkillBuffId::KnightBerserk, 0, 24, 60, 17, -7, 1000, remain, bstate),
               "apply Berserk getters");
        expect(secondary.berserk_defense_percent() == -7, "Berserk defense getter");
        expect(secondary.apply(
                   SkillBuffId::KnightElasticShield, 0, 34, 15, 350, 300, 1000, remain, bstate),
               "apply Elastic Shield getters");
        expect(secondary.elastic_shield_range_defense_points() == 300,
               "Elastic Shield ranged getter");
        expect(secondary.apply(
                   SkillBuffId::KnightPerfectDefense, 0, 40, 30, 90, 30, 1000, remain, bstate),
               "apply Perfect Defense getters");
        expect(secondary.perfect_defense_party_percent() == 30, "Perfect Defense party getter");
    }
    expect(set.apply(SkillBuffId::KnightConcentration, 0, 10, 900, 3, 0, 1000, remain, bstate),
           "apply concentration");
    expect(remain == 900, "concentration remain");
    expect(set.concentration_hit() == 3, "concentration hit");
    expect(bstate == openkal::protocol::kCbsSkillKnightConcentration, "concentration bstate");
    expect(!openkal::protocol::bstate_to_buff_icon(bstate).empty(), "concentration icon");
    {
        SkillBuffSet archer;
        struct ArcherBuffCase {
            SkillBuffId id;
            int skill_index;
            int skill_level;
            int value1;
            int value2;
        };
        const ArcherBuffCase cases[] = {
            {SkillBuffId::ArcherChanceCapture, 7, 3, 9, 3},
            {SkillBuffId::ArcherMuscleSolidation, 10, 1, 5, 0},
            {SkillBuffId::ArcherLightningResist, 24, 2, 20, 0},
            {SkillBuffId::ArcherIceResist, 25, 2, 20, 0},
            {SkillBuffId::ArcherFireResist, 26, 2, 20, 0},
            {SkillBuffId::ArcherCurseResist, 28, 2, 20, 0},
            {SkillBuffId::ArcherSpiritResist, 29, 2, 20, 0},
        };
        for (const ArcherBuffCase& buff : cases) {
            std::int32_t value1 = 0;
            std::int32_t value2 = 0;
            SkillBuffSet::default_values(
                buff.id, buff.skill_index, buff.skill_level, value1, value2);
            expect(value1 == buff.value1 && value2 == buff.value2, "Archer beneficial buff values");
            expect(SkillBuffSet::default_duration_sec(buff.id, buff.skill_level) == 900,
                   "Archer beneficial buff source duration");
            expect(archer.apply(
                       buff.id, 2, buff.skill_index, 900, value1, value2, 1000, remain, bstate),
                   "apply Archer beneficial buff");
            expect(!openkal::protocol::bstate_to_buff_icon(bstate).empty(),
                   "Archer beneficial buff icon");
        }
        expect(archer.archer_chance_capture_fatal_rate() == 9, "Chance Capture fatal-rate getter");
        expect(archer.archer_chance_capture_hit() == 3, "Chance Capture hit getter");
        expect(archer.archer_muscle_min_attack() == 5, "Muscle Solidation getter");
        expect(archer.archer_lightning_resistance() == 20, "lightning resistance getter");
        expect(archer.archer_ice_resistance() == 20, "ice resistance getter");
        expect(archer.archer_fire_resistance() == 20, "fire resistance getter");
        expect(archer.archer_curse_resistance() == 20, "curse resistance getter");
        expect(archer.archer_spirit_resistance() == 20, "spirit resistance getter");
        const auto* chance = archer.find(SkillBuffId::ArcherChanceCapture);
        expect(chance != nullptr && SkillBuffSet::remain_of(*chance, 1200) == 700,
               "typed Archer entry exposes deterministic expiry");
        expect(
            archer.apply(SkillBuffId::ArcherChanceCapture, 2, 7, 900, 11, 4, 1300, remain, bstate),
            "replace exact Archer buff");
        expect(archer.archer_chance_capture_fatal_rate() == 11 &&
                   archer.archer_chance_capture_hit() == 4,
               "exact-id replacement updates magnitudes");
        expect(archer.remove_skill(2, 10) == openkal::protocol::kCbsSkillArcherMuscleSolidation,
               "remove Archer buff by source skill");
        const std::uint64_t resistance_bits = archer.remove_category(SkillBuffCategory::Resistance);
        expect((resistance_bits & openkal::protocol::kCbsSkillResistLightning) != 0 &&
                   (resistance_bits & openkal::protocol::kCbsSkillResistIce) != 0 &&
                   (resistance_bits & openkal::protocol::kCbsSkillResistFire) != 0 &&
                   (resistance_bits & openkal::protocol::kCbsSkillResistCurse) != 0 &&
                   (resistance_bits & openkal::protocol::kCbsSkillResistSpirit) != 0,
               "remove all typed Archer resistances by category");
        expect(archer.clear() != 0, "clear remaining Archer skill buffs");
        expect(archer.entries().empty(), "clear empties skill buff set");
        {
            std::int32_t value1 = 0;
            std::int32_t value2 = 0;
            SkillBuffSet::default_values(SkillBuffId::ArcherEscape, 31, 1, value1, value2);
            expect(value1 == openkal::protocol::kArcherEscapeMoveSpeedPercent &&
                       SkillBuffSet::default_duration_sec(SkillBuffId::ArcherEscape, 1) ==
                           openkal::protocol::kArcherEscapeDurationSec,
                   "Emergency Escape is +200% for 38s");
            expect(archer.apply(SkillBuffId::ArcherEscape,
                                2,
                                31,
                                openkal::protocol::kArcherEscapeDurationSec,
                                value1,
                                0,
                                1000,
                                remain,
                                bstate),
                   "apply Emergency Escape");
            expect(bstate == openkal::protocol::kCbsSkillEscape, "Emergency Escape CBS");
            expect(archer.move_speed_percent() == openkal::protocol::kArcherEscapeMoveSpeedPercent,
                   "Escape move-speed percent");
            SkillBuffSet::default_values(SkillBuffId::ArcherSharpness, 46, 1, value1, value2);
            expect(value1 == openkal::protocol::kArcherSharpnessMoveSpeedPercent &&
                       SkillBuffSet::default_duration_sec(SkillBuffId::ArcherSharpness, 1) ==
                           openkal::protocol::kArcherSharpnessDurationSec,
                   "Sharpness is +90% for 30s");
            expect(archer.apply(SkillBuffId::ArcherSharpness,
                                2,
                                46,
                                openkal::protocol::kArcherSharpnessDurationSec,
                                value1,
                                0,
                                1000,
                                remain,
                                bstate),
                   "apply Sharpness");
            expect(bstate == openkal::protocol::kCbsSkillArcherSharpness, "Sharpness CBS");
            expect(openkal::protocol::bstate_to_buff_icon(bstate) == "active154.bmp",
                   "Sharpness icon");
            expect(archer.move_speed_percent() ==
                       openkal::protocol::kArcherEscapeMoveSpeedPercent +
                           openkal::protocol::kArcherSharpnessMoveSpeedPercent,
                   "Escape and Sharpness stack additively");
            expect(archer.apply(SkillBuffId::Haste, 1, 26, 600, 15, 0, 1000, remain, bstate),
                   "apply Haste on top of Archer speed buffs");
            expect(archer.move_speed_percent() ==
                       15 + openkal::protocol::kArcherEscapeMoveSpeedPercent +
                           openkal::protocol::kArcherSharpnessMoveSpeedPercent,
                   "Haste, Escape, and Sharpness stack additively");
            expect(archer.clear() != 0, "clear Escape and Sharpness");
        }
    }
    {
        expect(SkillBuffSet::archer_status_id_for_skill(9) == SkillBuffId::ArcherStun,
               "Stun Arrow target status mapping");
        expect(SkillBuffSet::archer_status_id_for_skill(21) == SkillBuffId::ArcherSpiritDamage,
               "Mystic Arrow target status mapping");
        expect(SkillBuffSet::archer_status_id_for_skill(35) == SkillBuffId::ArcherCurseCritical,
               "critical curse target status mapping");
        expect(SkillBuffSet::archer_status_id_for_skill(41) == SkillBuffId::ArcherCurseDefense,
               "defense curse target status mapping");
        SkillBuffSet target;
        expect(target.apply(SkillBuffId::ArcherStun, 2, 9, 3, 3, 0, 1000, remain, bstate),
               "apply internal Archer stun");
        expect(bstate == 0 && target.archer_stun_seconds() == 3,
               "internal stun has typed payload without invented CBS bit");
        expect(target.apply(SkillBuffId::ArcherSpiritDamage, 2, 21, 5, 10, 0, 1000, remain, bstate),
               "apply Mystic Arrow Spirit Damage");
        expect(target.archer_spirit_damage_per_tick() == 10, "Mystic Arrow periodic damage getter");
        std::int32_t value1 = 0;
        std::int32_t value2 = 0;
        SkillBuffSet::default_values(SkillBuffId::ArcherCurseDefense, 41, 2, value1, value2);
        expect(value1 == -20 && value2 == -14, "Curse of Defense source values");
        expect(SkillBuffSet::default_duration_sec(SkillBuffId::ArcherCurseDefense, 2) == 50,
               "Curse of Defense source duration");
        expect(
            target.apply(
                SkillBuffId::ArcherCurseDefense, 2, 41, 50, value1, value2, 1000, remain, bstate),
            "apply typed Archer defense curse");
        expect(target.value(SkillBuffId::ArcherCurseDefense) == -20 &&
                   target.value(SkillBuffId::ArcherCurseDefense, true) == -14,
               "generic typed status values");
        expect(target.remove_category(SkillBuffCategory::Curse) == 0,
               "internal curse removal needs no client CBS clear");
        expect(target.find(SkillBuffId::ArcherCurseDefense) == nullptr,
               "category removal clears internal curse");
        expect(target.tick(1006) == 0 && target.entries().empty(),
               "internal Archer statuses expire deterministically");
    }
    {
        expect(SkillBuffSet::archer_status_id_for_skill(15) == SkillBuffId::ArcherMagicStop,
               "Arrow of Silence status mapping");
        expect(SkillBuffSet::archer_status_id_for_skill(22) == SkillBuffId::ArcherMeshSpeed,
               "Mesh of Speed status mapping");
        expect(SkillBuffSet::archer_status_id_for_skill(23) == SkillBuffId::ArcherDarknessVision,
               "Sight Interruption status mapping");
        expect(SkillBuffSet::archer_status_id_for_skill(27) == SkillBuffId::ArcherSilenceShot,
               "Powerful Silence status mapping");
        expect(SkillBuffSet::archer_status_id_for_skill(33) == SkillBuffId::ArcherCloudSight,
               "Cloud Sight status mapping");
        expect(SkillBuffSet::archer_status_id_for_skill(34) == SkillBuffId::ArcherManaBurn,
               "Mana Burn status mapping");
        expect(SkillBuffSet::archer_status_id_for_skill(43) == SkillBuffId::ArcherVanishConfidence,
               "Loss of Confidence status mapping");
        expect(SkillBuffSet::archer_status_id_for_skill(44) == SkillBuffId::ArcherDestroySight,
               "Sight Blockade status mapping");
        expect(SkillBuffSet::default_duration_sec(SkillBuffId::ArcherMeshSpeed, 2) == 15,
               "Mesh of Speed duration");
        expect(SkillBuffSet::default_duration_sec(SkillBuffId::ArcherDarknessVision, 2) == 16,
               "Sight Interruption duration is 8*SL");
        expect(SkillBuffSet::default_duration_sec(SkillBuffId::ArcherVanishConfidence, 1) == 15,
               "Loss of Confidence duration is 15 seconds");
        expect(SkillBuffSet::default_duration_sec(SkillBuffId::ArcherManaBurn, 1) == 10,
               "Mana Burn duration");
        std::int32_t value1 = 0;
        std::int32_t value2 = 0;
        SkillBuffSet::default_values(SkillBuffId::ArcherMeshSpeed, 22, 2, value1, value2);
        expect(value1 == -60, "Mesh of Speed -30*SL");
        SkillBuffSet mesh_speed;
        expect(mesh_speed.apply(
                   SkillBuffId::ArcherMeshSpeed, 2, 22, 15, value1, value2, 1000, remain, bstate),
               "apply Mesh of Speed");
        expect(mesh_speed.archer_attack_speed_percent() == -60,
               "Mesh of Speed contributes to attack delay");
        SkillBuffSet::default_values(SkillBuffId::ArcherManaBurn, 34, 1, value1, value2);
        expect(value1 == 35, "Mana Burn per-tick (250*SL+100)/10");
        SkillBuffSet::default_values(SkillBuffId::ArcherCurseDefense, 41, 2, value1, value2);
        expect(value1 == -20 && value2 == -14, "Vanish Wall close percent vs range percent");
        SkillBuffSet ablation;
        expect(ablation.apply(SkillBuffId::Haste, 1, 26, 600, 15, 0, 1000, remain, bstate),
               "apply haste for ablation");
        expect(ablation.apply(
                   SkillBuffId::KnightPerfectDefense, 0, 40, 30, 10, 0, 1000, remain, bstate),
               "apply Perfect Defense for ablation");
        const std::uint64_t stripped = ablation.remove_category(SkillBuffCategory::Beneficial);
        expect((stripped & openkal::protocol::kCbsSkillHaste) != 0, "ablation strips haste");
        expect(ablation.find(SkillBuffId::KnightPerfectDefense) != nullptr,
               "Perfect Defense is protected from Benefit Ablation");
        SkillBuffSet cloud;
        expect(
            cloud.apply(
                SkillBuffId::ArcherCloudSight, 2, 33, 15, 50 * 160 / -100, 0, 1000, remain, bstate),
            "apply Cloud Sight");
        expect(cloud.archer_range_delta() == -80, "Cloud Sight stores half range as a flat delta");
    }
    expect(set.tick(1500) == 0, "no buff expired at 1500");
    const std::uint64_t cleared = set.tick(1000 + 901);
    expect((cleared & openkal::protocol::kCbsSkillStone) != 0, "stone expired");
    expect((cleared & openkal::protocol::kCbsSkillHaste) != 0, "haste expired");
    expect((cleared & openkal::protocol::kCbsSkillRefiningWeapon) != 0, "refining weapon expired");
    expect(set.entries().empty(), "all cleared");
    std::cout << "ok\n";
    return 0;
}

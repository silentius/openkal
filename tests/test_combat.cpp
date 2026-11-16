/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/map/Coords.h"
#include "openkal/protocol/BuffState.h"
#include "openkal/protocol/KnightSkill.h"
#include "openkal/protocol/MageSkill.h"
#include "openkal/protocol/ThiefSkill.h"
#include "openkal/server/Combat.h"
#include "openkal/server/PrefixEnchant.h"
#include "openkal/server/world/GameWorld.h"
#include "test_util.h"

#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>

int main() {
    using openkal::server::combat::Combatant;
    using openkal::server::combat::MagicResist;
    using openkal::test::expect;

    expect(openkal::protocol::thief_skill_mp_cost(6, 3, 60, 0) == 40,
           "Hiding uses converted Energy cost");
    expect(openkal::protocol::kThiefHidingMoveSpeedPercent == -30,
           "Hiding applies the movement modifier");
    expect(openkal::protocol::thief_skill_mp_cost(28, 4, 75, 0) == 287,
           "Lightning Slash cost includes character and skill level");
    expect(openkal::protocol::thief_skill_mp_cost(32, 5, 75, 0) == 482,
           "Shadow Slash uses the later third-job formula");
    expect(openkal::protocol::thief_skill_power(33, 2, 100, 7) == 1514,
           "Wrath of Heaven uses derived server power");
    expect(openkal::protocol::thief_skill_power(13, 2, 100, 7) == 214,
           "Dance of Killing replaces a normal hit with its configured power");
    expect(openkal::protocol::thief_skill_hit(13) == 10,
           "Dance of Killing uses the configured hit bonus");
    expect(openkal::protocol::thief_hit_count(36, 4) == 5,
           "Spin Blade hit count increases with rank");
    expect(openkal::protocol::thief_target_shape(29) == openkal::protocol::ThiefTargetShape::Party,
           "Wind Cry is a party skill");
    expect(openkal::protocol::thief_is_awaken_skill(90),
           "Awaken Thief skills remain a separate catalog");
    expect(openkal::protocol::thief_acrobatic_reduction_percent(1, false, 5) == 10,
           "Acrobatic reduces physical damage on its configured chance");
    expect(openkal::protocol::thief_acrobatic_reduction_percent(1, true, 5) == 25,
           "High grade Acrobatic takes precedence");
    expect(openkal::protocol::thief_counter_triggers(650),
           "Counter Attack uses the derived 650-of-1000 threshold");

    {
        using namespace openkal::server::combat;
        expect(archer_status_tick(13, 3, 50, 100, 20) == 64,
               "Poison Arrow applies palsy resistance to level plus rank damage");
        expect(archer_status_tick(19, 1, 50, 100, 20) == 33,
               "Pain divides resisted total damage into 30 one-second ticks");
        expect(archer_status_tick(19, 5, 50, 1000, 0) == 150, "Pain caps total damage at 4500");
        expect(archer_status_tick(13, 5, 50, 100, 100) == 0,
               "Full palsy resistance blocks Poison Arrow damage");
        expect(archer_life_absorption_damage(1, 100, 1000) == 180,
               "Life Absorption uses its own rank damage");
        expect(archer_life_absorption_damage(5, 100, 100) == 99 &&
                   archer_life_absorption_damage(5, 100, 1) == 0,
               "Life Absorption never kills its target");
        expect(archer_life_absorption_heal(1, 100) == 60 &&
                   archer_life_absorption_heal(5, 100) == 140,
               "Life Absorption heal scales from 60 to 140 percent");
        expect(knight_spin_slash_power(2, 100, 0, 5, 3000) == 710 &&
                   knight_spin_slash_power(2, 100, 5, 20, 6000) == 30740,
               "Spin Slash includes the Deathblow random bonus");
        expect(knight_infernal_power(5, 100, 1) == 376 && knight_infernal_power(5, 100, 5) == 656,
               "Brutal Attack scales with each of the five stored points");
        expect(skill_physical_power(kPcKnight, 93, 3, 0, 1000) == 1003 &&
                   knight_wave_of_earth_pvp_damage(1003) == 10,
               "Wave of Earth uses the authoritative hook power and raw PvP percentage");
        expect(mage_channel_power(1, 100, 5) == 705 && mage_channel_power(5, 100, 10) == 750,
               "later Mage channels apply rank random power on each pulse");
        expect(mage_intensification_bonus(0, 100, false, 5) == 0 &&
                   mage_intensification_bonus(2, 100, false, 5) == 210 &&
                   mage_intensification_bonus(2, 100, true, 5) == 315,
               "Magic Intensification uses hook damage and its distinct PvP coefficient");
        Combatant armor_attacker;
        Combatant armor_target;
        armor_target.armor_broken = true;
        expect(final_damage(armor_attacker, armor_target, 100) == 100 &&
                   final_damage(armor_attacker, armor_target, 120) == 126,
               "Armor Breaker increases resolved hits only above the hook's 100-damage threshold");
        expect(openkal::protocol::mage_area_radius_tiles(65) == 4 &&
                   openkal::protocol::mage_area_radius_tiles(66) == 3 &&
                   openkal::protocol::mage_skill_mp_cost(65, 5, 90, 0) == 480 &&
                   openkal::protocol::mage_skill_mp_cost(67, 1, 90, 0) == 480,
               "later Mage area and mana values follow the hooks");
        expect(openkal::protocol::knight_skill_mp_cost(10, 5, 60, 0) == 76 &&
                   openkal::protocol::knight_skill_mp_cost(22, 3, 60, 0) == 120,
               "Wild Accuracy and Shield Offense match MP formulas");
        expect(openkal::protocol::knight_skill_mp_cost(38, 1, 60, 0) == 209 &&
                   openkal::protocol::knight_skill_mp_cost(38, 5, 60, 0) == 300,
               "Spin Slash uses the hook rank cost");
        expect(openkal::protocol::thief_area_radius_tiles(23) == 0 &&
                   openkal::protocol::thief_area_radius_tiles(33) == 0,
               "Twin Blade Strike and Wrath of Heaven do not splash nearby targets");
        expect(openkal::protocol::thief_skill_power(23, 5, 100, 7) == 207,
               "Twin Blade Strike random damage is independent of rank");
        std::uint64_t next = 12000;
        expect(revise_skill_tick(500, 10000, next) == 0,
               "Switching to a shorter skill delay cannot underflow to full damage");
        Combatant a;
        a.level = 60;
        a.min_attack = a.max_attack = 1000;
        Combatant b;
        b.level = 60;
        const auto early = resolve_physical(a, b, 0, 0, 1000, 0, 1000, true);
        const auto ready = resolve_physical(a, b, 0, 0, 1000, 1000, 1000, true);
        expect(early.damage < ready.damage / 10,
               "An explicit zero elapsed time does not bypass physical delay scaling");
    }

    {
        constexpr int kKnight = 0;
        constexpr int kStrength = 0;
        constexpr int kStart = 20;
        const std::int32_t total =
            openkal::server::combat::pending_need_points(kKnight, kStrength, kStart, 3);
        std::int32_t stat = kStart;
        std::int32_t pu = total;
        expect(openkal::server::combat::spend_primary_points(kKnight, kStrength, 3, stat, pu) == 3,
               "authoritative stat batch applies every affordable step");
        expect(stat == kStart + 3 && pu == 0, "authoritative stat batch updates stat and PU");

        stat = kStart;
        pu = total - 1;
        expect(openkal::server::combat::spend_primary_points(kKnight, kStrength, 3, stat, pu) == 2,
               "authoritative stat batch stops before unaffordable step");
        expect(stat == kStart + 2 && pu >= 0, "partial stat batch never overspends PU");

        const std::int32_t unchanged_stat = stat;
        const std::int32_t unchanged_pu = pu;
        expect(openkal::server::combat::spend_primary_points(kKnight, 99, 1, stat, pu) == 0 &&
                   stat == unchanged_stat && pu == unchanged_pu,
               "invalid stat batch cannot mutate server state");
    }

    expect(openkal::server::combat::is_mage_magic_attack(2), "fire is mage attack");
    expect(openkal::server::combat::is_mage_magic_attack(4), "lightning is mage attack");
    expect(openkal::server::combat::is_mage_magic_attack(40), "soul destruction is mage attack");
    expect(openkal::server::combat::is_mage_magic_attack(41), "chain lightning is mage attack");
    expect(openkal::server::combat::is_mage_magic_attack(42), "thunder is mage attack");
    expect(openkal::server::combat::is_mage_magic_attack(44), "ice restriction is mage attack");
    expect(openkal::server::combat::is_mage_magic_attack(46), "explosion is mage attack");
    expect(openkal::server::combat::is_mage_magic_attack(47), "explosive burst is mage attack");
    expect(openkal::server::combat::is_mage_magic_attack(43), "thunder storm tick is mage attack");
    expect(openkal::server::combat::is_mage_magic_attack(45), "ice storm tick is mage attack");
    expect(openkal::server::combat::is_mage_magic_attack(48), "fire storm tick is mage attack");
    {
        using openkal::server::combat::can_persistent_area_attack;
        using openkal::server::combat::PersistentAreaAttackQuery;
        PersistentAreaAttackQuery self;
        self.owner_id = 1;
        self.target_id = 1;
        self.target_is_player = true;
        expect(!can_persistent_area_attack(self), "storm never hits the caster");
        PersistentAreaAttackQuery other;
        other.owner_id = 1;
        other.target_id = 2;
        other.target_is_player = true;
        expect(can_persistent_area_attack(other),
               "without PK flags other players match Poison Cloud");
        PersistentAreaAttackQuery monster;
        monster.owner_id = 1;
        monster.target_id = 9;
        monster.target_is_player = false;
        expect(can_persistent_area_attack(monster), "storm ticks hit monsters");
        PersistentAreaAttackQuery pk_village = other;
        pk_village.pk_flag = true;
        pk_village.in_village = true;
        expect(!can_persistent_area_attack(pk_village), "PK is blocked in village");
        PersistentAreaAttackQuery pk_wild = other;
        pk_wild.pk_flag = true;
        expect(can_persistent_area_attack(pk_wild), "PK hits outside village/safety");
    }
    expect(!openkal::server::combat::is_mage_magic_attack(27), "frost is not a damage skill");
    expect(openkal::server::combat::mage_skill_resist(2) == MagicResist::Fire, "fire resist type");
    expect(openkal::server::combat::mage_skill_resist(3) == MagicResist::Ice, "ice resist type");
    expect(openkal::server::combat::mage_skill_resist(4) == MagicResist::Lightning,
           "lightning resist type");

    // compatibility behavior at level 10, skill 1, INT 18, base 5.
    expect(openkal::server::combat::skill_magic_power(2, 1, 10, 18, 5) == 17, "fire GetMagic");
    expect(openkal::server::combat::skill_magic_power(3, 1, 10, 18, 5) == 14, "ice GetMagic");
    expect(openkal::server::combat::skill_magic_power(23, 2, 10, 18, 5, 0, 4, 0) == 35,
           "Spark composes the separately ranked Lightning skill");
    expect(openkal::server::combat::skill_magic_power(23, 2, 10, 18, 5, 0, 0, 0) == 0,
           "Spark requires learned Lightning");
    expect(openkal::server::combat::skill_magic_power(40, 1, 100, 100, 10) == 485,
           "soul destruction uses INT formula");
    expect(openkal::server::combat::skill_magic_power(44, 1, 100, 100, 10) == 510,
           "ice restriction applies fixed 500 cap before base");
    expect(openkal::server::combat::skill_magic_power(46, 1, 100, 100, 10) == 898,
           "explosion uses initial +580 formula");
    expect(openkal::server::combat::skill_magic_power(47, 1, 100, 100, 10) == 780,
           "explosive burst magic formula");
    expect(openkal::server::combat::skill_magic_power(45, 1, 100, 100, 10) == 264,
           "ice storm tick formula");
    expect(openkal::server::combat::skill_magic_power(48, 1, 100, 100, 10) == 412,
           "fire storm tick formula");
    for (int i = 0; i < 16; ++i) {
        const int storm = openkal::server::combat::skill_magic_power(43, 1, 100, 100, 10, 2);
        expect(storm >= 685 && storm <= 1275, "thunder storm uses uncapped mastery range");
    }
    expect(openkal::server::combat::mage_self_heal_power(1, 10, 18) == 112,
           "mage self heal uses level and wisdom");
    expect(openkal::server::combat::mage_heal_power(10, 1, 10, 18, 1000, 0) == 105, "cure formula");
    expect(openkal::server::combat::mage_heal_power(22, 1, 10, 18, 1000, 0) == 558,
           "cure two formula");
    expect(openkal::server::combat::mage_heal_power(28, 1, 50, 50, 1000, 0) == 180,
           "group cure rank cap");
    expect(openkal::server::combat::mage_heal_power(28, 1, 1, 1, 1000, 0) == 161,
           "group cure uncapped expression");
    expect(openkal::server::combat::mage_heal_power(35, 1, 50, 50, 1000, 100) == 1133,
           "cure three deterministic roll");
    expect(openkal::server::combat::mage_heal_power(55, 2, 50, 50, 1000, 0) == 550,
           "perfect cure percentage");
    expect(openkal::server::combat::mage_heal_power(57, 1, 60, 50, 1000, 100) == 693,
           "group cure two deterministic roll");
    expect(openkal::server::combat::mage_heal_power(58, 1, 60, 100, 2000) == 2000,
           "Group Healing uses the recipient maximum HP instead of returning zero");
    expect(openkal::server::combat::mage_heal_power(59, 1, 60, 50, 1234, 0) == 1234,
           "perfect healing restores max hp");
    {
        bool mastery_increased_lightning = false;
        for (int i = 0; i < 64; ++i) {
            if (openkal::server::combat::skill_magic_power(4, 1, 1000, 1000, 0, 10) > 42) {
                mastery_increased_lightning = true;
                break;
            }
        }
        expect(mastery_increased_lightning, "lightning mastery reaches magic formula");
    }

    expect(openkal::server::combat::monster_final_resist(100) == 10, "monster resist / 10");
    expect(openkal::server::combat::magic_through_percent(10, 10, 0) == 100,
           "no resist full through");
    expect(openkal::server::combat::magic_through_percent(10, 10, 10) == 90, "10 final resist");
    expect(openkal::server::combat::final_magic_damage(10, 10, 17) == 17, "equal level no revise");
    expect(openkal::server::combat::final_magic_damage(12, 10, 17) == 18, "mage two levels above");

    expect(openkal::server::combat::scale_by_skill_delay(17, 1200, 1200) == 17,
           "full delay no scale");
    expect(openkal::server::combat::scale_by_skill_delay(17, 600, 1200) == 8,
           "half delay half damage");
    {
        std::uint64_t next = 0;
        expect(openkal::server::combat::revise_skill_tick(1200, 10000, next) == 1200,
               "first cast full delay");
        expect(next == 11200, "m_dwTick is now + delay");
        expect(openkal::server::combat::revise_skill_tick(1200, 10700, next) == 700,
               "aspeed recast returns elapsed portion");
        expect(next == 11900, "early recast still pushes m_dwTick");
        expect(openkal::server::combat::revise_skill_tick(1200, 11900, next) == 1200,
               "full wait restores full delay");
    }

    Combatant mage;
    mage.level = 10;
    mage.min_magic = 5;
    mage.max_magic = 5;
    Combatant mob;
    mob.level = 10;

    const auto fire = openkal::server::combat::resolve_magic(mage, mob, 2, 1, 18);
    expect(fire.hit, "magic always hits");
    expect(fire.damage == 17, "fire damage equal level no resist");

    const auto fire_half =
        openkal::server::combat::resolve_magic(mage, mob, 2, 1, 18, 0, 600, 1200);
    expect(fire_half.hit, "early recast still hits");
    expect(fire_half.damage == 8, "early recast scales magic by ReviseTick");

    Combatant fire_plant = mob;
    fire_plant.res_fire = 100;
    const auto resisted = openkal::server::combat::resolve_magic(mage, fire_plant, 2, 1, 18);
    expect(resisted.hit, "resisted magic still hits");
    expect(resisted.damage == 15, "fire resist reduces damage");
    expect(resisted.damage < fire.damage, "resist is a damage cap not a miss");

    Combatant ice_plant = mob;
    ice_plant.res_ice = 100;
    const auto ice_vs_fire_resist =
        openkal::server::combat::resolve_magic(mage, ice_plant, 2, 1, 18);
    expect(ice_vs_fire_resist.damage == 17, "ice resist does not affect fire");

    Combatant high = mob;
    high.level = 30;
    const auto vs_high = openkal::server::combat::resolve_magic(mage, high, 2, 1, 18);
    expect(vs_high.hit, "level gap does not miss");
    expect(vs_high.damage == 0, "large level gap zeroes magic damage");

    Combatant dodgy = mob;
    dodgy.dodge = 1000;
    bool magic_ignores_dodge = true;
    for (int i = 0; i < 32; ++i) {
        magic_ignores_dodge = magic_ignores_dodge &&
                              openkal::server::combat::resolve_magic(mage, dodgy, 2, 1, 18).hit;
    }
    expect(magic_ignores_dodge, "magic ignores dodge");

    {
        Combatant monster_mage;
        monster_mage.level = 50;
        monster_mage.min_attack = 310;
        monster_mage.max_attack = 380;
        Combatant target;
        target.level = 50;

        const auto thunder =
            openkal::server::combat::resolve_monster_magic(monster_mage, target, 29, 1, 30);
        expect(thunder.hit && thunder.damage > 0,
               "Demon Worker Thunder Strike resolves as elemental damage");

        target.res_lightning = 100;
        const auto resisted_thunder =
            openkal::server::combat::resolve_monster_magic(monster_mage, target, 29, 1, 30);
        expect(resisted_thunder.damage < thunder.damage,
               "Thunder Strike uses lightning resistance rather than physical defense");

        target.res_lightning = 0;
        target.res_fire = 100;
        const auto fire_attack =
            openkal::server::combat::resolve_monster_magic(monster_mage, target, 31, 1, 30);
        expect(fire_attack.hit && fire_attack.damage > 0,
               "monster Fire Attack resolves as elemental damage");
        expect(openkal::server::combat::is_monster_magic_skill(29),
               "Thunder Strike is classified as monster magic");
        expect(!openkal::server::combat::is_monster_magic_skill(20),
               "unimplemented special monster skill is not classified as magic");
    }

    Combatant knight;
    knight.level = 10;
    knight.min_attack = 20;
    knight.max_attack = 20;
    knight.hit = 0;
    Combatant armored;
    armored.level = 10;
    armored.defense = 5;
    armored.absorb = 0;
    expect(openkal::server::combat::final_damage(knight, armored, 20) == 15,
           "physical still uses defense");

    {
        Combatant pvp_atk;
        pvp_atk.level = 20;
        pvp_atk.is_player = true;
        pvp_atk.clazz = 0;
        pvp_atk.attack_type = 0;
        Combatant pvp_tgt;
        pvp_tgt.level = 20;
        pvp_tgt.is_player = true;
        pvp_tgt.clazz = 0;
        pvp_tgt.defense = 5;
        pvp_tgt.absorb = 0;
        // GetFinalDefense += level/4 = 5; GetFinalAbsorb += 15.
        // damage = 100 - 10 = 90; 90 - 90*15/100 = 77.
        expect(openkal::server::combat::final_defense(pvp_atk, pvp_tgt) == 10,
               "PvP defense adds level / g_denoDef melee knight");
        expect(openkal::server::combat::final_absorb(pvp_atk, pvp_tgt) == 15, "PvP absorb adds 15");
        expect(openkal::server::combat::final_damage(pvp_atk, pvp_tgt, 100) == 77,
               "PvP physical uses extra defense and absorb");
        Combatant mob_atk = pvp_atk;
        mob_atk.is_player = false;
        expect(openkal::server::combat::final_defense(mob_atk, pvp_tgt) == 5,
               "monster vs player uses raw GetDefense");
        expect(openkal::server::combat::final_absorb(mob_atk, pvp_tgt) == 0,
               "monster vs player has no +15 absorb");
        expect(openkal::server::combat::final_damage(mob_atk, pvp_tgt, 100) == 95,
               "monster vs player skips PvP bonuses");
        Combatant archer_atk = pvp_atk;
        archer_atk.attack_type = 1;
        pvp_tgt.defense_range = 20;
        expect(openkal::server::combat::final_defense(archer_atk, pvp_tgt) == 40,
               "ranged vs player uses GetDefense(1) plus level/1");
        expect(openkal::server::combat::player_attack_type(0) == 0, "knight attack type melee");
        expect(openkal::server::combat::player_attack_type(1) == 0, "mage attack type melee");
        expect(openkal::server::combat::player_attack_type(2) == 1, "archer attack type range");
    }

    // compatibility behavior at level 1, DEX 19, base 5.
    expect(openkal::server::combat::skill_physical_power(2, 3, 1, 19, 5) == 71, "shot GetAttack");
    expect(openkal::server::combat::skill_physical_power(2, 4, 3, 40, 100) == 100,
           "Negative Arrow preserves its normal base attack");
    expect(openkal::server::combat::skill_physical_power(2, 6, 1, 19, 5) == 17,
           "flame arrow GetAttack");
    expect(openkal::server::combat::skill_physical_power(2, 9, 2, 40, 100) == 244,
           "stun arrow composes Shot attack");
    expect(openkal::server::combat::skill_physical_power(2, 14, 2, 40, 100) == 162,
           "explosion composes Flame Arrow attack");
    expect(openkal::server::combat::skill_physical_power(2, 9, 2, 40, 100, -1, 4) == 314,
           "Stun Arrow runtime path uses the separately ranked Shot skill");
    expect(openkal::server::combat::skill_physical_power(2, 14, 2, 40, 100, -1, 4) == 186,
           "Explosion runtime path uses the separately ranked Flamy Arrow skill");
    expect(openkal::server::combat::skill_physical_power(2, 21, 2, 40, 100) == 200,
           "mystic arrow GetAttack");
    expect(openkal::server::combat::archer_shot_power(4, 40, 0) == 190,
           "Shot helper keeps its own learned rank");
    expect(openkal::server::combat::archer_flame_arrow_power(4, 0) == 48,
           "Flame Arrow helper keeps its own learned rank");
    expect(openkal::server::combat::archer_stun_arrow_power(2, 4, 40, 100) == 314,
           "Stun Arrow composes separately ranked Shot");
    expect(openkal::server::combat::archer_stun_arrow_power(2, 0, 40, 100) == 0,
           "Stun Arrow requires learned Shot");
    expect(openkal::server::combat::archer_explosion_power(2, 4, 100) == 186,
           "Explosion composes separately ranked Flame Arrow");
    expect(openkal::server::combat::archer_explosion_power(2, 0, 100) == 0,
           "Explosion requires learned Flame Arrow");
    expect(openkal::server::combat::archer_mystic_arrow_power(2, 100) == 200,
           "mystic arrow (SL+2)*(BASE/2)");
    expect(openkal::server::combat::archer_arrow_rain_power(3, 100, 1800) == 6100,
           "Arrow Rain Attack*7 + SL*1800");
    expect(openkal::server::combat::archer_arrow_rain_power(3, 100, 2000) == 6700,
           "Arrow Rain Attack*7 + SL*2000");
    expect(openkal::server::combat::archer_arrow_rain_pvp_damage(1000) == 350,
           "Arrow Rain PvP is 35%");
    expect(openkal::server::combat::archer_poison_cloud_power(1, 400) == 100,
           "Poison Cloud SL1 Attack 400 is (200*SL+50)*Attack/100/10");
    expect(openkal::server::combat::archer_poison_cloud_power(2, 400) == 180,
           "Poison Cloud SL2 Attack 400 is 180 per tick");
    expect(openkal::server::combat::archer_poison_cloud_power(2, 303) == 136,
           "Poison Cloud SL2 Attack 303 is 136 per tick");
    expect(openkal::server::combat::archer_poison_cloud_power(2, 429) == 193,
           "Poison Cloud SL2 Attack 429 is 193 per tick");
    expect(openkal::server::combat::archer_poison_cloud_applied_damage(200, 180) == 180,
           "Poison Cloud applies the full tick above 1 HP");
    expect(openkal::server::combat::archer_poison_cloud_applied_damage(100, 180) == 99,
           "Poison Cloud leaves 1 HP");
    expect(openkal::server::combat::archer_poison_cloud_applied_damage(1, 180) == 0,
           "Poison Cloud does not tick at 1 HP");
    expect(openkal::coords::in_tile_rect(0, 0, 64, 0, 2),
           "Poison Cloud Chebyshev 2 includes 2 tiles");
    expect(!openkal::coords::in_tile_rect(0, 0, 96, 0, 2),
           "Poison Cloud Chebyshev 2 excludes 3 tiles");
    expect(openkal::server::combat::archer_arrow_explosion_power(3, 100, 1000) == 5000,
           "Arrow Explosion Attack*20 + SL*1000");
    expect(openkal::server::combat::archer_arrow_explosion_power(3, 100, 1200) == 5600,
           "Arrow Explosion Attack*20 + SL*1200");
    expect(openkal::server::combat::archer_arrow_explosion_pvp_damage(1000) == 300,
           "Arrow Explosion PvP is 30%");
    expect(openkal::server::combat::archer_shrewdness_dodge_points(200, 70) == 17,
           "Shrewdness is org_dodge * (CL/2) / 400");
    expect(openkal::server::combat::archer_shrewdness_dodge_points(200, 0) == 0,
           "Shrewdness is inactive without a level");
    expect(openkal::server::combat::archer_blessing_power_percent(0) == 0 &&
               openkal::server::combat::archer_blessing_power_percent(1) == 8,
           "Archer Blessing of Son of the Sky is +8%");
    expect(openkal::server::combat::archer_arrow_rain_canceled_by_move(10, 20, 10, 20) == false,
           "Arrow Rain stays while standing");
    expect(openkal::server::combat::archer_arrow_rain_canceled_by_move(10, 20, 11, 20),
           "Arrow Rain cancels on move");
    expect(openkal::coords::in_tile_rect(0, 0, 96, 0, 3),
           "Arrow Rain Chebyshev 3 includes 3 tiles");
    expect(!openkal::coords::in_tile_rect(0, 0, 128, 0, 3),
           "Arrow Rain Chebyshev 3 excludes 4 tiles");
    expect(openkal::coords::in_tile_rect(0, 0, 96, 96, 3),
           "Arrow Rain Chebyshev 3 includes the 3-tile corner");
    expect(openkal::server::combat::skill_physical_power(0, 3, 1, 0, 20) == 28,
           "lightning slash GetAttack");
    expect(openkal::server::combat::skill_physical_power(0, 5, 2, 0, 100) == 350,
           "transcendental GetAttack");
    expect(openkal::server::combat::skill_physical_power(0, 7, 3, 0, 100, 0) == 220,
           "upper slash minimum GetAttack");
    expect(openkal::server::combat::skill_physical_power(0, 7, 3, 0, 100, 3) == 223,
           "upper slash maximum GetAttack");
    expect(openkal::server::combat::skill_physical_power(0, 9, 3, 0, 100) == 180, "edge GetAttack");
    expect(openkal::server::combat::skill_physical_power(0, 13, 3, 0, 100, 0) == 252,
           "fatal wound minimum GetAttack");
    expect(openkal::server::combat::skill_physical_power(0, 13, 3, 0, 100, 3) == 255,
           "fatal wound maximum GetAttack");
    expect(openkal::server::combat::skill_physical_power(0, 16, 2, 0, 100) == 250,
           "upper smash GetAttack");
    expect(openkal::server::combat::skill_physical_power(0, 17, 2, 0, 100) == 100,
           "generic attack helper does not guess Infernal Mortal points");
    expect(openkal::server::combat::knight_infernal_power(2, 100, 0) == 219,
           "infernal GetAttack without Mortal points");
    expect(openkal::server::combat::knight_infernal_power(2, 100, 3) == 384,
           "infernal GetAttack includes Mortal points");
    expect(openkal::server::combat::skill_physical_power(0, 22, 3, 0, 100) == 190,
           "shield attack percentage below cap");
    expect(openkal::server::combat::skill_physical_power(0, 22, 10, 0, 200) == 400,
           "shield attack extra caps at 200");
    expect(openkal::server::combat::skill_physical_power(0, 25, 3, 0, 100) == 130,
           "pull swing per-target attack");
    expect(openkal::server::combat::skill_physical_power(0, 41, 1, 0, 100, 5) == 1321,
           "powerful widening wound GetAttack lv1");
    expect(openkal::server::combat::skill_physical_power(0, 41, 5, 0, 100, 10) == 2150,
           "powerful widening wound GetAttack lv5");
    expect(openkal::server::combat::skill_physical_power(0, 41, 1, 0, 100) != 100,
           "powerful widening wound does not fall through to base attack");
    for (int i = 0; i < 16; ++i) {
        const auto upper = openkal::server::combat::skill_physical_power(0, 7, 3, 0, 100);
        const auto wound = openkal::server::combat::skill_physical_power(0, 13, 3, 0, 100);
        expect(upper >= 220 && upper <= 223, "upper slash random term stays in rank bounds");
        expect(wound >= 252 && wound <= 255, "fatal wound random term stays in rank bounds");
    }
    expect(openkal::server::combat::skill_physical_hit(2, 3, 1) == 2, "shot GetHit");
    expect(openkal::server::combat::skill_physical_hit(2, 14, 2) == 4, "explosion GetHit");
    expect(openkal::server::combat::skill_physical_delay_ms(2, 3, 2, 900, 700) == 700,
           "shot rank reduces source delay");
    expect(openkal::server::combat::skill_physical_delay_ms(2, 9, 2, 900, 700) == 700,
           "stun arrow rank reduces source delay");
    expect(openkal::server::combat::skill_physical_hit(0, 3, 2) == 1, "lightning slash GetHit");
    expect(openkal::server::combat::skill_physical_hit(0, 5, 3) == 6, "transcendental GetHit");
    expect(openkal::server::combat::skill_physical_hit(0, 7, 3) == 4, "upper slash GetHit");
    expect(openkal::server::combat::skill_physical_hit(0, 9, 3) == 12, "edge GetHit");
    expect(openkal::server::combat::skill_physical_hit(0, 13, 3) == 6, "fatal wound GetHit");
    expect(openkal::server::combat::skill_physical_hit(0, 16, 3) == 10, "upper smash GetHit");
    expect(openkal::server::combat::skill_physical_hit(0, 41, 3) == 19,
           "powerful widening wound GetHit");
    expect(openkal::server::combat::skill_physical_hit(0, 17, 3) == 10, "infernal GetHit");
    expect(openkal::server::combat::skill_physical_hit(0, 25, 3) == 5, "pull swing GetHit");
    expect(openkal::server::combat::skill_physical_fatal(0, 5, 3) == 5,
           "transcendental GetFatalRate");
    expect(openkal::server::combat::skill_physical_delay_ms(0, 3, 1, 0, 700) == 700,
           "lightning slash GetDelay aspeed");
    expect(openkal::server::combat::skill_physical_delay_ms(0, 41, 1, 0, 700) == 700,
           "powerful widening wound GetDelay aspeed");
    expect(openkal::server::combat::is_knight_taunt(0, 4), "mockery is a taunt");
    expect(openkal::server::combat::is_knight_taunt(0, 8), "provocation is a taunt");
    expect(!openkal::server::combat::is_knight_taunt(0, 3), "lightning slash is not a taunt");
    expect(openkal::server::combat::skill_taunt_hostility(4, 1) == 200, "mockery GetHostility lv1");
    expect(openkal::server::combat::skill_taunt_hostility(8, 3) == 1400,
           "provocation GetHostility");
    expect(openkal::server::combat::skill_taunt_range(4, 1) == 122, "mockery compare range lv1");
    {
        const auto mastery = openkal::server::combat::knight_weapon_mastery_contribution(5);
        expect(mastery.hit == 2 && mastery.min_attack == 3 && mastery.max_attack == 3,
               "weapon mastery passive contributions");
        const auto critical = openkal::server::combat::knight_critical_contribution(12);
        expect(critical.fatal_pt == 12 && critical.fatal_rate == 10,
               "critical passive contribution and rate cap");
        expect(openkal::server::combat::knight_advanced_critical_fatal_pt() == 20,
               "advanced critical fixed fatal contribution");
        expect(openkal::server::combat::knight_life_extension_hp(200, 3) == 88,
               "life extension uses revised HTH HP contribution");
    }
    {
        const auto bow = openkal::server::combat::archer_bow_mastery_contribution(5);
        expect(bow.min_attack == 2 && bow.max_attack == 5,
               "Bow Mastery adds rank half to min and rank to max");
        expect(openkal::server::combat::archer_concentration_hit(5) == 10,
               "Archer Concentration adds twice rank hit");
        const auto evade = openkal::server::combat::archer_evasion_contribution(3, 2);
        expect(evade.base_percent == 10 && evade.advanced_percent == 2,
               "both Archer perfect-evasion tiers");
        const auto fatal = openkal::server::combat::archer_fatal_contribution(12, true);
        expect(fatal.fatal_pt == 12 && fatal.fatal_rate == 10 && fatal.advanced_fatal_pt == 80,
               "both Archer fatal tiers");
        expect(openkal::server::combat::archer_attack_range_bonus(3) == 60,
               "Range Extension adds twenty per rank");
        expect(openkal::server::combat::archer_life_extension_hp(100, 2) == 522,
               "Archer Life Extension starts from source base HTH expression");
    }
    {
        static constexpr int kElapsed[] = {
            999, 1000, 1699, 1700, 2299, 2300, 2599, 2600, 2899, 2900};
        static constexpr int kTier[] = {0, 1, 1, 2, 2, 3, 3, 4, 4, 5};
        for (std::size_t i = 0; i < sizeof(kElapsed) / sizeof(kElapsed[0]); ++i) {
            expect(openkal::server::combat::archer_focus_shot_tier(kElapsed[i]) == kTier[i],
                   "Focus Shot source charge boundary");
        }
        static constexpr std::int32_t kPower[] = {266, 337, 411, 482, 555};
        for (int tier = 1; tier <= 5; ++tier) {
            expect(openkal::server::combat::archer_focus_shot_power(2, 100, tier) ==
                       kPower[tier - 1],
                   "Focus Shot tier attack formula");
            expect(openkal::server::combat::archer_focus_shot_hit(2, tier) == 2 * tier,
                   "Focus Shot tier hit formula");
        }
        expect(openkal::server::combat::archer_focus_shot_power(100, 1000, 5) == 3000,
               "Focus Shot extra attack caps at 2000");
        expect(openkal::server::combat::archer_focus_shot_power(2, 100, 0) == 0,
               "Focus Shot rejects invalid charge tier");
        expect(openkal::server::combat::archer_skill_hostility(3) == 100,
               "Shot hostility override");
        expect(openkal::server::combat::archer_skill_hostility(9) == 50,
               "Stun Arrow hostility override");
        expect(openkal::server::combat::archer_skill_hostility(14) == 75,
               "Explosion hostility override");
        expect(openkal::server::combat::archer_skill_hostility(16) == 0,
               "Focus Shot uses default hostility");
        expect(openkal::server::combat::archer_mystic_spirit_tick(101) == 10,
               "Mystic Arrow stores one fifth of half base attack");
        expect(openkal::server::combat::archer_mystic_arrow_power(4, 100) == 300,
               "server Mysterious Arrow formula is (SL+2)*(BASE/2); client volley is 3");
        expect(openkal::server::combat::revised_attack_delay_ms(700, -30) == 910,
               "Mesh of Speed GetASpeed lengthens delay");
        expect(openkal::server::combat::revised_attack_delay_ms(700, 0) == 700,
               "unrevised attack delay");
        expect(openkal::server::combat::archer_chance_curse_delta(40, 2) == -30,
               "Curse of Critical Hit uses target fatal rate");
    }
    {
        openkal::server::GameWorld world;
        const std::string skills_dir = std::string(OPENKAL_TEST_RESOURCE_ROOT) + "/config/skills";
        expect(world.load_skills_json_dir(skills_dir), "load skill json catalog");
        const openkal::server::SkillDef* thunder = nullptr;
        const openkal::server::SkillDef* thunder_storm = nullptr;
        const openkal::server::SkillDef* fire_storm = nullptr;
        const openkal::server::SkillDef* assault = nullptr;
        const openkal::server::SkillDef* refining_weapon = nullptr;
        const openkal::server::SkillDef* cold_binding = nullptr;
        const openkal::server::SkillDef* spin_slash = nullptr;
        const openkal::server::SkillDef* widening_wound = nullptr;
        const openkal::server::SkillDef* provocation_of_blow = nullptr;
        const openkal::server::SkillDef* flamy = nullptr;
        for (const auto& s : world.skill_defs()) {
            if (s.clazz == 0 && s.index == 3) {
                thunder = &s;
            }
            if (s.clazz == 1 && s.index == 43) {
                thunder_storm = &s;
            }
            if (s.clazz == 1 && s.index == 48) {
                fire_storm = &s;
            }
            if (s.clazz == 3 && s.index == 7) {
                assault = &s;
            }
            if (s.clazz == 1 && s.index == 38) {
                refining_weapon = &s;
            }
            if (s.clazz == 1 && s.index == 37) {
                cold_binding = &s;
            }
            if (s.clazz == 0 && s.index == 38) {
                spin_slash = &s;
            }
            if (s.clazz == 0 && s.index == 41) {
                widening_wound = &s;
            }
            if (s.clazz == 0 && s.index == 42) {
                provocation_of_blow = &s;
            }
            if (s.clazz == 2 && s.index == 6) {
                flamy = &s;
            }
        }
        expect(thunder != nullptr && thunder->usable && thunder->compare_cooldown_ms == 700,
               "lightning slash compare[3] icon cooldown");
        expect(thunder_storm != nullptr && !thunder_storm->cast_range_by_level.empty() &&
                   thunder_storm->cast_range_by_level[0] == 190,
               "thunder storm compare[1] range is 190");
        expect(fire_storm != nullptr && !fire_storm->cast_range_by_level.empty() &&
                   fire_storm->cast_range_by_level[0] == 180,
               "fire storm compare[1] range is 180");
        expect(assault != nullptr && assault->usable && assault->mp == 0 &&
                   assault->compare_cooldown_ms == 30000,
               "thief assault is use-type with compare cooldown");
        const openkal::server::SkillDef* mockery = nullptr;
        for (const auto& s : world.skill_defs()) {
            if (s.clazz == 0 && s.index == 4) {
                mockery = &s;
            }
        }
        expect(mockery != nullptr && mockery->mp == 10 && mockery->delay_ms == 5000,
               "mockery is a timed taunt, not a blow");
        expect(refining_weapon != nullptr && refining_weapon->damage_type == 2 &&
                   refining_weapon->target_kinds.size() == 1 &&
                   refining_weapon->target_kinds[0] == 1 && refining_weapon->property.size() == 6 &&
                   refining_weapon->cast_range_by_level.size() == 5 &&
                   refining_weapon->cast_range_by_level[2] == 100,
               "server keeps refining weapon target and range metadata");
        expect(cold_binding != nullptr && cold_binding->damage_type == 3 &&
                   cold_binding->target_kinds.size() == 2 &&
                   cold_binding->cast_range_by_level.size() == 4 &&
                   cold_binding->cast_range_by_level[3] == 210,
               "server keeps cold binding area metadata");
        expect(spin_slash != nullptr && spin_slash->damage_type == 1 &&
                   !spin_slash->cast_range_by_level.empty() &&
                   spin_slash->cast_range_by_level[0] == 400,
               "spin slash compare[2] cast range is 400");
        expect(widening_wound != nullptr && widening_wound->usable &&
                   widening_wound->damage_type == 1,
               "widening wound remains classified as physical active skill");
        expect(provocation_of_blow != nullptr &&
                   !provocation_of_blow->cast_range_by_level.empty() &&
                   provocation_of_blow->cast_range_by_level[0] == 400,
               "Provocation of Blow compare[2] cast range is 400");
        expect(flamy != nullptr && flamy->delay_pre_ms == 800,
               "Flamy Arrow delay[0] is the initial 800 ms auto-repeat");
        const openkal::server::SkillDef* di6 = nullptr;
        for (const auto& s : world.skill_defs()) {
            if (s.clazz == 1 && s.index == 63) {
                di6 = &s;
            }
        }
        expect(di6 != nullptr && di6->mp == 102 && di6->delay_pre_ms == 2200 && di6->value1 == 3 &&
                   di6->value2 == 21,
               "defensive incantation 6 is castable with absorb 3 / def 21");
        expect(di6 != nullptr && (di6->mp > 0 || di6->delay_pre_ms > 0),
               "DI6 satisfies skill_is_castable (mp or delay_pre)");
    }
    {
        openkal::server::GameWorld world;
        openkal::server::MonsterEntity first;
        first.id = 1;
        first.hp = 100;
        first.x = 100;
        first.y = 100;
        first.map = 0;
        first.race = static_cast<int>(openkal::protocol::MonsterRace::Maguni);
        openkal::server::MonsterEntity edge = first;
        edge.id = 2;
        edge.x = 164;
        openkal::server::MonsterEntity outside = first;
        outside.id = 3;
        outside.x = 165;
        openkal::server::MonsterEntity other_map = first;
        other_map.id = 4;
        other_map.map = 1;
        openkal::server::MonsterEntity magic_field = first;
        magic_field.id = 5;
        magic_field.race = static_cast<int>(openkal::protocol::MonsterRace::Magic);
        world.monsters() = {first, edge, outside, other_map, magic_field};
        const auto selected = world.monsters_in_radius(100, 100, 0, 64);
        expect(selected.size() == 2 && selected[0] == 1 && selected[1] == 2,
               "mage area selects living combat monsters inside radius");
        const auto capped = world.monsters_in_radius(100, 100, 0, 64, 1);
        expect(capped.size() == 1 && capped[0] == 1, "mage area target cap");
        expect(world.apply_monster_control(1, 8.0f, true), "apply Frost movement lock");
        expect(world.find_monster(1)->dirty_slow, "control sets MState dirty for CMS_MOVESTOP");
        std::vector<openkal::server::MonsterMoveEvent> moves;
        world.tick(1.0f, moves);
        expect(world.find_monster(1)->control_left == 7.0f, "control duration ticks");
        (void) world.apply_player_attack(99, 1, 1, 100, 100);
        expect(world.find_monster(1)->control_left == 0.0f,
               "positive damage breaks configured control");
        expect(world.find_monster(1)->aggro, "damage establishes hostility");
        expect(world.find_monster(1)->hostility_by_player.at(99) == 1,
               "damage records per-player reward hostility");
        expect(world.find_monster(1)->reward_damage_by_player.at(99) == 1,
               "damage records actual HP loss separately from hostility");
        expect(world.apply_monster_hostility(99, 1, 1700, 100, 100),
               "Knight taunt applies quantified hostility");
        expect(world.find_monster(1)->aggro_hostility == 1701,
               "skill hostility accumulates with damage like CMonsterReal");
        expect(world.find_monster(1)->reward_damage_by_player.at(99) == 1,
               "skill hostility does not fabricate reward damage");
        expect(world.apply_monster_hostility(100, 1, 2000, 100, 100),
               "a second player can build independent hostility");
        expect(world.find_monster(1)->aggro_player_id == 100 &&
                   world.find_monster(1)->aggro_hostility == 2000,
               "monster targets the player with the highest hostility");
        (void) world.apply_player_attack(101, 1, 5, 1000, 1000);
        expect(world.find_monster(1)->hostility_by_player.count(101) == 0,
               "out-of-sight damage creates no reward hostility");
        world.clear_aggro_on_player(100);
        expect(world.find_monster(1)->aggro_player_id == 99 &&
                   world.find_monster(1)->aggro_hostility == 1701,
               "removing the top player retargets the next-highest hostility");
        expect(world.clear_monster_aggro(1), "Amnesia clears one monster");
        expect(!world.find_monster(1)->aggro && world.find_monster(1)->aggro_player_id == 0 &&
                   world.find_monster(1)->aggro_hostility == 0 &&
                   world.find_monster(1)->hostility_by_player.empty() &&
                   world.find_monster(1)->reward_damage_by_player.empty(),
               "Amnesia removes hostility without damage");

        openkal::server::MonsterEntity reward_monster;
        reward_monster.max_hp = 1000;
        reward_monster.reward_damage_by_player.emplace(99, 600);
        auto coverage = openkal::server::calculate_monster_reward_coverage(reward_monster, 1000);
        expect(coverage.credited_damage == 600 && coverage.exp_pool == 600 &&
                   coverage.loot_access == openkal::server::MonsterLootAccess::HostilityOwner,
               "60 percent credited damage grants 60 percent EXP and owned loot");
        reward_monster.reward_damage_by_player[99] = 499;
        coverage = openkal::server::calculate_monster_reward_coverage(reward_monster, 1000);
        expect(coverage.exp_pool == 499 &&
                   coverage.loot_access == openkal::server::MonsterLootAccess::ServerLocked,
               "21 through 49 percent damage creates server-locked loot");
        reward_monster.reward_damage_by_player[99] = 500;
        coverage = openkal::server::calculate_monster_reward_coverage(reward_monster, 1000);
        expect(coverage.loot_access == openkal::server::MonsterLootAccess::HostilityOwner,
               "exactly 50 percent damage creates contributor-owned loot");
        reward_monster.reward_damage_by_player[99] = 210;
        coverage = openkal::server::calculate_monster_reward_coverage(reward_monster, 1000);
        expect(coverage.loot_access == openkal::server::MonsterLootAccess::ServerLocked,
               "exactly 21 percent damage creates server-locked loot");
        reward_monster.reward_damage_by_player[99] = 209;
        coverage = openkal::server::calculate_monster_reward_coverage(reward_monster, 1000);
        expect(coverage.exp_pool == 209 &&
                   coverage.loot_access == openkal::server::MonsterLootAccess::None,
               "less than 21 percent damage grants proportional EXP without loot");

        openkal::server::DropOutcome drop;
        drop.item_index = 42;
        drop.count = 1;
        drop.x = 100;
        drop.y = 100;
        auto& ground = world.spawn_ground_item(900, drop, 99, 120.0f, 180.0f, 0, 7);
        expect(ground.owner_player_id == 99 && ground.owner_party_id == 7 &&
                   ground.ownership_left == 120.0f,
               "monster loot starts with player and party ownership");
        std::vector<std::uint32_t> removed_items;
        world.tick_ground_items(119.0f, removed_items);
        expect(world.find_ground_item(900)->owner_player_id == 99,
               "drop ownership remains exclusive before 120 seconds");
        world.tick_ground_items(1.0f, removed_items);
        expect(world.find_ground_item(900)->owner_player_id == 0 &&
                   world.find_ground_item(900)->owner_party_id == 0,
               "drop ownership opens to everyone after 120 seconds");
        auto& server_ground = world.spawn_ground_item(901, drop, 0, 120.0f, 180.0f, 0, 0, true);
        expect(server_ground.server_owned && server_ground.ownership_left == 120.0f,
               "partial-credit loot starts locked to the server");
        world.tick_ground_items(119.0f, removed_items);
        expect(world.find_ground_item(901)->server_owned,
               "server-owned loot rejects every player before 120 seconds");
        world.tick_ground_items(1.0f, removed_items);
        expect(!world.find_ground_item(901)->server_owned &&
                   world.find_ground_item(901)->ownership_left == 0.0f,
               "server-owned loot opens to everyone after 120 seconds");
        openkal::server::MonsterEntity corner = first;
        corner.id = 6;
        corner.x = 228;
        corner.y = 228;
        openkal::server::MonsterEntity beyond = first;
        beyond.id = 7;
        beyond.x = 256;
        beyond.y = 256;
        world.monsters() = {first, edge, outside, other_map, magic_field, corner, beyond};
        const auto rect = world.monsters_in_tile_rect(100, 100, 0, 4);
        expect(rect.size() == 4, "storm tile rect keeps living combat monsters in 9x9");
        bool saw_corner = false;
        bool saw_beyond = false;
        bool saw_magic = false;
        for (const std::uint32_t id : rect) {
            saw_corner = saw_corner || id == 6;
            saw_beyond = saw_beyond || id == 7;
            saw_magic = saw_magic || id == 5;
        }
        expect(saw_corner && !saw_beyond && !saw_magic,
               "storm tile rect includes Chebyshev 4 and skips magic fields");
        openkal::server::MonsterEntity near = first;
        near.id = 8;
        near.x = 100;
        near.y = 100;
        openkal::server::MonsterEntity splash = first;
        splash.id = 9;
        splash.x = 164;
        splash.y = 100;
        openkal::server::MonsterEntity blow_selected = first;
        blow_selected.id = 10;
        blow_selected.x = 380;
        blow_selected.y = 100;
        openkal::server::MonsterEntity blow_beyond_range = first;
        blow_beyond_range.id = 11;
        blow_beyond_range.x = 560;
        blow_beyond_range.y = 100;
        const int blow_tiles = openkal::protocol::knight_area_radius_tiles(42);
        world.monsters() = {near, splash, blow_selected, blow_beyond_range};
        const auto blow = world.monsters_in_tile_rect(100, 100, 0, blow_tiles);
        expect(blow.size() == 3, "Provocation of Blow square keeps the 400px pack");
        bool saw_near = false;
        bool saw_splash = false;
        bool saw_blow_selected = false;
        bool saw_blow_beyond = false;
        for (const std::uint32_t id : blow) {
            saw_near = saw_near || id == 8;
            saw_splash = saw_splash || id == 9;
            saw_blow_selected = saw_blow_selected || id == 10;
            saw_blow_beyond = saw_blow_beyond || id == 11;
        }
        expect(saw_near && saw_splash && saw_blow_selected && !saw_blow_beyond,
               "Provocation of Blow splash matches the 400px caster circle");
        expect(openkal::server::combat::in_attack_range(100, 100, 380, 100, 400),
               "selected target 280px away is inside Provocation of Blow range");
        expect(openkal::coords::in_tile_rect(100, 100, 380, 100, blow_tiles),
               "selected target inside the 400px circle takes splash damage");
        expect(!openkal::server::combat::in_attack_range(100, 100, 560, 100, 400),
               "target 460px away is outside Provocation of Blow range");
    }
    expect(openkal::server::combat::skill_physical_fatal(2, 3, 1) == 10, "shot GetFatalRate");
    expect(openkal::server::combat::skill_physical_fatal(2, 16, 1) == 6,
           "gather shot GetFatalRate");

    bool fatal = true;
    expect(openkal::server::combat::get_fatal_extra(100, 0, 0, 0, fatal) == 0,
           "no EB no skill fatal");
    expect(!fatal, "rate 0 never fatals");
    fatal = false;
    expect(openkal::server::combat::get_fatal_extra(100, 100, 0, 0, fatal) == 50,
           "weapon EB 100% adds 50% at pt 0");
    expect(fatal, "rate 100 always fatals");
    fatal = false;
    expect(openkal::server::combat::get_fatal_extra(100, 0, 5, 100, fatal) == 60,
           "skill fatal 100% uses 2*pt+50");
    expect(fatal, "add_fatal 100 always fatals");

    Combatant archer = knight;
    archer.hit = 1000;
    archer.fatal_rate = 0;
    archer.fatal_pt = 0;
    Combatant dummy = armored;
    dummy.dodge = 0;
    dummy.defense = 0;
    dummy.absorb = 0;
    {
        openkal::server::combat::AttackResult r;
        for (int i = 0; i < 24; ++i) {
            r = openkal::server::combat::resolve_physical(archer, dummy, 0, 100, 20, 0, 0);
            if (r.hit) {
                break;
            }
        }
        expect(r.hit, "physical skill fatal can hit");
        expect(r.fatal, "skill GetFatalRate 100% is a critical");
        expect(!r.explosive, "critical without weapon EB is not explosive blow");
        expect(r.damage == 30, "fatal extra +50% on 20");
    }
    archer.fatal_rate = 10;
    {
        openkal::server::combat::AttackResult r;
        for (int i = 0; i < 24; ++i) {
            r = openkal::server::combat::resolve_physical(archer, dummy, 0, 100, 20, 0, 0);
            if (r.hit) {
                break;
            }
        }
        expect(r.hit && r.fatal && !r.explosive,
               "critical hit without weapon EB is not explosive blow");
    }
    archer.fatal_rate = 0;
    {
        openkal::server::combat::AttackResult scaled;
        for (int i = 0; i < 24; ++i) {
            scaled = openkal::server::combat::resolve_physical(archer, dummy, 0, 0, 100, 400, 800);
            if (scaled.hit) {
                break;
            }
        }
        expect(scaled.hit, "scaled physical still hits");
        expect(scaled.damage == 50, "physical ReviseTick halves raw attack");
        expect(!scaled.fatal, "no fatal when rate and add_fatal are 0");
    }

    expect(openkal::server::combat::clamp_nonneg_i32(-1) == 0, "negative clamp to 0");
    expect(openkal::server::combat::clamp_nonneg_i32(0) == 0, "zero stays 0");
    expect(openkal::server::combat::clamp_nonneg_i32(
               static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()) + 1) ==
               std::numeric_limits<std::int32_t>::max(),
           "overflow clamp to INT32_MAX");

    {
        const std::int32_t hp = openkal::server::combat::max_hp_from_stats(50000, 1, 0);
        expect(hp == 500000132, "high hth hp uses int64 (no 2*hth*hth wrap)");
        const std::int32_t overflow_hp = openkal::server::combat::max_hp_from_stats(110000, 1, 0);
        expect(overflow_hp == std::numeric_limits<std::int32_t>::max(),
               "max hp clamps at INT32_MAX");
    }

    {
        // Demon Shock Trooper: hp=23 hth=31 level=22. initial
        // compatibility behavior + compatibility behavior → 711, not 330.
        expect(openkal::server::combat::monster_max_hp(23, 31, 22) == 711,
               "shock trooper max hp includes 52*level/3");
        openkal::server::combat::BaseStats primaries;
        primaries.str = 76;
        primaries.hth = 31;
        primaries.intl = 8;
        primaries.wis = 8;
        primaries.dex = 21;
        std::int32_t min_atk = 70;
        std::int32_t max_atk = 71;
        std::int32_t hit = 26;
        std::int32_t dodge = 22;
        std::int32_t min_magic = 0;
        std::int32_t max_magic = 0;
        std::int32_t res_fire = 13;
        std::int32_t res_ice = 13;
        std::int32_t res_lightning = 15;
        std::int32_t res_curse = 15;
        std::int32_t res_palsy = 15;
        std::int32_t max_hp = 0;
        openkal::server::combat::revise_monster_properties(22,
                                                           primaries,
                                                           23,
                                                           min_atk,
                                                           max_atk,
                                                           hit,
                                                           dodge,
                                                           min_magic,
                                                           max_magic,
                                                           res_fire,
                                                           res_ice,
                                                           res_lightning,
                                                           res_curse,
                                                           res_palsy,
                                                           max_hp);
        expect(max_hp == 711, "revised monster hp");
        expect(res_fire == 130, "monster definition resist stored *10");
        expect(res_palsy == 153, "palsy resist *10 plus HTH/9");
        expect(openkal::server::combat::monster_final_resist(res_fire) == 13,
               "GetFinalResist is json resist");
        expect(openkal::server::combat::monster_final_resist(res_palsy) == 15,
               "GetFinalResist palsy");
        expect(dodge == 29, "DEX/3 added to monster dodge");
        expect(hit == 49, "STR/DEX hit revise");
        expect(min_atk == 112, "STR/DEX/level min attack revise");
        expect(max_atk == 135, "STR/DEX/level max attack revise");
        primaries.intl = 90;
        res_fire = 13;
        std::int32_t unused_hp = 0;
        std::int32_t unused_min = 0;
        std::int32_t unused_max = 0;
        std::int32_t unused_hit = 0;
        std::int32_t unused_dodge = 0;
        std::int32_t unused_min_m = 0;
        std::int32_t unused_max_m = 0;
        std::int32_t unused_ice = 0;
        std::int32_t unused_lit = 0;
        std::int32_t unused_curse = 0;
        std::int32_t unused_palsy = 0;
        openkal::server::combat::revise_monster_properties(1,
                                                           primaries,
                                                           0,
                                                           unused_min,
                                                           unused_max,
                                                           unused_hit,
                                                           unused_dodge,
                                                           unused_min_m,
                                                           unused_max_m,
                                                           res_fire,
                                                           unused_ice,
                                                           unused_lit,
                                                           unused_curse,
                                                           unused_palsy,
                                                           unused_hp);
        expect(res_fire == 140, "INT/9 adds to stored fire resist");
        expect(openkal::server::combat::monster_final_resist(res_fire) == 14,
               "INT raises GetFinalResist by 1 per 90 INT");
    }

    {
        Combatant attacker;
        attacker.level = 10;
        Combatant tank;
        tank.level = 10;
        tank.defense = 100;
        expect(openkal::server::combat::final_damage(attacker, tank, 20) == 1,
               "defense above attack leaves the final minimum damage");
    }

    {
        using namespace openkal::server::combat;
        constexpr std::int32_t max32 = std::numeric_limits<std::int32_t>::max();
        constexpr std::int64_t max64 = std::numeric_limits<std::int64_t>::max();
        struct Sample {
            std::int32_t stat;
            std::int64_t damage;
        };
        constexpr Sample defense_samples[] = {{0, 100},
                                              {25, 75},
                                              {50, 50},
                                              {60, 40},
                                              {61, 39},
                                              {70, 32},
                                              {80, 26},
                                              {90, 22},
                                              {100, 20},
                                              {200, 8},
                                              {500, 3},
                                              {1000, 1},
                                              {5000, 0}};
        for (const auto& sample : defense_samples) {
            expect(mitigate_defense(100, sample.stat) == sample.damage,
                   "defense curve matches the documented truncated sample before the floor");
        }
        constexpr Sample absorb_samples[] = {{0, 100},
                                             {15, 85},
                                             {40, 60},
                                             {41, 59},
                                             {50, 51},
                                             {60, 45},
                                             {80, 36},
                                             {100, 30},
                                             {200, 16},
                                             {1000, 3}};
        for (const auto& sample : absorb_samples) {
            expect(mitigate_absorb(100, sample.stat) == sample.damage,
                   "absorb curve matches the documented truncated sample");
        }
        expect(mitigate_defense(0, 100) == 0 && mitigate_defense(-1, 0) == 0 &&
                   mitigate_defense(std::numeric_limits<std::int64_t>::min(), max32) == 0 &&
                   mitigate_defense(100, -1) == 100 && mitigate_defense(20, 100) == 0,
               "defense normalizes non-positive inputs without applying the final floor");
        expect(mitigate_absorb(0, 100) == 0 && mitigate_absorb(-1, 0) == 0 &&
                   mitigate_absorb(std::numeric_limits<std::int64_t>::min(), max32) == 0 &&
                   mitigate_absorb(100, -1) == 100 && mitigate_absorb(2, 40) == 2 &&
                   mitigate_absorb(2, 41) == 1,
               "absorb preserves legacy ceiling and tail truncation at the join");
        expect(mitigate_defense(1518500000, 1000000000) == 529850294 &&
                   mitigate_defense(1518500001, 1000000000) == 529850295 &&
                   mitigate_defense(3579139411LL, max32) == 1431655764 &&
                   mitigate_defense(max64, max32) == max64 - max32,
               "large defense calculations stay exact without an overflow fallback");
        expect(mitigate_absorb(max64, 15) == 7839866231326559436LL &&
                   mitigate_absorb(max64, 40) == 5534023222112865485LL &&
                   mitigate_absorb(max64, 41) == 5443301529947080804LL &&
                   mitigate_absorb(max64, 100) == 2767011611056432742LL &&
                   mitigate_absorb(max64, max32) == 154618821288LL,
               "absorb keeps exact remainders even for maximum signed 64-bit damage");

        for (std::int32_t attack = 1; attack <= 200; ++attack) {
            std::int64_t previous_defense_damage = attack;
            std::int64_t previous_absorb_damage = attack;
            for (std::int32_t stat = 0; stat <= 400; ++stat) {
                const auto defense_damage = mitigate_defense(attack, stat);
                const auto absorb_damage = mitigate_absorb(attack, stat);
                expect(defense_damage >= 0 && defense_damage <= previous_defense_damage &&
                           absorb_damage >= 0 && absorb_damage <= previous_absorb_damage,
                       "increasing defensive stats never increases remaining damage");
                expect(mitigate_defense(attack + 1, stat) >= defense_damage &&
                           mitigate_absorb(attack + 1, stat) >= absorb_damage,
                       "increasing incoming damage never decreases mitigation results");
                previous_defense_damage = defense_damage;
                previous_absorb_damage = absorb_damage;
                if (stat * 5 <= attack * 3) {
                    for (std::int32_t absorb = 0; absorb <= 40; ++absorb) {
                        const auto flat = attack - stat;
                        expect(mitigate_absorb(defense_damage, absorb) ==
                                   flat - flat * absorb / 100,
                               "the entire bounded legacy band remains bit-identical");
                    }
                }
            }
        }

        Combatant attacker;
        Combatant target;
        target.defense = 100;
        target.absorb = 15;
        expect(final_damage(attacker, target, 100) == 17,
               "equal defense and attack still leave damage after absorb");
        target.absorb = 100;
        expect(final_damage(attacker, target, 100) == 6,
               "one hundred absorb does not erase the defense remainder");
        target.defense = max32;
        target.absorb = max32;
        target.armor_broken = true;
        attacker.bstate = openkal::protocol::kCbsPayUpAttack;
        expect(final_damage(attacker, target, 1) == 1 && final_damage(attacker, target, 0) == 0 &&
                   final_damage(attacker, target, -1) == 0,
               "only a positive revised attack receives the final floor after all modifiers");
        attacker.level = 99;
        target.level = 0;
        target.defense = 100;
        target.absorb = 0;
        target.armor_broken = false;
        attacker.bstate = 0;
        expect(final_damage(attacker, target, max32) == max32 - 1,
               "level revision above INT32_MAX is not clamped before mitigation");
        expect(final_damage(attacker, target, 0) == 19,
               "positive level revision is applied before testing for zero attack");
        target.defense = max32;
        expect(final_damage(attacker, target, max32) == 429496773,
               "a large revised attack follows the exact defense tail");
        target.defense = 0;
        target.armor_broken = true;
        attacker.bstate = openkal::protocol::kCbsPayUpAttack;
        expect(final_damage(attacker, target, max32) == max32,
               "both damage bonuses use 64-bit intermediates before saturating storage");
        attacker.level = target.level;
        expect(final_damage(attacker, target, 110) == 126,
               "Armor Breaker truncation precedes the ten-percent attack bonus");
        attacker.level = 0;
        target.level = 99;
        expect(final_damage(attacker, target, 1) == 0,
               "a level revision that removes positive raw attack does not receive the floor");
    }

    expect(openkal::server::combat::resource_cost(6, true) == 0, "godmode mp cost is 0");
    expect(openkal::server::combat::incoming_damage(25, true) == 0, "godmode damage is 0");

    {
        using openkal::server::prefix_target_matches;
        using openkal::server::PrefixCombatDef;
        using openkal::server::roll_change_prefix;
        expect(roll_change_prefix(0, 16, 8, 55, 2, 1) == 8, "talisman roll 1 is first prefix");
        expect(roll_change_prefix(0, 16, 8, 55, 2, 16) == 8, "talisman roll 16 is first prefix");
        expect(roll_change_prefix(0, 16, 8, 55, 2, 17) == 2, "talisman roll 17 is second prefix");
        expect(roll_change_prefix(0, 16, 8, 55, 2, 55) == 2, "talisman roll 55 is second prefix");
        expect(roll_change_prefix(0, 16, 8, 55, 2, 56) == 0, "talisman roll 56 fails");
        expect(roll_change_prefix(0, 16, 8, 55, 2, 100) == 0, "talisman roll 100 fails");
        expect(prefix_target_matches("weapon", true, false, false),
               "weapon talisman matches sword");
        expect(!prefix_target_matches("weapon", false, true, false),
               "weapon talisman rejects armor");
        expect(prefix_target_matches("defense", false, true, false),
               "defense talisman matches armor");
        expect(prefix_target_matches("ornament", false, false, true),
               "ornament talisman matches ring");
        expect(!openkal::server::item_uses_endurance(true, 10), "ornaments never use endurance");
        expect(openkal::server::item_uses_endurance(false, 36),
               "weapons use item definition endurance");
        {
            const auto intact = openkal::server::apply_prefix_fail_endurance(true, 10, 10, 5);
            expect(!intact.destroyed && intact.cur_end == 10,
                   "failed prefix does not cut ornament endurance");
            const auto nick = openkal::server::apply_prefix_fail_endurance(false, 3, 36, 5);
            expect(nick.destroyed && nick.cur_end == 0, "failed prefix can destroy weapons");
            const auto dent = openkal::server::apply_prefix_fail_endurance(false, 20, 36, 5);
            expect(!dent.destroyed && dent.cur_end == 15, "failed prefix cuts weapon endurance");
        }
        PrefixCombatDef king;
        king.str = 6;
        king.hth = 6;
        king.hp = 100;
        king.defense = 5;
        int str = 19;
        int hth = 17;
        int intl = 9;
        int wis = 9;
        int dex = 11;
        king.apply_primaries(str, hth, intl, wis, dex);
        expect(str == 25 && hth == 23, "king prefix primaries");
        int hit = 0;
        int min_attack = 0;
        int max_attack = 0;
        int min_magic = 0;
        int max_magic = 0;
        int defense = 0;
        int absorb = 0;
        int res_fire = 0;
        int res_ice = 0;
        int res_lightning = 0;
        int res_curse = 0;
        int res_palsy = 0;
        king.apply_derived(hit,
                           min_attack,
                           max_attack,
                           min_magic,
                           max_magic,
                           defense,
                           absorb,
                           res_fire,
                           res_ice,
                           res_lightning,
                           res_curse,
                           res_palsy);
        expect(defense == 5, "king prefix defense");
    }

    {
        using openkal::server::apply_repair;
        using openkal::server::RepairOutcome;
        using openkal::server::revision_stones_for_grade;
        expect(revision_stones_for_grade(40, 0) == 1, "G40 min stones");
        expect(revision_stones_for_grade(40, 4) == 5, "G40 max stones");
        expect(revision_stones_for_grade(40, 99) == 5, "G40 extra is clamped");
        expect(revision_stones_for_grade(9, 0) == 1, "low grade still grants one stone");
        expect(revision_stones_for_grade(0, 0) == 0, "ungraded item grants no stones");
        const auto add = apply_repair(0, 5, 20, 28);
        expect(add.outcome == RepairOutcome::Repaired && add.cur_end == 25 && add.max_end == 28,
               "artisan tool adds five");
        const auto full = apply_repair(0, 1, 28, 28);
        expect(full.outcome == RepairOutcome::Rejected && full.cur_end == 28,
               "normal tool rejects full endurance");
        const auto doggebi_ok = apply_repair(1, 3, 10, 28);
        expect(doggebi_ok.outcome == RepairOutcome::Repaired && doggebi_ok.max_end == 25 &&
                   doggebi_ok.cur_end == 25,
               "doggebi fully repairs and cuts max");
        const auto doggebi_floor = apply_repair(1, 3, 8, 2);
        expect(doggebi_floor.outcome == RepairOutcome::Repaired && doggebi_floor.max_end == 1 &&
                   doggebi_floor.cur_end == 1,
               "doggebi max endurance floors at 1");
        const auto doggebi_kill = apply_repair(1, 3, 3, 28);
        expect(doggebi_kill.outcome == RepairOutcome::Destroyed && doggebi_kill.cur_end == 0,
               "doggebi destroys below 4 endurance");
    }

    {
        using openkal::server::ItemCombatDef;
        using openkal::server::persist_slot_of;
        using openkal::server::PrefixCombatDef;
        using openkal::server::wear_kind_of;
        using openkal::server::WearKind;
        ItemCombatDef helmet;
        helmet.is_defense = true;
        helmet.class_small = "helmet";
        helmet.defense_melee = 22;
        helmet.dodge = 5;
        expect(wear_kind_of(helmet) == WearKind::Helmet, "helmet wear kind");
        expect(persist_slot_of(WearKind::Helmet) == 2, "helmet persist slot");
        expect(persist_slot_of(WearKind::Ring) == 2, "ring persist slot");
        expect(persist_slot_of(WearKind::Trinket) == 2, "trinket persist slot");
        ItemCombatDef ring;
        ring.is_ornament = true;
        ring.class_small = "ring";
        ring.str = 3;
        ring.hth = 3;
        expect(wear_kind_of(ring) == WearKind::Ring, "ring wear kind");
        ItemCombatDef belt;
        belt.is_ornament = true;
        belt.class_small = "trinket";
        belt.str = 1;
        belt.hp = 280;
        expect(wear_kind_of(belt) == WearKind::Trinket, "belt/trinket wear kind");
        int str = 19;
        int hth = 17;
        int intl = 9;
        int wis = 9;
        int dex = 11;
        ring.apply_primaries(str, hth, intl, wis, dex);
        belt.apply_primaries(str, hth, intl, wis, dex);
        expect(str == 23 && hth == 20, "ring and trinket specialty primaries stack");
        PrefixCombatDef king;
        king.str = 6;
        king.defense = 5;
        king.apply_primaries(str, hth, intl, wis, dex);
        expect(str == 29, "prefix primaries stack on ornament specialty");
        int hit = 0;
        int dodge = 0;
        int min_attack = 0;
        int max_attack = 0;
        int min_magic = 0;
        int max_magic = 0;
        int defense = 0;
        int defense_range = 0;
        int absorb = 0;
        int fatal_rate = 0;
        int fatal_pt = 0;
        int res_fire = 0;
        int res_ice = 0;
        int res_lightning = 0;
        int res_curse = 0;
        int res_palsy = 0;
        helmet.apply_gear(hit,
                          dodge,
                          min_attack,
                          max_attack,
                          min_magic,
                          max_magic,
                          defense,
                          defense_range,
                          absorb,
                          fatal_rate,
                          fatal_pt,
                          res_fire,
                          res_ice,
                          res_lightning,
                          res_curse,
                          res_palsy);
        king.apply_derived(hit,
                           min_attack,
                           max_attack,
                           min_magic,
                           max_magic,
                           defense,
                           absorb,
                           res_fire,
                           res_ice,
                           res_lightning,
                           res_curse,
                           res_palsy);
        expect(defense == 27 && dodge == 5, "helmet defense plus prefix defense");
        expect(defense_range == 0, "single-value helmet defense is melee only");
    }

    expect(openkal::server::charm_bonus_attack(1) == 1, "charm bonus +1");
    expect(openkal::server::charm_bonus_attack(3) == 4, "charm bonus +3");
    expect(openkal::server::charm_bonus_attack(20) == 104, "charm bonus +20");
    expect(openkal::server::charm_success_rate(openkal::server::CharmKind::XAttack, 0) == 10000000,
           "attack 0 is 100%");
    expect(openkal::server::charm_success_rate(openkal::server::CharmKind::XAttack, 3) == 10000000,
           "attack +3 is 100%");
    expect(openkal::server::charm_success_rate(openkal::server::CharmKind::XAttack, 4) == 9200000,
           "attack +4 is 92%");
    expect(openkal::server::charm_success_rate(openkal::server::CharmKind::XDefense, 0) == 10000000,
           "defense 0 is 100%");
    expect(openkal::server::charm_success_rate(openkal::server::CharmKind::XDefense, 1) == 8900000,
           "defense +1 is 89%");
    expect(openkal::server::charm_success_rate(openkal::server::CharmKind::XHit, 1) == 6700000,
           "hit +1 is 67%");
    expect(openkal::server::charm_success_rate(openkal::server::CharmKind::XDodge, 0) == 5000000,
           "evasion 0->1 is 50%");
    expect(openkal::server::charm_success_rate(openkal::server::CharmKind::XDodge, 1) == 2500000,
           "evasion 1->2 is 25%");
    expect(openkal::server::charm_success_rate(openkal::server::CharmKind::XDodge, 2) == 1000000,
           "evasion 2->3 is 10%");
    expect(openkal::server::charm_success_rate(openkal::server::CharmKind::XDodge, 3) == 500000,
           "evasion 3->4 is 5%");
    expect(openkal::server::charm_success_rate(openkal::server::CharmKind::XDodge, 4) == 100000,
           "evasion 4->5 is 1%");
    expect(openkal::server::charm_success_rate(openkal::server::CharmKind::XDodge, 5) == 0,
           "evasion cap");
    expect(openkal::server::explosive_blow_chance(0, 0) == 50, "eb chance base");
    expect(openkal::server::explosive_blow_chance(0, 1) == 100, "one stone maxes level 0");
    expect(openkal::server::explosive_blow_stone_cost(0) == 1, "first stones cost 1");
    expect(openkal::server::explosive_blow_stone_cost(12) == 2, "level 12 costs 2");
    expect(openkal::server::explosive_blow_gold_cost(0) == 1500, "first eb gold");
    expect(openkal::server::explosive_blow_cap(false, 5, 10) == 5, "sword cap is xattack");
    expect(openkal::server::explosive_blow_cap(true, 5, 10) == 10, "wand cap is xmagic");
    {
        openkal::server::CharmTarget sword;
        sword.is_weapon = true;
        sword.grade = 40;
        openkal::server::CharmState state;
        state.x_attack = 2;
        const auto ok = openkal::server::apply_charm(
            openkal::server::CharmKind::XAttack, state, sword, 1, 1, 1);
        expect(ok.outcome == openkal::server::CharmOutcome::Success && ok.state.x_attack == 3,
               "attack charm success");
        openkal::server::CharmTarget wand;
        wand.is_weapon = true;
        wand.is_wand = true;
        wand.grade = 40;
        const auto reject =
            openkal::server::apply_charm(openkal::server::CharmKind::XAttack, state, wand, 1, 1, 1);
        expect(reject.outcome == openkal::server::CharmOutcome::Rejected, "xattack rejects wand");
        openkal::server::CharmTarget armor;
        armor.is_defense = true;
        armor.grade = 40;
        openkal::server::CharmState evade;
        const auto ev =
            openkal::server::apply_charm(openkal::server::CharmKind::XDodge, evade, armor, 1, 1, 1);
        expect(ev.outcome == openkal::server::CharmOutcome::Success && ev.state.x_dodge == 1,
               "evasion charm success");
    }
    {
        openkal::server::CharmTarget sword;
        sword.is_weapon = true;
        sword.grade = 40;
        openkal::server::CharmState state;
        state.x_attack = 5;
        int consumed = 0;
        expect(openkal::server::try_upgrade_rate_up(state, sword, 1, consumed) ==
                       openkal::server::UpgradeRateOutcome::Applied &&
                   consumed == 1 && state.upgrade_rate == 1,
               "revise consumes one stone");
        int gold = 0;
        const auto win = openkal::server::apply_upgrade_level(state, sword, 100000, 1, 1, gold);
        expect(win == openkal::server::UpgradeLevelOutcome::Success && state.upgrade_level == 1 &&
                   gold == 1500,
               "eb success spends gold and increments");
        state.upgrade_level = 0;
        state.upgrade_rate = 0;
        gold = 0;
        const auto keep = openkal::server::apply_upgrade_level(state, sword, 100000, 100, 10, gold);
        expect(keep == openkal::server::UpgradeLevelOutcome::FailKeep && state.upgrade_level == 0,
               "eb fail 1-60 keeps level");
        const auto down = openkal::server::apply_upgrade_level(state, sword, 100000, 100, 70, gold);
        expect(down == openkal::server::UpgradeLevelOutcome::FailDowngrade, "eb fail 61-90");
        const auto dead = openkal::server::apply_upgrade_level(state, sword, 100000, 100, 95, gold);
        expect(dead == openkal::server::UpgradeLevelOutcome::Destroyed, "eb fail 91-100 destroys");
        state.upgrade_level = 5;
        gold = 0;
        const auto capped = openkal::server::apply_upgrade_level(state, sword, 100000, 1, 1, gold);
        expect(capped == openkal::server::UpgradeLevelOutcome::Rejected && gold == 0,
               "eb rejects at xattack cap");
    }
    expect(openkal::server::combat::get_palsy_chance(1, 10, 10, false) == 10,
           "eb chance same-level monster");
    expect(openkal::server::combat::get_palsy_chance(1, 10, 20, false) == 8,
           "eb chance monster 10 levels higher");
    {
        Combatant atk;
        atk.upgrade_level = 1;
        atk.level = 10;
        atk.clazz = 0;
        atk.char_attack = 10;
        atk.weapon_min_attack = 20;
        atk.weapon_max_attack = 20;
        Combatant tgt;
        tgt.level = 10;
        tgt.is_player = false;
        const auto dmg = openkal::server::combat::get_palsy_extra(atk, tgt, 1, 20);
        expect(dmg > 0, "palsy extra on guaranteed roll");
        expect(openkal::server::combat::get_palsy_extra(atk, tgt, 100, 20) == 0,
               "palsy miss on high roll");
    }
    expect(openkal::server::combat::can_conti_skill(10, 10, 89), "equal-level ice slow 89%");
    expect(!openkal::server::combat::can_conti_skill(10, 10, 90), "equal-level ice slow miss");
    expect(openkal::server::combat::fire_mastery_delay_ms(4800, 800, 5) == 4600,
           "fire mastery cuts delay[0] by 5% per rank");
    {
        openkal::server::combat::AttackResult slow;
        openkal::server::combat::apply_ice_mastery_slow(slow, 10, 10, 5, 1);
        expect(slow.ice_slow_duration_sec == 7 && slow.ice_slow_percent == -100,
               "ice mastery slow duration and percent");
        openkal::server::combat::AttackResult miss;
        openkal::server::combat::apply_ice_mastery_slow(miss, 10, 10, 5, 100);
        expect(miss.ice_slow_duration_sec == 0, "ice mastery slow respects CanContiSkill");
    }
    {
        Combatant archer;
        archer.level = 20;
        archer.hit = 1000;
        archer.min_attack = 100;
        archer.max_attack = 100;
        Combatant target;
        target.level = 20;
        target.is_player = true;
        target.clazz = openkal::server::combat::kPcMage;
        for (unsigned seed = 0; seed < 64; ++seed) {
            std::srand(seed);
            const bool combined_miss = openkal::server::combat::irand(1, 100) <= 75;
            std::srand(seed);
            target.perfect_evasion_base = 100;
            target.perfect_evasion_advanced = 93;
            const auto combined =
                openkal::server::combat::resolve_physical(archer, target, 0, 0, 100, 0, 0, true);
            expect(combined.hit == !combined_miss && combined.damage == (combined_miss ? 0 : 100),
                   "high perfect evasion uses one capped roll even when the hit check is skipped");

            std::srand(seed);
            const bool legacy_miss = openkal::server::combat::irand(1, 100) <= 17 ||
                                     openkal::server::combat::irand(1, 100) <= 10;
            std::srand(seed);
            target.perfect_evasion_base = 17;
            target.perfect_evasion_advanced = 10;
            const auto legacy =
                openkal::server::combat::resolve_physical(archer, target, 0, 0, 100, 0, 0, true);
            expect(legacy.hit == !legacy_miss && legacy.damage == (legacy_miss ? 0 : 100),
                   "ordinary perfect evasion preserves both independent legacy rolls");
        }
    }
    {
        using openkal::server::combat::perfect_evasion_roll;
        struct Sample {
            std::int32_t base;
            std::int32_t advanced;
            bool legacy;
            std::int32_t combined;
        };
        constexpr auto max32 = std::numeric_limits<std::int32_t>::max();
        constexpr Sample samples[] = {{17, 10, true, 0},
                                      {40, 33, true, 0},
                                      {40, 40, true, 0},
                                      {41, 34, false, 62},
                                      {100, 93, false, 75},
                                      {262, 255, false, 75},
                                      {-1, -1, true, 0},
                                      {-1, 100, false, 70},
                                      {100, -1, false, 70},
                                      {max32, max32, false, 75}};
        for (const auto& sample : samples) {
            const auto policy = perfect_evasion_roll(sample.base, sample.advanced);
            expect(policy.legacy_two_rolls == sample.legacy &&
                       policy.combined_percent == sample.combined,
                   "perfect evasion selects the documented roll policy and capped percent");
        }
    }

    expect(openkal::server::combat::knight_weapon_defend_block(70) == 3,
           "Weapon Defend nBlock at level 70");
    {
        Combatant monster;
        monster.level = 10;
        monster.min_attack = 20;
        monster.max_attack = 20;
        monster.hit = 1000;
        Combatant knight;
        knight.level = 10;
        knight.is_player = true;
        knight.clazz = openkal::server::combat::kPcKnight;
        knight.wearing_shield = true;
        knight.block_percent = 10;
        expect(openkal::server::combat::check_block(monster, knight, 22),
               "same-level shield nBlock 10 blocks on roll 22");
        expect(!openkal::server::combat::check_block(monster, knight, 23),
               "same-level shield nBlock 10 misses block on roll 23");
        knight.block_percent = 100;
        const auto blocked =
            openkal::server::combat::resolve_physical(monster, knight, 0, 0, 20, 0, 0);
        expect(blocked.blocked && !blocked.hit && blocked.damage == 0,
               "CheckBlock true deals no damage and is not a hit");
    }
    {
        Combatant monster;
        monster.level = 10;
        monster.min_attack = 20;
        monster.max_attack = 20;
        monster.hit = 0;
        Combatant knight;
        knight.level = 10;
        knight.is_player = true;
        knight.clazz = openkal::server::combat::kPcKnight;
        knight.wearing_shield = true;
        knight.block_percent = 0;
        knight.dodge = 1000;
        expect(!openkal::server::combat::check_block(monster, knight, 1),
               "nBlock 0 never CheckBlocks");
        openkal::server::combat::AttackResult missed;
        bool saw_miss = false;
        for (int i = 0; i < 32; ++i) {
            missed = openkal::server::combat::resolve_physical(monster, knight, 0, 0, 20, 0, 0);
            if (!missed.hit) {
                saw_miss = true;
                break;
            }
        }
        expect(saw_miss && !missed.blocked && missed.damage == 0,
               "CheckHit miss with nBlock 0 is not blocked");
    }

    expect(openkal::server::combat::apply_percent_reduction(100, 90) == 10,
           "Perfect Defense self 90 percent");
    expect(openkal::server::combat::apply_percent_reduction(100, 30) == 70,
           "Perfect Defense party 30 percent");
    expect(openkal::server::combat::incoming_player_damage(100, false, 90) == 10,
           "incoming player damage applies Perfect Defense");
    expect(openkal::server::combat::incoming_player_damage(100, true, 90) == 0,
           "godmode still zeros incoming player damage");
    expect(openkal::server::combat::knight_blessing_power_percent(0) == 0,
           "unlearned Blessing of Son of the Sky");
    expect(openkal::server::combat::knight_blessing_power_percent(1) == 8,
           "Blessing rank 1 is 8 percent");
    expect(openkal::server::combat::knight_blessing_power_percent(7) == 32,
           "Blessing rank 7 is 32 percent");
    expect(openkal::server::combat::knight_defense_strengthening_points(70, 100) == 29,
           "Defense strengthening at 70 with 100 org defense");
    expect(openkal::server::combat::knight_defense_strengthening_points(64, 50) == 2,
           "Defense strengthening ignores negative level term");
    expect(openkal::server::combat::skill_physical_power(
               openkal::server::combat::kPcKnight, 42, 1, 11, 100, 5) == 705,
           "Provocation of Blow rank 1 attack 100 with roll 5");
    expect(openkal::server::combat::skill_physical_power(
               openkal::server::combat::kPcKnight, 42, 1, 11, 100, 10) == 710,
           "Provocation of Blow rank 1 attack 100 with roll 10");
    expect(openkal::server::combat::knight_provocation_of_blow_hostility(705) == 2115,
           "Provocation of Blow hostility is dealt times 3");
    expect(openkal::server::combat::knight_provocation_of_blow_pvp_damage(710) == 248,
           "Provocation of Blow PvP keeps 35 percent");
    expect(openkal::server::combat::skill_physical_power(
               openkal::server::combat::kPcKnight, 38, 1, 11, 100, 5) == 705,
           "Spin Slash rank 1 attack 100 with roll 5");
    expect(openkal::server::combat::skill_physical_power(
               openkal::server::combat::kPcKnight, 38, 1, 11, 100, 20) == 720,
           "Spin Slash rank 1 attack 100 with roll 20");
    expect(openkal::server::combat::knight_spin_slash_pvp_damage(720) == 252,
           "Spin Slash PvP keeps 35 percent");
    expect(openkal::server::combat::skill_taunt_hostility(42, 1) == 0,
           "Provocation of Blow hostility is not the Mockery table");

    return openkal::test::g_failures == 0 ? 0 : 1;
}

/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include "openkal/protocol/BuffState.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace openkal::server {

    /** Knight Call Harmony (Commander passive). Rank 1 at 61, rank 2 at 70. */
    inline constexpr int kKnightCallHarmonySkillIndex = 45;

    /** Distinct knight Call buff kinds. Rank 2 of Call Harmony keeps all of them. */
    inline constexpr int kKnightChantKindCount = 7;

    /**
     * Parallel Call slots from Call Harmony rank.
     * Rank 0 matches exclusive chants. Rank 1 keeps two. Rank 2 keeps all.
     * @param harmony_rank Learned Call Harmony level.
     * @return Maximum simultaneous knight Call buffs.
     */
    [[nodiscard]] constexpr int knight_chant_slots(int harmony_rank) {
        if (harmony_rank >= 2) {
            return kKnightChantKindCount;
        }
        if (harmony_rank >= 1) {
            return 2;
        }
        return 1;
    }

    /** Timed skill-buff kinds (parallel to pay-scroll BuffType). */
    enum class SkillBuffId : int {
        StoneShield = 1,
        Haste = 2,
        Meditation = 3,
        RefiningWeapon = 4,
        AuraDefense = 5,
        BlessStrength = 6,
        BlessHealth = 7,
        BlessAgility = 8,
        BlessIntelligence = 9,
        KnightDefenseChant = 10,
        KnightHealingChant = 11,
        KnightLightningChant = 12,
        KnightIceChant = 13,
        KnightFlameChant = 14,
        KnightAccuracyChant = 15,
        KnightBattleChant = 16,
        KnightBerserk = 17,
        /** Increasing Concentration (WildAccuracy / skill index 10). */
        KnightConcentration = 18,
        BlessCritical = 19,
        ArcherFireResist = 20,
        ArcherIceResist = 21,
        ArcherLightningResist = 22,
        ArcherEscape = 23,
        /** Force of Defense (skill index 18). */
        KnightForceDefense = 24,

        /** Protect (skill index 14). */
        KnightProtect = 25,

        /** Elastic Shield (skill index 34). */
        KnightElasticShield = 26,

        /** Perfect Defense (skill index 40). */
        KnightPerfectDefense = 27,
        ThiefHiding = 30,
        ArcherChanceCapture = 31,
        ArcherMuscleSolidation = 32,
        ArcherCurseResist = 33,
        ArcherSpiritResist = 34,
        ArcherStun = 35,
        ArcherSpiritDamage = 36,
        ArcherCurseCritical = 37,
        ArcherCurseStrength = 38,
        ArcherCurseHealth = 39,
        ArcherCurseAgility = 40,
        ArcherCurseIntelligence = 41,
        ArcherCurseWisdom = 42,
        ArcherCurseDefense = 43,
        /** Mesh of Speed (skill index 22). */
        ArcherMeshSpeed = 44,
        /** Sight Interruption (skill index 23). */
        ArcherDarknessVision = 45,
        /** Decrease of Target Range / Cloud Sight (skill index 33). */
        ArcherCloudSight = 46,
        /** MP Vaporization / Mana Burn (skill index 34). */
        ArcherManaBurn = 47,
        /** Loss of Confidence (skill index 43). */
        ArcherVanishConfidence = 48,
        /** Sight Blockade (skill index 44). */
        ArcherDestroySight = 49,
        /** Arrow of Silence (skill index 15). */
        ArcherMagicStop = 50,
        /** Powerful Arrow of Silence (skill index 27). */
        ArcherSilenceShot = 51,

        /** Sharpness (skill index 46). */
        ArcherSharpness = 52,

        /** Yungsang Rage (animal skill 106). Distinct from Call of On Target Point. */
        YungsangRage = 53,

        /** Dance of Killing (Thief 13): temporarily replaces normal attacks. */
        ThiefDanceOfKilling = 54,

        /** Counter Attack (Thief 20): enables the reactive counter. */
        ThiefCounterAttack = 55,

        /** Dual Shadow (Thief 12): temporary hit/evasion enhancement. */
        ThiefDualShadow = 56,

        /** Wind Cry (Thief 29): party evasion enhancement. */
        ThiefWindCry = 57,

        /** Wrath of Thief (Thief 31): party maximum-HP enhancement. */
        ThiefWrath = 58,

        /** Flame Dance (Thief 35): temporary flame guard. */
        ThiefFlameDance = 59,

        /** Ankle Amputate (Thief 15): movement reduction. */
        ThiefAnkleAmputate = 60,

        /** Armor Breaker (Thief 22): five-percent vulnerability above 100 damage. */
        ThiefArmorBreaker = 61,
        /** Icicle (Mage 66): five-second movement reduction. */
        MageIcicleSlow = 62,
        /** Magic Intensification's active damage/level-limit override (buff 306). */
        MageMagicIntensification = 63,
        /** Blessing (Mage 69): temporary protection for caster and recipient. */
        MageBlessing = 64,
    };

    /** Behavioral category for querying or removing related skill buffs. */
    enum class SkillBuffCategory : int {
        Beneficial = 0,
        KnightChant = 1,
        Resistance = 2,
        Control = 3,
        DamageOverTime = 4,
        Curse = 5,
        Concealment = 6,
    };

    /** One active skill buff on a character. */
    struct SkillBuffEntry {
        /** Buff kind. */
        SkillBuffId id = SkillBuffId::StoneShield;

        /** compatibility behavior bit used for MSG_ADDBSTATE sync. */
        std::uint64_t bstate = 0;

        /** Source skill index. */
        int skill_index = 0;

        /** Source class (0–3). */
        int clazz = 0;

        /** Primary magnitude (absorb / haste% / resist). */
        std::int32_t value1 = 0;

        /** Secondary magnitude (defense points, etc.). */
        std::int32_t value2 = 0;

        /** Unix expire time. */
        std::int64_t expire_unix = 0;
    };

    /**
     * Runtime manager for timed skill buffs (related compatibility states / chants).
     * Does not share storage with PayBuffSet.
     */
    class SkillBuffSet {
      public:
        /** @return Combined compatibility behavior bitmask of active skill buffs. */
        [[nodiscard]] std::uint64_t bstate() const;

        /** @return True when the given compatibility behavior bit is active. */
        [[nodiscard]] bool has(std::uint64_t bit) const;

        /** @return Active entries. */
        [[nodiscard]] const std::vector<SkillBuffEntry>& entries() const {
            return m_entries;
        }

        /**
         * Applies or replaces a skill buff.
         * @param id Buff kind.
         * @param clazz Source class.
         * @param skill_index Source skill index.
         * @param duration_sec Duration in seconds.
         * @param value1 Primary magnitude.
         * @param value2 Secondary magnitude.
         * @param now_unix Current unix time.
         * @param out_remain Out seconds sent to the client.
         * @param out_bstate Out compatibility behavior bit for ADDBSTATE.
         * @param chant_slots Max simultaneous knight Calls; 1 matches exclusivity.
         * @return True when applied.
         */
        [[nodiscard]] bool apply(SkillBuffId id,
                                 int clazz,
                                 int skill_index,
                                 int duration_sec,
                                 std::int32_t value1,
                                 std::int32_t value2,
                                 std::int64_t now_unix,
                                 std::int32_t& out_remain,
                                 std::uint64_t& out_bstate,
                                 int chant_slots = 1);

        /**
         * Applies an item-sourced Haste buff using its authored duration and movement bonus.
         * @param duration_sec Duration in seconds.
         * @param move_speed_percent Movement-speed bonus percentage.
         * @param now_unix Current unix time.
         * @param out_remain Out seconds sent to the client.
         * @param out_bstate Out compatibility behavior bit for ADDBSTATE.
         * @return True when applied; false while Haste is already active or values are invalid.
         */
        [[nodiscard]] bool apply_item_haste(int duration_sec,
                                            std::int32_t move_speed_percent,
                                            std::int64_t now_unix,
                                            std::int32_t& out_remain,
                                            std::uint64_t& out_bstate);

        /**
         * Expires timed buffs whose deadline has passed.
         * @param now_unix Current unix time.
         * @return compatibility behavior bits that were cleared.
         */
        [[nodiscard]] std::uint64_t tick(std::int64_t now_unix);

        /**
         * Removes one active buff immediately.
         * @param id Buff kind.
         * @return compatibility behavior bit that was removed, or zero.
         */
        [[nodiscard]] std::uint64_t remove(SkillBuffId id);

        /**
         * Remaining seconds for client sync.
         * @param entry Active entry.
         * @param now_unix Current unix time.
         * @return Seconds left; 0 when expired or permanent (`expire_unix == 0`).
         */
        [[nodiscard]] static std::int32_t remain_of(const SkillBuffEntry& entry,
                                                    std::int64_t now_unix);

        /**
         * Maps a skill cast to a buff id when the skill is a known self/party buff.
         * @param clazz Class.
         * @param skill_index Skill index.
         * @return Buff id, or nullopt when not a skill buff cast.
         */
        [[nodiscard]] static std::optional<SkillBuffId> buff_id_for_skill(int clazz,
                                                                          int skill_index);

        /**
         * Maps an Archer attack or curse to a target status.
         * @param skill_index Archer skill index.
         * @return Target status id, or nullopt when no managed status is produced.
         */
        [[nodiscard]] static std::optional<SkillBuffId> archer_status_id_for_skill(int skill_index);

        /**
         * Returns the behavioral category of a typed buff.
         * @param id Buff kind.
         * @return Buff category.
         */
        [[nodiscard]] static SkillBuffCategory category_for(SkillBuffId id);

        /**
         * Default duration (seconds) for a skill buff cast.
         * @param id Buff kind.
         * @param skill_level Learned rank.
         * @return Duration in seconds, zero for continuous, or negative when unconfirmed.
         */
        [[nodiscard]] static int default_duration_sec(SkillBuffId id, int skill_level);

        /**
         * Default magnitudes for a skill buff cast.
         * @param id Buff kind.
         * @param skill_index Skill index (stone shield tiers).
         * @param skill_level Learned rank.
         * @param out_v1 Out primary.
         * @param out_v2 Out secondary.
         */
        static void default_values(SkillBuffId id,
                                   int skill_index,
                                   int skill_level,
                                   std::int32_t& out_v1,
                                   std::int32_t& out_v2);

        /**
         * compatibility behavior bit for a skill buff id.
         * @param id Buff kind.
         * @return compatibility behavior bitmask, or 0.
         */
        [[nodiscard]] static std::uint64_t bstate_for(SkillBuffId id);

        /**
         * Finds one active typed entry.
         * @param id Buff kind.
         * @return Entry pointer, or nullptr when inactive.
         */
        [[nodiscard]] const SkillBuffEntry* find(SkillBuffId id) const;

        /**
         * Removes all buffs in one category.
         * @param category Category to clear.
         * @return Combined compatibility behavior bits removed.
         */
        [[nodiscard]] std::uint64_t remove_category(SkillBuffCategory category);

        /**
         * Removes buffs created by one source skill.
         * @param clazz Source class.
         * @param skill_index Source skill index.
         * @return Combined compatibility behavior bits removed.
         */
        [[nodiscard]] std::uint64_t remove_skill(int clazz, int skill_index);

        /**
         * Removes every active skill buff (ResetAllBuff).
         * @return Combined compatibility behavior bits removed.
         */
        [[nodiscard]] std::uint64_t clear();

        /**
         * Reads a typed entry magnitude.
         * @param id Buff kind.
         * @param secondary True to read value2.
         * @return Active magnitude, or zero.
         */
        [[nodiscard]] std::int32_t value(SkillBuffId id, bool secondary = false) const;

        /**
         * Sum of active stone-shield absorb points.
         * @return Absorb bonus.
         */
        [[nodiscard]] std::int32_t stone_absorb() const;

        /**
         * Sum of active stone-shield defense points.
         * @return Defense bonus.
         */
        [[nodiscard]] std::int32_t stone_defense() const;

        /**
         * Active haste move-speed percent (0 when none).
         * @return Percent bonus.
         */
        [[nodiscard]] std::int32_t haste_mspeed_percent() const;

        /**
         * Combined `P_MSPEED` percent from Haste, Emergency Escape, and Sharpness.
         * @return Additive percent used as `(100 + sum) / 100`.
         */
        [[nodiscard]] std::int32_t move_speed_percent() const;

        /**
         * Active Meditation rank used by the four-second MP tick.
         * @return Meditation rank, or zero when inactive.
         */
        [[nodiscard]] std::int32_t meditation_level() const;

        /**
         * Active Refining Weapon physical/magical attack points.
         * @return Flat attack and magic bonus.
         */
        [[nodiscard]] std::int32_t refining_weapon_points() const;

        /** @return Active Aura Defense close-defense points. */
        [[nodiscard]] std::int32_t aura_defense_points() const;

        /** @return Active Aura Defense ranged-defense points. */
        [[nodiscard]] std::int32_t aura_range_defense_points() const;

        /** @return Active Blessing of Strength points. */
        [[nodiscard]] std::int32_t blessing_strength_points() const;

        /** @return Active Blessing of Health points. */
        [[nodiscard]] std::int32_t blessing_health_points() const;

        /** @return Active Blessing of Agility points. */
        [[nodiscard]] std::int32_t blessing_agility_points() const;

        /** @return Active Blessing of Intelligence points. */
        [[nodiscard]] std::int32_t blessing_intelligence_points() const;

        /** @return Active Increase of Critical Hit points. */
        [[nodiscard]] std::int32_t blessing_critical_points() const;

        /**
         * Hit / on-target bonus from Increasing Concentration.
         * @return Hit points added.
         */
        [[nodiscard]] std::int32_t concentration_hit() const;

        /** @return Active Force of Defense close-defense percent. */
        [[nodiscard]] std::int32_t force_defense_percent() const;

        /** @return Active Force of Defense ranged-defense percent. */
        [[nodiscard]] std::int32_t force_range_defense_percent() const;

        /** @return Active Call of Defense close-defense percent. */
        [[nodiscard]] std::int32_t defense_chant_percent() const;

        /** @return Active Call of Defense ranged-defense percent. */
        [[nodiscard]] std::int32_t defense_chant_range_percent() const;

        /** @return Active Call of Recovery healing points. */
        [[nodiscard]] std::int32_t recovery_chant_points() const;

        /** @return Active Call of Lightning Defense resistance percent. */
        [[nodiscard]] std::int32_t lightning_chant_resistance() const;

        /** @return Active Call of Ice Defense resistance percent. */
        [[nodiscard]] std::int32_t ice_chant_resistance() const;

        /** @return Active Call of Fire Defense resistance percent. */
        [[nodiscard]] std::int32_t flame_chant_resistance() const;

        /** @return Active Call of On Target Point accuracy points. */
        [[nodiscard]] std::int32_t accuracy_chant_points() const;

        /** @return True while Yungsang Rage (always-hit) is active. */
        [[nodiscard]] bool has_yungsang_rage() const;

        /** @return Active Call of Physical Attack points. */
        [[nodiscard]] std::int32_t battle_chant_attack_points() const;

        /** @return Active Berserk close-attack percent. */
        [[nodiscard]] std::int32_t berserk_attack_percent() const;

        /** @return Active Berserk defense percent adjustment. */
        [[nodiscard]] std::int32_t berserk_defense_percent() const;

        /** @return Active Protect hostility percent adjustment. */
        [[nodiscard]] std::int32_t protect_hostility_percent() const;

        /** @return Active Elastic Shield close-defense points. */
        [[nodiscard]] std::int32_t elastic_shield_defense_points() const;

        /** @return Active Elastic Shield ranged-defense points. */
        [[nodiscard]] std::int32_t elastic_shield_range_defense_points() const;

        /** @return Active Perfect Defense reduction, or full protection during Mage Blessing. */
        [[nodiscard]] std::int32_t perfect_defense_self_percent() const;

        /** @return Active Perfect Defense party damage-reduction percent. */
        [[nodiscard]] std::int32_t perfect_defense_party_percent() const;

        /** @return Active Chance Capture fatal-rate points. */
        [[nodiscard]] std::int32_t archer_chance_capture_fatal_rate() const;

        /** @return Active Chance Capture hit points. */
        [[nodiscard]] std::int32_t archer_chance_capture_hit() const;

        /** @return Active Muscle Solidation minimum-attack points. */
        [[nodiscard]] std::int32_t archer_muscle_min_attack() const;

        /** @return Active Archer lightning resistance. */
        [[nodiscard]] std::int32_t archer_lightning_resistance() const;

        /** @return Active Archer ice resistance. */
        [[nodiscard]] std::int32_t archer_ice_resistance() const;

        /** @return Active Archer fire resistance. */
        [[nodiscard]] std::int32_t archer_fire_resistance() const;

        /** @return Active Archer curse resistance. */
        [[nodiscard]] std::int32_t archer_curse_resistance() const;

        /** @return Active Archer non-attribution / spirit resistance. */
        [[nodiscard]] std::int32_t archer_spirit_resistance() const;

        /** @return Active Archer stun duration payload in seconds. */
        [[nodiscard]] std::int32_t archer_stun_seconds() const;

        /** @return Active Mystic Arrow damage per tick. */
        [[nodiscard]] std::int32_t archer_spirit_damage_per_tick() const;

        /** @return Mesh of Speed attack-speed percent (negative is slower). */
        [[nodiscard]] std::int32_t archer_attack_speed_percent() const;

        /** @return Sight Interruption move-speed percent (negative is slower). */
        [[nodiscard]] std::int32_t archer_move_speed_percent() const;

        /** @return Cloud Sight flat range delta. */
        [[nodiscard]] std::int32_t archer_range_delta() const;

        /** @return Loss of Confidence hit percent (typically -50). */
        [[nodiscard]] std::int32_t archer_hit_percent() const;

        /** @return True when Arrow of Silence magic-stop is active. */
        [[nodiscard]] bool archer_magic_stopped() const;

        /** @return True when Powerful Arrow of Silence is active. */
        [[nodiscard]] bool archer_silence_shot() const;

        /** @return True when Sight Blockade is active. */
        [[nodiscard]] bool archer_destroy_sight() const;

        /** @return Mana Burn MP drained per second. */
        [[nodiscard]] std::int32_t archer_mana_burn_per_tick() const;

        /** @return Powerful Silence MP drained per second. */
        [[nodiscard]] std::int32_t archer_silence_shot_mp_drain() const;

        /**
         * Returns the payload stored for one active typed state.
         * @param id State identifier.
         * @param out_value1 Primary payload.
         * @param out_value2 Secondary payload.
         * @return True when the state is active.
         */
        [[nodiscard]] bool
        values(SkillBuffId id, std::int32_t& out_value1, std::int32_t& out_value2) const;

      private:
        /** Active skill buffs. */
        std::vector<SkillBuffEntry> m_entries;
    };

}  // namespace openkal::server

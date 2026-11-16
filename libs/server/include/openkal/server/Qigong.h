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

#include <string_view>

namespace openkal::server {

    /** Result of validating a Qigong item operation. */
    enum class QigongApplyResult {
        /** The operation may proceed. */
        Ok = 0,
        /** The target is not eligible for Qigong. */
        InvalidTarget,
        /** The target is currently equipped. */
        Equipped,
        /** A Qigong Talisman requires a perforated target. */
        NotPerforated,
        /** A Perforation Shot cannot perforate the target twice. */
        AlreadyPerforated,
    };

    /** Rolled Qigong bonus decoded from its persistent 1–80 id. */
    class QigongBonus {
      public:
        /** @return Qigong tier in [1, 8], or zero for no bonus. */
        [[nodiscard]] int tier() const {
            return m_tier;
        }
        /** @return Random primary-stat selector: 0 STR, 1 HTH, 2 DEX, 3 WIS, 4 INT. */
        [[nodiscard]] int primary_kind() const {
            return m_primary_kind;
        }
        /** @return Primary-stat points granted. */
        [[nodiscard]] int primary_points() const {
            return m_primary_points;
        }
        /** @return Weapon physical-attack percent. */
        [[nodiscard]] int attack_percent() const {
            return m_attack_percent;
        }
        /** @return Weapon magical-attack percent. */
        [[nodiscard]] int magic_percent() const {
            return m_magic_percent;
        }
        /** @return Armor defense points. */
        [[nodiscard]] int defense() const {
            return m_defense;
        }
        /** @return Flat HP granted by a weapon bonus. */
        [[nodiscard]] int hp() const {
            return m_hp;
        }
        /** @return Flat MP granted by a weapon bonus. */
        [[nodiscard]] int mp() const {
            return m_mp;
        }
        /** @return Hit granted by a non-wand weapon bonus. */
        [[nodiscard]] int hit() const {
            return m_hit;
        }
        /** @return Dodge granted by a wand bonus. */
        [[nodiscard]] int dodge() const {
            return m_dodge;
        }
        /** @return Display name for the tier. */
        [[nodiscard]] const char* tier_name() const;

        /**
         * Builds a Qigong bonus from the persistent roll id.
         * @param effect_id Roll id in [1, 80].
         * @param is_weapon True for a weapon target.
         * @param class_small Item class for armor-slot-specific values and wand dodge.
         * @return Decoded bonus, or an empty bonus for an invalid id/target.
         */
        [[nodiscard]] static QigongBonus
        from_effect(int effect_id, bool is_weapon, std::string_view class_small);

      private:
        int m_tier = 0;
        int m_primary_kind = 0;
        int m_primary_points = 0;
        int m_attack_percent = 0;
        int m_magic_percent = 0;
        int m_defense = 0;
        int m_hp = 0;
        int m_mp = 0;
        int m_hit = 0;
        int m_dodge = 0;
    };

    /** Qigong item ids and validation shared by client and server. */
    class Qigong {
      public:
        /** @return The only supported Qigong Talisman item index. */
        [[nodiscard]] static constexpr int talisman_index() {
            return 23020;
        }

        /** @return The only supported Perforation Shot item index. */
        [[nodiscard]] static constexpr int perforation_shot_index() {
            return 23018;
        }

        /** @return True if the item is the active Qigong Talisman. */
        [[nodiscard]] static constexpr bool is_talisman(int index) {
            return index == talisman_index();
        }

        /** @return True if the item is the active Perforation Shot. */
        [[nodiscard]] static constexpr bool is_perforation_shot(int index) {
            return index == perforation_shot_index();
        }

        /**
         * Validates applying a Perforation Shot.
         * @param is_weapon Target is a weapon.
         * @param is_defense Target is defense gear.
         * @param class_small Target item definition small class.
         * @param grade Target item level/grade.
         * @param equipped Target is equipped.
         * @param perforated Target already has perforation.
         * @return Validation result.
         */
        [[nodiscard]] static QigongApplyResult can_perforate(bool is_weapon,
                                                             bool is_defense,
                                                             std::string_view class_small,
                                                             int grade,
                                                             bool equipped,
                                                             bool perforated);

        /**
         * Validates applying a Qigong Talisman.
         * @param is_weapon Target is a weapon.
         * @param is_defense Target is defense gear.
         * @param class_small Target item definition small class.
         * @param grade Target item level/grade.
         * @param equipped Target is equipped.
         * @param perforated Target has perforation.
         * @return Validation result.
         */
        [[nodiscard]] static QigongApplyResult can_apply(bool is_weapon,
                                                         bool is_defense,
                                                         std::string_view class_small,
                                                         int grade,
                                                         bool equipped,
                                                         bool perforated);

        /** @return True for armor slots supported by the source Qigong table. */
        [[nodiscard]] static bool is_armor_part(std::string_view class_small);

        /**
         * Tests a roll against the configured historical default rate.
         * @param roll Uniform roll in [1, 100].
         * @return True on success.
         */
        [[nodiscard]] static bool perforation_succeeds(int roll);

        /** @return The Perforation Shot success rate. */
        [[nodiscard]] static constexpr int perforation_success_rate() {
            return 50;
        }

        /**
         * Selects a weighted Qigong effect id.
         * @param tier_roll Uniform roll in [1, 100]. Lower tiers have higher probability.
         * @param variant_roll Uniform roll in [0, 9] selecting a stat and value variant.
         * @return Effect id in [1, 80], or zero for an invalid input.
         */
        [[nodiscard]] static int roll_effect(int tier_roll, int variant_roll);
    };

}  // namespace openkal::server

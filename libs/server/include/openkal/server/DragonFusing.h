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

    /** Result of validating a Dragon Spirit Scroll operation. */
    enum class DragonFusingApplyResult { Ok = 0, InvalidTarget, Equipped };

    /** Rolled Dragon Fusing bonus decoded from its persistent effect id. */
    class DragonFusingBonus {
      public:
        /** @return Dragon grade in [1, 8], or zero for no bonus. */
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
        /** @return Display name for the dragon grade. */
        [[nodiscard]] const char* tier_name() const;

        /**
         * Builds a Dragon Fusing bonus from the persistent effect id.
         * @param effect_id Roll id in [1, 240].
         * @param is_weapon True for a weapon target.
         * @param class_small Item class for armor-slot-specific values and wand dodge.
         * @return Decoded bonus, or an empty bonus for an invalid id/target.
         */
        [[nodiscard]] static DragonFusingBonus
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

    /** Dragon Spirit Scroll ids and roll rules shared by client and server. */
    class DragonFusing {
      public:
        /** @return The Dragon Spirit Scroll item index. */
        [[nodiscard]] static constexpr int scroll_index() {
            return 23019;
        }

        /** @return True if the item is the active Dragon Spirit Scroll. */
        [[nodiscard]] static constexpr bool is_scroll(int index) {
            return index == scroll_index();
        }

        /**
         * Validates applying a Dragon Spirit Scroll.
         * @param is_weapon Target is a weapon.
         * @param is_defense Target is defense gear.
         * @param class_small Target item definition small class.
         * @param equipped Target is equipped.
         * @return Validation result.
         */
        [[nodiscard]] static DragonFusingApplyResult
        can_apply(bool is_weapon, bool is_defense, std::string_view class_small, bool equipped);

        /** @return True for armor slots supported by the Dragon Fusing table. */
        [[nodiscard]] static bool is_armor_part(std::string_view class_small);

        /**
         * Selects a weighted Dragon Fusing effect id.
         * @param tier_roll Uniform roll in [1, 100]. Lower grades have higher probability.
         * @param primary_roll Uniform roll in [0, 4] selecting a primary stat.
         * @param value_roll Uniform non-negative roll selecting the table value.
         * @param is_weapon True for a weapon target.
         * @param class_small Target item definition small class.
         * @return Effect id in [1, 240], or zero for an invalid input.
         */
        [[nodiscard]] static int roll_effect(int tier_roll,
                                             int primary_roll,
                                             int value_roll,
                                             bool is_weapon,
                                             std::string_view class_small);
    };

}  // namespace openkal::server

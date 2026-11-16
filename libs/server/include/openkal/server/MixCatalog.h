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

#include "openkal/platform/Json.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace openkal::server {

    /** Result applied to the center weapon after a successful mix. */
    enum class MixResultAction {
        /** Keep the center item and write mix_type. */
        ApplyMix = 0,

        /** Replace the center item index, then write mix_type. */
        ReplaceWeapon = 1,
    };

    /** Read-only material requirements for one recipe and center weapon grade. */
    struct MixMaterialRow {
        /** Source recipe identifier. */
        std::string recipe_id;
        /** Required center weapon grade, or -1 for any. */
        int center_grade = -1;
        /** Extra weapon grade, or -1 for any; zero means not required. */
        int extra_grade = 0;
        /** Armor grade, or -1 for any; zero means not required. */
        int armor_grade = 0;
        /** Whether the armor must be a chest piece. */
        bool upper_only = false;
        /** Number of stones consumed. */
        int stone_count = 0;
        /** Whether the stones must share their elemental type. */
        bool same_stone_type = false;
        /** Whether this is the two-stone Grade 3 upgrade. */
        bool grade3 = false;
        /** Gold cost configured for this recipe. */
        std::int64_t gold = 0;
    };

    /** One offered mix slot (center or outer pentagram cell). */
    class MixItemOffer {
      public:
        /** @return Bag instance id, or 0 when empty. */
        [[nodiscard]] std::uint32_t iid() const {
            return m_iid;
        }

        /**
         * Sets the bag instance id.
         * @param v Instance id.
         */
        void set_iid(std::uint32_t v) {
            m_iid = v;
        }

        /** @return item definition index. */
        [[nodiscard]] int index() const {
            return m_index;
        }

        /**
         * Sets the item definition index.
         * @param v Item index.
         */
        void set_index(int v) {
            m_index = v;
        }

        /** @return Item grade (item definition level). */
        [[nodiscard]] int grade() const {
            return m_grade;
        }

        /**
         * Sets the item grade.
         * @param v Grade.
         */
        void set_grade(int v) {
            m_grade = v;
        }

        /** @return mix_type low 8 bits. */
        [[nodiscard]] std::uint8_t mix_type() const {
            return m_mix_type;
        }

        /**
         * Sets mix_type low 8 bits.
         * @param v Mix type.
         */
        void set_mix_type(std::uint8_t v) {
            m_mix_type = v;
        }

        /** @return True when the item is wearable. */
        [[nodiscard]] bool wearable() const {
            return m_wearable;
        }

        /**
         * Sets the wearable flag.
         * @param v True when wear 1.
         */
        void set_wearable(bool v) {
            m_wearable = v;
        }

        /** @return True when class_big is weapon. */
        [[nodiscard]] bool is_weapon() const {
            return m_is_weapon;
        }

        /**
         * Sets the weapon flag.
         * @param v True when the item is a weapon.
         */
        void set_weapon(bool v) {
            m_is_weapon = v;
        }

        /** @return True when class_big is defense. */
        [[nodiscard]] bool is_defense() const {
            return m_is_defense;
        }

        /**
         * Sets the defense flag.
         * @param v True when the item is defense gear.
         */
        void set_defense(bool v) {
            m_is_defense = v;
        }

        /** @return Second class token (sword, upperarmor, ...). */
        [[nodiscard]] const std::string& class_small() const {
            return m_class_small;
        }

        /**
         * Sets the second class token.
         * @param v Class small name.
         */
        void set_class_small(std::string v) {
            m_class_small = std::move(v);
        }

      private:
        /** Bag instance id. */
        std::uint32_t m_iid = 0;

        /** item definition index. */
        int m_index = 0;

        /** item definition grade. */
        int m_grade = 0;

        /** mix_type low 8 bits. */
        std::uint8_t m_mix_type = 0;

        /** Wear 1. */
        bool m_wearable = false;

        /** class_big weapon. */
        bool m_is_weapon = false;

        /** class_big defense. */
        bool m_is_defense = false;

        /** class_small token. */
        std::string m_class_small;
    };

    /** Classified materials for one matching mix recipe. */
    class MixMatch {
      public:
        /** @return True when a recipe matched. */
        [[nodiscard]] bool valid() const {
            return m_valid;
        }

        /**
         * Marks the match as valid or not.
         * @param v True when matched.
         */
        void set_valid(bool v) {
            m_valid = v;
        }

        /** @return Recipe id. */
        [[nodiscard]] const std::string& recipe_id() const {
            return m_recipe_id;
        }

        /**
         * Sets the recipe id.
         * @param v Recipe id.
         */
        void set_recipe_id(std::string v) {
            m_recipe_id = std::move(v);
        }

        /** @return NPC mix mode (0 upgrade, 1 making). */
        [[nodiscard]] std::uint8_t mode() const {
            return m_mode;
        }

        /**
         * Sets the NPC mix mode.
         * @param v Mode.
         */
        void set_mode(std::uint8_t v) {
            m_mode = v;
        }

        /** @return Gold cost; 0 means free. */
        [[nodiscard]] std::int64_t gold() const {
            return m_gold;
        }

        /**
         * Sets the gold cost.
         * @param v Gold.
         */
        void set_gold(std::int64_t v) {
            m_gold = v;
        }

        /** @return Center weapon iid. */
        [[nodiscard]] std::uint32_t weapon_iid() const {
            return m_weapon_iid;
        }

        /**
         * Sets the center weapon iid.
         * @param v Instance id.
         */
        void set_weapon_iid(std::uint32_t v) {
            m_weapon_iid = v;
        }

        /** @return Extra weapon iid, or 0. */
        [[nodiscard]] std::uint32_t extra_weapon_iid() const {
            return m_extra_weapon_iid;
        }

        /**
         * Sets the extra weapon iid.
         * @param v Instance id.
         */
        void set_extra_weapon_iid(std::uint32_t v) {
            m_extra_weapon_iid = v;
        }

        /** @return Armor iid. */
        [[nodiscard]] std::uint32_t armor_iid() const {
            return m_armor_iid;
        }

        /**
         * Sets the armor iid.
         * @param v Instance id.
         */
        void set_armor_iid(std::uint32_t v) {
            m_armor_iid = v;
        }

        /** @return First stone iid. */
        [[nodiscard]] std::uint32_t stone1_iid() const {
            return m_stone1_iid;
        }

        /**
         * Sets the first stone iid.
         * @param v Instance id.
         */
        void set_stone1_iid(std::uint32_t v) {
            m_stone1_iid = v;
        }

        /** @return Second stone iid, or 0. */
        [[nodiscard]] std::uint32_t stone2_iid() const {
            return m_stone2_iid;
        }

        /**
         * Sets the second stone iid.
         * @param v Instance id.
         */
        void set_stone2_iid(std::uint32_t v) {
            m_stone2_iid = v;
        }

        /** @return Result action. */
        [[nodiscard]] MixResultAction action() const {
            return m_action;
        }

        /**
         * Sets the result action.
         * @param v Action.
         */
        void set_action(MixResultAction v) {
            m_action = v;
        }

        /** @return Replacement item definition index, or 0 to keep the center item. */
        [[nodiscard]] int replacement_index() const {
            return m_replacement_index;
        }

        /**
         * Sets the replacement item definition index.
         * @param v Item index.
         */
        void set_replacement_index(int v) {
            m_replacement_index = v;
        }

      private:
        /** True when matched. */
        bool m_valid = false;

        /** Recipe id. */
        std::string m_recipe_id;

        /** NPC mix mode. */
        std::uint8_t m_mode = 0;

        /** Gold cost. */
        std::int64_t m_gold = 0;

        /** Center weapon iid. */
        std::uint32_t m_weapon_iid = 0;

        /** Extra weapon iid. */
        std::uint32_t m_extra_weapon_iid = 0;

        /** Armor iid. */
        std::uint32_t m_armor_iid = 0;

        /** First stone iid. */
        std::uint32_t m_stone1_iid = 0;

        /** Second stone iid. */
        std::uint32_t m_stone2_iid = 0;

        /** Result action. */
        MixResultAction m_action = MixResultAction::ApplyMix;

        /** Replacement item index. */
        int m_replacement_index = 0;
    };

    /** Success roll plan for applying a stone to a weapon. */
    class MixRollPlan {
      public:
        /** @return True when this stone may be applied. */
        [[nodiscard]] bool allowed() const {
            return m_allowed;
        }

        /**
         * Sets whether the roll is allowed.
         * @param v True when allowed.
         */
        void set_allowed(bool v) {
            m_allowed = v;
        }

        /** @return Target mix level (1 or 2). */
        [[nodiscard]] int target_level() const {
            return m_target_level;
        }

        /**
         * Sets the target mix level.
         * @param v Level.
         */
        void set_target_level(int v) {
            m_target_level = v;
        }

        /** @return Success chance in percent. */
        [[nodiscard]] int success_percent() const {
            return m_success_percent;
        }

        /**
         * Sets the success chance.
         * @param v Percent.
         */
        void set_success_percent(int v) {
            m_success_percent = v;
        }

        /** @return Encoded mix_type to write on success. */
        [[nodiscard]] int new_mix_type() const {
            return m_new_mix_type;
        }

        /**
         * Sets the encoded mix_type.
         * @param v mix_type.
         */
        void set_new_mix_type(int v) {
            m_new_mix_type = v;
        }

      private:
        /** True when the stone may be applied. */
        bool m_allowed = false;

        /** Target mix level. */
        int m_target_level = 0;

        /** Success chance percent. */
        int m_success_percent = 0;

        /** Encoded mix_type. */
        int m_new_mix_type = 0;
    };

    /** Mix Master recipes and stones loaded from config/mixing. */
    class MixCatalog {
      public:
        /**
         * Loads every JSON file in a directory (stones, rules, grades, recipes).
         * @param dir Directory path.
         * @return True when at least one stone and one recipe were loaded.
         */
        [[nodiscard]] bool load_dir(const std::string& dir);

        /**
         * Loads one JSON object that may contain stones, rules, and/or recipes.
         * @param text JSON text.
         * @return True when the document parsed.
         */
        [[nodiscard]] bool load_json_text(std::string_view text);

        /** @return True when stones, recipes, and mix-grade rows are present. */
        [[nodiscard]] bool ready() const;

        /**
         * Mix-stone item definition check from JSON.
         * @param index item definition index.
         * @return True when the index is a mixing stone.
         */
        [[nodiscard]] bool is_mixing_stone(int index) const;

        /**
         * Mix type encoded in mix_type low nibble (1..11), or 0.
         * @param index item definition index.
         * @return Stone type, or 0.
         */
        [[nodiscard]] int stone_type(int index) const;

        /**
         * Minimum existing mix level required to use this stone (Mystery / Blood).
         * @param index item definition index.
         * @return Required mix level, or 0.
         */
        [[nodiscard]] int stone_require_mix_level(int index) const;

        /** Builds the NPC material list from the active recipe and grade tables.
         * @param mode NPC mode: 0 for upgrading, 1 for creating weapons.
         * @return Requirements sorted by stone count, center grade and recipe id.
         */
        [[nodiscard]] std::vector<MixMaterialRow> material_rows(std::uint8_t mode) const;

        /**
         * Matches NPC mode + offered items against loaded recipes.
         * @param mode 0 upgrade, 1 making.
         * @param center Center slot item.
         * @param outer Outer pentagram items (empty slots omitted).
         * @param player_class Knight/mage/archer/thief (0..3), or -1 to skip replace.
         * @return Filled match when a recipe fits; invalid otherwise.
         */
        [[nodiscard]] MixMatch match(std::uint8_t mode,
                                     const MixItemOffer& center,
                                     const std::vector<MixItemOffer>& outer,
                                     int player_class) const;

        /**
         * Builds the success-roll plan for applying a stone type to a weapon.
         *
         * Unmixed weapons go to mix level 1 of the stone. A level-1 mix accepts any
         * allowed stone (same or different) and becomes level 2 of that stone.
         * Mystery and Demon Blood are G3: they require an existing level-2 mix and
         * two stones of the same type (enforced by the upgrade_g3 recipe).
         *
         * @param weapon_mix_type Current mix_type low 8 bits.
         * @param stone_index First stone item definition index.
         * @return Plan; allowed() is false when the stone cannot be applied.
         */
        [[nodiscard]] MixRollPlan plan_roll(std::uint8_t weapon_mix_type, int stone_index) const;

        /** Checks whether a definition is an Imperial weapon produced by a making recipe.
         * @param index Item definition index.
         * @return True when a replace-weapon recipe produces this definition.
         */
        [[nodiscard]] bool is_imperial_weapon(int index) const;

        /** @return Number of loaded recipes. */
        [[nodiscard]] std::size_t recipe_count() const {
            return m_recipes.size();
        }

        /** @return Number of loaded stones. */
        [[nodiscard]] std::size_t stone_count() const {
            return m_stones.size();
        }

      private:
        /** One mix-list row: extra weapon and armor grades for a center grade. */
        class GradeRow {
          public:
            /** @return Center weapon grade. */
            [[nodiscard]] int center_grade() const {
                return m_center_grade;
            }

            /**
             * Sets the center weapon grade.
             * @param v Grade.
             */
            void set_center_grade(int v) {
                m_center_grade = v;
            }

            /** @return Required extra weapon grade. */
            [[nodiscard]] int extra_grade() const {
                return m_extra_grade;
            }

            /**
             * Sets the extra weapon grade.
             * @param v Grade.
             */
            void set_extra_grade(int v) {
                m_extra_grade = v;
            }

            /** @return Required armor grade. */
            [[nodiscard]] int armor_grade() const {
                return m_armor_grade;
            }

            /**
             * Sets the armor grade.
             * @param v Grade.
             */
            void set_armor_grade(int v) {
                m_armor_grade = v;
            }

            /** @return True when only upperarmor is accepted. */
            [[nodiscard]] bool upper_only() const {
                return m_upper_only;
            }

            /**
             * Sets the upper-only armor flag.
             * @param v True for chest-only.
             */
            void set_upper_only(bool v) {
                m_upper_only = v;
            }

          private:
            /** Center weapon grade. */
            int m_center_grade = 0;

            /** Extra weapon grade. */
            int m_extra_grade = 0;

            /** Armor grade. */
            int m_armor_grade = 0;

            /** True when armor must be upperarmor. */
            bool m_upper_only = false;
        };

        /** One material constraint from JSON. */
        class MaterialSpec {
          public:
            /** @return True when this material must be present. */
            [[nodiscard]] bool required() const {
                return m_required;
            }

            /**
             * Sets the required flag.
             * @param v True when required.
             */
            void set_required(bool v) {
                m_required = v;
            }

            /** @return Required grade, or -1 for any. */
            [[nodiscard]] int grade() const {
                return m_grade;
            }

            /**
             * Sets the required grade.
             * @param v Grade, or -1.
             */
            void set_grade(int v) {
                m_grade = v;
            }

            /** @return True when only upperarmor is accepted. */
            [[nodiscard]] bool upper_only() const {
                return m_upper_only;
            }

            /**
             * Sets the upper-only flag.
             * @param v True for chest only.
             */
            void set_upper_only(bool v) {
                m_upper_only = v;
            }

            /** @return True when wearable is required. */
            [[nodiscard]] bool wearable() const {
                return m_wearable;
            }

            /**
             * Sets the wearable requirement.
             * @param v True when wear 1 is required.
             */
            void set_wearable(bool v) {
                m_wearable = v;
            }

          private:
            /** True when the material must be present. */
            bool m_required = false;

            /** Required grade, or -1. */
            int m_grade = -1;

            /** True when only upperarmor is accepted. */
            bool m_upper_only = false;

            /** True when wearable is required. */
            bool m_wearable = false;
        };

        /** One class/kind → Imperial index row. */
        class ReplaceEntry {
          public:
            /** @return Player class 0..3, or -1 for any. */
            [[nodiscard]] int player_class() const {
                return m_player_class;
            }

            /**
             * Sets the player class.
             * @param v Class index.
             */
            void set_player_class(int v) {
                m_player_class = v;
            }

            /** @return Weapon class_small to match, or empty for any. */
            [[nodiscard]] const std::string& kind() const {
                return m_kind;
            }

            /**
             * Sets the weapon kind.
             * @param v class_small.
             */
            void set_kind(std::string v) {
                m_kind = std::move(v);
            }

            /** @return Replacement item definition index. */
            [[nodiscard]] int index() const {
                return m_index;
            }

            /**
             * Sets the replacement index.
             * @param v Item index.
             */
            void set_index(int v) {
                m_index = v;
            }

          private:
            /** Player class, or -1. */
            int m_player_class = -1;

            /** Weapon class_small. */
            std::string m_kind;

            /** Replacement index. */
            int m_index = 0;
        };

        /** One mix recipe. */
        class Recipe {
          public:
            /** @return Recipe id. */
            [[nodiscard]] const std::string& id() const {
                return m_id;
            }

            /**
             * Sets the recipe id.
             * @param v Id.
             */
            void set_id(std::string v) {
                m_id = std::move(v);
            }

            /** @return NPC mix mode. */
            [[nodiscard]] std::uint8_t mode() const {
                return m_mode;
            }

            /**
             * Sets the NPC mix mode.
             * @param v Mode.
             */
            void set_mode(std::uint8_t v) {
                m_mode = v;
            }

            /** @return Gold cost. */
            [[nodiscard]] std::int64_t gold() const {
                return m_gold;
            }

            /**
             * Sets the gold cost.
             * @param v Gold.
             */
            void set_gold(std::int64_t v) {
                m_gold = v;
            }

            /** @return Center weapon spec. */
            [[nodiscard]] const MaterialSpec& center() const {
                return m_center;
            }

            /** @return Mutable center spec. */
            [[nodiscard]] MaterialSpec& center() {
                return m_center;
            }

            /** @return Extra weapon spec. */
            [[nodiscard]] const MaterialSpec& extra_weapon() const {
                return m_extra_weapon;
            }

            /** @return Mutable extra weapon spec. */
            [[nodiscard]] MaterialSpec& extra_weapon() {
                return m_extra_weapon;
            }

            /** @return Armor spec. */
            [[nodiscard]] const MaterialSpec& armor() const {
                return m_armor;
            }

            /** @return Mutable armor spec. */
            [[nodiscard]] MaterialSpec& armor() {
                return m_armor;
            }

            /** @return Required stone count. */
            [[nodiscard]] int stone_count() const {
                return m_stone_count;
            }

            /**
             * Sets the required stone count.
             * @param v Count.
             */
            void set_stone_count(int v) {
                m_stone_count = v;
            }

            /** @return True when both stones must share a type. */
            [[nodiscard]] bool stones_same_type() const {
                return m_stones_same_type;
            }

            /**
             * Sets the same-type stone flag.
             * @param v True when types must match.
             */
            void set_stones_same_type(bool v) {
                m_stones_same_type = v;
            }

            /** @return Result action. */
            [[nodiscard]] MixResultAction action() const {
                return m_action;
            }

            /**
             * Sets the result action.
             * @param v Action.
             */
            void set_action(MixResultAction v) {
                m_action = v;
            }

            /** @return Replacement table. */
            [[nodiscard]] const std::vector<ReplaceEntry>& replace() const {
                return m_replace;
            }

            /** @return Mutable replacement table. */
            [[nodiscard]] std::vector<ReplaceEntry>& replace() {
                return m_replace;
            }

          private:
            /** Recipe id. */
            std::string m_id;

            /** NPC mix mode. */
            std::uint8_t m_mode = 0;

            /** Gold cost. */
            std::int64_t m_gold = 0;

            /** Center weapon constraint. */
            MaterialSpec m_center;

            /** Extra weapon constraint. */
            MaterialSpec m_extra_weapon;

            /** Armor constraint. */
            MaterialSpec m_armor;

            /** Required stone count. */
            int m_stone_count = 1;

            /** True when both stones must share a type. */
            bool m_stones_same_type = false;

            /** Result action. */
            MixResultAction m_action = MixResultAction::ApplyMix;

            /** Replacement table. */
            std::vector<ReplaceEntry> m_replace;
        };

        /** One mixing stone. */
        class StoneDef {
          public:
            /** @return item definition index. */
            [[nodiscard]] int index() const {
                return m_index;
            }

            /**
             * Sets the item definition index.
             * @param v Index.
             */
            void set_index(int v) {
                m_index = v;
            }

            /** @return Mix type 1..11. */
            [[nodiscard]] int type() const {
                return m_type;
            }

            /**
             * Sets the mix type.
             * @param v Type.
             */
            void set_type(int v) {
                m_type = v;
            }

            /** @return Required existing mix level. */
            [[nodiscard]] int require_mix_level() const {
                return m_require_mix_level;
            }

            /**
             * Sets the required mix level.
             * @param v Level.
             */
            void set_require_mix_level(int v) {
                m_require_mix_level = v;
            }

          private:
            /** item definition index. */
            int m_index = 0;

            /** Mix type. */
            int m_type = 0;

            /** Required existing mix level. */
            int m_require_mix_level = 0;
        };

        /**
         * Mix-list row for a center weapon grade (upgrade G1–G3).
         * @param center_grade Center item grade.
         * @return Row, or nullptr when that grade cannot be mixed.
         */
        [[nodiscard]] const GradeRow* find_grade(int center_grade) const;

        /**
         * Picks a replacement index for a class and weapon kind.
         * @param recipe Recipe.
         * @param player_class Player class 0..3, or -1.
         * @param kind Center weapon class_small.
         * @return Item index, or 0.
         */
        [[nodiscard]] static int
        pick_replacement(const Recipe& recipe, int player_class, std::string_view kind);

        /**
         * True when an offer satisfies a material spec (grade / slot / wearable).
         * @param spec Material spec.
         * @param item Offered item.
         * @param as_armor True to apply the upper-only slot check.
         * @return True when the item fits.
         */
        [[nodiscard]] static bool
        spec_fits(const MaterialSpec& spec, const MixItemOffer& item, bool as_armor);

        /**
         * Merges stones, rules, mix-grade rows, and recipes from one JSON object.
         * @param root JSON object.
         * @return True when the document was an object.
         */
        [[nodiscard]] bool merge_root(const platform::Json& root);

        /** Loaded recipes. */
        std::vector<Recipe> m_recipes;

        /** Stones by item definition index. */
        std::unordered_map<int, StoneDef> m_stones;

        /** Mix-list extra/armor grades by center weapon grade. */
        std::unordered_map<int, GradeRow> m_grades;

        /** Success percent for a new or replacement mix (level 1). */
        int m_success_level1_percent = 66;

        /** Success percent for same-stone level 2. */
        int m_success_level2_percent = 33;

        /** Maximum mix level. */
        int m_max_mix_level = 2;

        /** True when a max-level same-stone mix is rejected. */
        bool m_block_same_stone_at_max = true;
    };

}  // namespace openkal::server

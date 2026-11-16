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

#include "openkal/client/PrefixDb.h"
#include "openkal/protocol/Skin.h"
#include "openkal/server/BofCatalog.h"
#include "openkal/server/EssenceBox.h"

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace openkal::client {

    /**
     * Drop-glow / name color band stored on each item JSON.
     * 0 white, 1 blue, 2 green, 3 orange, 4 red, 5 purple.
     * Money (Zamogeon) uses a gold glow regardless of this value.
     */
    enum class ItemRarity : int {
        /** White glow (rarity 0). */
        White = 0,

        /** Blue glow (rarity 1). */
        Blue = 1,

        /** Green glow (rarity 2). */
        Green = 2,

        /** Orange glow (rarity 3). */
        Orange = 3,

        /** Red glow (rarity 4). */
        Red = 4,

        /** Purple glow (rarity 5). */
        Purple = 5,
    };

    /** Lowest valid ItemRarity value. */
    inline constexpr int kItemRarityMin = 0;

    /** Highest valid ItemRarity value. */
    inline constexpr int kItemRarityMax = 5;

    /** One item definition / JSON item definition. */
    class ItemDef {
      public:
        /** @return True for a cosmetic skin with a supported slot. */
        [[nodiscard]] bool is_skin() const;

        /** @return Cosmetic slot, or None for ordinary equipment. */
        [[nodiscard]] protocol::SkinSlot skin_slot() const;

        /** @return True for a skin that replaces all body clothing. */
        [[nodiscard]] bool full_body_skin() const;

        /** @return True when the skin uses a costume mesh, including partial costumes. */
        [[nodiscard]] bool uses_costume_mesh() const;

        /** Checks whether a whole-body costume retains the normal hair mesh.
         * @return True only when a whole-body skin explicitly keeps normal hair.
         */
        [[nodiscard]] bool skin_keeps_hair() const;

        /** Checks whether a whole-body costume supplies its own complete face.
         * @return True only when a whole-body skin explicitly replaces the face.
         */
        [[nodiscard]] bool skin_replaces_face() const;

        /** Checks whether this weapon skin accepts the actual weapon.
         * @param weapon Actual equipped item definition.
         * @return True only for matching, classified weapon types.
         */
        [[nodiscard]] bool skin_matches_weapon(const ItemDef& weapon) const;

        /** Resolves the combat weapon category accepted by this skin.
         * @return Required weapon category, or None for an unclassified appearance.
         */
        [[nodiscard]] protocol::WeaponSkinType weapon_skin_type() const {
            return protocol::resolve_weapon_skin_type(m_skin_weapon_type, m_class_limit);
        }

        /** Configures the weapon subclass required by this skin.
         * @param value Weapon subclass token; empty uses an unambiguous class default.
         */
        void set_skin_weapon_type(std::string value) {
            m_skin_weapon_type = std::move(value);
        }

        /** Retains normal hair for a costume without built-in hair or headwear.
         * @param value True to keep the character's normal hair mesh.
         */
        void set_skin_keep_hair(bool value) {
            m_skin_keep_hair = value;
        }

        /** Selects a costume's complete face instead of the character face.
         * @param value True to omit the normal face while this costume is worn.
         */
        void set_skin_replace_face(bool value) {
            m_skin_replace_face = value;
        }

        /** Selects the costume mesh family independently of the cosmetic slot.
         * @param value True to use the costume mesh family for an individual skin.
         */
        void set_skin_costume_mesh(bool value) {
            m_skin_costume_mesh = value;
        }

        /** @return True if this item is a weapon. */
        [[nodiscard]] bool is_weapon() const;

        /** @return True if this item is defense gear. */
        [[nodiscard]] bool is_defense() const;

        /** @return True if this item is an ornament (ring/necklace/trinket). */
        [[nodiscard]] bool is_ornament() const;

        /** @return True if this item is money (Zamogeon). */
        [[nodiscard]] bool is_coin() const;

        /** @return True if this item is a transform / animal item. */
        [[nodiscard]] bool is_transform() const;

        /**
         * item definition extra.race (0 tiger / 4 panther / 6 bird / 8 gale).
         * @return Race, or -1 when unset.
         */
        [[nodiscard]] int transform_race() const {
            return m_transform_race;
        }

        /**
         * Sets extra.race from the transform JSON.
         * @param v Race index.
         */
        void set_transform_race(int v) {
            m_transform_race = v;
        }

        /**
         * Cocoon divisor for MakeTip egg stats (A=2 .. E=6, F=0).
         * Slot order is STR, HTH, DEX, INT, WIS.
         * @param slot 0–4.
         * @return Divisor, or 0 when that stat is hidden.
         */
        [[nodiscard]] int cocoon_ability(int slot) const {
            if (slot < 0 || slot >= static_cast<int>(m_cocoon_ability.size())) {
                return 0;
            }
            return m_cocoon_ability[static_cast<std::size_t>(slot)];
        }

        /**
         * Sets one extra.cocoon letter's divisor.
         * @param slot 0–4.
         * @param divisor A=2 .. E=6, F=0.
         */
        void set_cocoon_ability(int slot, int divisor) {
            if (slot < 0 || slot >= static_cast<int>(m_cocoon_ability.size())) {
                return;
            }
            m_cocoon_ability[static_cast<std::size_t>(slot)] = divisor;
        }

        /**
         * Extra HyperText stem for a grown egg (item definition imageex).
         * @param slot 0 = levels 11–20, 1 = 21–30, 2 = 31+.
         * @return Image stem, or empty when unset / out of range.
         */
        [[nodiscard]] std::string image_ex(int slot) const {
            if (slot < 0 || slot >= static_cast<int>(m_image_ex.size())) {
                return {};
            }
            return m_image_ex[static_cast<std::size_t>(slot)];
        }

        /**
         * Sets one extra.imageex stem.
         * @param slot 0–2.
         * @param v HyperText stem without extension.
         */
        void set_image_ex(int slot, std::string v) {
            if (slot < 0 || slot >= static_cast<int>(m_image_ex.size())) {
                return;
            }
            m_image_ex[static_cast<std::size_t>(slot)] = std::move(v);
        }

        /** @return True if this definition belongs to the special quest item class. */
        [[nodiscard]] bool is_quest() const;

        /** @return True for special quest items granted only through quest scripts. */
        [[nodiscard]] bool is_task() const {
            return m_task;
        }

        /** Applies the classification derived from quest grants and loot sources.
         * @param task True for a quest-only special item.
         */
        void set_task(bool task) {
            m_task = task;
        }

        /** @return True if the item can be equipped. */
        [[nodiscard]] bool can_equip() const;

        /** @return True for standard or limited pet items. */
        [[nodiscard]] bool is_pet() const;

        /** @return True for common riding-animal registration items. */
        [[nodiscard]] bool is_ride() const;

        /**
         * Checks class and level limits for equipping.
         * @param clazz Player class index.
         * @param level Player level.
         * @return True if the player may equip this item.
         */
        [[nodiscard]] bool can_equip_by(int clazz, int level) const;

        [[nodiscard]] std::int32_t index() const {
            return m_index;
        }
        void set_index(std::int32_t v) {
            m_index = v;
        }

        [[nodiscard]] const std::string& name() const {
            return m_name;
        }
        void set_name(std::string v) {
            m_name = std::move(v);
        }

        [[nodiscard]] const std::string& image() const {
            return m_image;
        }
        void set_image(std::string v) {
            m_image = std::move(v);
        }

        [[nodiscard]] const std::string& clazz() const {
            return m_clazz;
        }
        void set_clazz(std::string v) {
            m_clazz = std::move(v);
        }

        [[nodiscard]] const std::string& class_big() const {
            return m_class_big;
        }
        void set_class_big(std::string v) {
            m_class_big = std::move(v);
        }

        [[nodiscard]] const std::string& class_small() const {
            return m_class_small;
        }
        void set_class_small(std::string v) {
            m_class_small = std::move(v);
        }

        [[nodiscard]] int action_class() const {
            return m_action_class;
        }
        void set_action_class(int v) {
            m_action_class = v;
        }

        [[nodiscard]] int action_index() const {
            return m_action_index;
        }
        void set_action_index(int v) {
            m_action_index = v;
        }

        [[nodiscard]] bool wearable() const {
            return m_wearable;
        }
        void set_wearable(bool v) {
            m_wearable = v;
        }

        /** @return True when the item definition enables stacking. */
        [[nodiscard]] bool stackable() const {
            return m_stackable;
        }

        /**
         * Sets the stacking flag from the item definition.
         * @param v True when JSON stackable is 1.
         */
        void set_stackable(bool v) {
            m_stackable = v;
        }

        /**
         * @return True if bag stacks of this item may merge. Wearable gear never
         *         stacks (wear 1); other items stack only with JSON stackable set to 1.
         */
        [[nodiscard]] bool is_stackable() const {
            return m_stackable && !m_wearable;
        }

        [[nodiscard]] int range() const {
            return m_range;
        }
        void set_range(int v) {
            m_range = v;
        }

        /** @return Item grade shown as [ Grade N ] (item definition level). */
        [[nodiscard]] int grade() const {
            return m_grade;
        }

        /** @param v Item grade. */
        void set_grade(int v) {
            m_grade = v;
        }

        /**
         * Drop-glow color band (0 white … 5 purple). Money still glows gold.
         * @return Rarity in 0..5.
         */
        [[nodiscard]] int rarity() const {
            return m_rarity;
        }

        /**
         * Sets the rarity band, clamped to 0..5.
         * @param v Requested rarity.
         */
        void set_rarity(int v);

        /**
         * RGB for the ground-drop glow. Zamogeon is gold; others follow rarity().
         * @param r Out red in 0–1.
         * @param g Out green in 0–1.
         * @param b Out blue in 0–1.
         */
        void glow_color(float& r, float& g, float& b) const;

        /** @return Minimum level to equip (0 = no level limit line). */
        [[nodiscard]] int level_limit() const {
            return m_level_limit;
        }
        void set_level_limit(int v) {
            m_level_limit = v;
        }

        [[nodiscard]] int class_limit() const {
            return m_class_limit;
        }
        void set_class_limit(int v) {
            m_class_limit = v;
        }

        [[nodiscard]] int min_attack() const {
            return m_min_attack;
        }
        void set_min_attack(int v) {
            m_min_attack = v;
        }

        [[nodiscard]] int max_attack() const {
            return m_max_attack;
        }
        void set_max_attack(int v) {
            m_max_attack = v;
        }

        [[nodiscard]] int min_magic() const {
            return m_min_magic;
        }
        void set_min_magic(int v) {
            m_min_magic = v;
        }

        [[nodiscard]] int max_magic() const {
            return m_max_magic;
        }
        void set_max_magic(int v) {
            m_max_magic = v;
        }

        [[nodiscard]] int hit() const {
            return m_hit;
        }
        void set_hit(int v) {
            m_hit = v;
        }

        [[nodiscard]] int defense() const {
            return m_defense;
        }
        void set_defense(int v) {
            m_defense = v;
        }

        [[nodiscard]] int dodge() const {
            return m_dodge;
        }
        void set_dodge(int v) {
            m_dodge = v;
        }

        [[nodiscard]] int absorb() const {
            return m_absorb;
        }
        void set_absorb(int v) {
            m_absorb = v;
        }

        [[nodiscard]] int str() const {
            return m_str;
        }
        void set_str(int v) {
            m_str = v;
        }

        [[nodiscard]] int hth() const {
            return m_hth;
        }
        void set_hth(int v) {
            m_hth = v;
        }

        [[nodiscard]] int intel() const {
            return m_intel;
        }
        void set_intel(int v) {
            m_intel = v;
        }

        [[nodiscard]] int wis() const {
            return m_wis;
        }
        void set_wis(int v) {
            m_wis = v;
        }

        [[nodiscard]] int dex() const {
            return m_dex;
        }
        void set_dex(int v) {
            m_dex = v;
        }

        [[nodiscard]] int hp() const {
            return m_hp;
        }
        void set_hp(int v) {
            m_hp = v;
        }

        [[nodiscard]] int mp() const {
            return m_mp;
        }
        void set_mp(int v) {
            m_mp = v;
        }

        [[nodiscard]] int resist_fire() const {
            return m_resist_fire;
        }
        void set_resist_fire(int v) {
            m_resist_fire = v;
        }

        [[nodiscard]] int resist_ice() const {
            return m_resist_ice;
        }
        void set_resist_ice(int v) {
            m_resist_ice = v;
        }

        [[nodiscard]] int resist_lightning() const {
            return m_resist_lightning;
        }
        void set_resist_lightning(int v) {
            m_resist_lightning = v;
        }

        [[nodiscard]] int resist_curse() const {
            return m_resist_curse;
        }
        void set_resist_curse(int v) {
            m_resist_curse = v;
        }

        [[nodiscard]] int resist_palsy() const {
            return m_resist_palsy;
        }
        void set_resist_palsy(int v) {
            m_resist_palsy = v;
        }

        /** @return Max endurance from item definition, or 0 if none (ornaments always 0). */
        [[nodiscard]] int endurance() const {
            return m_endurance;
        }
        void set_endurance(int v) {
            m_endurance = v;
        }

        /** @return NPC sell price from item definition Sell, or 0. */
        [[nodiscard]] int sell() const {
            return m_sell;
        }

        /** @return NPC purchase price from item definition Buy, or 0. */
        [[nodiscard]] int buy() const {
            return m_buy;
        }

        /**
         * Sets the NPC purchase price.
         * @param v item definition Buy value.
         */
        void set_buy(int v) {
            m_buy = v;
        }

        /**
         * Sets the NPC sell price.
         * @param v item definition Sell value.
         */
        void set_sell(int v) {
            m_sell = v;
        }

        /** @return Inline description text (may contain #n), or empty. */
        [[nodiscard]] const std::string& description() const {
            return m_description;
        }

        /** @param v Description text from the item JSON. */
        void set_description(std::string v) {
            m_description = std::move(v);
        }

        [[nodiscard]] int attack_speed_ms() const {
            return m_attack_speed_ms;
        }
        void set_attack_speed_ms(int v) {
            m_attack_speed_ms = v;
        }

        /** @return True if the item can be used from the bag (item definition use 1). */
        [[nodiscard]] bool usable() const {
            return m_usable;
        }

        /**
         * Marks the item as usable from the bag.
         * @param v True when (use 1).
         */
        void set_usable(bool v) {
            m_usable = v;
        }

        /**
         * @return Zero-based city teleport table index, or -1 if this is not a
         *         teleport scroll.
         */
        [[nodiscard]] int teleport_loc() const {
            return m_teleport_loc;
        }

        /**
         * Sets the city teleport destination index (kTeleportLocs).
         * @param v Zero-based loc, or -1 for none.
         */
        void set_teleport_loc(int v) {
            m_teleport_loc = v;
        }

        /** @return HP restored on consume, or 0. */
        [[nodiscard]] int refresh_hp() const {
            return m_refresh_hp;
        }

        /**
         * Sets HP restored on consume.
         * @param v Hit points, or 0.
         */
        void set_refresh_hp(int v) {
            m_refresh_hp = v;
        }

        /** @return True when this medicine can only be used by Auto HP. */
        bool auto_hp() const {
            return m_auto_hp;
        }

        /** Sets whether this medicine requires automatic use.
         * @param value True for Auto-HP medicine.
         */
        void set_auto_hp(bool value) {
            m_auto_hp = value;
        }

        /** @return MP restored on consume, or 0. */
        [[nodiscard]] int refresh_mp() const {
            return m_refresh_mp;
        }

        /**
         * Sets MP restored on consume.
         * @param v Mana points, or 0.
         */
        void set_refresh_mp(int v) {
            m_refresh_mp = v;
        }

        /** @return Shared refresh-item cooldown in milliseconds, or 0. */
        [[nodiscard]] int cooltime_ms() const {
            return m_cooltime_ms;
        }

        /**
         * Sets the shared refresh-item cooldown.
         * @param v Cooldown in milliseconds (item definition cooltime first value).
         */
        void set_cooltime_ms(int v) {
            m_cooltime_ms = v;
        }

        /** @return True if right-click should consume this stack (teleport / HP / MP / buff /
         * revival / Riding Cache / Essence Box). */
        [[nodiscard]] bool is_consumable() const {
            return m_teleport_loc >= 0 || m_refresh_hp > 0 || m_refresh_mp > 0 ||
                   m_buff_type >= 0 || m_revival_extra_cut >= 0 || is_riding_cache() ||
                   is_essence_box();
        }

        /** @return True for the server-authoritative Hanin Essence Box. */
        [[nodiscard]] bool is_essence_box() const {
            return m_usable && m_index == server::kEssenceBoxItem;
        }

        /** @return True for a server-authoritative Battle Riding Cache. */
        [[nodiscard]] bool is_riding_cache() const {
            return m_usable && m_class_small == "battleridingcache";
        }

        /** @return S2C_EFFECT id on use, or 0. */
        [[nodiscard]] int effect() const {
            return m_effect;
        }

        /**
         * Sets the S2C_EFFECT id played on use.
         * @param v Effect ordinal.
         */
        void set_effect(int v) {
            m_effect = v;
        }

        /** @return specialty.buff[0] BUFF_TYPE, or -1. */
        [[nodiscard]] int buff_type() const {
            return m_buff_type;
        }

        /**
         * Sets the buff type from specialty.buff[0].
         * @param v BUFF_TYPE ordinal, or -1.
         */
        void set_buff_type(int v) {
            m_buff_type = v;
        }

        /** @return specialty.buff[1] seconds or charge count. */
        [[nodiscard]] int buff_remain() const {
            return m_buff_remain;
        }

        /**
         * Sets buff remain from specialty.buff[1].
         * @param v Seconds or charges.
         */
        void set_buff_remain(int v) {
            m_buff_remain = v;
        }

        /** @return specialty.buff[2] amount. */
        [[nodiscard]] int buff_amount() const {
            return m_buff_amount;
        }

        /**
         * Sets buff amount from specialty.buff[2].
         * @param v Amount value.
         */
        void set_buff_amount(int v) {
            m_buff_amount = v;
        }

        /** @return specialty.revival extra-cut percent, or -1. */
        [[nodiscard]] int revival_extra_cut() const {
            return m_revival_extra_cut;
        }

        /**
         * Sets revival extra-cut from specialty.revival.
         * @param v Percent forgiven (100 = no loss), or -1.
         */
        void set_revival_extra_cut(int v) {
            m_revival_extra_cut = v;
        }

        /** @return True if specialty.changeprefix is present. */
        [[nodiscard]] bool has_changeprefix() const {
            return m_has_changeprefix;
        }

        /**
         * Marks the item as a changeprefix charm.
         * @param v True when specialty.changeprefix is set.
         */
        void set_has_changeprefix(bool v) {
            m_has_changeprefix = v;
        }

        /** @return Changeprefix target class (weapon / defense / ornament). */
        [[nodiscard]] const std::string& changeprefix_target() const {
            return m_changeprefix_target;
        }

        /**
         * Sets the changeprefix target class.
         * @param v Target class name.
         */
        void set_changeprefix_target(std::string v) {
            m_changeprefix_target = std::move(v);
        }

        /** @return Cumulative success rate for prefix A (1–100). */
        [[nodiscard]] int changeprefix_rate0() const {
            return m_changeprefix_rate0;
        }

        /**
         * Sets the cumulative success rate for prefix A.
         * @param v Rate 0–100.
         */
        void set_changeprefix_rate0(int v) {
            m_changeprefix_rate0 = v;
        }

        /** @return Prefix index applied on roll A. */
        [[nodiscard]] int changeprefix_prefix0() const {
            return m_changeprefix_prefix0;
        }

        /**
         * Sets prefix index A.
         * @param v Prefix index.
         */
        void set_changeprefix_prefix0(int v) {
            m_changeprefix_prefix0 = v;
        }

        /** @return Cumulative success rate for prefix B (1–100). */
        [[nodiscard]] int changeprefix_rate1() const {
            return m_changeprefix_rate1;
        }

        /**
         * Sets the cumulative success rate for prefix B.
         * @param v Rate 0–100.
         */
        void set_changeprefix_rate1(int v) {
            m_changeprefix_rate1 = v;
        }

        /** @return Prefix index applied on roll B. */
        [[nodiscard]] int changeprefix_prefix1() const {
            return m_changeprefix_prefix1;
        }

        /**
         * Sets prefix index B.
         * @param v Prefix index.
         */
        void set_changeprefix_prefix1(int v) {
            m_changeprefix_prefix1 = v;
        }

        /** @return True when specialty.repair is present. */
        [[nodiscard]] bool has_repair() const {
            return m_has_repair;
        }

        /**
         * Marks the item as a repair tool.
         * @param v True when specialty.repair is set.
         */
        void set_has_repair(bool v) {
            m_has_repair = v;
        }

        /** @return Repair JSON first value (0 = add, 1 = Doggebi). */
        [[nodiscard]] int repair_mode() const {
            return m_repair_mode;
        }

        /**
         * Sets the repair mode.
         * @param v Mode from specialty.repair[0].
         */
        void set_repair_mode(int v) {
            m_repair_mode = v;
        }

        /** @return Repair JSON second value (points added, or max reduced). */
        [[nodiscard]] int repair_amount() const {
            return m_repair_amount;
        }

        /**
         * Sets the repair amount.
         * @param v Amount from specialty.repair[1].
         */
        void set_repair_amount(int v) {
            m_repair_amount = v;
        }

        /** @return True when specialty.charming is present. */
        [[nodiscard]] bool has_charming() const {
            return !m_charming.empty();
        }

        /** @return specialty.charming token (xattack / xmagic / ...). */
        [[nodiscard]] const std::string& charming() const {
            return m_charming;
        }

        /**
         * Sets the charming token.
         * @param v Lowercase charming name.
         */
        void set_charming(std::string v) {
            m_charming = std::move(v);
        }

        /** @return True when specialty.bof is set. */
        [[nodiscard]] bool has_bof() const {
            return m_bof;
        }

        /**
         * Sets the Bead of Fire apply flag.
         * @param v True when this item applies BOF to armor.
         */
        void set_bof(bool v) {
            m_bof = v;
        }

      private:
        /** item definition index. */
        std::int32_t m_index = 0;

        /** Display name from the item JSON. */
        std::string m_name;

        /** item definition Image stem (HyperText/<image>.bmp). */
        std::string m_image;

        /** Combined class string, e.g. "weapon sword". */
        std::string m_clazz;

        /** Primary class bucket ("weapon" / "defense" / "general"). */
        std::string m_class_big;

        /** Secondary class ("sword" / "upperarmor" / ...). */
        std::string m_class_small;

        /** Individual skin uses a costume mesh without replacing the entire body. */
        bool m_skin_costume_mesh = false;

        /** Whole-body appearance exceptions; inactive for ordinary equipment. */
        bool m_skin_keep_hair = false;
        bool m_skin_replace_face = false;
        /** Explicit weapon category; ambiguous models remain unclassified. */
        std::string m_skin_weapon_type;

        /** (Action a b) → a. */
        int m_action_class = 0;

        /** (Action a b) → b. */
        int m_action_index = 0;

        /** (wear 1). */
        bool m_wearable = false;

        /** Stacks may merge when JSON stackable is 1 and the item is not wearable. */
        bool m_stackable = false;

        /** Attack range in server pixels. */
        int m_range = 0;

        /** item definition (level N) — tip grade. */
        int m_grade = 0;

        /** JSON rarity band (0 white … 5 purple). */
        int m_rarity = 0;

        /** Minimum level to equip (0 = unrestricted). */
        int m_level_limit = 0;

        /** -1 any; 0 knight / 1 magician / 2 archer / 3 thief. */
        int m_class_limit = -1;

        /** Minimum physical attack. */
        int m_min_attack = 0;

        /** Maximum physical attack. */
        int m_max_attack = 0;

        /** Minimum magical attack. */
        int m_min_magic = 0;

        /** Maximum magical attack. */
        int m_max_magic = 0;

        /** Hit / on-target bonus. */
        int m_hit = 0;

        /** Defense points. */
        int m_defense = 0;

        /** Evasion / dodge points. */
        int m_dodge = 0;

        /** Absorption rate. */
        int m_absorb = 0;

        /** Strength bonus. */
        int m_str = 0;

        /** Health (constitution) bonus. */
        int m_hth = 0;

        /** Intelligence bonus. */
        int m_intel = 0;

        /** Wisdom bonus. */
        int m_wis = 0;

        /** Agility / dexterity bonus. */
        int m_dex = 0;

        /** HP bonus. */
        int m_hp = 0;

        /** MP bonus. */
        int m_mp = 0;

        /** Fire resist. */
        int m_resist_fire = 0;

        /** Ice resist. */
        int m_resist_ice = 0;

        /** Lightning resist. */
        int m_resist_lightning = 0;

        /** Curse resist. */
        int m_resist_curse = 0;

        /** Non-elemental / palsy resist. */
        int m_resist_palsy = 0;

        /** Max endurance from definition. */
        int m_endurance = 0;

        /** NPC sell price from item definition Sell. */
        int m_sell = 0;

        /** NPC purchase price from item definition Buy. */
        int m_buy = 0;

        /** Inline description text from the item JSON. */
        std::string m_description;

        /** Attack speed in milliseconds. */
        int m_attack_speed_ms = 0;

        /** (use 1) — item can be activated from the bag. */
        bool m_usable = false;

        /** specialty.teleport.loc → kTeleportLocs index, or -1. */
        int m_teleport_loc = -1;

        /** specialty.refresh amount when stat is hp. */
        int m_refresh_hp = 0;

        /** specialty.refresh amount when stat is mp. */
        int m_refresh_mp = 0;

        /** specialty.refresh.auto_hp restricts use to the Auto-HP slot. */
        bool m_auto_hp = false;

        /** Shared refresh-item cooldown in milliseconds. */
        int m_cooltime_ms = 0;

        /** S2C_EFFECT id played on successful use. */
        int m_effect = 0;

        /** specialty.buff[0] BUFF_TYPE, or -1. */
        int m_buff_type = -1;

        /** specialty.buff[1] seconds or charge count. */
        int m_buff_remain = 0;

        /** specialty.buff[2] amount. */
        int m_buff_amount = 0;

        /** item definition extra.race for transform eggs, or -1. */
        int m_transform_race = -1;

        /** extra.cocoon divisors for MakeTip STR/HTH/DEX/INT/WIS. */
        std::array<int, 5> m_cocoon_ability{};

        /** extra.imageex stems for egg levels 11 / 21 / 31. */
        std::array<std::string, 3> m_image_ex{};

        /** specialty.revival extra-cut percent, or -1. */
        int m_revival_extra_cut = -1;

        /** True when specialty.changeprefix is present. */
        bool m_has_changeprefix = false;

        /** Changeprefix target class (weapon / defense / ornament). */
        std::string m_changeprefix_target;

        /** Cumulative success rate for prefix A. */
        int m_changeprefix_rate0 = 0;

        /** Prefix index for roll A. */
        int m_changeprefix_prefix0 = 0;

        /** Cumulative success rate for prefix B. */
        int m_changeprefix_rate1 = 0;

        /** Prefix index for roll B. */
        int m_changeprefix_prefix1 = 0;

        /** True when specialty.repair is present. */
        bool m_has_repair = false;

        /** Repair JSON first value (0 = add, 1 = Doggebi). */
        int m_repair_mode = 0;

        /** Repair JSON second value. */
        int m_repair_amount = 0;

        /** specialty.charming token, or empty. */
        std::string m_charming;

        /** True when specialty.bof is set. */
        bool m_bof = false;

        /** Derived quest-only acquisition category; ordinary rewards never set this. */
        bool m_task = false;
    };

    /** Catalog of item definitions loaded from JSON. */
    class ItemDb {
      public:
        /** Loads item definitions from JSON.
         * @param configuration_root Root for configuration paths.
         * @param data_root Client data root (HyperText).
         * @param items_dir Optional JSON items directory override.
         * @return True if at least one item was loaded.
         */
        [[nodiscard]] bool load(const std::string& configuration_root,
                                const std::string& data_root,
                                const std::string& items_dir = {});

        /**
         * Looks up an item by item definition index.
         * @param index Item index.
         * @return Pointer to definition, or nullptr.
         */
        [[nodiscard]] const ItemDef* find(std::int32_t index) const;

        /** Returns the source JSON path for a loaded item.
         * @param index Item index.
         * @return Source JSON path, or empty if the item is not loaded.
         */
        [[nodiscard]] std::string source_path(std::int32_t index) const;

        /**
         * Resolves an icon filename relative to HyperText search.
         * Grown eggs (levels 11 / 21 / 31) use extra.imageex when present.
         * @param index Item index.
         * @param egg_level Animal level; ignored for non-eggs.
         * @return Filename such as "item002.bmp", or empty.
         */
        [[nodiscard]] std::string icon_filename(std::int32_t index, int egg_level = 0) const;

        /**
         * Looks up a built-in interface label.
         * @param id Label id used by tooltips and buff names.
         * @return Label text, or a numeric fallback.
         */
        [[nodiscard]] std::string sys(int id) const;

        /** @return HyperText directory used for icon resolution. */
        [[nodiscard]] const std::string& hypertext_dir() const {
            return m_hypertext_dir;
        }

        /** @return All loaded definitions keyed by index. */
        [[nodiscard]] const std::unordered_map<std::int32_t, ItemDef>& by_index() const {
            return m_by_index;
        }

        /** @return Prefix catalog loaded with the items. */
        [[nodiscard]] const PrefixDb& prefixes() const {
            return m_prefixes;
        }

        /** @return Mutable prefix catalog. */
        [[nodiscard]] PrefixDb& prefixes() {
            return m_prefixes;
        }

        /** @return Beads of Fire catalog loaded with the items. */
        [[nodiscard]] const server::BofCatalog& bof() const {
            return m_bof;
        }

      private:
        /** Definitions keyed by item definition index. */
        std::unordered_map<std::int32_t, ItemDef> m_by_index;

        /** JSON source paths keyed by item index. */
        std::unordered_map<std::int32_t, std::string> m_source_paths;

        /** Display root prepended to relative JSON source paths. */
        std::string m_source_path_root;

        /** Resolved HyperText directory. */
        std::string m_hypertext_dir;

        /** Prefix catalog (config/prefixes). */
        PrefixDb m_prefixes;

        /** Beads of Fire catalog (config/bof.json). */
        server::BofCatalog m_bof;
    };

}  // namespace openkal::client

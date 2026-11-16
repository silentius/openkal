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

#include <cstdint>
#include <string>
#include <unordered_map>

namespace openkal::client {

    /** One JSON prefix definition. */
    class PrefixDef {
      public:
        /** @return Prefix index. */
        [[nodiscard]] std::int32_t index() const {
            return m_index;
        }

        /**
         * Sets the prefix index.
         * @param v Prefix index.
         */
        void set_index(std::int32_t v) {
            m_index = v;
        }

        /** @return Display name from prefixname, or empty. */
        [[nodiscard]] const std::string& name() const {
            return m_name;
        }

        /**
         * Sets the display name.
         * @param v Prefix name.
         */
        void set_name(std::string v) {
            m_name = std::move(v);
        }

        /** @return Prefix level requirement added to item level. */
        [[nodiscard]] int level() const {
            return m_level;
        }

        /**
         * Sets the prefix level.
         * @param v Level value.
         */
        void set_level(int v) {
            m_level = v;
        }

        /** @return Sell-price contribution. */
        [[nodiscard]] int sell() const {
            return m_sell;
        }

        /**
         * Sets the sell-price contribution.
         * @param v Sell value.
         */
        void set_sell(int v) {
            m_sell = v;
        }

        /** @return Strength bonus. */
        [[nodiscard]] int str() const {
            return m_str;
        }

        /**
         * Sets the strength bonus.
         * @param v Strength.
         */
        void set_str(int v) {
            m_str = v;
        }

        /** @return Health bonus. */
        [[nodiscard]] int hth() const {
            return m_hth;
        }

        /**
         * Sets the health bonus.
         * @param v Health.
         */
        void set_hth(int v) {
            m_hth = v;
        }

        /** @return Intelligence bonus. */
        [[nodiscard]] int intel() const {
            return m_intel;
        }

        /**
         * Sets the intelligence bonus.
         * @param v Intelligence.
         */
        void set_intel(int v) {
            m_intel = v;
        }

        /** @return Wisdom bonus. */
        [[nodiscard]] int wis() const {
            return m_wis;
        }

        /**
         * Sets the wisdom bonus.
         * @param v Wisdom.
         */
        void set_wis(int v) {
            m_wis = v;
        }

        /** @return Dexterity bonus. */
        [[nodiscard]] int dex() const {
            return m_dex;
        }

        /**
         * Sets the dexterity bonus.
         * @param v Dexterity.
         */
        void set_dex(int v) {
            m_dex = v;
        }

        /** @return Minimum physical attack bonus. */
        [[nodiscard]] int min_attack() const {
            return m_min_attack;
        }

        /**
         * Sets the minimum physical attack bonus.
         * @param v Attack min.
         */
        void set_min_attack(int v) {
            m_min_attack = v;
        }

        /** @return Maximum physical attack bonus. */
        [[nodiscard]] int max_attack() const {
            return m_max_attack;
        }

        /**
         * Sets the maximum physical attack bonus.
         * @param v Attack max.
         */
        void set_max_attack(int v) {
            m_max_attack = v;
        }

        /** @return Minimum magical attack bonus. */
        [[nodiscard]] int min_magic() const {
            return m_min_magic;
        }

        /**
         * Sets the minimum magical attack bonus.
         * @param v Magic min.
         */
        void set_min_magic(int v) {
            m_min_magic = v;
        }

        /** @return Maximum magical attack bonus. */
        [[nodiscard]] int max_magic() const {
            return m_max_magic;
        }

        /**
         * Sets the maximum magical attack bonus.
         * @param v Magic max.
         */
        void set_max_magic(int v) {
            m_max_magic = v;
        }

        /** @return HP bonus. */
        [[nodiscard]] int hp() const {
            return m_hp;
        }

        /**
         * Sets the HP bonus.
         * @param v Hit points.
         */
        void set_hp(int v) {
            m_hp = v;
        }

        /** @return MP bonus. */
        [[nodiscard]] int mp() const {
            return m_mp;
        }

        /**
         * Sets the MP bonus.
         * @param v Mana points.
         */
        void set_mp(int v) {
            m_mp = v;
        }

        /** @return Defense bonus. */
        [[nodiscard]] int defense() const {
            return m_defense;
        }

        /**
         * Sets the defense bonus.
         * @param v Defense.
         */
        void set_defense(int v) {
            m_defense = v;
        }

        /** @return Hit / on-target bonus. */
        [[nodiscard]] int hit() const {
            return m_hit;
        }

        /**
         * Sets the hit bonus.
         * @param v Hit.
         */
        void set_hit(int v) {
            m_hit = v;
        }

        /** @return Fire resist bonus. */
        [[nodiscard]] int resist_fire() const {
            return m_resist_fire;
        }

        /**
         * Sets the fire resist bonus.
         * @param v Fire resist.
         */
        void set_resist_fire(int v) {
            m_resist_fire = v;
        }

        /** @return Ice resist bonus. */
        [[nodiscard]] int resist_ice() const {
            return m_resist_ice;
        }

        /**
         * Sets the ice resist bonus.
         * @param v Ice resist.
         */
        void set_resist_ice(int v) {
            m_resist_ice = v;
        }

        /** @return Lightning resist bonus. */
        [[nodiscard]] int resist_lightning() const {
            return m_resist_lightning;
        }

        /**
         * Sets the lightning resist bonus.
         * @param v Lightning resist.
         */
        void set_resist_lightning(int v) {
            m_resist_lightning = v;
        }

        /** @return Curse resist bonus. */
        [[nodiscard]] int resist_curse() const {
            return m_resist_curse;
        }

        /**
         * Sets the curse resist bonus.
         * @param v Curse resist.
         */
        void set_resist_curse(int v) {
            m_resist_curse = v;
        }

        /** @return Palsy / non-elemental resist bonus. */
        [[nodiscard]] int resist_palsy() const {
            return m_resist_palsy;
        }

        /**
         * Sets the palsy resist bonus.
         * @param v Palsy resist.
         */
        void set_resist_palsy(int v) {
            m_resist_palsy = v;
        }

      private:
        /** Prefix index. */
        std::int32_t m_index = 0;

        /** Display name from prefixname. */
        std::string m_name;

        /** Prefix level. */
        int m_level = 0;

        /** Sell-price contribution. */
        int m_sell = 0;

        /** Strength bonus. */
        int m_str = 0;

        /** Health bonus. */
        int m_hth = 0;

        /** Intelligence bonus. */
        int m_intel = 0;

        /** Wisdom bonus. */
        int m_wis = 0;

        /** Dexterity bonus. */
        int m_dex = 0;

        /** Minimum physical attack bonus. */
        int m_min_attack = 0;

        /** Maximum physical attack bonus. */
        int m_max_attack = 0;

        /** Minimum magical attack bonus. */
        int m_min_magic = 0;

        /** Maximum magical attack bonus. */
        int m_max_magic = 0;

        /** HP bonus. */
        int m_hp = 0;

        /** MP bonus. */
        int m_mp = 0;

        /** Defense bonus. */
        int m_defense = 0;

        /** Hit bonus. */
        int m_hit = 0;

        /** Fire resist bonus. */
        int m_resist_fire = 0;

        /** Ice resist bonus. */
        int m_resist_ice = 0;

        /** Lightning resist bonus. */
        int m_resist_lightning = 0;

        /** Curse resist bonus. */
        int m_resist_curse = 0;

        /** Palsy resist bonus. */
        int m_resist_palsy = 0;
    };

    /** Catalog of prefix definitions loaded from JSON. */
    class PrefixDb {
      public:
        /**
         * Loads prefix JSON files from a directory.
         * @param prefixes_dir Directory containing prefix JSON files.
         * @return True if at least one prefix was loaded.
         */
        [[nodiscard]] bool load(const std::string& prefixes_dir);

        /**
         * Looks up a prefix by index.
         * @param index Prefix index.
         * @return Pointer to definition, or nullptr.
         */
        [[nodiscard]] const PrefixDef* find(std::int32_t index) const;

        /**
         * Looks up a prefix display name.
         * @param index Prefix index.
         * @return Name, or empty when unknown.
         */
        [[nodiscard]] std::string name(std::int32_t index) const;

        /** @return All loaded definitions keyed by index. */
        [[nodiscard]] const std::unordered_map<std::int32_t, PrefixDef>& by_index() const {
            return m_by_index;
        }

      private:
        /** Definitions keyed by prefix index. */
        std::unordered_map<std::int32_t, PrefixDef> m_by_index;
    };

}  // namespace openkal::client

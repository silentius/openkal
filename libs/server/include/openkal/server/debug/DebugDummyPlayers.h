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

#include "openkal/server/SkillBuff.h"

#include <cstdint>
#include <string>
#include <vector>

namespace openkal::server {

    /**
     * Sock-less debug stand-in for a real player (buff target, later party/PvP).
     * Not persisted and not inserted into the live ClientSession list.
     */
    class DummyPlayer {
      public:
        DummyPlayer() = default;

        /** @return Entity id. */
        [[nodiscard]] std::uint32_t player_id() const {
            return m_player_id;
        }

        /**
         * Sets the entity id.
         * @param id Entity id.
         */
        void set_player_id(std::uint32_t id) {
            m_player_id = id;
        }

        /** @return Display name. */
        [[nodiscard]] const std::string& name() const {
            return m_name;
        }

        /**
         * Sets the display name.
         * @param name Display name.
         */
        void set_name(std::string name) {
            m_name = std::move(name);
        }

        /** @return Class index (0 knight / 1 mage / 2 archer / 3 thief). */
        [[nodiscard]] int clazz() const {
            return m_clazz;
        }

        /**
         * Sets the class index.
         * @param clazz Class index.
         */
        void set_clazz(int clazz) {
            m_clazz = clazz;
        }

        /** @return Logical map id. */
        [[nodiscard]] int map() const {
            return m_map;
        }

        /**
         * Sets the logical map id.
         * @param map Map id.
         */
        void set_map(int map) {
            m_map = map;
        }

        /** @return Server pixel X. */
        [[nodiscard]] int px() const {
            return m_px;
        }

        /**
         * Sets server pixel X.
         * @param px Pixel X.
         */
        void set_px(int px) {
            m_px = px;
        }

        /** @return Server pixel Y. */
        [[nodiscard]] int py() const {
            return m_py;
        }

        /**
         * Sets server pixel Y.
         * @param py Pixel Y.
         */
        void set_py(int py) {
            m_py = py;
        }

        /** @return Server Z. */
        [[nodiscard]] int pz() const {
            return m_pz;
        }

        /**
         * Sets server Z.
         * @param pz Height.
         */
        void set_pz(int pz) {
            m_pz = pz;
        }

        /** @return Current HP. */
        [[nodiscard]] std::int32_t cur_hp() const {
            return m_cur_hp;
        }

        /**
         * Sets current HP.
         * @param hp Hit points.
         */
        void set_cur_hp(std::int32_t hp) {
            m_cur_hp = hp;
        }

        /** @return Maximum HP. */
        [[nodiscard]] std::int32_t max_hp() const {
            return m_max_hp;
        }

        /**
         * Sets maximum HP.
         * @param hp Hit points.
         */
        void set_max_hp(std::int32_t hp) {
            m_max_hp = hp;
        }

        /** @return Current MP. */
        [[nodiscard]] std::int32_t cur_mp() const {
            return m_cur_mp;
        }

        /**
         * Sets current MP.
         * @param mp Mana.
         */
        void set_cur_mp(std::int32_t mp) {
            m_cur_mp = mp;
        }

        /** @return Maximum MP. */
        [[nodiscard]] std::int32_t max_mp() const {
            return m_max_mp;
        }

        /**
         * Sets maximum MP.
         * @param mp Mana.
         */
        void set_max_mp(std::int32_t mp) {
            m_max_mp = mp;
        }

        /** @return Character level. */
        [[nodiscard]] std::int32_t level() const {
            return m_level;
        }

        /**
         * Sets character level.
         * @param level Level.
         */
        void set_level(std::int32_t level) {
            m_level = level;
        }

        /** @return Fatal rate used by Archer curse scaling. */
        [[nodiscard]] std::int32_t fatal_rate() const {
            return m_fatal_rate;
        }

        /** @return Mutable fatal rate. */
        [[nodiscard]] std::int32_t& mutable_fatal_rate() {
            return m_fatal_rate;
        }

        /** @return Control lock deadline in monotonic ms. */
        [[nodiscard]] std::uint64_t control_until_ms() const {
            return m_control_until_ms;
        }

        /** @return Mutable control lock deadline. */
        [[nodiscard]] std::uint64_t& mutable_control_until_ms() {
            return m_control_until_ms;
        }

        /** @return Always false; dummies are not god-moded. */
        [[nodiscard]] bool godmode() const {
            return false;
        }

        /** @return Wear item indices (7 slots). */
        [[nodiscard]] const std::uint16_t* wear() const {
            return m_wear;
        }

        /** @return Mutable wear item indices (7 slots). */
        [[nodiscard]] std::uint16_t* mutable_wear() {
            return m_wear;
        }

        /** @return GState bitmask. */
        [[nodiscard]] std::uint32_t gstate() const {
            return m_gstate;
        }

        /** @return Skill buff set. */
        [[nodiscard]] const SkillBuffSet& skill_buffs() const {
            return m_skill_buffs;
        }

        /** @return Mutable skill buff set. */
        [[nodiscard]] SkillBuffSet& skill_buffs() {
            return m_skill_buffs;
        }

        /**
         * Encodes S2C_CreatePlayer for this dummy (no hero 0x80 bit).
         * @return Framed packet.
         */
        [[nodiscard]] std::vector<std::uint8_t> create_player_frame() const;

        /** Restores HP/MP and clears skill buffs. */
        void reset();

        /**
         * Drains Mana Burn / Silence Shot MP once per unix second.
         * @param now_unix Current unix time.
         */
        void drain_archer_mp(std::int64_t now_unix);

      private:
        /** Entity id. */
        std::uint32_t m_player_id = 0;

        /** Display name. */
        std::string m_name;

        /** Class index. */
        int m_clazz = 0;

        /** Logical map id. */
        int m_map = 0;

        /** Server pixel X. */
        int m_px = 0;

        /** Server pixel Y. */
        int m_py = 0;

        /** Server Z. */
        int m_pz = 0;

        /** Current HP. */
        std::int32_t m_cur_hp = 100;

        /** Maximum HP. */
        std::int32_t m_max_hp = 100;

        /** Current MP. */
        std::int32_t m_cur_mp = 50;

        /** Maximum MP. */
        std::int32_t m_max_mp = 50;

        /** Character level. */
        std::int32_t m_level = 1;

        /** Fatal rate (unused on idle dummies). */
        std::int32_t m_fatal_rate = 0;

        /** Knight/Archer control lock deadline. */
        std::uint64_t m_control_until_ms = 0;

        /** Wear item indices. */
        std::uint16_t m_wear[7]{};

        /** GState bitmask. */
        std::uint32_t m_gstate = 0;

        /** Active skill buffs. */
        SkillBuffSet m_skill_buffs;

        /** Last unix second Mana Burn / Silence Shot was drained. */
        std::int64_t m_last_mp_drain_unix = 0;
    };

    /**
     * Owns the four Temporary Fort debug dummies (Knight, Mage, Archer, Thief).
     * Isolated from the live client list; Host only calls spawn / despawn / reset /
     * tick / find.
     */
    class DebugDummyPlayers {
      public:
        DebugDummyPlayers() = default;

        /**
         * Spawns all four class dummies at Temporary Fort if none exist.
         * @param next_entity_id In/out entity-id allocator.
         * @return True when four dummies are present afterwards.
         */
        [[nodiscard]] bool spawn(std::uint32_t& next_entity_id);

        /**
         * Removes every dummy.
         * @return Entity ids that were despawned (for S2C_REMOVEPLAYER).
         */
        std::vector<std::uint32_t> despawn();

        /** Restores HP/MP and clears buffs on every dummy. Does not despawn. */
        void reset();

        /**
         * Expires timed skill buffs.
         * @param now_unix Current unix time.
         */
        void tick(std::int64_t now_unix);

        /**
         * Looks up a dummy by entity id.
         * @param player_id Entity id.
         * @return Dummy pointer, or nullptr.
         */
        [[nodiscard]] DummyPlayer* find(std::uint32_t player_id);

        /**
         * Looks up a dummy by entity id.
         * @param player_id Entity id.
         * @return Dummy pointer, or nullptr.
         */
        [[nodiscard]] const DummyPlayer* find(std::uint32_t player_id) const;

        /** @return Live dummies. */
        [[nodiscard]] const std::vector<DummyPlayer>& all() const {
            return m_players;
        }

        /** @return True when the four dummies are spawned. */
        [[nodiscard]] bool spawned() const {
            return !m_players.empty();
        }

      private:
        /** Spawned debug characters. */
        std::vector<DummyPlayer> m_players;
    };

}  // namespace openkal::server

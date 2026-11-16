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

#include "openkal/protocol/Quest.h"
#include "openkal/server/world/QuestDb.h"

#include <cstdint>
#include <functional>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace openkal::server {

    /** Per-player quest progress. */
    using PlayerQuestMap = std::unordered_map<std::uint16_t, protocol::QuestFlagEntry>;

    /** Side-effect hooks used while executing quest actions. */
    struct QuestActionSink {
        /** Sends S2C_SENDHTML. */
        std::function<void(std::int32_t html_id)> send_html;

        /** Sends S2C_SAVEQUEST after a flag change. */
        std::function<void(const protocol::QuestFlagEntry&)> send_save_quest;

        /** Grants EXP. */
        std::function<void(std::int64_t exp)> add_exp;

        /** Counts owned items of an index. */
        std::function<int(int item_index)> count_item;

        /** Applies all item actions atomically; false leaves the inventory unchanged. */
        std::function<bool(const std::vector<QuestAction>& actions)> exchange_items;

        /** Grants skill points. */
        std::function<void(int points)> add_skill_points;

        /** Grants contribution points. */
        std::function<void(int points)> add_contribution;

        /** Executes the guide action. */
        std::function<void(int guide)> show_guide;

        /** Resets the player's allocated attributes. */
        std::function<void()> redistribute;

        /** Resolves a random item group into a concrete item action. */
        std::function<QuestAction(int group)> roll_items;

        /** Teleports the player. */
        std::function<void(int map, int x, int y)> teleport;

        /** Sets or clears a player gstate bit. */
        std::function<void(int bit_index, bool enabled)> set_gstate;

        /** Receives evaluation failures with the actual linked step key and reason.
         * An empty callback uses the normal warning logger. Read-only evaluators can
         * collect diagnostics without repeatedly logging speculative requests.
         */
        std::function<void(std::uint32_t key, std::string_view reason)> report_diagnostic;

        /** Player class. */
        int clazz = 0;

        /** Player specialty bitmask (bit 0 = wandering). */
        int specialty = 1;

        /** ORs a specialty bit (quest action `specialty N` → `1 << N`). */
        std::function<void(int bit_index)> add_specialty;

        /** Player level. */
        int level = 1;

        /** Player gstate bitmask. */
        std::uint32_t gstate = 0;
    };

    /** Evaluates quest steps against player state. */
    class QuestEngine final {
      public:
        /**
         * Constructs an engine bound to a quest database.
         * @param db Quest definitions.
         */
        explicit QuestEngine(const QuestDb& db);

        /**
         * Processes a quest key like CallProcess (skips linked steps).
         * @param key Quest key.
         * @param quests Mutable player quest map.
         * @param sink Action callbacks.
         * @return True when a step ran.
         */
        [[nodiscard]] bool
        call_process(std::uint32_t key, PlayerQuestMap& quests, QuestActionSink& sink) const;

        /**
         * Processes a quest key without the linked gate (used by link actions).
         * @param key Quest key.
         * @param quests Mutable player quest map.
         * @param sink Action callbacks.
         * @return True when a step ran.
         */
        [[nodiscard]] bool
        process(std::uint32_t key, PlayerQuestMap& quests, QuestActionSink& sink) const;

      private:
        /**
         * Checks whether all conditions of a case pass.
         * @param conditions Case conditions.
         * @param quests Player quests.
         * @param sink Context.
         * @return True when met.
         */
        [[nodiscard]] bool is_meet(const std::vector<QuestCondition>& conditions,
                                   const PlayerQuestMap& quests,
                                   const QuestActionSink& sink) const;

        /**
         * Runs one action.
         * @param action Action definition.
         * @param quests Mutable player quests.
         * @param sink Context.
         */
        void
        run_action(const QuestAction& action, PlayerQuestMap& quests, QuestActionSink& sink) const;

        /**
         * Saves or clears a quest flag and notifies the client.
         * @param quest Quest index.
         * @param flag Flag value.
         * @param clear Clear bit.
         * @param quests Mutable map.
         * @param sink Context.
         */
        void save_flag(std::uint16_t quest,
                       std::uint8_t flag,
                       std::uint8_t clear,
                       PlayerQuestMap& quests,
                       QuestActionSink& sink) const;

        /** Plans a complete linked step before changing inventory or progress.
         * @param key Quest step key.
         * @param quests Simulated progress.
         * @param sink Simulated player context.
         * @param items Simulated item counts.
         * @param plan Flattened actions in execution order.
         * @param depth Current link depth.
         * @return True when all actions can be evaluated.
         */
        [[nodiscard]] bool build_plan(std::uint32_t key,
                                      PlayerQuestMap& quests,
                                      QuestActionSink& sink,
                                      std::unordered_map<int, int>& items,
                                      std::vector<QuestAction>& plan,
                                      unsigned depth) const;

        /** Quest definitions. */
        const QuestDb& m_db;
    };

}  // namespace openkal::server

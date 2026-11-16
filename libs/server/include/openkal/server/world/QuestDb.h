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
#include <unordered_set>
#include <vector>

namespace openkal::server {

    class ItemGroupDb;

    /** One condition inside a quest case. */
    struct QuestCondition {
        /** Condition opcode (class, level, item, quest, clear, notclear, …). */
        std::string op;

        /** Primary integer operand. */
        int a = 0;

        /** Secondary integer operand. */
        int b = 0;

        /** Item requirements as [index, count] pairs. */
        std::vector<std::pair<int, int>> items;
    };

    /** One action inside a quest case. */
    struct QuestAction {
        /** Action opcode (html, link, item, exp, save, clear, teleport, …). */
        std::string op;

        /** Primary integer operand. */
        int a = 0;

        /** Secondary integer operand. */
        int b = 0;

        /** Tertiary integer operand. */
        int c = 0;

        /** Item grants as [index, prefix, count, account_bound?]. */
        std::vector<std::vector<int>> item_in;

        /** Item removals as [index, count]. */
        std::vector<std::vector<int>> item_out;
    };

    /** First-matching case with optional conditions. */
    struct QuestCase {
        /** Conditions that must all pass (empty = always). */
        std::vector<QuestCondition> conditions;

        /** Actions executed when the case matches. */
        std::vector<QuestAction> actions;
    };

    /** One quest step keyed by (index, flag). */
    struct QuestStep {
        /** Quest index. */
        std::uint16_t index = 0;

        /** Step flag. */
        std::uint8_t flag = 0;

        /** True when only reachable via link action. */
        bool linked = false;

        /** Ordered cases. */
        std::vector<QuestCase> cases;
    };

    /** Loads and indexes quest JSON steps. */
    class QuestDb final {
      public:
        /**
         * Loads all JSON quest steps from a directory.
         * @param dir Directory containing quest JSON files.
         * @return True when at least one step loads.
         */
        [[nodiscard]] bool load_dir(const std::string& dir);

        /**
         * Loads per-stage level requirements from task journal JSON files.
         * @param dir Directory containing task JSON files.
         * @return True when at least one positive level requirement loads.
         */
        [[nodiscard]] bool load_task_levels_dir(const std::string& dir);

        /**
         * Finds a quest step by packed key.
         * @param key (index << 16) | flag.
         * @return Step pointer, or nullptr.
         */
        [[nodiscard]] const QuestStep* find(std::uint32_t key) const;

        /**
         * Finds a quest step by index and flag.
         * @param index Quest index.
         * @param flag Step flag.
         * @return Step pointer, or nullptr.
         */
        [[nodiscard]] const QuestStep* find(std::uint16_t index, std::uint8_t flag) const;

        /**
         * Returns loaded step count.
         * @return Number of steps.
         */
        [[nodiscard]] std::size_t size() const {
            return m_steps.size();
        }

        /** Finds direct quest grants that have no configured loot-table source.
         * Callers restrict this set to special quest-class items, excluding ordinary rewards.
         * @param drops Configured loot groups; items obtainable there remain Quest Items.
         * @return Candidate quest-only item definition indices.
         */
        [[nodiscard]] std::unordered_set<int> task_item_indices(const ItemGroupDb& drops) const;

        /**
         * Lowest journal level requirement, or a script-level fallback when no journal data exists.
         * @param quest Quest index.
         * @return Required level, or 0 when the quest has no level gate.
         */
        [[nodiscard]] int required_level(std::uint16_t quest) const;

        /**
         * Returns the journal level requirement for one saved quest stage.
         * @param quest Quest index.
         * @param flag Saved quest flag.
         * @return Required level, or 0 when the task data has no requirement.
         */
        [[nodiscard]] int stage_required_level(std::uint16_t quest, std::uint8_t flag) const;

      private:
        /** Steps keyed by protocol quest key. */
        std::unordered_map<std::uint32_t, QuestStep> m_steps;

        /** Positive journal level requirements keyed by protocol quest key. */
        std::unordered_map<std::uint32_t, int> m_task_levels;
    };

}  // namespace openkal::server

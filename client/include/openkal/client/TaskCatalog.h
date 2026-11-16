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
#include <vector>

namespace openkal::client {

    /** One journal flag step. */
    struct TaskFlagDef {
        /** Flag key matching SaveQuest flag. */
        std::uint8_t key = 0;

        /** Step description. */
        std::string desc;

        /** Required player level for this stage, or 0 when unrestricted. */
        int level = 0;

        /** Required item kind (0 = none, 1 = carry, 2 = collect). */
        int item_kind = 0;

        /** Required item item definition index, or 0. */
        int item_index = 0;

        /** Required item count, or 0. */
        int item_count = 0;

        /** NPC description for the return target. */
        std::string npc_desc;

        /** NPC index for the return target, or 0. */
        int npc_index = 0;

        /** Optional map marker. */
        int x = 0;

        /** Optional map marker. */
        int y = 0;
    };

    /** Quest journal text entry. */
    struct TaskDef {
        /** Quest index. */
        std::uint16_t key = 0;

        /** Quest title. */
        std::string name;

        /** Overall description. */
        std::string desc;

        /** Starter NPC index. */
        int npc_index = 0;

        /** Per-flag descriptions. */
        std::vector<TaskFlagDef> flags;
    };

    /** Loads task journal JSON. */
    class TaskCatalog {
      public:
        /**
         * Loads all tasks from a directory.
         * @param dir config/tasks path.
         * @return True when at least one task loads.
         */
        [[nodiscard]] bool load_dir(const std::string& dir);

        /**
         * Finds a task by quest index.
         * @param key Quest index.
         * @return Task, or nullptr.
         */
        [[nodiscard]] const TaskDef* find(std::uint16_t key) const;

      private:
        /** Tasks keyed by quest index. */
        std::unordered_map<std::uint16_t, TaskDef> m_by_key;
    };

}  // namespace openkal::client

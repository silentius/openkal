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

#include <string>
#include <vector>

namespace openkal::client {
    /** One possible item, independent of its probability or stack size. */
    struct PowerbookDrop {
        int item_index = 0;      /**< Item definition ID, including the shared money ID. */
        bool quest_only = false; /**< Requires an eligible quest when true. */
    };

    /** Authored spawn rectangle in server pixels. */
    struct PowerbookSpawn {
        int map = 0;  /**< Logical map, aligned to named indoor cells. */
        int area = 0; /**< Authored spawn area identifier. */
        int x0 = 0;   /**< Minimum server X. */
        int y0 = 0;   /**< Minimum server Y. */
        int x1 = 0;   /**< Maximum server X. */
        int y1 = 0;   /**< Maximum server Y. */
    };

    /** Item obtainable only at a specific quest stage. */
    struct PowerbookQuestDrop {
        int quest = 0;      /**< Quest ID. */
        int flag = 0;       /**< Required current quest flag. */
        int item_index = 0; /**< Dropped item ID. */
    };

    /** Read-only monster entry; never includes drop chances. */
    struct PowerbookMonster {
        int index = 0;                               /**< Monster definition ID. */
        int level = 1;                               /**< Authored monster level. */
        std::string name;                            /**< Display name. */
        std::vector<PowerbookDrop> drops;            /**< Distinct possible item IDs. */
        std::vector<PowerbookSpawn> spawns;          /**< All enabled authored spawn areas. */
        std::vector<PowerbookQuestDrop> quest_drops; /**< Stage-specific quest sources. */
        bool drops_complete = true; /**< False if a referenced loot table is missing. */
    };

    /** Read-only view of optional runtime catalogs, without spawning game entities. */
    class PowerbookCatalog final {
      public:
        /** Replaces the catalog using the same configuration root as the client.
         * @param configuration_root Runtime JSON configuration root.
         * @return True when at least one monster was loaded. */
        [[nodiscard]] bool load(const std::string& configuration_root);

        /** @return All loaded monsters sorted by level, name, then definition ID. */
        const std::vector<PowerbookMonster>& monsters() const;

      private:
        std::vector<PowerbookMonster> m_monsters;
    };
}  // namespace openkal::client

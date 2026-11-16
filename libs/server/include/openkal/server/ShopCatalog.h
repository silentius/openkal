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

#include "openkal/server/world/GameWorld.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace openkal::server {

    /** Server-authoritative inventory for one NPC shop. */
    struct ShopDefinition {
        /** NPC definition index. */
        int npc_index = 0;
        /** Source store number retained for migration diagnostics. */
        int legacy_store_id = 0;
        /** Currency item, or zero for ordinary gold shops. */
        int currency_item = 0;
        /** Whether this service offers class-filtered cosmetic skins. */
        bool skin_catalog = false;
        /** Per-item ticket prices for the derived cosmetic catalog. */
        std::unordered_map<int, int> prices;
        /** item definition indices offered by the shop, in display order. */
        std::vector<int> items;
    };

    /** Loads and validates openkal.shop/v1 files. */
    class ShopCatalog {
      public:
        /**
         * Loads every JSON shop definition in a directory.
         * @param dir Shop configuration directory.
         * @param item_defs Loaded authoritative item definitions.
         * @return True when all discovered definitions are valid and at least one was loaded.
         */
        [[nodiscard]] bool load_dir(const std::string& dir,
                                    const std::unordered_map<int, ItemCombatDef>& item_defs);

        /**
         * Finds a shop by NPC definition index.
         * @param npc_index NPC definition index.
         * @return Definition pointer, or nullptr.
         */
        [[nodiscard]] const ShopDefinition* find(int npc_index) const;

        /** @return Number of loaded NPC shops. */
        [[nodiscard]] std::size_t size() const {
            return m_shops.size();
        }

      private:
        /** Shop definitions keyed by NPC definition index. */
        std::unordered_map<int, ShopDefinition> m_shops;
    };

}  // namespace openkal::server

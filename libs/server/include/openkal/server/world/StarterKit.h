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

#include "openkal/server/store/DataStore.h"
#include "openkal/server/world/GameWorld.h"

#include <array>
#include <atomic>
#include <string>
#include <unordered_map>
#include <vector>

namespace openkal::server {

    /** One starter-equipment row loaded from per-class JSON. */
    struct StarterItem {
        /** item definition index. */
        int item_id = 0;

        /** Prefix / upgrade byte. */
        int prefix = 0;

        /** Stack count. */
        int count = 1;

        /** True when the item is worn on first grant (weapon / armor / transform). */
        bool equipped = false;
    };

    /** Loads and grants per-class starter equipment from config/starters. */
    class StarterKit final {
      public:
        /**
         * Loads `common.json` plus `knight.json`, `mage.json`, `archer.json`, and
         * `thief.json`. Common rows are prepended to each class table. Spawn from
         * `common.json` is stored for new characters.
         * @param dir Directory containing starter JSON files.
         * @return True when at least one table loads.
         */
        [[nodiscard]] bool load_dir(const std::string& dir);

        /**
         * Returns starter rows for a class.
         * @param clazz Class index (0 knight / 1 mage / 2 archer / 3 thief).
         * @return Loaded rows, or empty when the class has no table.
         */
        [[nodiscard]] const std::vector<StarterItem>& items_for_class(int clazz) const;

        /**
         * Reports whether `common.json` contained a spawn position.
         * @return True when spawn was parsed.
         */
        [[nodiscard]] bool has_spawn() const {
            return m_has_spawn;
        }

        /**
         * Returns new-character spawn X in server pixels.
         * @return Spawn X.
         */
        [[nodiscard]] int spawn_x() const {
            return m_spawn_x;
        }

        /**
         * Returns new-character spawn Y in server pixels.
         * @return Spawn Y.
         */
        [[nodiscard]] int spawn_y() const {
            return m_spawn_y;
        }

        /**
         * Returns new-character spawn Z in server pixels.
         * @return Spawn Z.
         */
        [[nodiscard]] int spawn_z() const {
            return m_spawn_z;
        }

        /**
         * Writes the loaded spawn into a character record.
         * @param character Character to update.
         */
        void apply_spawn(CharacterRecord& character) const;

        /** Atomically grants character-bound starter items, money and spawn once per character.
         * An empty inventory alone does not make a previously granted character eligible.
         * @param store Persistence backend.
         * @param character Character to update (id, clazz, next_iid, wear, money, pose).
         * @param item_defs Combat item table used to pick equipped slots.
         * @param next_iid Optional process-wide iid seed (Host). Null snapshots MAX once.
         * @return True only when the complete grant was written for the first time.
         */
        [[nodiscard]] bool grant_if_empty(IDataStore& store,
                                          CharacterRecord& character,
                                          const std::unordered_map<int, ItemCombatDef>& item_defs,
                                          std::atomic<std::uint32_t>* next_iid = nullptr) const;

      private:
        /** Starter rows indexed by class (0..3). */
        std::array<std::vector<StarterItem>, 4> m_items;

        /** True when common.json contained a spawn position. */
        bool m_has_spawn = false;

        /** New-character spawn X in server pixels. */
        int m_spawn_x = 257285;

        /** New-character spawn Y in server pixels. */
        int m_spawn_y = 258744;

        /** New-character spawn Z in server pixels. */
        int m_spawn_z = 16100;
    };

}  // namespace openkal::server

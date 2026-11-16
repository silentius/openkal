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

#include "openkal/client/MonsterCatalog.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace openkal::client {

    /**
     * Particle attaches for world GBs (`object.dat` → config/world_objects.json).
     *
     * XModelTable::Init stores `(particle name bone ox oy oz …)` per model.
     * XBaseModel::Create copies that list; related compatibility states
     * then `matrix * offset` and compatibility behavior (autoRemove false)
     * while the object is drawn.
     */
    class WorldObjectCatalog {
      public:
        /** High bits for world-prop SpriteFx entity keys (NPC uses 0x80000000). */
        static constexpr std::uint32_t kFxBase = 0xE0000000u;

        /**
         * Loads particle rows from a JSON object with a `models` array.
         * @param path Absolute path of world_objects.json.
         * @return True when at least one model with particles was indexed.
         */
        [[nodiscard]] bool load_json(const std::string& path);

        /**
         * Looks up particles for a GB path (indoor `MAP\…` or OPL `Data\OBJECTS\…`).
         * Matching is case-insensitive; `/` and `\` are equivalent; a trailing `.gb`
         * is ignored; a missing `MAP\` prefix is tried for indoor-relative paths.
         * @param model_path Table key or map/OPL path.
         * @return Particle list, or nullptr when the model has no table row.
         */
        [[nodiscard]] const std::vector<MonsterFxAttach>* find(std::string_view model_path) const;

        /**
         * Builds the Create-name used by MMap::Load (`MAP\\` + relative).
         * @param map_relative_path Indoor MAP_OBJECT path (often without `.gb`).
         * @return Lookup key with a MAP\ prefix when the path is not already rooted.
         */
        [[nodiscard]] static std::string indoor_lookup_key(std::string_view map_relative_path);

        /**
         * Allocates a SpriteFx entity id in the world-prop range.
         * @param serial In/out counter; advanced by one. Zero is treated as one.
         * @return Entity key (`kFxBase | serial`).
         */
        [[nodiscard]] static std::uint32_t take_fx_entity(std::uint32_t& serial);

        /** @return Number of models that have at least one particle. */
        [[nodiscard]] std::size_t size() const {
            return m_entries.size();
        }

      private:
        /** One object.dat / JSON model with its particle list. */
        class Entry {
          public:
            /** Particle attaches (name, EFFECTBONE, local offset). */
            std::vector<MonsterFxAttach> particles;
        };

        /**
         * Inserts lookup aliases for a stored model path.
         * @param path JSON `path` string.
         * @param index Index into runtime state.
         */
        void index_path(std::string_view path, std::size_t index);

        /** Particle rows keyed by insertion order. */
        std::vector<Entry> m_entries;

        /** Lowercased, slash-normalized path → runtime state index. */
        std::unordered_map<std::string, std::size_t> m_index;
    };

}  // namespace openkal::client

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
#include <string_view>
#include <vector>

namespace openkal::map {

    /** One `(minimap …)` record from the compatible client macro (compatibility behavior). */
    class MiniMapSize {
      public:
        /** Constructs an empty record. */
        MiniMapSize() = default;

        /**
         * Typical dungeon atlas (512×512, 256 px tiles, no crop).
         * @param mapname map definition stem used in minimap_Dungeon_<stem>_NNN.GTX.
         * @return Default dungeon size record.
         */
        [[nodiscard]] static MiniMapSize dungeon_defaults(std::string mapname);

        /** @return map definition stem (`tomb`, `tomb_01`). */
        [[nodiscard]] const std::string& mapname() const {
            return m_mapname;
        }

        /** @param value map definition stem. */
        void set_mapname(std::string value) {
            m_mapname = std::move(value);
        }

        /** @return Atlas width in pixels. */
        [[nodiscard]] int size_x() const {
            return m_size_x;
        }

        /** @param value Atlas width in pixels. */
        void set_size_x(int value) {
            m_size_x = value;
        }

        /** @return Atlas height in pixels. */
        [[nodiscard]] int size_y() const {
            return m_size_y;
        }

        /** @param value Atlas height in pixels. */
        void set_size_y(int value) {
            m_size_y = value;
        }

        /** @return Tile width in pixels. */
        [[nodiscard]] int tile_width() const {
            return m_tile_width;
        }

        /** @param value Tile width in pixels. */
        void set_tile_width(int value) {
            m_tile_width = value;
        }

        /** @return Tile height in pixels. */
        [[nodiscard]] int tile_height() const {
            return m_tile_height;
        }

        /** @param value Tile height in pixels. */
        void set_tile_height(int value) {
            m_tile_height = value;
        }

        /** @return West crop in MAP_SIZE cells (runtime state). */
        [[nodiscard]] int map_left() const {
            return m_map_left;
        }

        /** @param value West crop in MAP_SIZE cells. */
        void set_map_left(int value) {
            m_map_left = value;
        }

        /** @return South crop in MAP_SIZE cells (runtime state). */
        [[nodiscard]] int map_bottom() const {
            return m_map_bottom;
        }

        /** @param value South crop in MAP_SIZE cells. */
        void set_map_bottom(int value) {
            m_map_bottom = value;
        }

        /** @return Extra atlas offset in pixels (runtime state). */
        [[nodiscard]] int correction() const {
            return m_correction;
        }

        /** @param value Extra atlas offset in pixels. */
        void set_correction(int value) {
            m_correction = value;
        }

      private:
        /** map definition stem used as the dungeon GTX filename infix. */
        std::string m_mapname;

        /** Atlas width in pixels. */
        int m_size_x = 512;

        /** Atlas height in pixels. */
        int m_size_y = 512;

        /** Tile width in pixels. */
        int m_tile_width = 256;

        /** Tile height in pixels. */
        int m_tile_height = 256;

        /** West crop in MAP_SIZE cells. */
        int m_map_left = 0;

        /** South crop in MAP_SIZE cells. */
        int m_map_bottom = 0;

        /** Extra atlas offset in pixels. */
        int m_correction = 0;
    };

    /** Loads dungeon MiniMapSize records converted from macro-e `(minimap …)`. */
    class MiniMapSizeCatalog {
      public:
        /**
         * Replaces entries from a JSON file.
         * @param path Path of dungeon_minimaps.json.
         * @return True when at least one named dungeon record was parsed.
         */
        [[nodiscard]] bool load_json(std::string_view path);

        /**
         * Replaces entries from a JSON document.
         * @param text JSON text.
         * @return True when at least one named dungeon record was parsed.
         */
        [[nodiscard]] bool load_json_text(std::string_view text);

        /** Clears all parsed records. */
        void clear();

        /**
         * Looks up a dungeon atlas by map definition stem.
         * @param mapname Stem (`tomb_01`), not `Map\\tomb_01`.
         * @return Matching record, or nullptr.
         */
        [[nodiscard]] const MiniMapSize* find(std::string_view mapname) const;

        /** @return Parsed records. */
        [[nodiscard]] const std::vector<MiniMapSize>& entries() const {
            return m_entries;
        }

      private:
        /** Parsed dungeon size records. */
        std::vector<MiniMapSize> m_entries;
    };

}  // namespace openkal::map

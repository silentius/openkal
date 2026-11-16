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

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace openkal::map {

    /** One map definition cell (index, kind, xy, fog, size, initPos, mapname or filename). */
    class MapInfo {
      public:
        /** Constructs an empty map definition record. */
        MapInfo() = default;

        /** Returns the map definition index field.
         * @return map definition index.
         */
        [[nodiscard]] int index() const {
            return m_index;
        }

        /** Sets the map definition index field.
         * @param value map definition index.
         */
        void set_index(int value) {
            m_index = value;
        }

        /** Returns the map kind (outdoor field maps use 1).
         * @return Map kind.
         */
        [[nodiscard]] int kind() const {
            return m_kind;
        }

        /** Sets the map kind.
         * @param value Map kind.
         */
        void set_kind(int value) {
            m_kind = value;
        }

        /** Returns the cell X coordinate from the optional `xy` pair.
         * @return Cell X.
         */
        [[nodiscard]] int x() const {
            return m_x;
        }

        /** Sets the cell X.
         * @param value Cell X.
         */
        void set_x(int value) {
            m_x = value;
        }

        /** Returns the cell Y coordinate from the optional `xy` pair.
         * @return Cell Y.
         */
        [[nodiscard]] int y() const {
            return m_y;
        }

        /** Sets the cell Y.
         * @param value Cell Y.
         */
        void set_y(int value) {
            m_y = value;
        }

        /** Returns the cell size X from (size SX SY).
         * @return Size X (2 for indoor 2×2 blocks, 0 for outdoor area cells).
         */
        [[nodiscard]] int size_x() const {
            return m_size_x;
        }

        /** Sets the cell size X.
         * @param value Size X.
         */
        void set_size_x(int value) {
            m_size_x = value;
        }

        /** Returns the cell size Y from (size SX SY).
         * @return Size Y.
         */
        [[nodiscard]] int size_y() const {
            return m_size_y;
        }

        /** Sets the cell size Y.
         * @param value Size Y.
         */
        void set_size_y(int value) {
            m_size_y = value;
        }

        /** Returns the fog end distance (fogEnd).
         * @return Fog end.
         */
        [[nodiscard]] int fog_end() const {
            return m_fog_end;
        }

        /** Sets the fog end distance.
         * @param value Fog end.
         */
        void set_fog_end(int value) {
            m_fog_end = value;
        }

        /** Returns the fog red component (0–1).
         * @return Fog red.
         */
        [[nodiscard]] float fog_r() const {
            return m_fog_r;
        }

        /** Sets the fog red component.
         * @param value Fog red.
         */
        void set_fog_r(float value) {
            m_fog_r = value;
        }

        /** Returns the fog green component (0–1).
         * @return Fog green.
         */
        [[nodiscard]] float fog_g() const {
            return m_fog_g;
        }

        /** Sets the fog green component.
         * @param value Fog green.
         */
        void set_fog_g(float value) {
            m_fog_g = value;
        }

        /** Returns the fog blue component (0–1).
         * @return Fog blue.
         */
        [[nodiscard]] float fog_b() const {
            return m_fog_b;
        }

        /** Sets the fog blue component.
         * @param value Fog blue.
         */
        void set_fog_b(float value) {
            m_fog_b = value;
        }

        /** Returns the init-position X from (initpos X Y).
         * @return Init-position X in server pixels.
         */
        [[nodiscard]] int init_pos_x() const {
            return m_init_pos_x;
        }

        /** Sets the init-position X.
         * @param value Init-position X in server pixels.
         */
        void set_init_pos_x(int value) {
            m_init_pos_x = value;
        }

        /** Returns the init-position Y from (initpos X Y).
         * @return Init-position Y in server pixels.
         */
        [[nodiscard]] int init_pos_y() const {
            return m_init_pos_y;
        }

        /** Sets the init-position Y.
         * @param value Init-position Y in server pixels.
         */
        void set_init_pos_y(int value) {
            m_init_pos_y = value;
        }

        /** Returns the optional filename field.
         * @return Filename string (may be empty).
         */
        [[nodiscard]] const std::string& filename() const {
            return m_filename;
        }

        /** Sets the optional filename field.
         * @param value Filename string.
         */
        void set_filename(std::string value) {
            m_filename = std::move(value);
        }

        /** Returns the indoor mapname field (`Map\\tomb`).
         * @return Mapname string (may be empty for outdoor cells).
         */
        [[nodiscard]] const std::string& mapname() const {
            return m_mapname;
        }

        /** Sets the indoor mapname field.
         * @param value Mapname string.
         */
        void set_mapname(std::string value) {
            m_mapname = std::move(value);
        }

      private:
        /** map definition index. */
        int m_index = 0;

        /** map definition kind. */
        int m_kind = 0;

        /** Cell X used to build n_%03d_%03darea-map. */
        int m_x = 0;

        /** Cell Y used to build n_%03d_%03darea-map. */
        int m_y = 0;

        /** Cell size X from (size SX SY). */
        int m_size_x = 0;

        /** Cell size Y from (size SX SY). */
        int m_size_y = 0;

        /** Fog end distance from (fog END R G B). */
        int m_fog_end = 0;

        /** Fog red from (fog END R G B). */
        float m_fog_r = 0.0f;

        /** Fog green from (fog END R G B). */
        float m_fog_g = 0.0f;

        /** Fog blue from (fog END R G B). */
        float m_fog_b = 0.0f;

        /** Init-position X from (initpos X Y). */
        int m_init_pos_x = 0;

        /** Init-position Y from (initpos X Y). */
        int m_init_pos_y = 0;

        /** Optional filename from the JSON record. */
        std::string m_filename;

        /** Indoor visual mapname (`Map\\tomb`) from mapinfo.json. */
        std::string m_mapname;
    };

    /** Parses map definition JSON (`config/mapinfo.json` or `config/mapinfo.json`). */
    class MapCatalog {
      public:
        /** Loads map definition records from a JSON path.
         * @param path Path to mapinfo.json or mapinfo.json.
         * @return True if at least one entry was parsed.
         */
        [[nodiscard]] bool load(std::string_view path);

        /**
         * Loads map definition JSON (`config/mapinfo.json` or `config/mapinfo.json`).
         * @param path JSON file path.
         * @return True when at least one cell loaded.
         */
        [[nodiscard]] bool load_json(std::string_view path);

        /**
         * Parses map definition JSON text.
         * @param text JSON document (`{ "maps": [...] }` or a raw array).
         * @return True when at least one cell parsed.
         */
        [[nodiscard]] bool load_json_text(std::string_view text);

        /** Clears all parsed entries. */
        void clear();

        /** Returns the parsed map definition records.
         * @return Const reference to the entry list.
         */
        [[nodiscard]] const std::vector<MapInfo>& entries() const {
            return m_entries;
        }

        /** True if no entries are loaded.
         * @return True when the catalog is empty.
         */
        [[nodiscard]] bool empty() const {
            return m_entries.empty();
        }

        /** Number of parsed entries.
         * @return Entry count.
         */
        [[nodiscard]] std::size_t size() const {
            return m_entries.size();
        }

        /**
         * Finds the map definition cell for a logical map index and world cell xy.
         * @param index Logical map index (1–4 for D1–D4).
         * @param cell_x World cell X (server pixel / 8192).
         * @param cell_y World cell Y.
         * @return Matching record, or nullptr.
         */
        [[nodiscard]] const MapInfo* find_cell(int index, int cell_x, int cell_y) const;

        /**
         * First record with this map definition index (fallback when the cell is missing).
         * @param index Logical map index.
         * @return Matching record, or nullptr.
         */
        [[nodiscard]] const MapInfo* find_index(int index) const;

        /**
         * Indoor visual cell: first record at `cell_x,cell_y` with a nonempty mapname.
         * Outdoor map definition index 0 occupies the same xy grid but has no mapname.
         * @param cell_x World cell X.
         * @param cell_y World cell Y.
         * @return Matching indoor record, or nullptr.
         */
        [[nodiscard]] const MapInfo* find_named_cell(int cell_x, int cell_y) const;

        /**
         * Logical map index for a server-pixel pose (0 = overworld).
         * @param pixel_x Server pixel X.
         * @param pixel_y Server pixel Y.
         * @return map definition index of the named indoor cell, or 0.
         */
        [[nodiscard]] int index_at_pixel(int pixel_x, int pixel_y) const;

        /**
         * Returns the map definition index for the physical cell at a server-pixel pose.
         * Unlike index_at_pixel(), this includes outdoor cells that have no mapname.
         * @param pixel_x Server pixel X.
         * @param pixel_y Server pixel Y.
         * @return Cell index when an map definition cell exists, otherwise std::nullopt.
         */
        [[nodiscard]] std::optional<int> cell_index_at_pixel(int pixel_x, int pixel_y) const;

        /**
         * Named indoor map definition index at the pixel when present, otherwise `fallback`.
         * Used for spawn-map alignment helpers; `/teleport area` keeps the GenMonster map
         * and refuses destinations without a walkable area cell.
         * @param pixel_x Server pixel X.
         * @param pixel_y Server pixel Y.
         * @param fallback GenMonster / JSON map when the cell is outdoor.
         * @return Named map definition index, or fallback.
         */
        [[nodiscard]] int resolve_teleport_map(int pixel_x, int pixel_y, int fallback) const;

        /**
         * True when this map definition index has a nonempty indoor mapname (dungeon .map).
         * Instance maps such as Emok (index 20) reuse outdoor KCM cells and have no mapname.
         * @param index Logical map index (0 = overworld).
         * @return True if indoor visuals should load.
         */
        [[nodiscard]] bool has_indoor_mapname(int index) const;

      private:
        /** Parsed map definition records. */
        std::vector<MapInfo> m_entries;
    };

    /** Thin free-function forward to MapCatalog::load.
     * @param path Path to mapinfo.json or mapinfo.json.
     * @param out Destination entry list (replaced on success).
     * @return True if at least one entry was parsed.
     */
    [[nodiscard]] bool parse_map_catalog(const std::string& path, std::vector<MapInfo>& out);

}  // namespace openkal::map

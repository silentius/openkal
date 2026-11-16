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

    /** Chat usage for `/teleport d1|d2|d3|d4|d5|d6|lord|emok|vod` and `/teleport area <id>`. */
    inline constexpr const char* kTeleportUsage =
        "Usage: /teleport d1|d2|d3|d4|d5|d6|lord|emok|vod | /teleport area <id>";

    /** Max rtFrom area (tiles) that may fire without TILE_PORTAL. Huge D4
     * lookup rects stay gated on the area-map bit so spawn-in-rect does not bounce.
     */
    inline constexpr int kMaxUngatedPortalArea = 64;

    /** Whether a portal sends the player into a dungeon or back to the overworld. */
    enum class PortalKind {
        /** Overworld → dungeon. */
        Enter = 0,

        /** Dungeon → overworld. */
        Exit = 1,
    };

    /** One compatibility behavior record: tile rect on fromMap, pixel dest on toMap. */
    class PortalRecord {
      public:
        /** Constructs an empty portal. */
        PortalRecord() = default;

        /** @return Stable id (e.g. d1_enter). */
        [[nodiscard]] const std::string& id() const {
            return m_id;
        }

        /** @param value Stable id. */
        void set_id(std::string value) {
            m_id = std::move(value);
        }

        /** @return Dungeon index 1–6, or 0 when unset. */
        [[nodiscard]] int dungeon() const {
            return m_dungeon;
        }

        /** @param value Dungeon index 1–6. */
        void set_dungeon(int value) {
            m_dungeon = value;
        }

        /** @return Enter or exit. */
        [[nodiscard]] PortalKind kind() const {
            return m_kind;
        }

        /** @param value Enter or exit. */
        void set_kind(PortalKind value) {
            m_kind = value;
        }

        /** @return Source logical map (0 = overworld). */
        [[nodiscard]] int from_map() const {
            return m_from_map;
        }

        /** @param value Source logical map. */
        void set_from_map(int value) {
            m_from_map = value;
        }

        /** @return Whether entering the source rect triggers this portal automatically. */
        [[nodiscard]] bool field_trigger_enabled() const {
            return m_field_trigger_enabled;
        }

        /** @param value Whether entering the source rect triggers this portal automatically. */
        void set_field_trigger_enabled(bool value) {
            m_field_trigger_enabled = value;
        }

        /** @return Inclusive-left tile of rtFrom (Windows RECT). */
        [[nodiscard]] int left() const {
            return m_left;
        }

        /** @param value Left tile. */
        void set_left(int value) {
            m_left = value;
        }

        /** @return Inclusive-top tile of rtFrom. */
        [[nodiscard]] int top() const {
            return m_top;
        }

        /** @param value Top tile. */
        void set_top(int value) {
            m_top = value;
        }

        /** @return Exclusive-right tile of rtFrom. */
        [[nodiscard]] int right() const {
            return m_right;
        }

        /** @param value Right tile (exclusive). */
        void set_right(int value) {
            m_right = value;
        }

        /** @return Exclusive-bottom tile of rtFrom. */
        [[nodiscard]] int bottom() const {
            return m_bottom;
        }

        /** @param value Bottom tile (exclusive). */
        void set_bottom(int value) {
            m_bottom = value;
        }

        /** @return Destination logical map. */
        [[nodiscard]] int to_map() const {
            return m_to_map;
        }

        /** @param value Destination logical map. */
        void set_to_map(int value) {
            m_to_map = value;
        }

        /** @return Destination server pixel X. */
        [[nodiscard]] int to_x() const {
            return m_to_x;
        }

        /** @param value Destination pixel X. */
        void set_to_x(int value) {
            m_to_x = value;
        }

        /** @return Destination server pixel Y. */
        [[nodiscard]] int to_y() const {
            return m_to_y;
        }

        /** @param value Destination pixel Y. */
        void set_to_y(int value) {
            m_to_y = value;
        }

        /** @return Destination server Z. */
        [[nodiscard]] int to_z() const {
            return m_to_z;
        }

        /** @param value Destination Z. */
        void set_to_z(int value) {
            m_to_z = value;
        }

        /** @return `/teleport` stand point pixel X (map 0, in front of the gate). */
        [[nodiscard]] int entrance_x() const {
            return m_entrance_x;
        }

        /** @param value Entrance pixel X. */
        void set_entrance_x(int value) {
            m_entrance_x = value;
            m_has_entrance = true;
        }

        /** @return `/teleport` stand point pixel Y. */
        [[nodiscard]] int entrance_y() const {
            return m_entrance_y;
        }

        /** @param value Entrance pixel Y. */
        void set_entrance_y(int value) {
            m_entrance_y = value;
            m_has_entrance = true;
        }

        /** @return `/teleport` stand point Z. */
        [[nodiscard]] int entrance_z() const {
            return m_entrance_z;
        }

        /** @param value Entrance Z. */
        void set_entrance_z(int value) {
            m_entrance_z = value;
            m_has_entrance = true;
        }

        /** @return True when JSON supplied an explicit entrance. */
        [[nodiscard]] bool has_entrance() const {
            return m_has_entrance;
        }

        /**
         * Windows PtInRect on rtFrom: left <= tx < right, top <= ty < bottom.
         * @param tile_x World tile X.
         * @param tile_y World tile Y.
         * @return True when the tile is inside the portal rect.
         */
        [[nodiscard]] bool contains_tile(int tile_x, int tile_y) const;

        /**
         * Pixel centre of rtFrom (for portal visuals).
         * @param pixel_x Receives server pixel X.
         * @param pixel_y Receives server pixel Y.
         */
        void center_pixel(int& pixel_x, int& pixel_y) const;

        /** Fills entrance from the from-rect when it was omitted. */
        void derive_entrance_if_missing();

      private:
        /** Stable id. */
        std::string m_id;

        /** Dungeon 1–6, or 0 for overworld places such as Emok Island. */
        int m_dungeon = 0;

        /** Enter or exit. */
        PortalKind m_kind = PortalKind::Enter;

        /** Source map. */
        int m_from_map = 0;

        /** Whether movement into rtFrom activates this portal. */
        bool m_field_trigger_enabled = true;

        /** rtFrom.left (tiles). */
        int m_left = 0;

        /** rtFrom.top (tiles). */
        int m_top = 0;

        /** rtFrom.right (tiles, exclusive). */
        int m_right = 0;

        /** rtFrom.bottom (tiles, exclusive). */
        int m_bottom = 0;

        /** Destination map. */
        int m_to_map = 0;

        /** Destination pixel X. */
        int m_to_x = 0;

        /** Destination pixel Y. */
        int m_to_y = 0;

        /** Destination Z. */
        int m_to_z = 0;

        /** Stand-in-front pixel X. */
        int m_entrance_x = 0;

        /** Stand-in-front pixel Y. */
        int m_entrance_y = 0;

        /** Stand-in-front Z. */
        int m_entrance_z = 16384;

        /** True when entrance was set explicitly. */
        bool m_has_entrance = false;
    };

    /** Loads compatibility behavior records from JSON. */
    class PortalCatalog {
      public:
        /** Removes all records. */
        void clear();

        /**
         * Loads `config/portals.json`.
         * @param path JSON file path.
         * @return True when at least one portal loaded.
         */
        [[nodiscard]] bool load_json(std::string_view path);

        /**
         * Parses portal JSON text.
         * @param text JSON document (`{ "portals": [...] }` or a raw array).
         * @return True when at least one portal parsed.
         */
        [[nodiscard]] bool load_json_text(std::string_view text);

        /** @return Loaded portals. */
        [[nodiscard]] const std::vector<PortalRecord>& portals() const {
            return m_portals;
        }

        /**
         * Finds the portal whose from-rect contains the tile on `from_map`.
         * @param from_map Player logical map.
         * @param tile_x World tile X.
         * @param tile_y World tile Y.
         * @return Matching portal, or nullptr.
         */
        [[nodiscard]] const PortalRecord* find(int from_map, int tile_x, int tile_y) const;

        /**
         * Finds a loaded portal by stable id.
         * @param id Record id (e.g. emok_enter).
         * @return Matching portal, or nullptr.
         */
        [[nodiscard]] const PortalRecord* find_by_id(std::string_view id) const;

        /**
         * Finds the enter portal for dungeon 1–6 (`/teleport dN` stand point).
         * @param dungeon Dungeon index 1–6.
         * @return Enter record, or nullptr.
         */
        [[nodiscard]] const PortalRecord* entrance_for_dungeon(int dungeon) const;

        /**
         * Parses `d1`–`d6` (case-insensitive).
         * @param args Chat arguments after `/teleport`.
         * @param dungeon Receives 1–6 on success.
         * @return True when the argument is a valid dungeon token.
         */
        [[nodiscard]] static bool parse_dungeon_arg(std::string_view args, int& dungeon);

        /**
         * Parses `/teleport` place tokens such as `lord`, `emok`, or `vod` (case-insensitive).
         * @param args Chat arguments after `/teleport`.
         * @param id Receives the portal id on success.
         * @return True when the argument is a known teleport destination.
         */
        [[nodiscard]] static bool parse_place_arg(std::string_view args, std::string& id);

        /**
         * Chat reply for a successful `/teleport` place.
         * @param id Portal id from parse_place_arg.
         * @return Reply text.
         */
        [[nodiscard]] static const char* place_teleport_reply(std::string_view id);

        /**
         * Parses `area <id>` (case-insensitive keyword, positive integer).
         * @param args Chat arguments after `/teleport`.
         * @param area Receives the GenMonster area id on success.
         * @return True when the argument is a valid area token.
         */
        [[nodiscard]] static bool parse_area_arg(std::string_view args, int& area);

      private:
        /** Loaded portal records. */
        std::vector<PortalRecord> m_portals;
    };

}  // namespace openkal::map

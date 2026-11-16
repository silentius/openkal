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

#include "openkal/client/FreetypeFont.h"
#include "openkal/client/UiLayout.h"
#include "openkal/client/WidgetTools.h"
#include "openkal/map/Constants.h"
#include "openkal/map/MiniMapSize.h"
#include "openkal/map/WorldGrid.h"

#include <algorithm>
#include <string>
#include <string_view>

namespace openkal::client {

    /** Per-frame snapshot used to pan the minimap onto the local player. */
    class MinimapWidgetData {
      public:
        /** @return Player world X. */
        [[nodiscard]] float world_x() const {
            return m_world_x;
        }

        /** @return Player world Z. */
        [[nodiscard]] float world_z() const {
            return m_world_z;
        }

        /** @return Camera yaw in radians (look direction on the XZ plane). */
        [[nodiscard]] float camera_yaw_rad() const {
            return m_camera_yaw_rad;
        }

        /** @return Player yaw in radians (character facing on the XZ plane). */
        [[nodiscard]] float player_yaw_rad() const {
            return m_player_yaw_rad;
        }

        /** @return Discrete atlas zoom (1, 4, or 8). */
        [[nodiscard]] int zoom() const {
            return m_zoom;
        }

        /**
         * Sets the player world position shown at the minimap centre.
         * @param x World X.
         * @param z World Z.
         */
        void set_world_pos(float x, float z) {
            m_world_x = x;
            m_world_z = z;
        }

        /** Sets the coordinate origin for the current profile.
         * @param origin_patch Origin patch used to convert client X/Z to server coordinates.
         */
        void set_origin_patch(int origin_patch) {
            m_origin_patch = origin_patch;
        }

        /** @return Resolved display name, or empty to use the map-type fallback. */
        const std::string& area_name() const {
            return m_area_name;
        }

        /** Sets the display name supplied by the region resolver.
         * @param name Localized area name; empty uses the map-type fallback.
         */
        void set_area_name(std::string name) {
            m_area_name = std::move(name);
        }

        /**
         * Sets the camera look yaw used for the facing marker.
         * @param yaw_rad Yaw in radians.
         */
        void set_camera_yaw_rad(float yaw_rad) {
            m_camera_yaw_rad = yaw_rad;
        }

        /**
         * Sets the character yaw used for the player marker.
         * @param yaw_rad Yaw in radians.
         */
        void set_player_yaw_rad(float yaw_rad) {
            m_player_yaw_rad = yaw_rad;
        }

        /**
         * Sets the atlas zoom used when drawing tiles.
         * @param zoom Discrete zoom (1, 4, or 8).
         */
        void set_zoom(int zoom) {
            m_zoom = zoom;
        }

        /**
         * Switches the HUD onto a dungeon floor plan (KMiniMapSystem::SetMiniMapInfo).
         * @param size MiniMapSize for the indoor stem.
         * @param origin_cell_x Even map definition / orgPatch X of the 2×2 dungeon block.
         * @param origin_cell_y Even map definition / orgPatch Y of the 2×2 dungeon block.
         * @param origin_patch Profile origin patch used for world XZ (usually 32).
         */
        void set_dungeon(const map::MiniMapSize& size,
                         int origin_cell_x,
                         int origin_cell_y,
                         int origin_patch) {
            m_dungeon = !size.mapname().empty();
            m_mapname = size.mapname();
            m_size_x = size.size_x();
            m_size_y = size.size_y();
            m_tile_width = size.tile_width();
            m_tile_height = size.tile_height();
            m_map_left = size.map_left();
            m_map_bottom = size.map_bottom();
            m_correction = size.correction();
            m_origin_cell_x = origin_cell_x;
            m_origin_cell_y = origin_cell_y;
            m_origin_patch = origin_patch;
        }

        /** @return True when the HUD should draw dungeon GTX tiles. */
        [[nodiscard]] bool dungeon() const {
            return m_dungeon;
        }

        /** @return Indoor stem used in minimap_Dungeon_<stem>_NNN.GTX. */
        [[nodiscard]] const std::string& mapname() const {
            return m_mapname;
        }

        /** @return Atlas width in pixels. */
        [[nodiscard]] int size_x() const {
            return m_size_x;
        }

        /** @return Atlas height in pixels. */
        [[nodiscard]] int size_y() const {
            return m_size_y;
        }

        /** @return Tile width in pixels. */
        [[nodiscard]] int tile_width() const {
            return m_tile_width;
        }

        /** @return Tile height in pixels. */
        [[nodiscard]] int tile_height() const {
            return m_tile_height;
        }

        /** @return West crop in MAP_SIZE cells. */
        [[nodiscard]] int map_left() const {
            return m_map_left;
        }

        /** @return South crop in MAP_SIZE cells. */
        [[nodiscard]] int map_bottom() const {
            return m_map_bottom;
        }

        /** @return Extra atlas offset in pixels. */
        [[nodiscard]] int correction() const {
            return m_correction;
        }

        /** @return Even dungeon orgPatch X. */
        [[nodiscard]] int origin_cell_x() const {
            return m_origin_cell_x;
        }

        /** @return Even dungeon orgPatch Y. */
        [[nodiscard]] int origin_cell_y() const {
            return m_origin_cell_y;
        }

        /** @return Profile origin patch used for world XZ. */
        [[nodiscard]] int origin_patch() const {
            return m_origin_patch;
        }

      private:
        /** Player world X. */
        float m_world_x = 0.0f;

        /** Player world Z. */
        float m_world_z = 0.0f;

        /** Camera yaw in radians. */
        float m_camera_yaw_rad = 0.0f;

        /** Player yaw in radians. */
        float m_player_yaw_rad = 0.0f;

        /** Discrete atlas zoom (1, 4, or 8). */
        int m_zoom = 1;

        /** True when drawing dungeon floor-plan tiles. */
        bool m_dungeon = false;

        /** Indoor stem for dungeon GTX files. */
        std::string m_mapname;

        /** Optional resolved area label, independent of the dungeon asset stem. */
        std::string m_area_name;

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

        /** Even dungeon orgPatch X. */
        int m_origin_cell_x = 0;

        /** Even dungeon orgPatch Y. */
        int m_origin_cell_y = 0;

        /** Profile origin patch (ToClient, usually 32). */
        int m_origin_patch = 32;
    };

    /**
     * Renewal minimap: HyperText/MiniMap GTX tiles inside the open frame.
     *
     * Atlas math matches KMiniMapSystem after DEF_MINIMAP_RECREATE_KENSHIN_2006_07_07
     * (0.5 atlas pixels per terrain cell, 256-pixel tiles covering 2x2 patches).
     */
    class MinimapWidget {
      public:
        /** Atlas size in pixels (world overworld). */
        static constexpr float kAtlasSize = 8192.0f;

        /** Atlas pixels per 2x2 patch group (MAP_SIZE). */
        static constexpr float kTileSize = static_cast<float>(map::kMapSize);

        /** Atlas pixels per terrain cell. */
        static constexpr float kPixelsPerCell = 0.5f;

        /** Dungeon atlas pixels per terrain cell (MiniMap::OnPaint rate = 1). */
        static constexpr float kDungeonPixelsPerCell = 1.0f;

        /** Native width of main-mini map-open.bmp. */
        static constexpr float kNativeWidth = 213.0f;

        /** Native height of main-mini map-open.bmp. */
        static constexpr float kNativeHeight = 207.0f;

        /** Overview zoom (MAP_NORMAL). */
        static constexpr int kZoomNormal = 1;

        /** Mid zoom-in (stronger than MAP_ZOOM_A). */
        static constexpr int kZoomA = 4;

        /** Closest zoom-in (stronger than MAP_ZOOM_B). */
        static constexpr int kZoomB = 8;

        /**
         * Maps a world X onto the overworld minimap atlas.
         * @param world_x Player/world X.
         * @return Atlas X in pixels.
         */
        [[nodiscard]] static float atlas_x(float world_x) {
            const float map_center_cells =
                static_cast<float>(map::kMapSize * map::kPatchGrid) * 0.5f;
            return (map_center_cells + world_x / static_cast<float>(map::kTerrainCellSize)) *
                   kPixelsPerCell;
        }

        /**
         * Maps a world Z onto the overworld minimap atlas (Y-flipped like DrawMiniMap).
         * @param world_z Player/world Z.
         * @return Atlas Y in pixels (top-down, +Z is up / smaller Y).
         */
        [[nodiscard]] static float atlas_y(float world_z) {
            const float map_center_cells =
                static_cast<float>(map::kMapSize * map::kPatchGrid) * 0.5f;
            const float unflipped =
                (map_center_cells + world_z / static_cast<float>(map::kTerrainCellSize)) *
                kPixelsPerCell;
            return kAtlasSize - unflipped;
        }

        /**
         * Maps a world axis onto dungeon-local client space (orgPatch origin).
         * @param world World X or Z in profile origin_patch space.
         * @param origin_patch Profile origin patch (usually 32).
         * @param origin_cell Even dungeon orgPatch on that axis.
         * @return Coordinate as if ToClient used origin_cell.
         */
        [[nodiscard]] static float dungeon_local(float world, int origin_patch, int origin_cell) {
            return world - static_cast<float>((origin_cell - origin_patch) * map::kWorldSize);
        }

        /**
         * Maps a world X onto the dungeon minimap atlas (unflipped).
         * @param world_x Player world X.
         * @param origin_patch Profile origin patch.
         * @param origin_cell_x Even dungeon orgPatch X.
         * @param map_left MiniMapSize runtime state.
         * @param correction MiniMapSize runtime state.
         * @return Atlas X in pixels.
         */
        [[nodiscard]] static float dungeon_atlas_x(
            float world_x, int origin_patch, int origin_cell_x, int map_left, int correction) {
            const float local = dungeon_local(world_x, origin_patch, origin_cell_x);
            return local / static_cast<float>(map::kTerrainCellSize) * kDungeonPixelsPerCell +
                   static_cast<float>(map_left * map::kMapSize + correction);
        }

        /**
         * Maps a world Z onto the dungeon minimap atlas (Y-flipped like DrawMiniMap).
         * @param world_z Player world Z.
         * @param origin_patch Profile origin patch.
         * @param origin_cell_y Even dungeon orgPatch Y.
         * @param map_bottom MiniMapSize runtime state.
         * @param correction MiniMapSize runtime state.
         * @param size_y Atlas height in pixels.
         * @return Atlas Y in pixels (top-down, +Z is up / smaller Y).
         */
        [[nodiscard]] static float dungeon_atlas_y(float world_z,
                                                   int origin_patch,
                                                   int origin_cell_y,
                                                   int map_bottom,
                                                   int correction,
                                                   int size_y) {
            const float local = dungeon_local(world_z, origin_patch, origin_cell_y);
            const float unflipped =
                local / static_cast<float>(map::kTerrainCellSize) * kDungeonPixelsPerCell +
                static_cast<float>(map_bottom * map::kMapSize + correction);
            return static_cast<float>(size_y) - unflipped;
        }

        /**
         * Atlas rectangle of dungeon GTX tile `index` packed left-to-right
         * (`extended minimap initialization`).
         * @param index Sequential tile index (000, 001, …).
         * @param tile_width Tile width in pixels.
         * @param tile_height Tile height in pixels.
         * @param size_x Atlas width in pixels (wraps a row when X >= size_x).
         * @param left Out tile left in atlas pixels.
         * @param top Out tile top in atlas pixels.
         * @param right Out tile right in atlas pixels.
         * @param bottom Out tile bottom in atlas pixels.
         */
        static void dungeon_tile_rect(int index,
                                      int tile_width,
                                      int tile_height,
                                      int size_x,
                                      float& left,
                                      float& top,
                                      float& right,
                                      float& bottom) {
            const int width = std::max(1, tile_width);
            const int height = std::max(1, tile_height);
            const int cols = std::max(1, size_x / width);
            const int col = index % cols;
            const int row = index / cols;
            left = static_cast<float>(col * width);
            top = static_cast<float>(row * height);
            right = left + static_cast<float>(width);
            bottom = top + static_cast<float>(height);
        }

        /**
         * Atlas rectangle of the GTX tile for even patch indices i,j in [0, 62].
         * @param patch_x Even patch X (i).
         * @param patch_y Even patch Y (j).
         * @param left Out tile left in atlas pixels.
         * @param top Out tile top in atlas pixels.
         * @param right Out tile right in atlas pixels.
         * @param bottom Out tile bottom in atlas pixels.
         */
        static void
        tile_rect(int patch_x, int patch_y, float& left, float& top, float& right, float& bottom) {
            const float n_top = kAtlasSize - kTileSize;
            left = static_cast<float>(patch_x / 2) * kTileSize;
            top = n_top - static_cast<float>(patch_y / 2) * kTileSize;
            right = left + kTileSize;
            bottom = top + kTileSize;
        }

        /**
         * Clamps a zoom value onto the 1 / 4 / 8 steps.
         * @param zoom Requested zoom.
         * @return 1, 4, or 8.
         */
        [[nodiscard]] static int sanitize_zoom(int zoom) {
            if (zoom >= kZoomB) {
                return kZoomB;
            }
            if (zoom >= kZoomA) {
                return kZoomA;
            }
            return kZoomNormal;
        }

        /**
         * Steps one zoom level in (+ button).
         * @param zoom Current zoom.
         * @return Next zoom, or 8 when already at maximum.
         */
        [[nodiscard]] static int step_zoom_in(int zoom) {
            const int current = sanitize_zoom(zoom);
            if (current < kZoomA) {
                return kZoomA;
            }
            return kZoomB;
        }

        /**
         * Steps one zoom level out (- button).
         * @param zoom Current zoom.
         * @return Previous zoom, or 1 when already at minimum.
         */
        [[nodiscard]] static int step_zoom_out(int zoom) {
            const int current = sanitize_zoom(zoom);
            if (current > kZoomA) {
                return kZoomA;
            }
            return kZoomNormal;
        }

        /**
         * Screen-space hit rect of the baked + zoom button.
         * @param panel_x Panel left.
         * @param panel_y Panel top.
         * @param panel_w Panel width.
         * @param panel_h Panel height.
         * @param layout Minimap layout document, or nullptr for baked defaults.
         * @param x Out button left.
         * @param y Out button top.
         * @param w Out button width.
         * @param h Out button height.
         * @return True when the rect has a positive size.
         */
        [[nodiscard]] static bool zoom_plus_rect(float panel_x,
                                                 float panel_y,
                                                 float panel_w,
                                                 float panel_h,
                                                 const UiLayout* layout,
                                                 float& x,
                                                 float& y,
                                                 float& w,
                                                 float& h);

        /**
         * Screen-space hit rect of the baked - zoom button.
         * @param panel_x Panel left.
         * @param panel_y Panel top.
         * @param panel_w Panel width.
         * @param panel_h Panel height.
         * @param layout Minimap layout document, or nullptr for baked defaults.
         * @param x Out button left.
         * @param y Out button top.
         * @param w Out button width.
         * @param h Out button height.
         * @return True when the rect has a positive size.
         */
        [[nodiscard]] static bool zoom_minus_rect(float panel_x,
                                                  float panel_y,
                                                  float panel_w,
                                                  float panel_h,
                                                  const UiLayout* layout,
                                                  float& x,
                                                  float& y,
                                                  float& w,
                                                  float& h);

        /** Tests the map, its rim and explicit controls while leaving empty corners clickable.
         * @param mx Pointer X in screen pixels.
         * @param my Pointer Y in screen pixels.
         * @param x Panel left.
         * @param y Panel top.
         * @param w Panel width.
         * @param h Panel height.
         * @param layout Minimap layout, or nullptr for legacy rectangular bounds.
         * @return True on the interactive surface.
         */
        [[nodiscard]] static bool
        contains(float mx, float my, float x, float y, float w, float h, const UiLayout* layout);

        /** Formats absolute world tile coordinates for sharing locations.
         * @param data Player position and profile origin.
         * @return Horizontal X, Y tile coordinates, without vertical height.
         */
        [[nodiscard]] static std::string coordinates_text(const MinimapWidgetData& data);

        /** Resolves the named overworld area containing a client-space position.
         * @param world_x Player world X in client space.
         * @param world_z Player world Z in client space.
         * @param origin_patch Profile origin patch used for coordinate conversion.
         * @param areas Loaded area attributes; a settlement name requires the Village flag.
         * @return Area name, or the overworld fallback when no named area contains the position.
         */
        [[nodiscard]] static std::string_view
        area_name(float world_x, float world_z, int origin_patch, const map::WorldGrid& areas);

        /** Resolves the dungeon label independently of shared minimap asset names.
         * @param map_index Current server map ID, including alternate dungeon floor IDs.
         * @return D1 through D6 for known dungeons, otherwise Dungeon.
         */
        [[nodiscard]] static std::string_view dungeon_area_name(int map_index);

        /**
         * Draws the minimap frame, visible GTX tiles, and the player facing marker.
         * @param ui Shared UI helpers.
         * @param font Regular FreeType font.
         * @param bold Bold FreeType font.
         * @param x Panel left.
         * @param y Panel top.
         * @param w Panel width.
         * @param h Panel height.
         * @param data Player position, camera yaw, and zoom.
         * @param layout Minimap layout document, or nullptr for baked defaults.
         */
        static void draw(WidgetTools& ui,
                         const FreetypeFont& font,
                         const FreetypeFont& bold,
                         float x,
                         float y,
                         float w,
                         float h,
                         const MinimapWidgetData& data,
                         const UiLayout* layout = nullptr);
    };

}  // namespace openkal::client

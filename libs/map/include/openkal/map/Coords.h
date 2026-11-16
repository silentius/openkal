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

#include "openkal/map/Constants.h"

#include <cmath>

namespace openkal::coords {

    /** Default origin patch for client/server coordinate conversion. */
    inline constexpr int kDefaultOriginPatch = 32;

    /** Converts a server horizontal coordinate to client space.
     * @param server Server pixel coordinate.
     * @param origin_patch Origin patch (default 32).
     * @return Client-space coordinate.
     */
    inline float to_client(int server, int origin_patch = kDefaultOriginPatch) {
        return static_cast<float>(server) -
               static_cast<float>(origin_patch * map::kMapSize * map::kTerrainCellSize);
    }

    /** Converts a server Z coordinate to client Y.
     * @param server_z Server Z.
     * @return Client Y.
     */
    inline float to_client_y(int server_z) {
        return static_cast<float>(server_z) * 0.1f;
    }

    /** Converts a client horizontal coordinate to server space.
     * @param client Client-space coordinate.
     * @param origin_patch Origin patch (default 32).
     * @return Server pixel coordinate.
     */
    inline int to_server(float client, int origin_patch = kDefaultOriginPatch) {
        return static_cast<int>(client) + origin_patch * map::kMapSize * map::kTerrainCellSize;
    }

    /** Converts server pixels to world tiles (pixel >> 5).
     * @param pixel_x Server pixel X.
     * @param pixel_y Server pixel Y.
     * @param tile_x Receives world tile X.
     * @param tile_y Receives world tile Y.
     */
    inline void pixel_to_tile(int pixel_x, int pixel_y, int& tile_x, int& tile_y) {
        tile_x = pixel_x >> 5;
        tile_y = pixel_y >> 5;
    }

    /**
     * True when a pixel lies in compatibility behavior(center, radius_tiles).
     * Inclusive Chebyshev tile distance: center ± radius (9×9 tiles at radius 4).
     * @param center_x Area center pixel X.
     * @param center_y Area center pixel Y.
     * @param pixel_x Test pixel X.
     * @param pixel_y Test pixel Y.
     * @param radius_tiles Inclusive tile radius.
     * @return True when the test pixel's tile is inside the square.
     */
    [[nodiscard]] inline bool
    in_tile_rect(int center_x, int center_y, int pixel_x, int pixel_y, int radius_tiles) {
        if (radius_tiles < 0) {
            return false;
        }
        int ctx = 0;
        int cty = 0;
        int tx = 0;
        int ty = 0;
        pixel_to_tile(center_x, center_y, ctx, cty);
        pixel_to_tile(pixel_x, pixel_y, tx, ty);
        const int dx = tx - ctx;
        const int dy = ty - cty;
        return dx <= radius_tiles && dx >= -radius_tiles && dy <= radius_tiles &&
               dy >= -radius_tiles;
    }

    /** Converts world tiles to server pixels (tile << 5).
     * @param tile_x World tile X.
     * @param tile_y World tile Y.
     * @param pixel_x Receives server pixel X.
     * @param pixel_y Receives server pixel Y.
     */
    inline void tile_to_pixel(int tile_x, int tile_y, int& pixel_x, int& pixel_y) {
        pixel_x = tile_x << 5;
        pixel_y = tile_y << 5;
    }

    /** Sub-cell index within a tile (0..15), matching compatibility behavior.
     * @param pixel_x Server pixel X.
     * @param pixel_y Server pixel Y.
     * @return Sub-cell index in 0..15.
     */
    inline int subcell_index(int pixel_x, int pixel_y) {
        return ((pixel_x >> 3) & 3) + 4 * ((pixel_y >> 3) & 3);
    }

    /** Splits an absolute world tile into area cell slot plus local tile.
     * @param world_tx Absolute world tile X.
     * @param world_ty Absolute world tile Y.
     * @param cell_x Receives cell X.
     * @param cell_y Receives cell Y.
     * @param local_tx Receives local tile X within the cell.
     * @param local_ty Receives local tile Y within the cell.
     */
    inline void world_tile_to_cell(
        int world_tx, int world_ty, int& cell_x, int& cell_y, int& local_tx, int& local_ty) {
        cell_x = world_tx >> 8;
        cell_y = world_ty >> 8;
        local_tx = world_tx & (map::kTileCount - 1);
        local_ty = world_ty & (map::kTileCount - 1);
    }

    /** Builds an absolute world tile from cell slot plus local tile.
     * @param cell_x Cell X.
     * @param cell_y Cell Y.
     * @param local_tx Local tile X within the cell.
     * @param local_ty Local tile Y within the cell.
     * @param world_tx Receives absolute world tile X.
     * @param world_ty Receives absolute world tile Y.
     */
    inline void cell_local_to_world_tile(
        int cell_x, int cell_y, int local_tx, int local_ty, int& world_tx, int& world_ty) {
        world_tx = (cell_x << 8) + local_tx;
        world_ty = (cell_y << 8) + local_ty;
    }

    /** Narootuh default new-player spawn X (server pixels). */
    inline constexpr int kNarootuhServerX = 257285;

    /** Narootuh default new-player spawn Y (server pixels). */
    inline constexpr int kNarootuhServerY = 258744;

    /** Narootuh default new-player spawn Z (server). */
    inline constexpr int kNarootuhServerZ = 16100;

    /** Sight radius in tiles (compatibility behavior GetRect(..., 25)). */
    inline constexpr int kSightTileRadius = 25;

    /** True if entity pixels are inside the player's tile Chebyshev sight rect.
     * @param player_px Player server pixel X.
     * @param player_py Player server pixel Y.
     * @param entity_px Entity server pixel X.
     * @param entity_py Entity server pixel Y.
     * @param radius_tiles Sight radius in tiles.
     * @return True if the entity is in sight.
     */
    inline bool in_sight(int player_px,
                         int player_py,
                         int entity_px,
                         int entity_py,
                         int radius_tiles = kSightTileRadius) {
        int ptx = 0;
        int pty = 0;
        int etx = 0;
        int ety = 0;
        pixel_to_tile(player_px, player_py, ptx, pty);
        pixel_to_tile(entity_px, entity_py, etx, ety);
        return std::abs(ptx - etx) <= radius_tiles && std::abs(pty - ety) <= radius_tiles;
    }

    /** Even-aligned 2x2 area-map block origin (`MMap::LoadCurrentSvMap` uses `x & ~1`).
     * @param cell Cell index.
     * @return Even cell index.
     */
    [[nodiscard]] inline int even_cell(int cell) {
        return cell & ~1;
    }

    /** Server pixels of a area cell origin.
     * @param cell Cell index.
     * @return Pixel coordinate of the cell's min corner.
     */
    [[nodiscard]] inline int cell_origin_pixels(int cell) {
        return cell * map::kWorldSize;
    }

    /** Converts a client Y coordinate to server Z.
     * @param client_y Client vertical coordinate.
     * @return Server Z (height) in Kal pixel units.
     */
    inline int to_server_z(float client_y) {
        return static_cast<int>(client_y * 10.0f);
    }

    /** Outdoor patch index for a client XZ coordinate.
     * @param client_xz Client X or Z.
     * @return Patch index on the outdoor grid.
     */
    inline int patch_index_for_client(float client_xz) {
        const float tile = client_xz / static_cast<float>(map::kTerrainCellSize);
        const int patch_rel =
            static_cast<int>(std::floor(tile / static_cast<float>(map::kMapSize)));
        return patch_rel + map::kPatchGrid / 2;
    }

}  // namespace openkal::coords

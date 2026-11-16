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

namespace openkal::map {

    /** SVMAP / MainSvr compatibility behavior shared grid version. */
    inline constexpr int kVersion = 1;

    /** Pixels per tile. */
    inline constexpr int kTileSize = 32;

    /** Sub-cell size within a tile (pixels). */
    inline constexpr int kCellSize = 8;

    /** Tiles per area-map cell on one axis. */
    inline constexpr int kTileCount = 256;

    /** Sub-cells per tile on one axis (kTileSize / kCellSize). */
    inline constexpr int kCellsPerTile = kTileSize / kCellSize;

    /** Sub-cells per area-map cell on one axis. */
    inline constexpr int kCellCount = kTileCount * kCellsPerTile;

    /** Pixel extent of one area-map cell on one axis. */
    inline constexpr int kWorldSize = kTileSize * kTileCount;

    /** Outdoor .kcm patch (ROAM) level; same horizontal scale as one SVMAP cell. */
    inline constexpr int kMapLevel = 8;

    /** Outdoor patch size in tiles (1 << kMapLevel). */
    inline constexpr int kMapSize = 1 << kMapLevel;

    /** Height samples per outdoor patch axis (kMapSize + 1). */
    inline constexpr int kHeightSamples = kMapSize + 1;

    /** Outdoor map layer count. */
    inline constexpr int kMapLayer = 8;

    /** Outdoor patch grid level. */
    inline constexpr int kPatchLevel = 6;

    /** Outdoor patch grid size (1 << kPatchLevel). */
    inline constexpr int kPatchGrid = 1 << kPatchLevel;

    /** Terrain cell size in pixels (compatibility behavior in roam.h). */
    inline constexpr int kTerrainCellSize = 32;

    /** Global water plane height: (kTerrainCellSize * 99) / 2. */
    inline constexpr float kWaterPosY = (kTerrainCellSize * 99) / 2.0f;

}  // namespace openkal::map

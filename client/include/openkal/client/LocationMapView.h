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

#include "openkal/client/PowerbookCatalog.h"
#include "openkal/client/WorldMapWidget.h"
#include "openkal/map/MapCatalog.h"
#include "openkal/map/MiniMapSize.h"

namespace openkal::client {
    /** Shared, clipped outdoor/dungeon map for authored spawn areas and NPC points. */
    class LocationMapView final {
      public:
        /** Loads optional map metadata.
         * @param root Runtime configuration root. */
        void load(const std::string& root);

        /** Draws an aspect-preserving map centered on the selected location.
         * @param engine Renderer.
         * @param ui Texture helpers.
         * @param font FreeType font for missing-data messages.
         * @param x Canvas left.
         * @param y Canvas top.
         * @param w Canvas width.
         * @param h Canvas height.
         * @param scale UI scale.
         * @param spawns Server-coordinate rectangles, or points for NPCs.
         * @param selected Location to center.
         * @param magnification Zoom multiplier relative to the automatic framing. */
        void draw(engine::IEngine& engine,
                  WidgetTools& ui,
                  const FreetypeFont& font,
                  float x,
                  float y,
                  float w,
                  float h,
                  float scale,
                  const std::vector<PowerbookSpawn>& spawns,
                  int selected,
                  float magnification = 1.0f) const;

      private:
        map::MapCatalog m_maps;
        map::MiniMapSizeCatalog m_minimaps;
        mutable WorldMapWidget::TileLoadState m_tiles;
        mutable std::unordered_map<std::string, std::string> m_dungeon_tiles;
    };
}  // namespace openkal::client

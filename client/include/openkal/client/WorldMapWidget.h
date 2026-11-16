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
#include "openkal/client/MinimapWidget.h"

#include <array>
#include <cstddef>
#include <string>
#include <utility>

namespace openkal::engine {
    class IEngine;
}

namespace openkal::client {
    class WidgetTools;

    /** Full-screen world-map overlay assembled from the overworld minimap sections. */
    class WorldMapWidget {
      public:
        /** Incremental cache state for world-map minimap sections. */
        class TileLoadState {
          public:
            /** Clears the cached section names so they are resolved again. */
            void reset() {
                m_tile_names.fill({});
                m_tile_resolved.fill(false);
            }

            /** @param index Section index in the 32x32 world-map cache. @return True after
             * resolution. */
            [[nodiscard]] bool resolved(std::size_t index) const {
                return m_tile_resolved[index];
            }

            /** @param index Section index in the 32x32 world-map cache. @return Resolved texture
             * path. */
            [[nodiscard]] const std::string& tile_name(std::size_t index) const {
                return m_tile_names[index];
            }

            /**
             * Stores a resolved section texture path, including an empty path when unavailable.
             * @param index Section index in the 32x32 world-map cache.
             * @param name Resolved texture path.
             */
            void resolve(std::size_t index, std::string name) {
                m_tile_names[index] = std::move(name);
                m_tile_resolved[index] = true;
            }

          private:
            /** Resolved texture file name for each 2x2 overworld patch group. */
            std::array<std::string, 32 * 32> m_tile_names{};

            /** True after the section's candidate file names were checked. */
            std::array<bool, 32 * 32> m_tile_resolved{};
        };

        /**
         * Returns the player marker location for the full minimap-section atlas.
         * @param screen_w Framebuffer width.
         * @param screen_h Framebuffer height.
         * @param data Player and map-coordinate snapshot.
         * @param out_x Receives the marker X in framebuffer pixels.
         * @param out_y Receives the marker Y in framebuffer pixels.
         */
        static void marker_position(float screen_w,
                                    float screen_h,
                                    const MinimapWidgetData& data,
                                    float& out_x,
                                    float& out_y);

        /**
         * Tests the lower-right close control.
         * @param mouse_x Pointer X in framebuffer pixels.
         * @param mouse_y Pointer Y in framebuffer pixels.
         * @param screen_w Framebuffer width.
         * @param screen_h Framebuffer height.
         * @return True when the control contains the pointer.
         */
        [[nodiscard]] static bool
        close_contains(float mouse_x, float mouse_y, float screen_w, float screen_h);

        /**
         * Draws the 5x section-based world-map viewport and player direction marker.
         * @param ui Shared texture helpers.
         * @param engine UI renderer.
         * @param font FreeType face used for the close hint.
         * @param screen_w Framebuffer width.
         * @param screen_h Framebuffer height.
         * @param data Player and map-coordinate snapshot.
         * @param tile_state Incremental section-loading state retained between frames.
         */
        static void draw(WidgetTools& ui,
                         engine::IEngine& engine,
                         const FreetypeFont& font,
                         float screen_w,
                         float screen_h,
                         const MinimapWidgetData& data,
                         TileLoadState& tile_state);
    };
}  // namespace openkal::client

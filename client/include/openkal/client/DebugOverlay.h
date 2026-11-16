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

#include "openkal/engine/engine.h"

#include <array>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace openkal::map {
    class WorldGrid;
}

namespace openkal::terrain {
    class FloorField;
}

namespace openkal::client {

    using terrain::FloorField;
    class FreetypeFont;

    /** World/UI debug overlays; DEBUG=1 sets the initial value, /debug toggles. */
    class DebugOverlay {
      public:
        /** Non-instantiable utility class. */
        DebugOverlay() = delete;

        /** @return True when the debug overlay is currently enabled. */
        [[nodiscard]] static bool enabled();

        /**
         * Enables or disables the overlay at runtime (chat /debug).
         * @param on True to show hitboxes, chunk name, and player pose.
         */
        static void set_enabled(bool on);

        /**
         * Builds the outdoor patch stem n_XXX_YYY for the given patch indices.
         * @param patch_x Patch X.
         * @param patch_y Patch Y.
         * @return Patch file stem without extension.
         */
        [[nodiscard]] static std::string patch_name(int patch_x, int patch_y);

        /**
         * Allocates the unit cube / wireframe meshes used by draw_monster_hitbox.
         * @param engine Engine used to create GPU resources.
         * @return True when both meshes and the fill texture are ready.
         */
        [[nodiscard]] static bool init(engine::IEngine& engine);

        /**
         * Releases hitbox and diagnostic text GPU resources.
         * @param engine Engine owning the handles.
         */
        static void destroy(engine::IEngine& engine);

        /** Draws readable, wrapped diagnostics at the top-right of the viewport.
         * @param engine Engine used to submit UI draws.
         * @param lines Diagnostic lines to display.
         * @param width Viewport width in pixels.
         * @param height Viewport height in pixels.
         */
        static void draw_text(engine::IEngine& engine,
                              const std::vector<std::string>& lines,
                              int width,
                              int height);

        /** Draws a compact FPS counter at the top-left, independently of debug text.
         * @param engine Engine used to submit UI draws.
         * @param fps Measured frames per second.
         * @param width Viewport width in pixels.
         * @param height Viewport height in pixels.
         */
        static void draw_fps(engine::IEngine& engine, float fps, int width, int height);

        /**
         * Draws the world-space monster or NPC pick AABB as a translucent yellow box
         * with dark edges.
         * @param engine Engine used to submit the draw.
         * @param world_x World X (box center on XZ).
         * @param base_y Feet world Y (box bottom). NPCs must use spawn Y, not GetFloor.
         * @param world_z World Z (box center on XZ).
         * @param half_extent World-space XZ half-size.
         * @param height World-space box height.
         */
        static void draw_monster_hitbox(engine::IEngine& engine,
                                        float world_x,
                                        float base_y,
                                        float world_z,
                                        float half_extent,
                                        float height);

      private:
        /** Smaller TrueType face for the standalone FPS counter. */
        static FreetypeFont m_fps_font;

        /** Last requested FPS font size, including failed load attempts. */
        static int m_fps_pixel_size;

        /** TrueType face rasterized at the current diagnostic text size. */
        static FreetypeFont m_text_font;

        /** Last requested font size, including failed load attempts. */
        static int m_text_pixel_size;

        /** Solid unit AABB mesh: x/z in [-1,1], y in [0,1]. */
        static engine::MeshHandle m_fill_mesh;

        /** Thin edge beams for the same unit AABB. */
        static engine::MeshHandle m_edge_mesh;

        /** Flat yellow texel used for the translucent fill. */
        static engine::TextureHandle m_fill_tex;

        /** Runtime overlay flag; DEBUG=1 applies on first enabled() read. */
        static bool m_enabled;

        /** True after the DEBUG environment variable has been applied. */
        static bool m_env_applied;
    };

    /** Renders selectable monster-spawn and area attribute overlays for debugging. */
    class WorldDebugOverlay {
      public:
        /** area layers supported by the `/show` and `/hide` commands. */
        enum class AreaLayer : int {
            Blocked,
            NotMoveable,
            Village,
            Safety,
            Portal,
            Object,
            AtkCastle,
            DefCastle,
            Count,
        };

        /** Loads monster spawn rectangles from the converted monster JSON files.
         * @param directory Directory containing monster JSON definitions.
         * @return True when at least one spawn rectangle was loaded.
         */
        [[nodiscard]] bool load_spawn_areas(std::string_view directory);

        /** Applies a `/show` command argument.
         * @param args Text after `/show`.
         * @return User-facing command result.
         */
        [[nodiscard]] std::string show(std::string_view args);

        /** Applies a `/hide` command argument.
         * @param args Text after `/hide`.
         * @return User-facing command result.
         */
        [[nodiscard]] std::string hide(std::string_view args);

        /** Rebuilds geometry after streamed area cells or the selected map change.
         * @param engine Engine that owns overlay meshes.
         * @param floor Floor sampler used to conform the overlay to the ground geometry.
         * @param grid Streamed area cells.
         * @param origin_patch Client coordinate-system origin patch.
         * @param server_x Current server X position.
         * @param server_y Current server Y position.
         * @param map_index Current logical map index.
         * @param is_indoor True when an indoor visual map is loaded.
         * @param player_floor_y Current visible player floor height.
         */
        void update(engine::IEngine& engine,
                    const FloorField& floor,
                    const map::WorldGrid& grid,
                    int origin_patch,
                    int server_x,
                    int server_y,
                    int map_index,
                    bool is_indoor,
                    float player_floor_y);

        /** Draws all enabled translucent overlay surfaces.
         * @param engine Engine that owns the meshes.
         */
        void draw(engine::IEngine& engine) const;

        /** Releases GPU resources created by update.
         * @param engine Engine that owns the meshes.
         */
        void destroy(engine::IEngine& engine);

      private:
        struct SpawnArea {
            int m_area = 0;
            int m_map = 0;
            int m_x0 = 0;
            int m_y0 = 0;
            int m_x1 = 0;
            int m_y1 = 0;
        };

        [[nodiscard]] static const char* layer_name(AreaLayer layer);
        [[nodiscard]] static bool layer_from_name(std::string_view name, AreaLayer& out);
        [[nodiscard]] static std::string usage();
        void mark_dirty();
        void clear_meshes(engine::IEngine& engine);

        std::vector<SpawnArea> m_spawn_areas;
        std::unordered_set<int> m_visible_areas;
        std::array<bool, static_cast<int>(AreaLayer::Count)> m_visible_layers{};
        std::array<engine::MeshHandle, static_cast<int>(AreaLayer::Count)> m_layer_meshes{};
        engine::MeshHandle m_spawn_mesh{};
        engine::TextureHandle m_fill_texture{};
        bool m_dirty = false;
        int m_center_cell_x = -1;
        int m_center_cell_y = -1;
        int m_map_index = -1;
        bool m_is_indoor = false;
        float m_player_floor_y = 0.0f;
    };

}  // namespace openkal::client

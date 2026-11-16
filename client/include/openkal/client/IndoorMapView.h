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

#include "openkal/client/Floor.h"
#include "openkal/client/WorldRenderCache.h"
#include "openkal/engine/engine.h"
#include "openkal/map/IndoorMap.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace openkal::client {

    /** Particle table for indoor/OPL world GBs (`WorldObjectCatalog.h`). */
    class WorldObjectCatalog;

    /**
     * GPU instances for one indoor `.map` (region GBs + MAP_OBJECT placements).
     *
     * MMap::Load places region meshes at identity (vertices already in
     * map-local server-pixel XZ). The 2x2 even area-map block origin is added so D1/D3
     * (cells 6,4 / 6,2) line up with the player; Y is already world height.
     * Region GBs are walkable floors plus camera blockers. MAP_OBJECT GBs (statues)
     * add compatibility behavior hulls as obstacles so the player bumps into them without standing
     * on them.
     */
    class IndoorMapView {
      public:
        /** Sentinel FloorField patch index for indoor decks (not outdoor stream). */
        static constexpr int kFloorPatch = -1;

        /**
         * Loads `{map_file}` into GPU props and walkable decks.
         * @param engine Engine used to upload meshes.
         * @param map_root Indoor map directory (profile map_root).
         * @param map_file Absolute path of the `.map` binary.
         * @param origin_patch_x Origin patch on the X axis (usually 32).
         * @param origin_patch_y Origin patch on the Y/Z axis (usually 32).
         * @param cell_x map definition cell X of the player (even-aligned to the 2x2 block).
         * @param cell_y map definition cell Y of the player.
         * @param model_cache Shared GB GPU cache.
         * @param tex_cache Shared texture cache.
         * @param floor Walkable decks (indoor uses kFloorPatch).
         * @param object_catalog World-object particle table, or nullptr.
         * @param next_fx_serial World-prop SpriteFx id counter, or nullptr.
         * @return True when at least one mesh was placed.
         */
        [[nodiscard]] bool
        load(engine::IEngine& engine,
             const std::string& map_root,
             const std::string& map_file,
             int origin_patch_x,
             int origin_patch_y,
             int cell_x,
             int cell_y,
             std::unordered_map<std::string, WorldRenderCache::GpuModel>& model_cache,
             std::unordered_map<std::string, engine::TextureHandle>& tex_cache,
             FloorField& floor,
             const WorldObjectCatalog* object_catalog = nullptr,
             std::uint32_t* next_fx_serial = nullptr);

        /**
         * Drops indoor instances and their floor decks. GPU models stay in the cache.
         * @param floor Walkable decks to strip kFloorPatch from.
         */
        void unload(FloorField& floor);

        /**
         * Marks props visible from the camera leaf (RenderMap/RenderObject).
         * When the BSP probe misses, every instance stays visible.
         * @param eye_x World-space camera X.
         * @param eye_y World-space camera Y.
         * @param eye_z World-space camera Z.
         * @param look_x World-space look-at X.
         * @param look_y World-space look-at Y.
         * @param look_z World-space look-at Z.
         * @param frustum Current world-space camera frustum.
         */
        void update_visibility(float eye_x,
                               float eye_y,
                               float eye_z,
                               float look_x,
                               float look_y,
                               float look_z,
                               const FrustumPlanes& frustum);

        /** @return True when a map is currently loaded. */
        [[nodiscard]] bool loaded() const {
            return !m_path.empty();
        }

        /** @return Loaded `.map` path. */
        [[nodiscard]] const std::string& path() const {
            return m_path;
        }

        /** @return Placed indoor meshes. */
        [[nodiscard]] const std::vector<WorldRenderCache::DrawnProp>& props() const {
            return m_props;
        }

        /**
         * Tests one indoor region submesh against the current camera-leaf PVS.
         * Missing, malformed, and mismatched visibility data remains visible.
         * @param prop_index Index into props().
         * @param part_index Index into the prop GPU model's parts.
         * @return True when the submesh should be drawn.
         */
        [[nodiscard]] bool region_part_visible(std::size_t prop_index,
                                               std::size_t part_index) const;

        /** @return Indoor map stem (`tomb_01`). */
        [[nodiscard]] const std::string& stem() const {
            return m_stem;
        }

        /** @return Even map definition / orgPatch X of the loaded 2×2 block. */
        [[nodiscard]] int cell_x() const {
            return m_cell_x;
        }

        /** @return Even map definition / orgPatch Y of the loaded 2×2 block. */
        [[nodiscard]] int cell_y() const {
            return m_cell_y;
        }

      private:
        /** Source `.map` path. */
        std::string m_path;

        /** Indoor map stem used for dungeon minimap GTX files. */
        std::string m_stem;

        /** Even map definition cell X of the 2×2 dungeon block. */
        int m_cell_x = 0;

        /** Even map definition cell Y of the 2×2 dungeon block. */
        int m_cell_y = 0;

        /** GPU instances. */
        std::vector<WorldRenderCache::DrawnProp> m_props;

        /** Parsed `.map` kept for BSP PVS queries. */
        map::IndoorMap m_map;

        /** Per-leaf MAP_OBJECT prop indices (MMap::InsertObject). */
        std::vector<std::vector<int>> m_leaf_objects;

        /** Per-region submesh visibility; empty entries are conservative fallbacks. */
        std::vector<std::vector<std::uint8_t>> m_region_part_visibility;

        /** Number of region props at the front of runtime state. */
        int m_region_count = 0;

        /** World X added to map-local vertices. */
        float m_shift_x = 0.0f;

        /** World Z added to map-local vertices. */
        float m_shift_z = 0.0f;
    };

}  // namespace openkal::client

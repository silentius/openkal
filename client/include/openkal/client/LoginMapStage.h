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
#include "openkal/client/IndoorMapView.h"
#include "openkal/client/WorldObjectCatalog.h"

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace openkal::client {

    class SpriteFx;

    /** Asset root and placement for the character login room. */
    class LoginMapStagePaths {
      public:
        /** Root containing `login.map` and its GB assets. */
        std::string map_root;

        /** Converted object.dat particle assignments. */
        std::string world_objects_path;

        /** Map-origin patch used to place the login room on the X axis. */
        int origin_patch_x = 0;

        /** Map-origin patch used to place the login room on the Y/Z axis. */
        int origin_patch_y = 6;
    };

    /**
     * Shared character-select/create room (`Map\\login_map`).
     *
     * The room includes the authored dragon-head geometry, so the shell does not
     * manufacture a parallel backdrop or placeholder props.
     */
    class LoginMapStage {
      public:
        /** Selection/creation focus point in map-local client units. */
        static constexpr float kCenterX = 150.0f;
        static constexpr float kCenterY = 3010.0f;
        static constexpr float kCenterZ = 153.0f;

        LoginMapStage() = default;
        LoginMapStage(const LoginMapStage&) = delete;
        LoginMapStage& operator=(const LoginMapStage&) = delete;

        /**
         * Releases GPU resources owned by this stage.
         * @param engine Engine that owns the resources.
         */
        void destroy(engine::IEngine& engine);

        /**
         * Loads the indoor login room and its static geometry.
         * @param engine Engine with the current render context.
         * @param paths Map asset root and map-origin patches.
         * @return True if the map placed at least one drawable mesh.
         */
        [[nodiscard]] bool load(engine::IEngine& engine, const LoginMapStagePaths& paths);

        /** @return True when the login room has been loaded. */
        [[nodiscard]] bool loaded() const {
            return m_view.loaded();
        }

        /**
         * Samples the authored room floor, retaining `fallback_y` outside a deck.
         * @param world_x World-space X.
         * @param world_z World-space Z.
         * @param fallback_y Height used where no floor deck is present.
         * @return Floor height or `fallback_y`.
         */
        [[nodiscard]] float sample_floor(float world_x, float world_z, float fallback_y) const;

        /**
         * Draws map geometry with its indoor PVS and alpha/polygon-offset passes.
         * @param engine Engine with camera already configured.
         * @param eye Current camera position.
         * @param target Current camera target.
         * @param view Current view matrix.
         * @param projection Current projection matrix.
         * @param anim_time_sec UV-animation clock.
         */
        void draw(engine::IEngine& engine,
                  const engine::Vec3& eye,
                  const engine::Vec3& target,
                  const engine::Mat4& view,
                  const engine::Mat4& projection,
                  float anim_time_sec);

        /**
         * Synchronizes visible login-map object particles and draws them.
         * @param engine Engine owning particle resources.
         * @param sprite_fx Shared Sprite/Fx script renderer.
         * @param dt Frame time in seconds.
         * @param eye Current camera position for particle culling.
         */
        void draw_effects(engine::IEngine& engine,
                          SpriteFx& sprite_fx,
                          float dt,
                          const engine::Vec3& eye);

      private:
        /** Indoor map instances and PVS state. */
        IndoorMapView m_view;

        /** Walkable decks used for actor placement. */
        FloorField m_floor;

        /** Stage-owned GB GPU models. */
        std::unordered_map<std::string, WorldRenderCache::GpuModel> m_model_cache;

        /** Stage-owned GB textures. */
        std::unordered_map<std::string, engine::TextureHandle> m_texture_cache;

        /** Object.dat particle assignments for placed login objects. */
        WorldObjectCatalog m_object_catalog;

        /** IDs of map-object emitters kept alive during the previous frame. */
        std::unordered_set<std::uint32_t> m_live_effects;

        /** Prevents repeated attachment summaries while the shell is open. */
        bool m_effect_sync_reported = false;

        /** Next reserved SpriteFx entity number for map-object attachments. */
        std::uint32_t m_next_effect_serial = 1;
    };

}  // namespace openkal::client

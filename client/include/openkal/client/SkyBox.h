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
#include <cstddef>
#include <string>

namespace openkal::client {

    /** KalOnline compatibility behavior: six sample_sky_N textures around the camera (view
     * translation zeroed). */
    class SkyBox {
      public:
        /** Number of cube faces, in compatibility behavior initialization order. */
        static constexpr std::size_t kFaceCount = 6;

        /**
         * Builds the six face quads and loads sample_sky_0..5 from
         * OBJECTS/common/tex.
         * @param engine Engine used to create the meshes and textures.
         * @param data_root Client data root.
         * @return True when all six faces were loaded.
         */
        [[nodiscard]] bool load(engine::IEngine& engine, const std::string& data_root);

        /**
         * Releases every face mesh and texture.
         * @param engine Engine owning the handles.
         */
        void destroy(engine::IEngine& engine);

        /**
         * Draws the cube with depth off. The caller must set a view matrix with the
         * translation cleared so the cube stays locked to the camera.
         * @param engine Engine used to submit the draws.
         */
        void draw(engine::IEngine& engine) const;

        /** @return True once all six faces are loaded. */
        [[nodiscard]] bool ready() const {
            return m_ready;
        }

      private:
        /** Face quads in compatibility behavior order (front, back, left, right, top, down). */
        std::array<engine::MeshHandle, kFaceCount> m_faces{};

        /** Face textures, parallel to runtime state. */
        std::array<engine::TextureHandle, kFaceCount> m_textures{};

        /** True once all six faces are loaded. */
        bool m_ready = false;
    };

}  // namespace openkal::client

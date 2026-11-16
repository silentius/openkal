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

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace openkal::tools {

    /** Optional sampled motion and per-model rigid attachment bones for diagnostics. */
    struct SkinPreviewPose {
        /** Skeleton GB path; empty together with motion for the static bind pose. */
        std::string skeleton;
        /** Motion GB path, sampled with the normal runtime skinning functions. */
        std::string motion;
        /** Sample time in milliseconds. */
        float time_ms = 0.0f;
        /** Per-path rigid bone indices; -1 uses normal skinning. Empty skins every path. */
        std::vector<int> attachments;
    };

    /** Owns an offscreen preview session for static costume comparisons. */
    class SkinPreview final {
      public:
        /** Creates an inactive preview session. */
        SkinPreview() = default;

        /** Releases uploaded resources and the rendering context. */
        ~SkinPreview();

        /** Initializes the OpenGL rendering session.
         * @return True if a rendering context is available.
         */
        [[nodiscard]] bool initialize();

        /** Loads meshes with normal textures and an optional sampled runtime pose.
         * @param paths Model paths selected for one costume appearance.
         * @param pose Optional skeleton/motion and rigid attachment selection.
         * @return True if all selected models and textures loaded.
         */
        [[nodiscard]] bool load(const std::vector<std::string>& paths,
                                const SkinPreviewPose& pose = {});

        /** Captures a fixed front or back view with automatic framing.
         * @param path Destination PNG filename.
         * @param back True for the back view; false for the front view.
         * @param width Image width in pixels.
         * @param height Image height in pixels.
         * @return True if rendering and PNG writing succeeded.
         */
        [[nodiscard]] bool capture(const std::string& path, bool back, int width, int height);

        /** Describes the last load or rendering failure.
         * @return The last error message, empty after a successful load.
         */
        [[nodiscard]] const std::string& error() const;

      private:
        struct Part {
            engine::MeshHandle mesh;
            engine::TextureHandle texture;
            bool alpha_blend = false;
            bool alpha_test = false;
            bool two_sided = false;
            /** Keep glossy transparent parts on the normal-aware mesh path. */
            bool specular = false;
            std::vector<engine::VertexP3T2> transparent_vertices;
            std::vector<std::uint32_t> transparent_indices;
        };

        /** Releases the currently loaded preview resources. */
        void clear();

        std::unique_ptr<engine::IEngine> m_engine;
        bool m_initialized = false;
        std::vector<Part> m_parts;
        std::unordered_map<std::string, engine::TextureHandle> m_textures;
        engine::Vec3 m_center;
        engine::Vec3 m_extent;
        std::string m_error;
    };

}  // namespace openkal::tools

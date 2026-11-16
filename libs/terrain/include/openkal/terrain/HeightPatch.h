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
#include "openkal/terrain/EnvLoader.h"
#include "openkal/terrain/ImageRgba.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace openkal::terrain {

    /** Three-dimensional position. */
    struct Vec3 {
        /** X coordinate. */
        float x = 0;

        /** Y coordinate. */
        float y = 0;

        /** Z coordinate. */
        float z = 0;
    };

    /** Terrain mesh vertex. */
    struct Vertex {
        /** Vertex position. */
        Vec3 position;

        /** Horizontal texture coordinate. */
        float u = 0;

        /** Vertical texture coordinate. */
        float v = 0;
    };

    /** Prepared terrain layer data. */
    struct TerrainLayerGpu {
        /** RGBA splat texture for this pass. */
        ImageRgba color;

        /** Decoded detail texture. */
        ImageRgba detail;

        /** Detail-texture repetition factor. */
        float repeat = 1.0f;

        /** Whether this layer uses alpha blending. */
        bool alpha_blend = false;
    };

    /** Outdoor ROAM terrain patch. */
    struct HeightPatch {
        /** Horizontal patch index. */
        int patch_x = 0;

        /** Vertical patch index. */
        int patch_y = 0;

        /** File format version. */
        std::uint32_t version = 0;

        /** File checksum. */
        std::uint32_t checksum = 0;

        /** Per-cell layer indices. */
        std::vector<std::uint8_t> layers;

        /** Per-cell alpha values. */
        std::vector<std::uint8_t> alphas;

        /** Height samples. */
        std::vector<std::uint16_t> heights;

        /** Vertex-color RGB bytes. */
        std::vector<std::uint8_t> color_rgb;

        /** Decoration-table indices for the eight per-cell mask bits. */
        std::array<std::uint8_t, 8> deco_palette{};

        /** Per-cell decoration bit masks. */
        std::vector<std::uint8_t> deco_masks;

        /** Builds the patch mesh.
         * @param vertices Destination vertex list.
         * @param indices Destination index list.
         */
        void build_mesh(std::vector<Vertex>& vertices, std::vector<std::uint32_t>& indices) const;

        /** Builds layer splats and loads detail textures.
         * @param tex_dir Texture directory.
         * @param env_layers Environment layer metadata.
         * @param out Destination prepared layers.
         * @return True when all layers were prepared.
         */
        [[nodiscard]] bool prepare_layers(const std::string& tex_dir,
                                          const std::vector<LayerInfo>& env_layers,
                                          std::vector<TerrainLayerGpu>& out) const;

        /** Bakes a low-resolution diffuse preview.
         * @param tex_dir Texture directory.
         * @param env_layers Environment layer metadata.
         * @param out Destination image.
         * @param resolution Output width and height.
         * @return True when the image was baked.
         */
        [[nodiscard]] bool bake_diffuse(const std::string& tex_dir,
                                        const std::vector<LayerInfo>& env_layers,
                                        ImageRgba& out,
                                        int resolution = 512) const;
    };

    /** Loads KCM terrain patches. */
    class KcmLoader final {
      public:
        /** Loads a terrain patch.
         * @param path KCM file path.
         * @param out Destination patch.
         * @return True when the patch was loaded.
         */
        [[nodiscard]] static bool load(const std::string& path, HeightPatch& out);
    };

    /** Loads a terrain patch.
     * @param path KCM file path.
     * @param out Destination patch.
     * @return True when the patch was loaded.
     */
    [[nodiscard]] bool load_kcm(const std::string& path, HeightPatch& out);

}  // namespace openkal::terrain

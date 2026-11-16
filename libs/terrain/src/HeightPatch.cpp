/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/terrain/HeightPatch.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Log.h"
#include "openkal/terrain/GtxLoader.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>
#include <string>

namespace openkal::terrain {
    namespace {

        struct KcmHeader {
            std::uint32_t checksum = 0;
            std::uint32_t map_no = 0;
            std::uint32_t x = 0;
            std::uint32_t y = 0;
            std::uint32_t reserved[4]{};
            std::uint32_t version = 0;
        };

        constexpr int kMaxDecoPalette = 8;

        bool read_bytes(const std::vector<std::uint8_t>& src,
                        std::size_t& offset,
                        void* dst,
                        std::size_t nbytes) {
            if (offset + nbytes > src.size()) {
                return false;
            }
            std::memcpy(dst, src.data() + offset, nbytes);
            offset += nbytes;
            return true;
        }

        std::string find_gtx(const std::string& tex_dir, const std::string& stem) {
            const char* exts[] = {".gtx", ".GTX", ".dds", ".DDS"};
            for (const char* ext : exts) {
                const std::string path = platform::join_path(tex_dir, stem + ext);
                if (platform::file_exists(path)) {
                    return path;
                }
            }
            return {};
        }

        void sample_rgba(const ImageRgba& img, float u, float v, float out[4]) {
            if (img.width <= 0 || img.height <= 0 || img.rgba.empty()) {
                out[0] = out[1] = out[2] = 0.5f;
                out[3] = 1.0f;
                return;
            }
            // Wrap like D3DTADDRESS_WRAP.
            u -= std::floor(u);
            v -= std::floor(v);
            if (u < 0) {
                u += 1.0f;
            }
            if (v < 0) {
                v += 1.0f;
            }
            const float x = u * static_cast<float>(img.width);
            const float y = v * static_cast<float>(img.height);
            int x0 = static_cast<int>(x) % img.width;
            int y0 = static_cast<int>(y) % img.height;
            if (x0 < 0) {
                x0 += img.width;
            }
            if (y0 < 0) {
                y0 += img.height;
            }
            const std::size_t i =
                (static_cast<std::size_t>(y0) * img.width + static_cast<std::size_t>(x0)) * 4;
            out[0] = img.rgba[i + 0] / 255.0f;
            out[1] = img.rgba[i + 1] / 255.0f;
            out[2] = img.rgba[i + 2] / 255.0f;
            out[3] = img.rgba[i + 3] / 255.0f;
        }

    }  // namespace

    bool KcmLoader::load(const std::string& path, HeightPatch& out) {
        const auto bytes = platform::read_file(path);
        if (!bytes) {
            log::error("load_kcm: cannot read " + path);
            return false;
        }

        std::size_t offset = 0;
        KcmHeader header{};
        if (!read_bytes(*bytes, offset, &header, sizeof(header))) {
            log::error("load_kcm: truncated header: " + path);
            return false;
        }
        if (header.version < 4) {
            log::error("load_kcm: version too old (" + std::to_string(header.version) + ")");
            return false;
        }

        std::uint8_t layer_slots[map::kMapLayer];
        std::fill(std::begin(layer_slots), std::end(layer_slots), 0xFF);
        const std::size_t layer_bytes = header.version >= 5 ? 8u : 4u;
        if (!read_bytes(*bytes, offset, layer_slots, layer_bytes)) {
            return false;
        }

        if (header.version >= 6) {
            if (!read_bytes(*bytes, offset, out.deco_palette.data(), out.deco_palette.size())) {
                return false;
            }
            std::sort(out.deco_palette.begin(), out.deco_palette.end());
        } else {
            for (int i = 0; i < kMaxDecoPalette; ++i) {
                out.deco_palette[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(i);
            }
        }

        // Same as Patch::Load: sort layer indices, then count used slots.
        std::sort(layer_slots, layer_slots + map::kMapLayer);
        out.layers.clear();
        for (int i = 0; i < map::kMapLayer; ++i) {
            if (layer_slots[i] != 0xFF) {
                out.layers.push_back(layer_slots[i]);
            }
        }
        if (out.layers.empty()) {
            log::error("load_kcm: no terrain layers: " + path);
            return false;
        }

        const int n_layer = static_cast<int>(out.layers.size());
        const std::size_t map_size2 = static_cast<std::size_t>(map::kMapSize) * map::kMapSize;
        if (n_layer > 1) {
            const std::size_t alpha_bytes = map_size2 * static_cast<std::size_t>(n_layer - 1);
            if (offset + alpha_bytes > bytes->size()) {
                return false;
            }
            out.alphas.assign(bytes->data() + offset, bytes->data() + offset + alpha_bytes);
            offset += alpha_bytes;
        } else {
            out.alphas.clear();
        }

        const std::size_t height_count =
            static_cast<std::size_t>(map::kHeightSamples) * map::kHeightSamples;
        const std::size_t height_bytes = height_count * sizeof(std::uint16_t);
        if (offset + height_bytes > bytes->size()) {
            return false;
        }
        out.heights.resize(height_count);
        std::memcpy(out.heights.data(), bytes->data() + offset, height_bytes);
        offset += height_bytes;

        // Shared RGB block (MAP_SIZE2 * 3), then decolayer (MAP_SIZE2).
        if (offset + map_size2 * 3 <= bytes->size()) {
            out.color_rgb.assign(bytes->data() + offset, bytes->data() + offset + map_size2 * 3);
            offset += map_size2 * 3;
        } else {
            out.color_rgb.clear();
            out.deco_masks.clear();
            return false;
        }
        if (offset + map_size2 <= bytes->size()) {
            out.deco_masks.assign(bytes->data() + offset, bytes->data() + offset + map_size2);
        } else {
            out.deco_masks.clear();
        }

        out.patch_x = static_cast<int>(header.x);
        out.patch_y = static_cast<int>(header.y);
        out.version = header.version;
        out.checksum = header.checksum;
        return true;
    }

    bool load_kcm(const std::string& path, HeightPatch& out) {
        return KcmLoader::load(path, out);
    }

    void HeightPatch::build_mesh(std::vector<Vertex>& vertices,
                                 std::vector<std::uint32_t>& indices) const {
        vertices.clear();
        indices.clear();
        if (heights.size() != static_cast<std::size_t>(map::kHeightSamples) * map::kHeightSamples) {
            return;
        }

        const int x_off = (patch_x - map::kPatchGrid / 2) * map::kMapSize;
        const int y_off = (patch_y - map::kPatchGrid / 2) * map::kMapSize;
        const int n = map::kHeightSamples;
        vertices.reserve(static_cast<std::size_t>(n) * n);

        for (int z = 0; z < n; ++z) {
            for (int x = 0; x < n; ++x) {
                const std::uint16_t h =
                    heights[static_cast<std::size_t>(z) * n + static_cast<std::size_t>(x)];
                Vertex v;
                v.position.x = static_cast<float>((x + x_off) * map::kTerrainCellSize);
                v.position.y = static_cast<float>(h);
                v.position.z = static_cast<float>((z + y_off) * map::kTerrainCellSize);
                // Patch-local UVs 0..1 for the baked diffuse.
                v.u = static_cast<float>(x) / static_cast<float>(map::kMapSize);
                v.v = static_cast<float>(z) / static_cast<float>(map::kMapSize);
                vertices.push_back(v);
            }
        }

        indices.reserve(static_cast<std::size_t>(map::kMapSize) * map::kMapSize * 6);
        for (int z = 0; z < map::kMapSize; ++z) {
            for (int x = 0; x < map::kMapSize; ++x) {
                const std::uint32_t i0 = static_cast<std::uint32_t>(z * n + x);
                const std::uint32_t i1 = i0 + 1;
                const std::uint32_t i2 = i0 + static_cast<std::uint32_t>(n);
                const std::uint32_t i3 = i2 + 1;
                indices.push_back(i0);
                indices.push_back(i2);
                indices.push_back(i1);
                indices.push_back(i1);
                indices.push_back(i2);
                indices.push_back(i3);
            }
        }
    }

    bool HeightPatch::prepare_layers(const std::string& tex_dir,
                                     const std::vector<LayerInfo>& env_layers,
                                     std::vector<TerrainLayerGpu>& out) const {
        out.clear();
        if (layers.empty()) {
            return false;
        }

        const std::size_t map_size2 = static_cast<std::size_t>(map::kMapSize) * map::kMapSize;
        const bool have_rgb = color_rgb.size() >= map_size2 * 3;

        // Client builds runtime state[layer] for layer=1..n-1, then runtime state[0] =
        // runtime state[1]. Each color texel: B,G,R from shared block + A from that
        // layer's alpha.
        auto build_color = [&](std::size_t alpha_layer_index /*1..n-1*/, ImageRgba& img) {
            img.width = img.height = map::kMapSize;
            img.rgba.assign(map_size2 * 4, 255);
            const std::uint8_t* alpha =
                (alpha_layer_index >= 1 && alphas.size() >= map_size2 * alpha_layer_index)
                    ? alphas.data() + map_size2 * (alpha_layer_index - 1)
                    : nullptr;
            for (std::size_t i = 0; i < map_size2; ++i) {
                std::uint8_t r = 255, g = 255, b = 255, a = 255;
                if (have_rgb) {
                    b = color_rgb[i * 3 + 0];
                    g = color_rgb[i * 3 + 1];
                    r = color_rgb[i * 3 + 2];
                }
                if (alpha) {
                    a = alpha[i];
                }
                img.rgba[i * 4 + 0] = r;
                img.rgba[i * 4 + 1] = g;
                img.rgba[i * 4 + 2] = b;
                img.rgba[i * 4 + 3] = a;
            }
        };

        ImageRgba color_for_upper;
        if (layers.size() >= 2) {
            build_color(1, color_for_upper);
        } else {
            build_color(0, color_for_upper);  // solid RGB, A=255
        }

        for (std::size_t li = 0; li < layers.size(); ++li) {
            TerrainLayerGpu layer;
            layer.alpha_blend = (li > 0);
            if (li == 0) {
                // Base pass uses same color map as layer 1 (roam.cpp: runtime state[0] =
                // runtime state[1]).
                layer.color = color_for_upper;
            } else {
                build_color(li, layer.color);
            }

            const unsigned idx = layers[li];
            if (idx < env_layers.size()) {
                layer.repeat = env_layers[idx].scale_u > 0
                                   ? static_cast<float>(env_layers[idx].scale_u)
                                   : 1.0f;
                const std::string path = find_gtx(tex_dir, env_layers[idx].texture_name);
                if (path.empty() || !load_gtx_rgba(path, layer.detail)) {
                    log::warn("prepare_layers: missing detail " + env_layers[idx].texture_name);
                    layer.detail.width = layer.detail.height = 1;
                    layer.detail.rgba = {128, 128, 128, 255};
                }
            } else {
                layer.repeat = 1.0f;
                layer.detail.width = layer.detail.height = 1;
                layer.detail.rgba = {128, 128, 128, 255};
            }
            out.push_back(std::move(layer));
        }
        return !out.empty();
    }

    bool HeightPatch::bake_diffuse(const std::string& tex_dir,
                                   const std::vector<LayerInfo>& env_layers,
                                   ImageRgba& out,
                                   int resolution) const {
        if (layers.empty() || resolution <= 0) {
            return false;
        }

        struct LayerTex {
            float repeat = 1.0f;
            ImageRgba image;
        };
        std::vector<LayerTex> layer_tex(layers.size());

        for (std::size_t i = 0; i < layers.size(); ++i) {
            const unsigned idx = layers[i];
            if (idx >= env_layers.size()) {
                log::warn("bake_diffuse: layer index " + std::to_string(idx) + " missing in n.env");
                continue;
            }
            const LayerInfo& info = env_layers[idx];
            layer_tex[i].repeat = info.scale_u > 0 ? static_cast<float>(info.scale_u) : 1.0f;
            const std::string path = find_gtx(tex_dir, info.texture_name);
            if (path.empty() || !load_gtx_rgba(path, layer_tex[i].image)) {
                log::warn("bake_diffuse: missing texture " + info.texture_name);
            }
        }

        const std::size_t map_size2 = static_cast<std::size_t>(map::kMapSize) * map::kMapSize;

        auto sample_color_rgb = [&](float u, float v, float rgb[3]) {
            rgb[0] = rgb[1] = rgb[2] = 1.0f;
            if (color_rgb.size() < map_size2 * 3) {
                return;
            }
            const int cx =
                std::min(map::kMapSize - 1, std::max(0, static_cast<int>(u * map::kMapSize)));
            const int cy =
                std::min(map::kMapSize - 1, std::max(0, static_cast<int>(v * map::kMapSize)));
            const std::size_t ci =
                (static_cast<std::size_t>(cy) * map::kMapSize + static_cast<std::size_t>(cx)) * 3;
            // D3D LockRect writes file bytes as B,G,R,A into A8R8G8B8.
            const float b = color_rgb[ci + 0] / 255.0f;
            const float g = color_rgb[ci + 1] / 255.0f;
            const float r = color_rgb[ci + 2] / 255.0f;
            if (r + g + b > 0.02f) {
                rgb[0] = r;
                rgb[1] = g;
                rgb[2] = b;
            }
        };

        auto sample_alpha = [&](std::size_t layer_i, float u, float v) -> float {
            if (layer_i == 0 || alphas.size() < map_size2 * layer_i) {
                return 0.0f;
            }
            const int ax =
                std::min(map::kMapSize - 1, std::max(0, static_cast<int>(u * map::kMapSize)));
            const int ay =
                std::min(map::kMapSize - 1, std::max(0, static_cast<int>(v * map::kMapSize)));
            const std::uint8_t* plane = alphas.data() + map_size2 * (layer_i - 1);
            return plane[static_cast<std::size_t>(ay) * map::kMapSize +
                         static_cast<std::size_t>(ax)] /
                   255.0f;
        };

        // Detail UVs follow Patch::Init *early* runtime state (before runtime state
        // rewrite):
        //   u = local/MAP_SIZE, v = 1 - local/MAP_SIZE  (negative _32 flips V).
        auto sample_detail = [&](std::size_t li, float u_local, float v_local, float out_rgb[3]) {
            out_rgb[0] = out_rgb[1] = out_rgb[2] = 0.5f;
            if (li >= layer_tex.size() || layer_tex[li].image.rgba.empty()) {
                return;
            }
            const float rep = layer_tex[li].repeat;
            float tmp[4];
            sample_rgba(layer_tex[li].image, u_local * rep, (1.0f - v_local) * rep, tmp);
            out_rgb[0] = tmp[0];
            out_rgb[1] = tmp[1];
            out_rgb[2] = tmp[2];
        };

        out.width = resolution;
        out.height = resolution;
        out.rgba.assign(static_cast<std::size_t>(resolution) * resolution * 4, 0);

        for (int y = 0; y < resolution; ++y) {
            for (int x = 0; x < resolution; ++x) {
                const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(resolution);
                const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(resolution);

                float color[3];
                sample_color_rgb(u, v, color);

                float detail0[3];
                sample_detail(0, u, v, detail0);

                // compatibility behavior MODULATE(color, light) then MODULATE2X(detail, current)
                // with white light.
                float rgb[3] = {
                    std::min(1.0f, 2.0f * detail0[0] * color[0]),
                    std::min(1.0f, 2.0f * detail0[1] * color[1]),
                    std::min(1.0f, 2.0f * detail0[2] * color[2]),
                };

                for (std::size_t li = 1; li < layers.size(); ++li) {
                    const float a = sample_alpha(li, u, v);
                    if (a <= 0.001f) {
                        continue;
                    }
                    float detail[3];
                    sample_detail(li, u, v, detail);
                    const float layer_rgb[3] = {
                        std::min(1.0f, 2.0f * detail[0] * color[0]),
                        std::min(1.0f, 2.0f * detail[1] * color[1]),
                        std::min(1.0f, 2.0f * detail[2] * color[2]),
                    };
                    for (int c = 0; c < 3; ++c) {
                        rgb[c] = rgb[c] * (1.0f - a) + layer_rgb[c] * a;
                    }
                }

                const std::size_t di =
                    (static_cast<std::size_t>(y) * resolution + static_cast<std::size_t>(x)) * 4;
                out.rgba[di + 0] =
                    static_cast<std::uint8_t>(std::clamp(rgb[0], 0.0f, 1.0f) * 255.0f);
                out.rgba[di + 1] =
                    static_cast<std::uint8_t>(std::clamp(rgb[1], 0.0f, 1.0f) * 255.0f);
                out.rgba[di + 2] =
                    static_cast<std::uint8_t>(std::clamp(rgb[2], 0.0f, 1.0f) * 255.0f);
                out.rgba[di + 3] = 255;
            }
        }
        return true;
    }

}  // namespace openkal::terrain

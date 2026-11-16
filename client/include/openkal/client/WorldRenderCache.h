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

#include "openkal/client/NpcCatalog.h"
#include "openkal/client/Player.h"
#include "openkal/engine/engine.h"
#include "openkal/terrain/EnvLoader.h"
#include "openkal/terrain/GbModel.h"
#include "openkal/terrain/HeightPatch.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace openkal::client {

    /** Six frustum planes from a column-major view-projection matrix. */
    class FrustumPlanes {
      public:
        /** Plane coefficients A,B,C,D for plane i in [0,6). */
        float p[6][4]{};
    };

    /**
     * World GPU cache helpers: terrain patches, OPL props, NPC/player clothes
     * meshes, frustum tests.
     */
    class WorldRenderCache {
      public:
        /** One terrain splat layer uploaded to the GPU. */
        class GpuLayer {
          public:
            engine::TextureHandle color{};
            engine::TextureHandle detail{};
            float repeat = 1.0f;
            bool alpha_blend = false;
            bool owns_detail = false;
        };

        /** Uploaded KCM patch mesh plus its splat layers. */
        class DrawnPatch {
          public:
            engine::MeshHandle mesh{};
            std::vector<GpuLayer> layers;

            /** Outdoor patch X this mesh was uploaded from. */
            int patch_x = 0;

            /** Outdoor patch Y this mesh was uploaded from. */
            int patch_y = 0;

            /** Decoration-table indices for each bit in deco_masks. */
            std::array<std::uint8_t, 8> deco_palette{};

            /** Per-cell decoration masks retained for vegetation rendering. */
            std::vector<std::uint8_t> deco_masks;

            /** Height samples retained for vegetation placement. */
            std::vector<std::uint16_t> heights;

            /** Floating-point KCM heights used by projected shadows. */
            engine::TextureHandle shadow_height_map{};
        };

        /** compatibility behavior-side terrain data ready for a render-thread GPU upload. */
        class PreparedPatch {
          public:
            /** Parsed terrain patch, retained for floor collision after upload. */
            terrain::HeightPatch patch;

            /** Decoded terrain splat and detail layers. */
            std::vector<terrain::TerrainLayerGpu> layers;

            /** Terrain vertices prepared without an OpenGL context. */
            std::vector<terrain::Vertex> vertices;

            /** Terrain triangle indices prepared without an OpenGL context. */
            std::vector<std::uint32_t> indices;

            /** Cache keys for the decoded terrain detail textures. */
            std::vector<std::string> detail_keys;
        };

        /** One prop/NPC submesh on the GPU. */
        class GpuSubmesh {
          public:
            engine::MeshHandle mesh{};
            engine::TextureHandle texture{};
            engine::TextureHandle lightmap{};
            bool alpha_blend = false;
            bool alpha_test = false;
            bool two_sided = false;
            terrain::GbMaterial uv_material{};
            std::vector<std::uint16_t> uv_key_times;
            bool procedural_water_scroll = false;
            bool procedural_waterfall_scroll = false;
        };

        /** Cached multi-part GPU model. */
        class GpuModel {
          public:
            std::vector<GpuSubmesh> parts;

            /** Idle-pose EFFECTBONE_HEAD Y in model space, valid when has_name_anchor.
             */
            float name_local_y = 0.0f;

            /** True when name_local_y was evaluated from the skeleton. */
            bool has_name_anchor = false;

            /** True when the uploaded meshes stored a usable model-space AABB. */
            bool has_bounds = false;

            /** Model-space AABB minimum X. */
            float min_x = 0.0f;

            /** Model-space AABB minimum Y. */
            float min_y = 0.0f;

            /** Model-space AABB minimum Z. */
            float min_z = 0.0f;

            /** Model-space AABB maximum X. */
            float max_x = 0.0f;

            /** Model-space AABB maximum Y. */
            float max_y = 0.0f;

            /** Model-space AABB maximum Z. */
            float max_z = 0.0f;
        };

        /** One placed prop instance referencing a cached GpuModel. */
        class DrawnProp {
          public:
            engine::Mat4 model{};
            const GpuModel* gpu = nullptr;
            float wx = 0;
            float wy = 0;
            float wz = 0;

            /** World AABB half-extent X; 0 uses cull_radius sphere instead. */
            float cull_hx = 0;

            /** World AABB half-extent Y. */
            float cull_hy = 0;

            /** World AABB half-extent Z. */
            float cull_hz = 0;

            /** Bounding-sphere radius when cull_hx is 0. */
            float cull_radius = 400.0f;

            /** Outdoor patch X this instance was streamed from. */
            int patch_x = 0;

            /** Outdoor patch Y this instance was streamed from. */
            int patch_y = 0;

            /** True when this instance is drawn with a polygon offset (indoor objects
             * sitting on region floors).
             */
            bool polygon_offset = false;

            /** False hides this instance (indoor PVS). Outdoor props stay true. */
            bool draw = true;

            /** Resolved GB asset path used for render-category filtering. */
            std::string asset_path;

            /** object.dat / world_objects.json lookup key; empty when none. */
            std::string catalog_key;

            /** SpriteFx entity in the 0xE0000000 world-prop range, or 0. */
            std::uint32_t fx_entity = 0;
        };

        /**
         * Fills world AABB cull fields from the GB model bounds and instance matrix.
         * Skipped when cull_hx is already set (indoor region boxes) or the GB has no
         * usable AABB. wx/wy/wz become the world AABB center.
         * @param p Instance to update.
         */
        static void apply_instance_bounds(DrawnProp& p);

        /** Identifies tree and large-foliage assets by their GB path.
         * @param path Resolved or client-relative GB path.
         * @return True for trees, bushes, and leafy world props.
         */
        [[nodiscard]] static bool is_tree_asset_path(std::string_view path);

        /** Extracts normalized frustum planes from a view-projection matrix. */
        static void extract_frustum_planes(const engine::Mat4& vp, FrustumPlanes& out);

        /** @return True when a sphere intersects the frustum. */
        [[nodiscard]] static bool
        frustum_contains_sphere(const FrustumPlanes& fr, float x, float y, float z, float radius);

        /** @return True when an AABB intersects the frustum. */
        [[nodiscard]] static bool frustum_contains_aabb(const FrustumPlanes& fr,
                                                        float min_x,
                                                        float min_y,
                                                        float min_z,
                                                        float max_x,
                                                        float max_y,
                                                        float max_z);

        /** Squared distance from a 2D point to a segment. */
        [[nodiscard]] static float
        point_segment_distance_sq(float px, float py, float ax, float ay, float bx, float by);

        /** Samples animated UV offsets for a prop submesh. */
        static void sample_prop_uv(
            const GpuSubmesh& part, float time_sec, float& uv_u, float& uv_v, float& opacity);

        /** Draws one uploaded GB submesh, using the lightmap path when present.
         * @param engine Renderer.
         * @param part Uploaded submesh.
         * @param model Instance matrix.
         * @param alpha_blend Soft blend.
         * @param alpha_test Cutout test.
         * @param uv_u Diffuse UV scroll U.
         * @param uv_v Diffuse UV scroll V.
         * @param opacity Material opacity.
         */
        static void draw_prop_part(engine::IEngine& engine,
                                   const GpuSubmesh& part,
                                   const engine::Mat4& model,
                                   bool alpha_blend,
                                   bool alpha_test,
                                   float uv_u = 0.0f,
                                   float uv_v = 0.0f,
                                   float opacity = 1.0f);

        /** Builds n_XXX_YYY.kcm path under maps_dir. */
        [[nodiscard]] static std::string patch_path(const std::string& maps_dir, int x, int y);

        /** Resolves an OPL asset name to a .gb path under data_root. */
        [[nodiscard]] static std::string opl_name_to_gb_path(const std::string& data_root,
                                                             const std::string& opl_name);

        /** Loads or returns a cached .gb prop model. */
        [[nodiscard]] static const GpuModel*
        get_or_load_gb(engine::IEngine& engine,
                       const std::string& gb_path,
                       std::unordered_map<std::string, GpuModel>& model_cache,
                       std::unordered_map<std::string, engine::TextureHandle>& tex_cache);

        /** Loads or returns a cached NPC clothes model for a shape. */
        [[nodiscard]] static const GpuModel*
        get_or_load_npc_shape(engine::IEngine& engine,
                              const std::string& data_root,
                              int shape,
                              std::unordered_map<std::string, GpuModel>& model_cache,
                              std::unordered_map<std::string, engine::TextureHandle>& tex_cache,
                              const NpcCatalog& npc_catalog);

        /** Loads or returns a cached default player-class clothes model. */
        [[nodiscard]] static const GpuModel*
        get_or_load_player_class(engine::IEngine& engine,
                                 const std::string& data_root,
                                 Class clazz,
                                 std::unordered_map<std::string, GpuModel>& model_cache,
                                 std::unordered_map<std::string, engine::TextureHandle>& tex_cache);

        /**
         * Decodes textures and builds mesh data for one terrain patch without using the GPU.
         * @param patch Parsed terrain patch to prepare.
         * @param tex_dir Directory containing terrain detail textures.
         * @param env_layers Terrain layer definitions.
         * @return Prepared patch, or an empty value when the patch has no drawable layers.
         */
        [[nodiscard]] static std::optional<PreparedPatch>
        prepare_patch(terrain::HeightPatch patch,
                      const std::string& tex_dir,
                      const std::vector<terrain::LayerInfo>& env_layers);

        /**
         * Uploads compatibility behavior-prepared terrain data while the render context is current.
         * @param engine Rendering engine owning the active GPU context.
         * @param patch compatibility behavior-prepared terrain patch.
         * @param detail_cache Shared terrain detail-texture cache.
         * @return GPU resources for the uploaded patch, or an empty patch on failure.
         */
        [[nodiscard]] static DrawnPatch
        upload_prepared_patch(engine::IEngine& engine,
                              const PreparedPatch& patch,
                              std::unordered_map<std::string, engine::TextureHandle>& detail_cache);

        /** Uploads one HeightPatch to the GPU. */
        [[nodiscard]] static DrawnPatch
        upload_patch(engine::IEngine& engine,
                     terrain::HeightPatch& patch,
                     const std::string& tex_dir,
                     const std::vector<terrain::LayerInfo>& env_layers,
                     std::unordered_map<std::string, engine::TextureHandle>& detail_cache);
    };

}  // namespace openkal::client

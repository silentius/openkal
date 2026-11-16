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

#include "openkal/terrain/ImageRgba.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace openkal::terrain {

    /** GB mesh vertex encoding. */
    enum class GbVertexType : std::uint8_t {
        Rigid = 0,
        Blend1 = 1,
        Blend2 = 2,
        Blend3 = 3,
        Blend4 = 4,
        RigidDouble = 5,
    };

    /** GB mesh index encoding. */
    enum class GbFaceType : std::uint8_t {
        List = 0,
        Strip = 1,
    };

    /** UV and opacity values from a GB material frame. */
    struct GbMaterialFrame {
        /** Material opacity. */
        float opacity = 1.0f;

        /** Horizontal texture offset. */
        float offset_u = 0.0f;

        /** Vertical texture offset. */
        float offset_v = 0.0f;

        /** Packed material colors; the specular channels may encode control data. */
        std::uint32_t ambient = 0, diffuse = 0, specular = 0;
    };

    /** GB material description. */
    struct GbMaterial {
        /** Referenced texture name. */
        std::string texture;

        /** Lightmap texture name (next C-string after the diffuse name). */
        std::string lightmap;

        /** Material option string. */
        std::string option;

        /** Material-map option flags. */
        std::uint16_t mapping_flags = 0;

        /** Stored highlight exponent, independent of the packed frame colors. */
        float specular_power = 0.0f;

        /** Per-keyframe material state. */
        std::vector<GbMaterialFrame> frames;
    };

    /** Checks whether a texture is a lake or river surface.
     * @param name Texture name.
     * @return True for water-surface textures.
     */
    [[nodiscard]] bool is_water_surface_texture(const std::string& name);

    /** Checks whether a texture is a waterfall.
     * @param name Texture name.
     * @return True for waterfall textures.
     */
    [[nodiscard]] bool is_waterfall_texture(const std::string& name);

    /** Samples animated material UV and opacity.
     * @param mat Material to sample.
     * @param key_times Material key times.
     * @param time_ms Sample time in milliseconds.
     * @param out_u Destination horizontal offset.
     * @param out_v Destination vertical offset.
     * @param out_opacity Destination opacity.
     * @return True when the material has animation data.
     */
    [[nodiscard]] bool sample_material_uv(const GbMaterial& mat,
                                          const std::vector<std::uint16_t>& key_times,
                                          float time_ms,
                                          float& out_u,
                                          float& out_v,
                                          float& out_opacity);

    /** Bind-pose vertex used for compatibility behavior skinning. */
    struct GbSkinVert {
        /** Bind-pose position. */
        float px = 0, py = 0, pz = 0;

        /** Texture coordinates. */
        float u = 0, v = 0;

        /** Explicit blend weights. */
        float w[3] = {0, 0, 0};

        /** Mesh-local bone palette indices. */
        std::uint8_t idx[4] = {};

        /** Number of active bone influences. */
        int influence_count = 0;

        /** Bind-pose surface normal. */
        std::array<float, 3> normal{};
    };

    /** compatibility behavior-side GB mesh data. */
    struct GbMeshCpu {
        /** Mesh name from the GB string table (e.g. effect_00). */
        std::string name;

        /** Material index. */
        int material_index = 0;

        /** Vertex encoding. */
        GbVertexType vertex_layout = GbVertexType::Rigid;

        /** Upload-ready position and UV values. */
        std::vector<float> pos_uv;

        /** Surface normals, one per vertex, in the same space as pos_uv. */
        std::vector<std::array<float, 3>> normals;

        /** Highlight exponent; zero disables the optional specular pass. */
        float specular_power = 0.0f;

        /** Second UV set for baked lightmaps (two floats per vertex). Empty when
         * the mesh is not VT_RIGID_DOUBLE. */
        std::vector<float> lightmap_uv;

        /** Lightmap texture name resolved from the GB string table. */
        std::string lightmap;

        /** Bind data used to skin the mesh again. */
        std::vector<GbSkinVert> skin_verts;

        /** Mesh-local to model-global bone mapping. */
        std::vector<std::uint8_t> bone_palette;

        /** Triangle indices. */
        std::vector<std::uint32_t> indices;

        /** Whether both face orientations are rendered. */
        bool two_sided = false;

        /** Whether the material uses alpha testing. */
        bool alpha_test = false;

        /** Whether the material uses alpha blending. */
        bool alpha_blend = false;
    };

    /** Transforms mesh normals by an inverse-transpose matrix and normalizes them.
     * Singular transforms produce zero normals, disabling their highlights.
     * @param mesh Mesh whose current normals are updated.
     * @param transform Column-major local-to-target transform.
     */
    void transform_mesh_normals(GbMeshCpu& mesh, const std::array<float, 16>& transform);

    /** GB skeleton bone. */
    struct GbBone {
        /** Inverse bind matrix. */
        std::array<float, 16> inverse{};

        /** Parent bone index, or 255 for a root. */
        std::uint8_t parent = 255;
    };

    /** One local animation transform sample. */
    struct GbAnimSample {
        /** Translation components. */
        float px = 0, py = 0, pz = 0;

        /** Rotation quaternion components. */
        float qx = 0, qy = 0, qz = 0, qw = 1;

        /** Scale components. */
        float sx = 1, sy = 1, sz = 1;
    };

    /** Animation clip containing key times and per-bone samples. */
    struct GbAnimClip {
        /** Number of keyframes. */
        int keyframe_count = 0;

        /** Number of sampled bones. */
        int bone_count = 0;

        /** Keyframe times in milliseconds. */
        std::vector<std::uint16_t> key_times;

        /** Per-keyframe option strings (compatibility behavior), parallel to
         * key_times. */
        std::vector<std::string> key_options;

        /** Samples indexed by keyframe then bone. */
        std::vector<GbAnimSample> samples;

        /** Keyframe-zero samples for compatibility and static idle. */
        std::vector<GbAnimSample> bone0;

        /** Loop restart time in milliseconds, read from clip or keyframe options. */
        float loop_start_ms = 0.0f;

        /** Returns the clip duration in milliseconds.
         * @return Clip duration.
         */
        [[nodiscard]] float duration_ms() const {
            if (key_times.empty()) {
                return 0.0f;
            }
            return static_cast<float>(key_times.back());
        }
    };

    /** Parsed GB model data. */
    struct GbModel {
        /** Source file path. */
        std::string path;

        /** Model option string. */
        std::string option;

        /** Model materials. */
        std::vector<GbMaterial> materials;

        /** Model meshes. */
        std::vector<GbMeshCpu> meshes;

        /** Skeleton bones. */
        std::vector<GbBone> bones;

        /** Animation clips. */
        std::vector<GbAnimClip> anims;

        /** Key times shared by animated material frames. */
        std::vector<std::uint16_t> material_key_times;

        /** Bone count from the file header. */
        std::uint8_t bone_count = 0;

        /** Model minimum bounds. */
        float min_x = 0, min_y = 0, min_z = 0;

        /** Model maximum bounds. */
        float max_x = 0, max_y = 0, max_z = 0;

        /**
         * Collision (compatibility behavior) triangles in model space, nine floats each.
         *
         * Outdoor GetFloor stands on these, not the render mesh. Empty when
         * the GB has no compatibility behavior blob. Collision-only props (bridge planks) have
         * compatibility behavior and no meshes.
         */
        std::vector<float> cls_tris;
    };

    /** Loads GB models and their textures. */
    class GbLoader final {
      public:
        /** Loads a GB model.
         * @param path GB file path.
         * @param out Destination model.
         * @return True when the model was loaded.
         */
        [[nodiscard]] static bool load(const std::string& path, GbModel& out);

        /** Loads a model texture.
         * @param model_gb_path Path of the referencing GB model.
         * @param texture_name Referenced texture name.
         * @param out Destination image.
         * @return True when the texture was loaded.
         */
        [[nodiscard]] static bool load_texture(const std::string& model_gb_path,
                                               const std::string& texture_name,
                                               ImageRgba& out);
    };

    /** Provides GB skeleton evaluation and compatibility behavior skinning operations. */
    class GbSkin final {
      public:
        /** Evaluates skin matrices at keyframe zero.
         * @param skeleton_and_or_anim Source model.
         * @param anim_index Animation index.
         * @param skin_out Destination skin matrices.
         * @return True when matrices were evaluated.
         */
        [[nodiscard]] static bool eval_matrices_kf0(const GbModel& skeleton_and_or_anim,
                                                    int anim_index,
                                                    std::vector<std::array<float, 16>>& skin_out);

        /** Evaluates skin matrices at an animation time.
         * @param bones Skeleton bones.
         * @param clip Animation clip.
         * @param time_ms Sample time in milliseconds.
         * @param loop Whether to loop the animation.
         * @param skin_out Destination skin matrices.
         * @return True when matrices were evaluated.
         */
        [[nodiscard]] static bool
        eval_matrices_at_time(const std::vector<GbBone>& bones,
                              const GbAnimClip& clip,
                              float time_ms,
                              bool loop,
                              std::vector<std::array<float, 16>>& skin_out);

        /** Evaluates bone-world and skin matrices at an animation time.
         * @param bones Skeleton bones.
         * @param clip Animation clip.
         * @param time_ms Sample time in milliseconds.
         * @param loop Whether to loop the animation.
         * @param bone_world_out Destination bone-world matrices.
         * @param skin_out Destination skin matrices.
         * @return True when matrices were evaluated.
         */
        [[nodiscard]] static bool
        eval_bone_and_skin_matrices_at_time(const std::vector<GbBone>& bones,
                                            const GbAnimClip& clip,
                                            float time_ms,
                                            bool loop,
                                            std::vector<std::array<float, 16>>& bone_world_out,
                                            std::vector<std::array<float, 16>>& skin_out);

        /** compatibility behavior-skins one mesh.
         * @param mesh Mesh to skin.
         * @param skin Skin matrices.
         * @param skin_normals Transform normals; false preserves them for position-only consumers.
         */
        static void skin_mesh(GbMeshCpu& mesh,
                              const std::vector<std::array<float, 16>>& skin,
                              bool skin_normals = true);

        /** Applies the embedded idle animation.
         * @param model Model to update.
         */
        static void apply_embedded_idle(GbModel& model);

        /** Skins clothes with a separate skeleton and motion.
         * @param clothes Clothes model to update.
         * @param skeleton Skeleton model.
         * @param motion Motion model.
         * @return True when skinning succeeded.
         */
        [[nodiscard]] static bool
        skin_with_skeleton_anim(GbModel& clothes, const GbModel& skeleton, const GbModel& motion);
    };

    /** Loads a GB model.
     * @param path GB file path.
     * @param out Destination model.
     * @return True when the model was loaded.
     */
    [[nodiscard]] bool load_gb(const std::string& path, GbModel& out);

    /** Loads a model texture.
     * @param model_gb_path Path of the referencing GB model.
     * @param texture_name Referenced texture name.
     * @param out Destination image.
     * @return True when the texture was loaded.
     */
    [[nodiscard]] bool load_gb_texture(const std::string& model_gb_path,
                                       const std::string& texture_name,
                                       ImageRgba& out);

    /** Evaluates skin matrices at keyframe zero.
     * @param skeleton_and_or_anim Source model.
     * @param anim_index Animation index.
     * @param skin_out Destination skin matrices.
     * @return True when matrices were evaluated.
     */
    [[nodiscard]] bool eval_skin_matrices_kf0(const GbModel& skeleton_and_or_anim,
                                              int anim_index,
                                              std::vector<std::array<float, 16>>& skin_out);

    /** Evaluates skin matrices at an animation time.
     * @param bones Skeleton bones.
     * @param clip Animation clip.
     * @param time_ms Sample time in milliseconds.
     * @param loop Whether to loop the animation.
     * @param skin_out Destination skin matrices.
     * @return True when matrices were evaluated.
     */
    [[nodiscard]] bool eval_skin_matrices_at_time(const std::vector<GbBone>& bones,
                                                  const GbAnimClip& clip,
                                                  float time_ms,
                                                  bool loop,
                                                  std::vector<std::array<float, 16>>& skin_out);

    /** Evaluates bone-world and skin matrices at an animation time.
     * @param bones Skeleton bones.
     * @param clip Animation clip.
     * @param time_ms Sample time in milliseconds.
     * @param loop Whether to loop the animation.
     * @param bone_world_out Destination bone-world matrices.
     * @param skin_out Destination skin matrices.
     * @return True when matrices were evaluated.
     */
    [[nodiscard]] bool
    eval_bone_and_skin_matrices_at_time(const std::vector<GbBone>& bones,
                                        const GbAnimClip& clip,
                                        float time_ms,
                                        bool loop,
                                        std::vector<std::array<float, 16>>& bone_world_out,
                                        std::vector<std::array<float, 16>>& skin_out);

    /** compatibility behavior-skins one mesh.
     * @param mesh Mesh to skin.
     * @param skin Skin matrices.
     * @param skin_normals Transform normals; false preserves them for position-only consumers.
     */
    void skin_mesh(GbMeshCpu& mesh,
                   const std::vector<std::array<float, 16>>& skin,
                   bool skin_normals = true);

    /** Applies the embedded idle animation.
     * @param model Model to update.
     */
    void apply_embedded_idle(GbModel& model);

    /** Skins clothes with a separate skeleton and motion.
     * @param clothes Clothes model to update.
     * @param skeleton Skeleton model.
     * @param motion Motion model.
     * @return True when skinning succeeded.
     */
    [[nodiscard]] bool
    skin_with_skeleton_anim(GbModel& clothes, const GbModel& skeleton, const GbModel& motion);

    /** Two-sided material flag. */
    inline constexpr std::uint16_t kMatTwoSided = 1;

    /** Opacity-map material flag. */
    inline constexpr std::uint16_t kMatMapOpacity = 2;

    /** Alpha-RGB material flag. */
    inline constexpr std::uint16_t kMatMapAlphaRgb = 4;

    /** Enables directional specular highlights using the stored exponent. */
    inline constexpr std::uint16_t kMatSpecular = 8;

    /** Baked lightmap material flag (MATERIAL_LIGHTMAP). */
    inline constexpr std::uint16_t kMatLightmap = 0x100;

    /** Model skeleton flag. */
    inline constexpr std::uint8_t kModelBoneFlag = 1;

}  // namespace openkal::terrain

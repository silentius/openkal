/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/WorldPick.h"

#include <algorithm>
#include <cmath>

namespace openkal::client {
    namespace {
        constexpr float kPi = 3.14159265358979323846f;

        [[nodiscard]] bool build_screen_ray(const FollowCamera& cam,
                                            const LocalPlayer& pl,
                                            float mx,
                                            float my,
                                            float sw,
                                            float sh,
                                            engine::Vec3& out_eye,
                                            engine::Vec3& out_dir) {
            if (sw < 1.0f || sh < 1.0f) {
                return false;
            }
            const float aspect = sw / sh;
            const float fov = 60.0f * kPi / 180.0f;
            const auto proj = engine::Mat4::perspective(fov, aspect, kNearZ, cam.clip_far());
            const auto view = cam.view(pl);
            const auto vp = engine::Mat4::multiply(proj, view);
            engine::Mat4 inv_vp;
            if (!engine::Mat4::invert(vp, inv_vp)) {
                return false;
            }

            const float ndc_x = (mx / sw) * 2.0f - 1.0f;
            const float ndc_y = 1.0f - (my / sh) * 2.0f;

            auto unproject = [&](float z_ndc) {
                const float x =
                    inv_vp.m[0] * ndc_x + inv_vp.m[4] * ndc_y + inv_vp.m[8] * z_ndc + inv_vp.m[12];
                const float y =
                    inv_vp.m[1] * ndc_x + inv_vp.m[5] * ndc_y + inv_vp.m[9] * z_ndc + inv_vp.m[13];
                const float z =
                    inv_vp.m[2] * ndc_x + inv_vp.m[6] * ndc_y + inv_vp.m[10] * z_ndc + inv_vp.m[14];
                const float w =
                    inv_vp.m[3] * ndc_x + inv_vp.m[7] * ndc_y + inv_vp.m[11] * z_ndc + inv_vp.m[15];
                const float iw = (std::fabs(w) > 1e-8f) ? (1.0f / w) : 1.0f;
                return engine::Vec3{x * iw, y * iw, z * iw};
            };

            out_eye = cam.eye(pl);
            const engine::Vec3 p_far = unproject(0.999f);
            out_dir = {p_far.x - out_eye.x, p_far.y - out_eye.y, p_far.z - out_eye.z};
            const float len =
                std::sqrt(out_dir.x * out_dir.x + out_dir.y * out_dir.y + out_dir.z * out_dir.z);
            if (len < 1e-6f) {
                return false;
            }
            out_dir.x /= len;
            out_dir.y /= len;
            out_dir.z /= len;
            return true;
        }

        /** Slab-method ray vs AABB. @return True on a hit; out_t is the near
         * intersection distance. */
        [[nodiscard]] bool ray_aabb(const engine::Vec3& origin,
                                    const engine::Vec3& dir,
                                    float min_x,
                                    float min_y,
                                    float min_z,
                                    float max_x,
                                    float max_y,
                                    float max_z,
                                    float& out_t) {
            float t_min = 0.0f;
            float t_max = 1.0e30f;

            auto slab = [&](float o, float d, float mn, float mx) -> bool {
                if (std::fabs(d) < 1e-8f) {
                    return o >= mn && o <= mx;
                }
                float t0 = (mn - o) / d;
                float t1 = (mx - o) / d;
                if (t0 > t1) {
                    std::swap(t0, t1);
                }
                t_min = std::max(t_min, t0);
                t_max = std::min(t_max, t1);
                return t_min <= t_max;
            };

            if (!slab(origin.x, dir.x, min_x, max_x) || !slab(origin.y, dir.y, min_y, max_y) ||
                !slab(origin.z, dir.z, min_z, max_z)) {
                return false;
            }
            if (t_max < 0.0f) {
                return false;
            }
            out_t = (t_min >= 0.0f) ? t_min : t_max;
            return out_t >= 0.0f;
        }

    }  // namespace

    bool WorldPick::pick_ground(const FollowCamera& cam,
                                const LocalPlayer& pl,
                                const FloorField& floor,
                                float mx,
                                float my,
                                float sw,
                                float sh,
                                float& out_x,
                                float& out_z) {
        engine::Vec3 eye{};
        engine::Vec3 dir{};
        if (!build_screen_ray(cam, pl, mx, my, sw, sh, eye, dir)) {
            return false;
        }

        // Indoor dungeon: Pick raycasts compatibility behavior floors with no INNER_HEIGHT
        // filter, so a click further up a long ramp still lands. The feet-plane
        // fallback would miss the slope and send the cursor outside the hull.
        if (!floor.height_field().covers(pl.pos().x, pl.pos().z)) {
            constexpr float kMaxDist = 4000.0f;
            float t_hit = kMaxDist;
            if (!floor.raycast_walkable(
                    eye.x, eye.y, eye.z, dir.x, dir.y, dir.z, kMaxDist, t_hit)) {
                return false;
            }
            out_x = eye.x + dir.x * t_hit;
            out_z = eye.z + dir.z * t_hit;
            return true;
        }

        // March along the ray until we cross below the sampled floor. Indoor
        // sample ignores arch lintels (kStepHeight), so a click through a
        // doorway lands on the passage floor instead of the arch top.
        constexpr float kMaxDist = 4000.0f;
        constexpr float kStep = 8.0f;
        float prev_x = eye.x;
        float prev_y = eye.y;
        float prev_z = eye.z;
        float prev_above = 1.0f;
        bool have_prev = false;
        for (float t = kStep; t <= kMaxDist; t += kStep) {
            const float x = eye.x + dir.x * t;
            const float y = eye.y + dir.y * t;
            const float z = eye.z + dir.z * t;
            const float ground = floor.sample(x, z, pl.pos().y);
            const float above = y - ground;
            if (have_prev && prev_above > 0.0f && above <= 0.0f) {
                // Linear refine between previous and current sample.
                const float denom = prev_above - above;
                const float u = (denom > 1e-6f) ? (prev_above / denom) : 1.0f;
                out_x = prev_x + (x - prev_x) * u;
                out_z = prev_z + (z - prev_z) * u;
                return true;
            }
            prev_x = x;
            prev_y = y;
            prev_z = z;
            prev_above = above;
            have_prev = true;
            if (y < pl.pos().y - 500.0f && above < -50.0f) {
                break;
            }
        }

        // Fallback: intersect horizontal plane at feet (old behaviour).
        if (std::fabs(dir.y) < 1e-5f) {
            return false;
        }
        const float t_plane = (pl.pos().y - eye.y) / dir.y;
        if (t_plane < 1.0f) {
            return false;
        }
        out_x = eye.x + dir.x * t_plane;
        out_z = eye.z + dir.z * t_plane;
        return true;
    }

    bool WorldPick::project_world_to_screen(const FollowCamera& cam,
                                            const LocalPlayer& pl,
                                            float world_x,
                                            float world_y,
                                            float world_z,
                                            float screen_w,
                                            float screen_h,
                                            float& screen_x,
                                            float& screen_y) {
        if (screen_w < 1.0f || screen_h < 1.0f) {
            return false;
        }
        const auto proj = engine::Mat4::perspective(
            60.0f * kPi / 180.0f, screen_w / screen_h, kNearZ, cam.clip_far());
        const auto vp = engine::Mat4::multiply(proj, cam.view(pl));
        const float clip_x = vp.m[0] * world_x + vp.m[4] * world_y + vp.m[8] * world_z + vp.m[12];
        const float clip_y = vp.m[1] * world_x + vp.m[5] * world_y + vp.m[9] * world_z + vp.m[13];
        const float clip_w = vp.m[3] * world_x + vp.m[7] * world_y + vp.m[11] * world_z + vp.m[15];
        if (clip_w <= 1e-4f) {
            return false;
        }
        const float ndc_x = clip_x / clip_w;
        const float ndc_y = clip_y / clip_w;
        screen_x = (ndc_x * 0.5f + 0.5f) * screen_w;
        screen_y = (1.0f - (ndc_y * 0.5f + 0.5f)) * screen_h;
        return true;
    }

    void WorldPick::size_from_aabb(float min_x,
                                   float min_y,
                                   float min_z,
                                   float max_x,
                                   float max_y,
                                   float max_z,
                                   float scale,
                                   float& half_extent,
                                   float& height) {
        const float mesh_h = max_y - std::min(min_y, 0.0f);
        const float mesh_half = 0.5f * std::max(max_x - min_x, max_z - min_z);
        if (mesh_h < 0.5f && mesh_half < 0.5f) {
            return;
        }
        const float s = scale > 0.01f ? scale : 1.0f;
        half_extent = std::max(mesh_half, 0.5f) * s;
        height = std::max(mesh_h, 0.5f) * s;
    }

    bool WorldPick::pick_monster_box(const FollowCamera& cam,
                                     const LocalPlayer& pl,
                                     float mx,
                                     float my,
                                     float screen_w,
                                     float screen_h,
                                     float world_x,
                                     float base_y,
                                     float world_z,
                                     float half_extent,
                                     float height,
                                     float& out_t) {
        engine::Vec3 eye{};
        engine::Vec3 dir{};
        if (!build_screen_ray(cam, pl, mx, my, screen_w, screen_h, eye, dir)) {
            return false;
        }
        const float half = half_extent > 0.01f ? half_extent : kMonsterHalfExtent;
        const float h = height > 0.01f ? height : kMonsterBaseHeight;
        return ray_aabb(eye,
                        dir,
                        world_x - half,
                        base_y,
                        world_z - half,
                        world_x + half,
                        base_y + h,
                        world_z + half,
                        out_t);
    }

}  // namespace openkal::client

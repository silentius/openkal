/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/Player.h"

#include "openkal/client/Equipment.h"
#include "openkal/map/Coords.h"

#include <algorithm>
#include <cmath>

namespace openkal::client {

    char LocalPlayer::job_letter(Class c) {
        switch (c) {
        case Class::Magician:
            return 'r';
        case Class::Archer:
            return 'm';
        case Class::Thief:
            return 't';
        case Class::Knight:
        default:
            return 'w';
        }
    }

    const char* LocalPlayer::class_name(Class c) {
        switch (c) {
        case Class::Magician:
            return "Magician";
        case Class::Archer:
            return "Archer";
        case Class::Thief:
            return "Thief";
        case Class::Knight:
        default:
            return "Knight";
        }
    }

    std::vector<std::string>
    LocalPlayer::default_clothes_paths(const std::string& data_root, Class c, int face, int hair) {
        std::vector<std::string> paths;
        for (int i = 0; i < static_cast<int>(ClothPart::Count); ++i) {
            const auto part = static_cast<ClothPart>(i);
            const int index = part == ClothPart::Face
                                  ? std::clamp(face, 1, 255)
                                  : (part == ClothPart::Helmet ? std::clamp(hair, 1, 255) : 1);
            std::string p = EquipmentPaths::clothes_gb_path(data_root, c, part, 0, index);
            if (p.empty() && index != 1) {
                p = EquipmentPaths::clothes_gb_path(data_root, c, part, 0, 1);
            }
            if (!p.empty()) {
                paths.push_back(std::move(p));
            }
        }
        return paths;
    }

    void LocalPlayer::set_pos(float x, float y, float z) {
        m_pos.x = x;
        m_pos.y = y;
        m_pos.z = z;
        m_on_ground = true;
        m_vel_y = 0.0f;
        m_air_vx = 0.0f;
        m_air_vz = 0.0f;
    }

    void LocalPlayer::set_click_target(float wx, float wz) {
        if (m_dead) {
            return;
        }
        m_click_tx = wx;
        m_click_tz = wz;
        m_has_click_target = true;
    }

    void LocalPlayer::toggle_boost() {
        if (m_resting || m_dead) {
            return;
        }
        m_walking_on_air = false;
        m_boosting = !m_boosting;
    }

    bool LocalPlayer::may_stand_at(float world_x,
                                   float world_z,
                                   float fallback_y,
                                   const FloorField& floor,
                                   const map::WorldGrid* walk,
                                   int origin_patch) {
        if (walk != nullptr &&
            walk->not_moveable_world_pixel(coords::to_server(world_x, origin_patch),
                                           coords::to_server(world_z, origin_patch))) {
            // The mask has no height. A reachable bridge deck can span blocked terrain.
            const float deck_y = floor.sample(world_x, world_z, fallback_y);
            const float terrain_y = floor.sample_terrain(world_x, world_z, deck_y);
            if (deck_y <= terrain_y + FloorField::kStandableDeckEpsilon) {
                return false;
            }
        }

        // Indoor: a deck more than INNER_HEIGHT above the feet is a wall, not a
        // step (cannot climb back up a dungeon ledge). Outdoor arches use the
        // same step limit so lintels are not roofs.
        return floor.can_occupy(world_x, world_z, fallback_y);
    }

    void LocalPlayer::snap_to_ground(const FloorField& floor) {
        m_pos.y = floor.sample(m_pos.x, m_pos.z, m_pos.y);
        m_on_ground = true;
        m_vel_y = 0.0f;
        m_air_vx = 0.0f;
        m_air_vz = 0.0f;
    }

    void LocalPlayer::apply_observed_pose(const engine::Vec3& position,
                                          float yaw,
                                          bool moving,
                                          const FloorField& floor) {
        const float ground = floor.sample(position.x, position.z, position.y);
        set_pos(position.x, std::max(position.y, ground), position.z);
        m_on_ground = m_pos.y <= ground + 0.01f;
        m_moving = moving;
        if (moving) {
            m_yaw = yaw;
        }
        clear_click_target();
    }

    void LocalPlayer::update(const engine::FrameInput& in,
                             float camera_yaw_rad,
                             const FloorField& floor,
                             const map::WorldGrid* walk,
                             int origin_patch,
                             bool movement_locked) {
        engine::Vec3 forward{std::sin(camera_yaw_rad), 0.0f, std::cos(camera_yaw_rad)};
        engine::Vec3 right{std::sin(camera_yaw_rad - 3.14159265f * 0.5f),
                           0.0f,
                           std::cos(camera_yaw_rad - 3.14159265f * 0.5f)};

        if (m_dead || m_resting || movement_locked) {
            m_moving = false;
            m_running = false;
            if (m_dead || movement_locked) {
                m_has_click_target = false;
            }
            snap_to_ground(floor);
            return;
        }

        const float dt = std::max(0.0f, std::min(in.dt, 0.1f));
        auto walkable_xz = [&](float wx, float wz) {
            return may_stand_at(wx, wz, m_pos.y, floor, walk, origin_patch);
        };
        auto try_move_segment = [&](float nx, float nz) {
            if (!walkable_xz(nx, nz)) {
                return false;
            }
            const bool outdoor = floor.height_field().covers(m_pos.x, m_pos.z);
            if (!outdoor) {
                const float distance = std::hypot(nx - m_pos.x, nz - m_pos.z);
                const int steps = std::max(1, static_cast<int>(std::ceil(distance)));
                for (int step = 1; step < steps; ++step) {
                    const float t = static_cast<float>(step) / static_cast<float>(steps);
                    if (!walkable_xz(m_pos.x + (nx - m_pos.x) * t, m_pos.z + (nz - m_pos.z) * t)) {
                        return false;
                    }
                }
            }
            if (floor.has_blockers()) {
                const float dx = nx - m_pos.x;
                const float dz = nz - m_pos.z;
                const float dist = std::sqrt(dx * dx + dz * dz);
                if (dist > 1e-4f) {
                    const float inv = 1.0f / dist;
                    const auto wall_distance = [&](float height, float range, RayHitFilter filter) {
                        float hit = range;
                        (void) floor.raycast(m_pos.x,
                                             m_pos.y + height,
                                             m_pos.z,
                                             dx * inv,
                                             0.0f,
                                             dz * inv,
                                             range,
                                             hit,
                                             filter);
                        return hit;
                    };
                    const float reach = dist + kBodyRadius;
                    const float body_hit = wall_distance(kBodyHeight, reach, RayHitFilter::Walls);
                    if (body_hit < reach) {
                        if (!outdoor || !m_on_ground || body_hit <= dist) {
                            return false;
                        }
                        // A reachable tread can raise the body before the wall ahead.
                        const float rise = floor.sample(nx, nz, m_pos.y) - m_pos.y;
                        if (rise <= 0.0f || rise > FloorField::kStepHeight ||
                            wall_distance(kBodyHeight + rise, reach, RayHitFilter::Walls) < reach) {
                            return false;
                        }
                    }
                    // Catch outdoor risers above the step limit that pass under the body ray.
                    // Test only the current segment so the next riser does not veto this step.
                    constexpr float riser_probe_slack = 0.01f;
                    if (outdoor &&
                        wall_distance(FloorField::kStepHeight + riser_probe_slack,
                                      dist + riser_probe_slack,
                                      RayHitFilter::StepObstacles) < dist + riser_probe_slack) {
                        return false;
                    }
                    if (outdoor) {
                        const float upper = floor.get_floor(nx, nz);
                        const float clearance = upper - m_pos.y;
                        if (clearance > FloorField::kStepHeight &&
                            floor.get_floor(m_pos.x, m_pos.z) < upper - riser_probe_slack) {
                            // A tapered side can already overhang the feet when both forward
                            // rays miss it. A ceiling at the destination allows an underpass.
                            const float origin_y = m_pos.y + riser_probe_slack;
                            float hit = clearance - riser_probe_slack;
                            if (floor.raycast(m_pos.x,
                                              origin_y,
                                              m_pos.z,
                                              0.0f,
                                              1.0f,
                                              0.0f,
                                              hit,
                                              hit,
                                              RayHitFilter::UpwardSlopes)) {
                                hit = clearance;
                                if (!floor.raycast(nx,
                                                   origin_y,
                                                   nz,
                                                   0.0f,
                                                   1.0f,
                                                   0.0f,
                                                   hit,
                                                   hit,
                                                   RayHitFilter::Ceilings)) {
                                    return false;
                                }
                            }
                        }
                    }
                }
            }
            m_pos.x = nx;
            m_pos.z = nz;
            return true;
        };
        auto try_set_pos = [&](float nx, float nz) {
            if (try_move_segment(nx, nz)) {
                return true;
            }
            if (!m_on_ground || !floor.height_field().covers(m_pos.x, m_pos.z)) {
                return false;
            }

            // Long frames can reach past the next tread before resolving its foot height.
            constexpr float max_segment_length = 2.0f;
            const auto start = m_pos;
            const float dx = nx - start.x;
            const float dz = nz - start.z;
            const int segments =
                static_cast<int>(std::ceil(std::hypot(dx, dz) / max_segment_length));
            if (segments < 2) {
                return false;
            }
            for (int i = 1; i <= segments; ++i) {
                const float fraction = static_cast<float>(i) / static_cast<float>(segments);
                const float x = start.x + dx * fraction;
                const float z = start.z + dz * fraction;
                const float height = floor.sample(x, z, m_pos.y);
                if (height < m_pos.y - FloorField::kStepHeight || !try_move_segment(x, z)) {
                    m_pos = start;
                    return false;
                }
                m_pos.y = height;
            }
            return true;
        };

        if (!m_on_ground) {
            const float nx = m_pos.x + m_air_vx * dt;
            const float nz = m_pos.z + m_air_vz * dt;
            if (!try_set_pos(nx, nz) && !try_set_pos(nx, m_pos.z) && !try_set_pos(m_pos.x, nz)) {
                m_air_vx = 0.0f;
                m_air_vz = 0.0f;
            }
            const float dest = floor.sample(m_pos.x, m_pos.z, m_pos.y);
            const float dv = 0.5f * kGravity * dt;
            const float vy = m_vel_y - dv;
            m_vel_y = vy - dv;
            m_pos.y += vy * dt;
            if (m_pos.y <= dest) {
                m_pos.y = dest;
                m_on_ground = true;
                m_vel_y = 0.0f;
                m_air_vx = 0.0f;
                m_air_vz = 0.0f;
            }
            m_moving = true;
            m_running = false;
            return;
        }

        const float old_x = m_pos.x;
        const float old_z = m_pos.z;

        engine::Vec3 move{0, 0, 0};
        auto madd = [](engine::Vec3& p, engine::Vec3 d, float s) {
            p.x += d.x * s;
            p.z += d.z * s;
        };
        if (in.key_w) {
            madd(move, forward, 1.0f);
        }
        if (in.key_s) {
            madd(move, forward, -1.0f);
        }
        if (in.key_d) {
            madd(move, right, -1.0f);
        }
        if (in.key_a) {
            madd(move, right, 1.0f);
        }

        const float len2 = move.x * move.x + move.z * move.z;
        const bool wasd = len2 > 1e-6f;
        // Haste (skill 0 / compatibility behavior). Shift still works as a local hold-to-run
        // assist.
        const bool want_run = m_boosting || m_walking_on_air || in.key_shift;
        if (wasd) {
            m_has_click_target = false;
            const float inv = 1.0f / std::sqrt(len2);
            move.x *= inv;
            move.z *= inv;
            m_running = want_run;
            const float speed =
                (m_walking_on_air ? kWalkOnAirSpeed : (m_running ? m_run_speed : m_walk_speed)) *
                dt;
            const float nx = m_pos.x + move.x * speed;
            const float nz = m_pos.z + move.z * speed;
            if (try_set_pos(nx, nz) || try_set_pos(nx, m_pos.z) || try_set_pos(m_pos.x, nz)) {
                m_yaw = std::atan2(-move.x, -move.z);
                m_moving = true;
            } else {
                m_moving = false;
                m_running = false;
            }
        } else if (m_has_click_target) {
            const float dx = m_click_tx - m_pos.x;
            const float dz = m_click_tz - m_pos.z;
            const float dist = std::sqrt(dx * dx + dz * dz);
            if (dist < 2.0f) {
                if (try_set_pos(m_click_tx, m_click_tz)) {
                    m_has_click_target = false;
                }
                m_moving = false;
                m_running = false;
            } else {
                m_running = want_run;
                const float speed = (m_walking_on_air ? kWalkOnAirSpeed
                                                      : (m_running ? m_run_speed : m_walk_speed)) *
                                    dt;
                const float step = std::min(speed, dist);
                const float nx = m_pos.x + dx / dist * step;
                const float nz = m_pos.z + dz / dist * step;
                if (try_set_pos(nx, nz) || try_set_pos(nx, m_pos.z) || try_set_pos(m_pos.x, nz)) {
                    m_yaw = std::atan2(-dx, -dz);
                    m_moving = true;
                } else {
                    m_has_click_target = false;
                    m_moving = false;
                    m_running = false;
                }
            }
        } else {
            m_moving = false;
            m_running = false;
        }

        const float dest = floor.sample(m_pos.x, m_pos.z, m_pos.y);
        if (dest < m_pos.y - FloorField::kStepHeight) {
            m_on_ground = false;
            m_vel_y = 0.0f;
            if (dt > 1e-6f) {
                m_air_vx = (m_pos.x - old_x) / dt;
                m_air_vz = (m_pos.z - old_z) / dt;
            }
        } else {
            m_pos.y = dest;
        }
    }

    engine::Mat4 LocalPlayer::model_matrix() const {
        const float hy = m_yaw * 0.5f;
        const auto R = engine::Mat4::from_quaternion(0.0f, std::sin(hy), 0.0f, std::cos(hy));
        const auto T = engine::Mat4::translation(m_pos.x, m_pos.y, m_pos.z);
        return engine::Mat4::multiply(T, R);
    }

    void FollowCamera::update_look(const engine::FrameInput& in) {
        m_indoor_eye_valid = false;
        if (m_fly) {
            return;
        }
        // Orbit while holding right mouse (free cursor otherwise).
        if (in.mouse_right) {
            m_yaw_deg += in.mouse_dx * 0.12f;
            m_pitch_deg -= in.mouse_dy * 0.12f;
        }
        // Arrow keys: yaw / pitch without touching the mouse (doesn't fight WASD).
        constexpr float kKeyYawDegPerSec = 90.0f;
        constexpr float kKeyPitchDegPerSec = 60.0f;
        const float dt = std::max(0.0f, std::min(in.dt, 0.1f));
        if (in.key_left) {
            m_yaw_deg -= kKeyYawDegPerSec * dt;
        }
        if (in.key_right) {
            m_yaw_deg += kKeyYawDegPerSec * dt;
        }
        if (in.key_up) {
            m_pitch_deg += kKeyPitchDegPerSec * dt;
        }
        if (in.key_down) {
            m_pitch_deg -= kKeyPitchDegPerSec * dt;
        }
        if (m_pitch_deg > -5.0f) {
            m_pitch_deg = -5.0f;
        }
        if (m_pitch_deg < -60.0f) {
            m_pitch_deg = -60.0f;
        }
        // Wheel zoom always (world map); charsel/chargen never call this.
        if (std::fabs(in.mouse_scroll) > 0.0f) {
            m_distance -= in.mouse_scroll * 25.0f;
            if (m_distance < kCamDistMin) {
                m_distance = kCamDistMin;
            }
            if (m_distance > kCamDistMax) {
                m_distance = kCamDistMax;
            }
        }
    }

    void FollowCamera::start_shake(float duration_sec, float amplitude) {
        if (duration_sec <= 0.0f || amplitude <= 0.0f) {
            return;
        }
        m_shake_left_sec = std::max(m_shake_left_sec, duration_sec);
        m_shake_duration_sec = std::max(m_shake_duration_sec, duration_sec);
        m_shake_amplitude = std::max(m_shake_amplitude, amplitude);
    }

    void FollowCamera::sustain_shake(float amplitude) {
        if (amplitude <= 0.0f) {
            return;
        }
        constexpr float kSustainWindowSec = 0.2f;
        m_shake_left_sec = kSustainWindowSec;
        m_shake_duration_sec = kSustainWindowSec;
        m_shake_amplitude = amplitude;
    }

    void FollowCamera::tick_shake(float dt) {
        if (m_shake_left_sec <= 0.0f) {
            return;
        }
        const float clamped_dt = std::clamp(dt, 0.0f, 0.1f);
        m_shake_left_sec = std::max(0.0f, m_shake_left_sec - clamped_dt);
        m_shake_time_sec += clamped_dt;
        if (m_shake_left_sec == 0.0f) {
            m_shake_duration_sec = 0.0f;
            m_shake_amplitude = 0.0f;
        }
    }

    void FollowCamera::limit_indoor_zoom(const LocalPlayer& player,
                                         const FloorField& floor,
                                         float aspect) {
        m_indoor_eye_valid = false;
        if (m_fly) {
            return;
        }
        m_distance = std::min(m_distance, kCamDistMaxIndoor);
        const auto& p = player.pos();
        const float near_half_height = kNearZ * std::tan(30.0f * 3.14159265f / 180.0f);
        const float radius = std::max(2.5f, kNearZ + near_half_height * std::max(1.0f, aspect));
        engine::Vec3 target{p.x, p.y + m_look_height, p.z};
        auto desired = eye(player);  // Include shake before collision resolution.
        float ceiling = m_look_height + radius;
        if (floor.raycast(p.x, p.y + 0.1f, p.z, 0, 1, 0, ceiling, ceiling, RayHitFilter::All)) {
            target.y = std::min(target.y, p.y + ceiling - radius);
            desired.y += target.y - (p.y + m_look_height);
        }
        m_indoor_target = target;
        const engine::Vec3 delta{desired.x - target.x, desired.y - target.y, desired.z - target.z};
        const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
        if (length < 1e-4f) {
            return;
        }
        const engine::Vec3 direction{delta.x / length, delta.y / length, delta.z / length};
        float distance = length;
        // Sweep a small volume around the eye, including the near-plane corners.
        // All faces block the camera, regardless of winding or slope.
        for (float x : {-radius, 0.0f, radius}) {
            for (float y : {-radius, 0.0f, radius}) {
                for (float z : {-radius, 0.0f, radius}) {
                    float hit = length;
                    if (floor.raycast(target.x + x,
                                      target.y + y,
                                      target.z + z,
                                      direction.x,
                                      direction.y,
                                      direction.z,
                                      length,
                                      hit,
                                      RayHitFilter::All)) {
                        distance = std::min(distance, std::max(0.01f, hit - 0.1f));
                    }
                }
            }
        }
        m_indoor_eye = {target.x + direction.x * distance,
                        target.y + direction.y * distance,
                        target.z + direction.z * distance};
        m_indoor_eye_valid = true;
    }

    void FollowCamera::enter_fly(const LocalPlayer& player) {
        if (m_fly) {
            return;
        }
        m_fly_eye = eye(player);
        m_fly_yaw = 3.14159265f - yaw_rad();
        m_fly_pitch = std::clamp(m_pitch_deg * 3.14159265f / 180.0f, -1.45f, 1.45f);
        m_fly_moved = false;
        m_fly = true;
    }

    void FollowCamera::exit_fly(LocalPlayer& player, const FloorField& floor) {
        if (!m_fly) {
            return;
        }
        m_yaw_deg = (3.14159265f - m_fly_yaw) * 180.0f / 3.14159265f;
        player.set_pos(m_fly_eye.x, m_fly_eye.y, m_fly_eye.z);
        player.snap_to_ground(floor);
        player.clear_click_target();
        player.set_moving(false);
        m_fly = false;
        m_fly_moved = false;
    }

    void FollowCamera::update_fly(const engine::FrameInput& in) {
        m_fly_moved = false;
        if (!m_fly) {
            return;
        }
        m_fly_yaw -= in.mouse_dx * kFlyLookSensitivity;
        m_fly_pitch = std::clamp(m_fly_pitch - in.mouse_dy * kFlyLookSensitivity, -1.45f, 1.45f);

        const float cy = std::cos(m_fly_yaw);
        const float sy = std::sin(m_fly_yaw);
        const float cp = std::cos(m_fly_pitch);
        const float sp = std::sin(m_fly_pitch);
        const engine::Vec3 forward{sy * cp, sp, -cy * cp};
        const engine::Vec3 right{cy, 0.0f, sy};
        engine::Vec3 move{0.0f, 0.0f, 0.0f};
        if (in.key_w) {
            move.x += forward.x;
            move.y += forward.y;
            move.z += forward.z;
        }
        if (in.key_s) {
            move.x -= forward.x;
            move.y -= forward.y;
            move.z -= forward.z;
        }
        if (in.key_d) {
            move.x -= right.x;
            move.z -= right.z;
        }
        if (in.key_a) {
            move.x += right.x;
            move.z += right.z;
        }
        const float len = std::sqrt(move.x * move.x + move.y * move.y + move.z * move.z);
        const float speed =
            (in.key_shift ? kFlySprintSpeed : kFlyWalkSpeed) * std::max(in.dt, 0.0f);
        if (len > 1e-4f) {
            const float inv = speed / len;
            m_fly_eye.x += move.x * inv;
            m_fly_eye.y += move.y * inv;
            m_fly_eye.z += move.z * inv;
            m_fly_moved = true;
        }
        const float vmul = in.key_shift ? 2.5f : 1.0f;
        const float vspeed = kFlyVerticalSpeed * std::max(in.dt, 0.0f) * vmul;
        if (in.key_space) {
            m_fly_eye.y += vspeed;
            m_fly_moved = true;
        }
        if (in.key_ctrl) {
            m_fly_eye.y -= vspeed;
            m_fly_moved = true;
        }
    }

    float FollowCamera::yaw_rad() const {
        if (m_fly) {
            return 3.14159265f - m_fly_yaw;
        }
        return m_yaw_deg * 3.14159265f / 180.0f;
    }

    engine::Vec3 FollowCamera::eye(const LocalPlayer& player) const {
        if (!m_fly && m_indoor_eye_valid) {
            return m_indoor_eye;
        }
        if (m_fly) {
            const engine::Vec3 offset = shake_offset();
            return {m_fly_eye.x + offset.x, m_fly_eye.y + offset.y, m_fly_eye.z + offset.z};
        }
        const float yaw = yaw_rad();
        const float pitch = m_pitch_deg * 3.14159265f / 180.0f;
        engine::Vec3 forward{
            std::cos(pitch) * std::sin(yaw),
            std::sin(pitch),
            std::cos(pitch) * std::cos(yaw),
        };
        const engine::Vec3& p = player.pos();
        const engine::Vec3 target{p.x, p.y + m_look_height, p.z};
        const engine::Vec3 offset = shake_offset();
        return {target.x - forward.x * m_distance + offset.x,
                target.y - forward.y * m_distance + offset.y,
                target.z - forward.z * m_distance + offset.z};
    }

    engine::Mat4 FollowCamera::view(const LocalPlayer& player) const {
        if (m_fly) {
            const float cy = std::cos(m_fly_yaw);
            const float sy = std::sin(m_fly_yaw);
            const float cp = std::cos(m_fly_pitch);
            const float sp = std::sin(m_fly_pitch);
            const engine::Vec3 forward{sy * cp, sp, -cy * cp};
            const engine::Vec3 eye_pos = eye(player);
            const engine::Vec3 center{
                eye_pos.x + forward.x, eye_pos.y + forward.y, eye_pos.z + forward.z};
            const auto look = engine::Mat4::look_at(eye_pos, center, {0.0f, 1.0f, 0.0f});
            return engine::Mat4::multiply(engine::Mat4::scaling(-1.0f, 1.0f, 1.0f), look);
        }
        const engine::Vec3 e = eye(player);
        const engine::Vec3& p = player.pos();
        const engine::Vec3 target =
            m_indoor_eye_valid ? m_indoor_target : engine::Vec3{p.x, p.y + m_look_height, p.z};
        const auto look = engine::Mat4::look_at(e, target, {0, 1, 0});
        return engine::Mat4::multiply(engine::Mat4::scaling(-1, 1, 1), look);
    }

    engine::Vec3 FollowCamera::shake_offset() const {
        if (m_shake_left_sec <= 0.0f || m_shake_duration_sec <= 0.0f) {
            return {0.0f, 0.0f, 0.0f};
        }
        const float fade = std::clamp(m_shake_left_sec / m_shake_duration_sec, 0.0f, 1.0f);
        const float strength = m_shake_amplitude * fade;
        return {std::sin(m_shake_time_sec * 43.0f) * strength,
                std::sin(m_shake_time_sec * 67.0f + 0.7f) * strength * 0.55f,
                std::sin(m_shake_time_sec * 59.0f + 1.4f) * strength};
    }

}  // namespace openkal::client

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
#include "openkal/engine/engine.h"
#include "openkal/map/WorldGrid.h"
#include "openkal/protocol/Move.h"

#include <string>
#include <vector>

namespace openkal::client {

    /**
     * Player job, matching PLAYER_CLASS / GameObject::runtime state ("WRMT") — key 0..3
     * in macro characterinfo.
     */
    enum class Class : int {
        /** W → Cw_* meshes. */
        Knight = 0,

        /** R → Cr_* meshes. */
        Magician = 1,

        /** M → Cm_* meshes. */
        Archer = 2,

        /** T → Ct_* / compatibility behavior* meshes (macro characterinfo key 3). */
        Thief = 3,
    };

    /** Scale factor converting configured movement speed to client units. */
    inline constexpr float kMoveDenom = 32000.0f;

    /** Walk tick from characterinfo (mspeed 800 533). */
    inline constexpr float kWalkTick = 800.0f;

    /** Walk speed in world units per second (40). */
    inline constexpr float kWalkSpeed = kMoveDenom / kWalkTick;

    /** Run speed in world units per second (~60.04). */
    inline constexpr float kRunSpeed = protocol::kRunSpeed;

    /** Walk on the Air speed in world units per second (80). */
    inline constexpr float kWalkOnAirSpeed = protocol::kWalkOnAirSpeed;

    /** GameCharacter.cpp: runtime state && ActionNo==walk advances FrameMove by counter *
     * RUN_SPEED. */
    inline constexpr float kRunAnimSpeed = 1.5f;

    /** World units per KalOnline METER. */
    inline constexpr float kMeter = 10.0f;

    /** Player gravity in world units per second squared (9.8 * kMeter). */
    inline constexpr float kGravity = 9.8f * kMeter;

    /** Height of the body center used for wall-collision rays. */
    inline constexpr float kBodyHeight = 11.5f;

    /** Horizontal body radius used when testing dungeon walls. */
    inline constexpr float kBodyRadius = 4.0f;

    /** Minimum camera orbit distance in world units. */
    inline constexpr float kCamDistMin = 16.0f;

    /** Maximum outdoor orbit distance in world units (25.6 meters). */
    inline constexpr float kCamDistMax = 25.6f * kMeter;

    /** Maximum indoor camera orbit distance in world units. */
    inline constexpr float kCamDistMaxIndoor = 156.0f;

    /** Perspective near plane (compatible client uses 1). */
    inline constexpr float kNearZ = 1.0f;

    /** Outdoor far plane, extended to include distant mountains. */
    inline constexpr float kFarZ = 200000.0f;

    /** Indoor far plane; limits depth-buffer precision loss at dungeon seams. */
    inline constexpr float kFarZIndoor = 3200.0f;

    /** Maximum draw distance for indoor map objects. */
    inline constexpr float kFarZIndoorObject = 1500.0f;

    /** tile_view free-fly look sensitivity (radians per mouse pixel). */
    inline constexpr float kFlyLookSensitivity = 0.0024f;

    /** tile_view free-fly walk speed in world units per second. */
    inline constexpr float kFlyWalkSpeed = 1800.0f;

    /** tile_view free-fly sprint speed in world units per second. */
    inline constexpr float kFlySprintSpeed = 7000.0f;

    /** tile_view Space/Ctrl vertical speed in world units per second. */
    inline constexpr float kFlyVerticalSpeed = 1400.0f;

    /**
     * Offline-capable local player. Walks on a FloorField (terrain + OPL) so the
     * client runs without MainSvr, and mirrors server snaps when connected.
     */
    class LocalPlayer {
      public:
        /**
         * Maps a class to its lowercase mesh/motion job letter.
         * @param c Player class.
         * @return Job letter ('w', 'r', 'm' or 't').
         */
        [[nodiscard]] static char job_letter(Class c);

        /**
         * Maps a class to its display name.
         * @param c Player class.
         * @return Static, null-terminated class name.
         */
        [[nodiscard]] static const char* class_name(Class c);

        /**
         * Builds the default unequipped look: Data/Model/Clothes/C{job}_0_{part}01.gb
         * (parts SGPAFH).
         * @param data_root Client data root.
         * @param c Player class.
         * @param face Natural face mesh index.
         * @param hair Natural hair mesh index.
         * @return Existing .gb paths, in part order.
         */
        [[nodiscard]] static std::vector<std::string>
        default_clothes_paths(const std::string& data_root, Class c, int face = 1, int hair = 1);

        /** @return Player class. */
        [[nodiscard]] Class clazz() const {
            return m_clazz;
        }

        /**
         * Sets the player class.
         * @param v New class.
         */
        void set_clazz(Class v) {
            m_clazz = v;
        }

        /** @return World-space feet position. */
        [[nodiscard]] const engine::Vec3& pos() const {
            return m_pos;
        }

        /**
         * Teleports the player without running movement collision.
         * @param v New world-space position.
         */
        void set_pos(const engine::Vec3& v) {
            m_pos = v;
        }

        /**
         * Teleports the player without running movement collision.
         * @param x World X.
         * @param y World Y (feet height).
         * @param z World Z.
         */
        void set_pos(float x, float y, float z);

        /** @return Facing angle in radians. */
        [[nodiscard]] float yaw() const {
            return m_yaw;
        }

        /**
         * Sets the facing angle.
         * @param v Angle in radians.
         */
        void set_yaw(float v) {
            m_yaw = v;
        }

        /** @return Walk speed in world units per second. */
        [[nodiscard]] float walk_speed() const {
            return m_walk_speed;
        }

        /**
         * Sets the walk speed.
         * @param v World units per second.
         */
        void set_walk_speed(float v) {
            m_walk_speed = v;
        }

        /** @return Run speed in world units per second. */
        [[nodiscard]] float run_speed() const {
            return m_run_speed;
        }

        /**
         * Sets the run speed.
         * @param v World units per second.
         */
        void set_run_speed(float v) {
            m_run_speed = v;
        }

        /** @return True while the player moved during the last update. */
        [[nodiscard]] bool moving() const {
            return m_moving;
        }

        /**
         * Overrides the last-frame moving flag (used by free-fly so NetClient still
         * syncs pose).
         * @param v True when the player should be reported as moving.
         */
        void set_moving(bool v) {
            m_moving = v;
        }

        /** @return True while moving with the run/haste modifier active. */
        [[nodiscard]] bool running() const {
            return m_running;
        }

        /** @return True while haste (skill 0 / compatibility behavior) is toggled on. */
        [[nodiscard]] bool boosting() const {
            return m_boosting;
        }

        /**
         * Forces the haste toggle, bypassing the resting/dead guard of
         * toggle_boost().
         * @param v New haste state.
         */
        void set_boosting(bool v) {
            m_boosting = v;
            if (v) {
                m_walking_on_air = false;
            }
        }

        /** @return True while Walk on the Air is toggled on. */
        [[nodiscard]] bool walking_on_air() const {
            return m_walking_on_air;
        }

        /**
         * Sets Walk on the Air. Enabling it clears Run.
         * @param v New Walk on the Air state.
         */
        void set_walking_on_air(bool v) {
            m_walking_on_air = v;
            if (v) {
                m_boosting = false;
            }
        }

        /** @return True while sitting (skill 11 / C2S_REST). */
        [[nodiscard]] bool resting() const {
            return m_resting;
        }

        /**
         * Sets the sitting state.
         * @param v True to sit.
         */
        void set_resting(bool v) {
            m_resting = v;
            if (v) {
                m_walking_on_air = false;
            }
        }

        /** @return True while the player is dead (HP 0 — die clip, controls locked).
         */
        [[nodiscard]] bool dead() const {
            return m_dead;
        }

        /**
         * Sets the dead state.
         * @param v True when HP reached 0.
         */
        void set_dead(bool v) {
            m_dead = v;
        }

        /** @return True while a click-to-move destination is pending. */
        [[nodiscard]] bool has_click_target() const {
            return m_has_click_target;
        }

        /** @return World X of the pending click-to-move destination. */
        [[nodiscard]] float click_tx() const {
            return m_click_tx;
        }

        /** @return World Z of the pending click-to-move destination. */
        [[nodiscard]] float click_tz() const {
            return m_click_tz;
        }

        /**
         * Requests click-to-move towards a world position. Ignored while dead.
         * @param wx World X.
         * @param wz World Z.
         */
        void set_click_target(float wx, float wz);

        /** Drops the pending click-to-move destination. */
        void clear_click_target() {
            m_has_click_target = false;
        }

        /** Toggles haste — KalOnline Z → Send_Skill(0) / compatibility behavior / runtime state. */
        void toggle_boost();

        /** True when the local player may occupy world XZ.
         *
         * Geometry determines the reachable floor. NotMoveable area masks block terrain and
         * indoor floors, while reachable outdoor decks above terrain can span masked cells.
         * Raised decks more than FloorField::kStepHeight above fallback_y are not a step
         * (dungeon ledges and outdoor arch lintels). Collision hulls still block walls.
         * @param world_x World X.
         * @param world_z World Z.
         * @param fallback_y Current feet height used as the step-up reference.
         * @param floor Floor field for step and hull tests.
         * @param walk Optional area grid containing NotMoveable terrain masks.
         * @param origin_patch Profile origin patch used for area lookups.
         * @return True when the geometric floor and applicable area mask allow occupancy.
         */
        [[nodiscard]] static bool may_stand_at(float world_x,
                                               float world_z,
                                               float fallback_y,
                                               const FloorField& floor,
                                               const map::WorldGrid* walk,
                                               int origin_patch);

        /**
         * Snaps the feet height onto the floor under the current position.
         * @param floor Floor field to sample.
         */
        void snap_to_ground(const FloorField& floor);

        /** Presents a confirmed pose without placing the feet below a loaded standing surface.
         * Heights above the floor are preserved, including airborne poses and upper floors.
         * @param position Interpolated server-confirmed world-space feet position.
         * @param yaw Interpolated facing angle in radians.
         * @param moving Whether the observed movement segment is still active.
         * @param floor Existing terrain and walkable surface geometry.
         */
        void apply_observed_pose(const engine::Vec3& position,
                                 float yaw,
                                 bool moving,
                                 const FloorField& floor);

        /**
         * Advances movement for one frame from WASD or the pending click target.
         * @param in Frame input state.
         * @param camera_yaw_rad Camera yaw in radians, used as the movement basis.
         * @param floor Floor field for height sampling.
         * @param walk Loaded area-cell grid containing NotMoveable terrain masks.
         * @param origin_patch Profile origin patch used for area lookups.
         * @param movement_locked True to stop movement and discard click targets for this frame.
         */
        void update(const engine::FrameInput& in,
                    float camera_yaw_rad,
                    const FloorField& floor,
                    const map::WorldGrid* walk = nullptr,
                    int origin_patch = 32,
                    bool movement_locked = false);

        /** @return Model matrix (yaw rotation followed by translation). */
        [[nodiscard]] engine::Mat4 model_matrix() const;

      private:
        /** Player class. */
        Class m_clazz = Class::Knight;

        /** World-space feet position. */
        engine::Vec3 m_pos{0, 0, 0};

        /** Facing angle in radians. */
        float m_yaw = 0.0f;

        /** Walk speed in world units per second. */
        float m_walk_speed = kWalkSpeed;

        /** Run speed in world units per second. */
        float m_run_speed = kRunSpeed;

        /** True if the player moved during the last update. */
        bool m_moving = false;

        /** True while moving with haste or the hold-to-run assist. */
        bool m_running = false;

        /** Haste toggle (skill 0 / compatibility behavior). */
        bool m_boosting = false;

        /** Walk on the Air toggle (skill 88). */
        bool m_walking_on_air = false;

        /** Sitting toggle (skill 11 / C2S_REST). */
        bool m_resting = false;

        /** HP 0 — die clip plays and controls are locked. */
        bool m_dead = false;

        /** True while a click-to-move destination is pending. */
        bool m_has_click_target = false;

        /** World X of the pending click-to-move destination. */
        float m_click_tx = 0.0f;

        /** World Z of the pending click-to-move destination. */
        float m_click_tz = 0.0f;

        /** True while feet are supported (MovePlayer runtime state). */
        bool m_on_ground = true;

        /** Vertical velocity while airborne, world units per second. */
        float m_vel_y = 0.0f;

        /** Horizontal X velocity carried off a ledge. */
        float m_air_vx = 0.0f;

        /** Horizontal Z velocity carried off a ledge. */
        float m_air_vz = 0.0f;
    };

    /** Orbiting third-person camera that trails the local player. */
    class FollowCamera {
      public:
        /** @return True while /fly free-camera mode is active. */
        [[nodiscard]] bool fly_mode() const {
            return m_fly;
        }

        /** @return True when the last update_fly() moved the eye. */
        [[nodiscard]] bool fly_moved() const {
            return m_fly_moved;
        }

        /**
         * Switches to tile_view-style free-fly, capturing the current orbit eye.
         * @param player Player the orbit camera currently trails.
         */
        void enter_fly(const LocalPlayer& player);

        /**
         * Leaves free-fly, places the player under the eye, and restores orbit.
         * @param player Player to snap onto the floor under the fly eye.
         * @param floor Floor field for the landing height.
         */
        void exit_fly(LocalPlayer& player, const FloorField& floor);

        /**
         * Advances free-fly look and WASD/Space/Ctrl movement for one frame.
         * @param in Frame input state.
         */
        void update_fly(const engine::FrameInput& in);

        /** @return Orbit yaw in degrees. */
        [[nodiscard]] float yaw_deg() const {
            return m_yaw_deg;
        }

        /**
         * Sets the orbit yaw.
         * @param v Angle in degrees.
         */
        void set_yaw_deg(float v) {
            m_yaw_deg = v;
        }

        /** @return Orbit pitch in degrees (negative looks down). */
        [[nodiscard]] float pitch_deg() const {
            return m_pitch_deg;
        }

        /**
         * Sets the orbit pitch.
         * @param v Angle in degrees.
         */
        void set_pitch_deg(float v) {
            m_pitch_deg = v;
        }

        /** @return Orbit distance in world units. */
        [[nodiscard]] float distance() const {
            return m_distance;
        }

        /**
         * Sets the orbit distance.
         * @param v Distance in world units.
         */
        void set_distance(float v) {
            m_distance = v;
        }

        /** @return Height above the feet that the camera aims at. */
        [[nodiscard]] float look_height() const {
            return m_look_height;
        }

        /**
         * Sets the aim height above the feet.
         * @param v Height in world units.
         */
        void set_look_height(float v) {
            m_look_height = v;
        }

        /**
         * Sets the perspective far plane used by drawing and picking.
         * @param z Far clip in world units.
         */
        void set_clip_far(float z) {
            m_clip_far = z;
        }

        /** @return Perspective far plane matching the current draw projection. */
        [[nodiscard]] float clip_far() const {
            return m_clip_far;
        }

        /**
         * Applies mouse orbit, arrow-key orbit and wheel zoom for one frame.
         * @param in Frame input state.
         */
        void update_look(const engine::FrameInput& in);

        /**
         * Starts or strengthens an earth-shake camera impulse.
         * @param duration_sec Shake duration in seconds.
         * @param amplitude World-unit displacement at full strength.
         */
        void start_shake(float duration_sec, float amplitude);

        /**
         * Keeps an earth-shake at full strength while called every frame.
         * @param amplitude World-unit displacement at full strength.
         */
        void sustain_shake(float amplitude);

        /**
         * Advances the active earth-shake impulse.
         * @param dt Frame delta in seconds.
         */
        void tick_shake(float dt);

        /** Constrains the eye and near plane against dungeon walls, floors and ceilings.
         * Includes camera shake and permits distances below the wheel zoom minimum.
         * @param player Player the camera trails.
         * @param floor Indoor region hull used as camera blockers.
         * @param aspect Viewport width divided by height for near-plane clearance.
         */
        void limit_indoor_zoom(const LocalPlayer& player,
                               const FloorField& floor,
                               float aspect = 1.777f);

        /** Clears the cached indoor eye when returning to an outdoor map. */
        void clear_indoor_collision() {
            m_indoor_eye_valid = false;
        }

        /** @return Orbit yaw in radians. */
        [[nodiscard]] float yaw_rad() const;

        /**
         * Computes the camera eye position.
         * @param player Player the camera trails.
         * @return World-space eye position.
         */
        [[nodiscard]] engine::Vec3 eye(const LocalPlayer& player) const;

        /**
         * Computes the view matrix.
         * @param player Player the camera trails.
         * @return View matrix, mirrored on X to match the renderer convention.
         */
        [[nodiscard]] engine::Mat4 view(const LocalPlayer& player) const;

      private:
        /** @return Current deterministic shake offset in world units. */
        [[nodiscard]] engine::Vec3 shake_offset() const;

        /** Orbit yaw in degrees. */
        float m_yaw_deg = 0.0f;

        /** Orbit pitch in degrees. */
        float m_pitch_deg = -18.0f;

        /** Desired orbit distance; zoom range [kCamDistMin, kCamDistMax]. */
        float m_distance = 80.0f;

        /** Resolved eye for the current indoor frame, including shake. */
        engine::Vec3 m_indoor_eye{};
        /** Aim point lowered when a low ceiling intersects the normal look height. */
        engine::Vec3 m_indoor_target{};
        bool m_indoor_eye_valid = false;

        /** Aim height above the feet (~character torso center). */
        float m_look_height = 12.0f;

        /** Perspective far plane; indoor uses kFarZIndoor. */
        float m_clip_far = kFarZ;

        /** True while /fly free-camera mode is active. */
        bool m_fly = false;

        /** True when the last update_fly() moved the eye. */
        bool m_fly_moved = false;

        /** Free-fly eye in world space. */
        engine::Vec3 m_fly_eye{0.0f, 0.0f, 0.0f};

        /** Free-fly yaw in radians (tile_view convention). */
        float m_fly_yaw = 0.0f;

        /** Free-fly pitch in radians (tile_view convention). */
        float m_fly_pitch = -0.45f;

        /** Seconds remaining in the current camera-shake impulse. */
        float m_shake_left_sec = 0.0f;

        /** Duration used for fade-out. */
        float m_shake_duration_sec = 0.0f;

        /** Full-strength shake displacement in world units. */
        float m_shake_amplitude = 0.0f;

        /** Running phase for deterministic shake motion. */
        float m_shake_time_sec = 0.0f;
    };

}  // namespace openkal::client
